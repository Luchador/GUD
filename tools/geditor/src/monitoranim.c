#include "monitoranim.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint32_t Read32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static float ReadFloat(const unsigned char *p)
{
    uint32_t bits = Read32(p);
    float value;
    memcpy(&value, &bits, 4);
    return value;
}

void MonitorBankFree(MonitorBank *bank)
{
    free(bank->code); free(bank->images); free(bank->scripts);
    memset(bank, 0, sizeof(*bank));
}

int MonitorBankClone(MonitorBank *out, const MonitorBank *source)
{
    *out = *source;
    out->code = malloc(source->codesize);
    out->images = malloc((size_t)source->imagecount * 12);
    out->scripts = malloc((size_t)source->scriptcount * sizeof(*out->scripts));
    if (!out->code || !out->images || !out->scripts) { MonitorBankFree(out); return 0; }
    memcpy(out->code, source->code, source->codesize);
    memcpy(out->images, source->images, (size_t)source->imagecount * 12);
    memcpy(out->scripts, source->scripts, (size_t)source->scriptcount * sizeof(*out->scripts));
    return 1;
}

int MonitorAnimationDecode(MonitorAnimation *out, const unsigned char *data, size_t size)
{
    MonitorTween *tweens[4] = {&out->xscale, &out->yscale, &out->xmid, &out->ymid};
    int i;
    if (!data || size < 116) { return 0; }
    memset(out, 0, sizeof(*out));
    out->script = Read32(data);
    out->pause = (int16_t)((unsigned)data[6] << 8 | data[7]);
    out->texture = Read32(data + 8);
    out->rotation = ReadFloat(data + 12);
    for (i = 0; i < 4; i++)
    {
        MonitorTween *t = tweens[i];
        const unsigned char *p = data + 16 + i * 20;
        t->value = ReadFloat(p); t->fraction = ReadFloat(p + 4);
        t->increment = ReadFloat(p + 8); t->old = ReadFloat(p + 12); t->target = ReadFloat(p + 16);
        if (!isfinite(t->value) || !isfinite(t->fraction) || !isfinite(t->increment)
            || !isfinite(t->old) || !isfinite(t->target)) { return 0; }
        out->color[i] = data[96 + i * 3];
        out->oldcolor[i] = data[97 + i * 3];
        out->targetcolor[i] = data[98 + i * 3];
    }
    out->colorfraction = ReadFloat(data + 108);
    out->colorincrement = ReadFloat(data + 112);
    return isfinite(out->rotation) && isfinite(out->colorfraction) && isfinite(out->colorincrement);
}

void MonitorAnimationStart(MonitorAnimation *out, const MonitorBank *bank, int id, uint32_t seed)
{
    *out = bank->initial;
    out->script = bank->scripts[id >= 0 && (uint32_t)id < bank->scriptcount ? id : 0];
    out->offset = 0;
    out->random = seed ? seed : 1;
}

static const unsigned char *Code(const MonitorBank *bank, uint32_t pointer, uint32_t size)
{
    uint32_t at;
    if (pointer < bank->address) { return NULL; }
    at = pointer - bank->address;
    if (at > bank->codesize || size > bank->codesize - at) { return NULL; }
    return bank->code + at;
}

const unsigned char *MonitorAnimationImage(const MonitorAnimation *state, const MonitorBank *bank)
{
    if (state->texture < 100)
    {
        return state->texture < bank->imagecount ? bank->images + state->texture * 12 : NULL;
    }
    return Code(bank, state->texture, 12);
}

static void TweenTick(MonitorTween *t)
{
    if (t->increment <= 0) { return; }
    t->fraction += t->increment;
    if (t->fraction >= 1) { t->fraction = 1; t->increment = 0; t->value = t->target; }
    else { t->value = t->old + (t->target - t->old) * t->fraction; }
}

void MonitorAnimationTick(MonitorAnimation *s, const MonitorBank *bank)
{
    unsigned budget;
    int i, yielding = 0;
    if (s->stopped) { return; }
    for (budget = 0; !yielding && budget < 1024; budget++)
    {
        const unsigned char *p;
        uint32_t op, a = 0, b = 0, words;
        MonitorTween *t;
        if (s->offset > UINT32_MAX / 4 || s->script > UINT32_MAX - s->offset * 4) { goto invalid; }
        p = Code(bank, s->script + s->offset * 4, 4);
        if (!p || (s->script & 3)) { goto invalid; }
        op = Read32(p);
        if (op > 15) { goto invalid; }
        words = op == 0 || op == 11 || op == 12 ? 1 :
                (op >= 1 && op <= 6) || op == 10 || op == 13 ? 3 : 2;
        p = Code(bank, s->script + s->offset * 4, words * 4);
        if (!p) { goto invalid; }
        if (words > 1) { a = Read32(p + 4); }
        if (words > 2) { b = Read32(p + 8); }
        switch (op)
        {
        case 0: s->xmid.increment = s->ymid.increment = 0; break;
        case 1: case 2: case 3: case 4: case 5: case 6:
            t = op == 5 ? &s->xscale : op == 6 ? &s->yscale :
                (op == 1 || op == 3) ? &s->xmid : &s->ymid;
            t->fraction = 0; t->old = t->value;
            t->target = (int32_t)a / 1024.0f + (op <= 2 ? t->value : 0);
            t->increment = b ? 1.0f / b : INFINITY;
            break;
        case 7: s->texture = a; break;
        case 8:
            if (s->pause >= 0)
            {
                s->pause--;
                if (s->pause >= 0) { yielding = 1; words = 0; }
            }
            else { s->pause = (int16_t)a; yielding = 1; words = 0; }
            break;
        case 9: s->script = a; s->offset = 0; words = 0; break;
        case 10:
            /* Private PRNG: previewing never mutates game or editor random state. */
            s->random ^= s->random << 13; s->random ^= s->random >> 17; s->random ^= s->random << 5;
            if ((s->random >> 16) < b) { s->script = a; s->offset = 0; words = 0; }
            break;
        case 11: s->offset = 0; words = 0; break;
        case 12: yielding = 1; words = 0; break;
        case 13:
            s->colorfraction = 0; s->colorincrement = b ? 1.0f / b : INFINITY;
            for (i = 0; i < 4; i++)
            { s->oldcolor[i] = s->color[i]; s->targetcolor[i] = (unsigned char)(a >> (24 - i * 8)); }
            break;
        case 14: s->rotation = (int32_t)a * (6.28318530718f / 65536.0f); break;
        case 15: s->rotation = fmodf(s->rotation + (int32_t)a * (6.28318530718f / 65536.0f), 6.28318530718f); break;
        }
        s->offset += words;
    }
    if (!yielding) { goto invalid; }
    TweenTick(&s->xscale); TweenTick(&s->yscale); TweenTick(&s->xmid); TweenTick(&s->ymid);
    if (s->colorincrement > 0)
    {
        s->colorfraction += s->colorincrement;
        if (s->colorfraction >= 1) { s->colorfraction = 1; s->colorincrement = 0; }
        for (i = 0; i < 4; i++)
        { s->color[i] = s->oldcolor[i] + (int)((s->targetcolor[i] - s->oldcolor[i]) * s->colorfraction); }
    }
    return;
invalid:
    s->stopped = 1;
}

void MonitorAnimationUvs(const MonitorAnimation *s, float uv[4][2])
{
    float xc = s->xscale.value * .5f, ys = s->yscale.value * .5f, xs = xc, yc = ys;
    if (s->rotation != 0)
    {
        float c = cosf(s->rotation) * 1.4141999f, n = sinf(s->rotation) * 1.4141999f;
        xc *= c; ys *= n; xs *= n; yc *= c;
    }
    uv[0][0] = s->xmid.value + xc; uv[0][1] = s->ymid.value + ys;
    uv[1][0] = s->xmid.value - xs; uv[1][1] = s->ymid.value + yc;
    uv[2][0] = s->xmid.value - xc; uv[2][1] = s->ymid.value - ys;
    uv[3][0] = s->xmid.value + xs; uv[3][1] = s->ymid.value - yc;
}
