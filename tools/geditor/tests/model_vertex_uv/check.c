/* Host regression driver. Native model/glTF/edit-store code is production;
 * only project texture lookup, PNG encoding and the ROM directory are mocked.
 * The file shim is shared with the image import tests. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "modelcompile.h"
#include "modeledits.h"
#include "texload.h"

static const char *why = "";
static const char *assetpath;
static BOOL realtextures;
static BOOL wrapsizes;
static int wrapwidth=32, wrapheight=33;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #expr, why); exit(1); } } while (0)

static unsigned char *Read(const char *path, DWORD *size)
{
    FILE *file = fopen(path, "rb");
    unsigned char *data;
    long length;
    CHECK(file != NULL);
    CHECK(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file); CHECK(length > 0);
    CHECK(fseek(file, 0, SEEK_SET) == 0);
    *size = (DWORD)length; data = malloc(*size); CHECK(data != NULL);
    CHECK(fread(data, 1, *size, file) == *size); CHECK(fclose(file) == 0);
    return data;
}

BOOL TexGetProjectImageSize(const char *project, DWORD id, int *w, int *h)
{
    (void)project;
    if (wrapsizes && id==0x237) { *w=wrapwidth;*h=wrapheight;return *w>0 && *h>0; }
    *w = id == 0xeee ? 64 : 32; *h = 16;
    if (realtextures) { *w = *h = id == 0x9b6 ? 64 : 32; }
    return TRUE;
}
BOOL TexLoadProjectImage(const char *project, DWORD id, TexPixel *out, int *w, int *h)
{
    TexGetProjectImageSize(project, id, w, h);
    memset(out, 255, (size_t)*w * *h * sizeof(*out));
    return TRUE;
}
BOOL TexEncodePng(const TexPixel *pixels, int w, int h, unsigned char **data, DWORD *size)
{
    /* Image bindings come from slots; the importer does not decode these PNGs. */
    static const unsigned char png[] = {137,80,78,71,13,10,26,10};
    (void)pixels; (void)w; (void)h;
    *size = sizeof(png); *data = malloc(*size); CHECK(*data != NULL);
    memcpy(*data, png, *size); return TRUE;
}
BOOL RomLoad(const char *path, RomFile *rom, const char **reason)
{
    (void)path; (void)reason;
    memset(rom, 0, sizeof(*rom)); rom->data = Read(assetpath, &rom->size); return TRUE;
}
void RomFree(RomFile *rom) { free(rom->data); }
BOOL RomFindFile(const RomFile *rom, const char *name, DWORD *offset, DWORD *size, const char **reason)
{
    (void)name; (void)reason;
    *offset = 0; *size = rom->size; return TRUE;
}

static GltfModelImport Copy(const ModelSource *source)
{
    GltfModelImport copy = {0};
    DWORD i;
    copy.count = source->count;
    copy.vertices = malloc(copy.count * 3 * sizeof(*copy.vertices));
    copy.tags = malloc(copy.count * sizeof(*copy.tags));
    copy.sourcevertices = malloc(copy.count * 3 * sizeof(*copy.sourcevertices));
    CHECK(copy.vertices && copy.tags && copy.sourcevertices);
    memcpy(copy.vertices, source->vertices, copy.count * 3 * sizeof(*copy.vertices));
    memcpy(copy.tags, source->tags, copy.count * sizeof(*copy.tags));
    for (i = 0; i < copy.count * 3; i++)
    {
        int w = 1, h = 1;
        if (BG_TEX_ID(copy.tags[i / 3]) != BG_TEX_NONE)
        { TexGetProjectImageSize("", BG_TEX_ID(copy.tags[i / 3]), &w, &h); }
        copy.vertices[i].s /= w; copy.vertices[i].t /= h;
        copy.sourcevertices[i] = i;
    }
    return copy;
}
static void Change(GltfModelImport *copy)
{
    DWORD i;
    for (i = 0; i < copy->count * 3; i++)
    {
        /* Nonuniform deformation, fractional quantization, wrapped UVs. */
        copy->vertices[i].x *= 0.75f;
        copy->vertices[i].y += 2.25f;
        copy->vertices[i].z -= 3.75f;
        copy->vertices[i].s += 2.0f;
        copy->vertices[i].t -= 1.0f;
    }
}
static void Paint(GltfModelImport *copy, BOOL rgb)
{
    DWORD i;
    for (i = 0; i < copy->count * 3; i++)
    {
        copy->vertices[i].r = 17; copy->vertices[i].g = 99; copy->vertices[i].b = 201;
        copy->vertices[i].a = rgb ? 255 : 173;
    }
}
static int S16(const unsigned char *p)
{ int n = p[0] * 256 + p[1]; return n >= 32768 ? n - 65536 : n; }

static void Positions(const unsigned char *base, const ModelSource *source,
    const unsigned char *compiled, DWORD size, const GltfModelImport *edited)
{
    ModelSource check = {0};
    DWORD i;
    CHECK(ModelReadSource(compiled, size, &check, &why));
    CHECK(check.count == edited->count);
    for (i = 0; i < edited->count * 3; i++)
    {
        DWORD id = edited->sourcevertices[i], offset = source->vertexoffsets[id];
        const BgVertex *a = &edited->vertices[i], *b = &source->vertices[id];
        int x = (int)round(S16(base + offset) + (double)a->x - b->x);
        int y = (int)round(S16(base + offset + 2) + (double)a->y - b->y);
        int z = (int)round(S16(base + offset + 4) + (double)a->z - b->z);
        CHECK(S16(compiled + offset) == x && S16(compiled + offset + 2) == y
            && S16(compiled + offset + 4) == z);
        CHECK(fabsf(check.vertices[i].x - (b->x - S16(base + offset) + x)) < 0.001f);
        CHECK(fabsf(check.vertices[i].y - (b->y - S16(base + offset + 2) + y)) < 0.001f);
        CHECK(fabsf(check.vertices[i].z - (b->z - S16(base + offset + 4) + z)) < 0.001f);
        if (BG_TEX_ID(edited->tags[i / 3]) != BG_TEX_NONE)
        {
            CHECK(S16(compiled + offset + 8) == (int)round((double)a->s * 32 * 32));
            CHECK(S16(compiled + offset + 10) == (int)round((double)a->t * 16 * 32));
        }
    }
    ModelFreeSource(&check);
}
static void Reject(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *edit, const char *message)
{
    unsigned char *out = NULL;
    DWORD length = 0;
    CHECK(!ModelCompileImport(data, size, source, edit, "", &out, &length, &why));
    CHECK(out == NULL && length == 0 && strstr(why, message) != NULL);
}

/* Independently inspect the state each G_VTX captured. The preview loader
 * deliberately exposes unscaled ST, so position/color checks alone would not
 * catch a lost gSPTexture scale or a wrong lighting/fog mode on an extra load. */
typedef struct VertexState { DWORD matrix, mode, scale, geometry; } VertexState;
static DWORD Word(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }
static VertexState *VertexStates(const unsigned char *data, const ModelSource *source)
{
    VertexState *states = calloc(source->count * 3, sizeof(*states));
    DWORD list, face = 0;
    CHECK(states != NULL);
    for (list = 0; list < source->listcount; list++)
    {
        const ModelSourceList *part = &source->lists[list];
        VertexState state = {0}, cache[16];
        DWORD pc;
        if (part->preserve) { continue; }
        state.mode = part->initial.modeword0; state.scale = part->initial.modeword1;
        for (pc = part->offset; pc < part->end; pc += 8)
        {
            const unsigned char *cmd = data + pc;
            if (cmd[0] == 1) { state.matrix = Word(cmd + 4); }
            if (cmd[0] == 0xbb) { state.mode = Word(cmd); state.scale = Word(cmd + 4); }
            if (cmd[0] == 0xb6) { state.geometry &= ~Word(cmd + 4); }
            if (cmd[0] == 0xb7) { state.geometry |= Word(cmd + 4); }
            if (cmd[0] == 4)
            {
                int i, first = cmd[1] & 15, end = first + (cmd[1] >> 4) + 1;
                CHECK(end <= 16);
                for (i = first; i < end; i++) { cache[i] = state; }
            }
            while (face < source->count && source->faces[face].list == list && source->faces[face].command == pc)
            {
                int indices[3], k, tri = source->faces[face].slot;
                if (cmd[0] == 0xbf) { for (k = 0; k < 3; k++) { indices[k] = cmd[5 + k] / 10; } }
                else
                {
                    CHECK(cmd[0] == 0xb1);
                    indices[0] = cmd[7 - tri] & 15; indices[1] = cmd[7 - tri] >> 4;
                    indices[2] = (cmd[3 - tri / 2] >> ((tri % 2) * 4)) & 15;
                }
                for (k = 0; k < 3; k++) { states[face * 3 + k] = cache[indices[k]]; }
                face++;
            }
        }
    }
    CHECK(face == source->count);
    return states;
}

/* Compare each surviving source corner, including after face reordering. The
 * output deliberately has new native offsets, while local XYZ and the native
 * flag/normal bytes must still come from the original corner's binding. */
static void Corners(const unsigned char *base, const ModelSource *source,
    const unsigned char *compiled, DWORD size, const GltfModelImport *edited)
{
    ModelSource check = {0};
    DWORD i, list;
    VertexState *beforestate, *afterstate;
    CHECK(ModelReadSource(compiled, size, &check, &why));
    CHECK(check.count == edited->count);
    beforestate = VertexStates(base, source); afterstate = VertexStates(compiled, &check);
    for (i = 0; i < edited->count * 3; i++)
    {
        DWORD id = edited->sourcevertices[i], face = 0, j, before = source->vertexoffsets[id], after;
        const BgVertex *a = &edited->vertices[i], *old = &source->vertices[id], *b;
        int w, h;
        for (j = 0; j < edited->count; j++) { if (edited->sourcevertices[j * 3] / 3 < id / 3) { face++; } }
        b = &check.vertices[face * 3 + id % 3]; after = check.vertexoffsets[face * 3 + id % 3];
        CHECK(beforestate[id].matrix == afterstate[face * 3 + id % 3].matrix);
        CHECK(((beforestate[id].geometry ^ afterstate[face * 3 + id % 3].geometry) & 0x001f0004u) == 0);
        if (BG_TEX_ID(edited->tags[i / 3]) != BG_TEX_NONE)
        {
            CHECK(beforestate[id].mode == afterstate[face * 3 + id % 3].mode);
            CHECK(beforestate[id].scale == afterstate[face * 3 + id % 3].scale);
        }
        CHECK(fabs(b->x - (old->x - S16(base + before) + round(S16(base + before) + (double)a->x - old->x))) < 0.001);
        CHECK(fabs(b->y - (old->y - S16(base + before + 2) + round(S16(base + before + 2) + (double)a->y - old->y))) < 0.001);
        CHECK(fabs(b->z - (old->z - S16(base + before + 4) + round(S16(base + before + 4) + (double)a->z - old->z))) < 0.001);
        CHECK(!memcmp(base + before + 6, compiled + after + 6, 2));
        if (source->faces[id / 3].normalmask & (1u << (id % 3)))
        { CHECK(!memcmp(base + before + 12, compiled + after + 12, 3)); }
        CHECK(a->r == b->r && a->g == b->g && a->b == b->b && a->a == b->a);
        CHECK(BG_TEX_ID(check.tags[face]) == BG_TEX_ID(edited->tags[i / 3]));
        CHECK(check.faces[face].normalmask == source->faces[id / 3].normalmask);
        if (BG_TEX_ID(edited->tags[i / 3]) != BG_TEX_NONE)
        {
            TexGetProjectImageSize("", BG_TEX_ID(edited->tags[i / 3]), &w, &h);
            CHECK(b->s == round((double)a->s * w * 32) / 32);
            CHECK(b->t == round((double)a->t * h * 32) / 32);
        }
    }
    /* Segment-4 loads must be inside the expanded runtime array, including
     * on a part's secondary pass. A file-absolute clone is not a substitute. */
    for (list = 0; list < check.listcount; list++)
    {
        const ModelSourceList *part = &check.lists[list];
        DWORD pc, n = (DWORD)compiled[part->vertexpointer + 4] * 256 + compiled[part->vertexpointer + 5];
        if (part->preserve) { continue; }
        if (part->pointusagepointer && Word(base + source->lists[list].pointusagepointer))
        {
            const ModelSourceList *old = &source->lists[list];
            DWORD oldcount = (DWORD)base[old->vertexpointer + 4] * 256 + base[old->vertexpointer + 5];
            DWORD oldlinks = Word(base + old->pointusagepointer) & 0xffffffu;
            DWORD newlinks = Word(compiled + part->pointusagepointer) & 0xffffffu;
            unsigned char *seen = calloc(n, 1);
            DWORD j;
            CHECK(seen != NULL && n >= oldcount);
            CHECK(Word(base + old->vertexpointer + 8) == Word(compiled + part->vertexpointer + 8));
            for (j = 0; j < oldcount; j++)
            {
                int index = S16(compiled + newlinks + j * 2);
                while (index >= (int)oldcount)
                {
                    CHECK(index < (int)n && !seen[index]); seen[index] = 1;
                    CHECK(!memcmp(compiled + part->vertexbase + j * 16, compiled + part->vertexbase + index * 16, 6));
                    index = S16(compiled + newlinks + index * 2);
                }
                CHECK(index == S16(base + oldlinks + j * 2));
            }
            for (j = oldcount; j < n; j++) { CHECK(seen[j]); }
            free(seen);
        }
        for (pc = part->offset; pc < part->end; pc += 8)
        {
            const unsigned char *cmd = compiled + pc;
            if (cmd[0] == 4) { CHECK((cmd[1] & 15) + (cmd[1] >> 4) + 1 <= 16); }
            if (cmd[0] == 4 && cmd[4] == 4)
            {
                DWORD offset = (DWORD)cmd[5] << 16 | (DWORD)cmd[6] << 8 | cmd[7];
                CHECK(offset + ((cmd[1] >> 4) + 1) * 16u <= n * 16u);
            }
        }
    }
    free(beforestate); free(afterstate); ModelFreeSource(&check);
}

static void Seam(const unsigned char *data, DWORD size, const ModelSource *source, const GltfModelImport *edit)
{
    unsigned char *out, *twice;
    DWORD length, twicesize;
    ModelSource rebuilt = {0};
    GltfModelImport again;
    CHECK(ModelCompileImport(data, size, source, edit, "", &out, &length, &why));
    CHECK(length != size || memcmp(out, data, size));
    Corners(data, source, out, length, edit);
    CHECK(ModelReadSource(out, length, &rebuilt, &why));
    again = Copy(&rebuilt);
    CHECK(ModelCompileImport(out, length, &rebuilt, &again, "", &twice, &twicesize, &why));
    CHECK(twicesize == length && !memcmp(twice, out, length));
    free(twice); GltfFreeModelImport(&again); ModelFreeSource(&rebuilt); free(out);
}

static void Unit(const unsigned char *data, DWORD size, ModelSource *source)
{
    GltfModelImport edit = Copy(source);
    unsigned char *out = NULL, *mask = calloc(size, 1);
    DWORD length, i, first = 0, second = 0;
    CHECK(mask != NULL);
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    CHECK(length == size && !memcmp(out, data, size)); free(out);
    Change(&edit);
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    CHECK(length == size); Positions(data, source, out, length, &edit);
    for (i = 0; i < source->count * 3; i++)
    {
        DWORD offset = source->vertexoffsets[i];
        memset(mask + offset, 1, 6); memset(mask + offset + 8, 1, 4);
    }
    for (i = 0; i < size; i++) { CHECK(mask[i] || data[i] == out[i]); }
    CHECK(memcmp(out, data, size)); free(out); free(mask);
    GltfFreeModelImport(&edit);

    /* These records must remain in their original matrix-local spaces. */
    if (strstr(assetpath, "mixed"))
    {
        CHECK(source->count == 2);
        CHECK(source->vertices[0].x == 10 && source->vertices[2].x == 130);
    }
    for (i = 1; i < source->count * 3 && !second; i++)
    {
        DWORD j;
        for (j = 0; j < i; j++)
        { if (source->vertexoffsets[i] == source->vertexoffsets[j]) { first = j; second = i; break; } }
    }
    CHECK(second != 0);
    edit = Copy(source);
    edit.vertices[first].x += 8;
    Reject(data, size, source, &edit, "different positions");
    edit.vertices[first].x -= 8; edit.vertices[first].s += 1;
    Seam(data, size, source, &edit);
    GltfFreeModelImport(&edit);
    edit = Copy(source);
    edit.vertices[0].x = NAN; Reject(data, size, source, &edit, "position");
    edit.vertices[0].x = 100000; Reject(data, size, source, &edit, "position");
    edit.vertices[0].x = source->vertices[0].x;
    edit.vertices[0].s = INFINITY; Reject(data, size, source, &edit, "UV");
    edit.vertices[0].s = 100000; Reject(data, size, source, &edit, "UV");
    GltfFreeModelImport(&edit);
    edit = Copy(source); edit.vertices[first].r ^= 128;
    Seam(data, size, source, &edit); GltfFreeModelImport(&edit);
    edit = Copy(source); edit.vertices[first].a ^= 128;
    Seam(data, size, source, &edit); GltfFreeModelImport(&edit);
    edit = Copy(source);
    for (i = 0; i < edit.count * 3; i++)
    {
        edit.vertices[i].s += (float)(i % 7) * 0.25f;
        edit.vertices[i].r = i & 255; edit.vertices[i].g = (i >> 8) & 255;
        edit.vertices[i].a = 100 + i % 155;
    }
    Seam(data, size, source, &edit);
    /* Seams survive triangle deletion and material removal in the same edit. */
    edit.count--; edit.tags[0] = BG_TEX_NONE;
    Seam(data, size, source, &edit); GltfFreeModelImport(&edit);
    edit = Copy(source); edit.sourcevertices[1] = 2; edit.sourcevertices[2] = 1;
    Reject(data, size, source, &edit, "winding"); GltfFreeModelImport(&edit);
    edit = Copy(source); edit.tags[0] = 0xeee;
    for (i=0;i<3;i++) { edit.vertices[i].s=i==1;edit.vertices[i].t=i==2; }
    CHECK(ModelCompileImport(data,size,source,&edit,"",&out,&length,&why));
    { ModelSource resized={0};CHECK(ModelReadSource(out,length,&resized,&why));
      CHECK(BG_TEX_ID(resized.tags[0])==0xeee && resized.vertices[1].s==64 && resized.vertices[2].t==16);
      ModelFreeSource(&resized); }
    free(out);GltfFreeModelImport(&edit);
    edit = Copy(source);
    {
        BgRenderFlags saved = source->flags[0];
        source->flags[0] |= BG_RENDER_ENVIRONMENT; edit.vertices[0].s += 1;
        Reject(data, size, source, &edit, "reflection UVs"); source->flags[0] = saved;
    }
    GltfFreeModelImport(&edit);

    /* Exporters may rotate a triangle's corner order without reversing it. */
    edit = Copy(source);
    {
        BgVertex saved = edit.vertices[0];
        edit.vertices[0] = edit.vertices[1]; edit.vertices[1] = edit.vertices[2]; edit.vertices[2] = saved;
        edit.sourcevertices[0] = 1; edit.sourcevertices[1] = 2; edit.sourcevertices[2] = 0;
        CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
        CHECK(length == size && !memcmp(out, data, size)); free(out);
    }
    GltfFreeModelImport(&edit);

    /* Material removal and same-size reassignment can accompany deformation. */
    edit = Copy(source); Change(&edit); Paint(&edit, FALSE);
    for (i = 0; i < edit.count; i++) { edit.tags[i] = i ? 0xffd : BG_TEX_NONE; }
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    Positions(data, source, out, length, &edit);
    {
        ModelSource check = {0};
        CHECK(ModelReadSource(out, length, &check, &why));
        for (i = 0; i < edit.count; i++) { CHECK(BG_TEX_ID(check.tags[i]) == edit.tags[i]); }
        ModelFreeSource(&check);
    }
    free(out); GltfFreeModelImport(&edit);

    /* Exercise triangle-command rebuilding as well as the in-place path. */
    edit = Copy(source); Change(&edit); Paint(&edit, FALSE); edit.count--;
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    Positions(data, source, out, length, &edit);
    {
        ModelSource rebuilt = {0};
        GltfModelImport again;
        unsigned char *twice;
        DWORD twicesize;
        CHECK(ModelReadSource(out, length, &rebuilt, &why));
        again = Copy(&rebuilt);
        CHECK(ModelCompileImport(out, length, &rebuilt, &again, "", &twice, &twicesize, &why));
        CHECK(twicesize == length && !memcmp(twice, out, length));
        free(twice); GltfFreeModelImport(&again); ModelFreeSource(&rebuilt);
    }
    free(out); GltfFreeModelImport(&edit);
    printf("PASS native edits, joint spaces, preserved commands, validation, face deletion: %s\n", assetpath);
}

static void Dynamic(const unsigned char *data, DWORD size, const ModelSource *source)
{
    GltfModelImport edit = Copy(source);
    unsigned char *out;
    DWORD length, i;
    CHECK(source->count == 2 && source->listcount == 2 && source->lists[1].preserve);
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    CHECK(length == size && !memcmp(out, data, size)); free(out);
    for (i = 0; i < edit.count * 3; i++) { edit.vertices[i].y += 1; }
    Reject(data, size, source, &edit, "dynamic model effect");
    GltfFreeModelImport(&edit);
    edit = Copy(source); Paint(&edit, FALSE);
    Reject(data, size, source, &edit, "dynamic model effect");
    GltfFreeModelImport(&edit);
    printf("PASS shared dynamic vertices are preserved\n");
}

static void Colors(const unsigned char *base, const ModelSource *source,
    const unsigned char *compiled, DWORD size, const GltfModelImport *edited)
{
    ModelSource check = {0};
    unsigned char *mask = calloc(size, 1);
    DWORD i;
    CHECK(mask && ModelReadSource(compiled, size, &check, &why));
    CHECK(check.count == edited->count);
    for (i = 0; i < edited->count * 3; i++)
    {
        DWORD offset = source->vertexoffsets[i];
        const BgVertex *a = &edited->vertices[i], *b = &check.vertices[i];
        CHECK(b->r == a->r && b->g == a->g && b->b == a->b && b->a == a->a);
        if (!(source->faces[i / 3].normalmask & (1u << (i % 3))))
        { CHECK(compiled[offset + 12] == a->r && compiled[offset + 13] == a->g && compiled[offset + 14] == a->b); }
        else { CHECK(!memcmp(base + offset + 12, compiled + offset + 12, 3)); }
        memset(mask + offset + 12, 1, 4);
    }
    for (i = 0; i < ModelMaterialsNativeSize(compiled,size); i++) { CHECK(mask[i] || base[i] == compiled[i]); }
    free(mask); ModelFreeSource(&check);
}

static void Inherited(const unsigned char *data, DWORD size, ModelSource *source)
{
    GltfModelImport edit = Copy(source);
    unsigned char *out;
    DWORD length;
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    CHECK(length == size && !memcmp(out, data, size)); free(out);
    edit.vertices[3].r ^= 128;
    Reject(data, size, source, &edit, "inherited or unsupported RSP");
    GltfFreeModelImport(&edit);
    printf("PASS inherited RSP state is not guessed during splitting\n");
}

static void Special(const unsigned char *data, DWORD size, ModelSource *source, const char *kind)
{
    GltfModelImport edit = Copy(source);
    unsigned char *out;
    DWORD length, i;
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    CHECK(length == size && !memcmp(out, data, size)); free(out);
    if (!strcmp(kind, "normals") || !strcmp(kind, "reflection"))
    {
        CHECK(source->faces[0].normalmask == 7 && source->faces[1].normalmask == 7);
        CHECK(!(source->faces[0].state.geometrymode & 0x20000)); /* cleared after G_VTX */
        for (i = 0; i < edit.count * 3; i++) { edit.vertices[i].a = 173; }
        CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
        Colors(data, source, out, length, &edit); free(out);
        edit.vertices[1].a = 99; Seam(data, size, source, &edit);
        edit.vertices[0].r ^= 128;
        Reject(data, size, source, &edit, "lighting normals");
    }
    else if (!strcmp(kind, "shared-normals"))
    {
        CHECK(source->faces[0].normalmask == 7 && source->faces[1].normalmask == 0);
        for (i = 3; i < 6; i++) { edit.vertices[i].r = 17; }
        Seam(data, size, source, &edit);
    }
    else
    {
        BOOL shared = !strcmp(kind, "shared-alpha");
        for (i = 0; i < edit.count * 3; i++)
        {
            edit.vertices[i].r = 17; edit.vertices[i].g = 99; edit.vertices[i].b = 201;
            if (shared && i < 3) { edit.vertices[i].a = 173; }
        }
        CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
        CHECK(length == size); Colors(data, source, out, length, &edit);
        if (!shared)
        {
            for (i = 0; i < edit.count * 3; i++)
            {
                DWORD offset = source->vertexoffsets[i];
                CHECK(edit.vertices[i].a == 128 && out[offset + 15] == data[offset + 15]);
            }
        }
        free(out);
        edit.vertices[3].a = 100;
        Reject(data, size, source, &edit, "material controls");
    }
    GltfFreeModelImport(&edit);
    printf("PASS native color semantics: %s\n", kind);
}

static void PropertyGuards(const unsigned char *data, DWORD size, const ModelSource *source)
{
    unsigned char *out = NULL;
    DWORD length = 0, face = 0, invalid = source->count;
    ModelSource custom = {0};
    CHECK(!ModelCompileProperties(data, size, source, &invalid, 1, -1, 1, -1, -1, &out, &length, &why));
    CHECK(out == NULL && length == 0);
    CHECK((source->faces[0].state.geometryknown & 0x3000) != 0x3000);
    CHECK(!ModelCompileProperties(data, size, source, &face, 1, 0, -1, -1, -1, &out, &length, &why));
    CHECK(strstr(why, "inherit culling") && out == NULL && length == 0);
    CHECK(ModelCompileProperties(data, size, source, &face, 1, -1, 1, -1, -1, &out, &length, &why));
    free(out); out = NULL;
    CHECK(ModelReadSource(data, size, &custom, &why));
    custom.faces[0].state.othermodehigh = 0x00300000; /* fill cycle has no surface preset */
    CHECK(!ModelCompileProperties(data, size, &custom, &face, 1, -1, 1, -1, -1, &out, &length, &why));
    CHECK(strstr(why, "custom render pipeline") && out == NULL && length == 0);
    ModelFreeSource(&custom);
    puts("PASS property validation and inherited-culling/custom-cycle guards");
}

static void Properties(const unsigned char *data, DWORD size, const ModelSource *source, const char *project)
{
    DWORD chosen[2], count = 0, i, length = 0, revision, savedsize, replacementsize;
    unsigned char *out = NULL, *again = NULL, *replacement = NULL;
    const unsigned char *saved;
    ModelSource check = {0}, loaded = {0};
    char path[MAX_PATH];
    GltfModelImport roundtrip = {0};
    /* Pick two nonadjacent faces with authored culling, including TRI4 slots.
       Deliberately retain other faces in each same native draw command. */
    for (i = 0; i < source->count && count < 2; i++)
    {
        if ((source->faces[i].state.geometryknown & 0x3000) == 0x3000
            && (!count || i > chosen[0] + 1)) { chosen[count++] = i; }
    }
    CHECK(count == 2);
    for (int mode = 0; mode < 3; mode++)
    {
        CHECK(ModelCompileProperties(data, size, source, chosen, count, 1, mode, 1, 2, &out, &length, &why));
        CHECK(ModelReadSource(out, length, &check, &why));
        CHECK(check.count == source->count);
        for (i = 0; i < source->count; i++)
        {
            BOOL edited = i == chosen[0] || i == chosen[1];
            CHECK(!memcmp(check.vertices + i * 3, source->vertices + i * 3, 3 * sizeof(BgVertex)));
            CHECK(!memcmp(check.vertexoffsets + i * 3, source->vertexoffsets + i * 3, 3 * sizeof(DWORD)));
            CHECK(check.tags[i] == source->tags[i]);
            CHECK(check.faces[i].normalmask == source->faces[i].normalmask);
            CHECK(check.faces[i].state.othermodehigh == source->faces[i].state.othermodehigh);
            if (edited)
            {
                DWORD mask = ((source->faces[i].state.othermodehigh & 0x00300000u) == 0 ? 0xF0000u : 0x30000u) | 0xFFF8u;
                CHECK((check.faces[i].state.geometrymode & 0x3000) == 0x2000);
                CHECK((check.flags[i] & BG_RENDER_WRAP_MASK) == (BG_RENDER_CLAMP_S | BG_RENDER_MIRROR_T));
                CHECK((check.faces[i].state.othermode & ~mask) == (source->faces[i].state.othermode & ~mask));
                CHECK(!!(check.flags[i] & BG_RENDER_BLEND) == (mode == 2));
                CHECK(!!(check.flags[i] & BG_RENDER_ALPHA_TEST) == (mode == 1));
                CHECK(!!(check.flags[i] & BG_RENDER_DEPTH_WRITE) == (mode != 2 && !!(source->flags[i] & BG_RENDER_DEPTH_TEST)));
            }
            else
            {
                CHECK(check.faces[i].state.geometrymode == source->faces[i].state.geometrymode);
                CHECK(check.faces[i].state.othermode == source->faces[i].state.othermode);
                CHECK(check.flags[i] == source->flags[i]);
            }
        }
        /* Reapplying the same settings is byte-exact and never grows the DL. */
        CHECK(ModelCompileProperties(out, length, &check, chosen, count, 1, mode, 1, 2, &again, &savedsize, &why));
        CHECK(savedsize == length && !memcmp(again, out, length)); free(again); again = NULL;
        /* Source identity and native state survive the geometry compiler too. */
        roundtrip = Copy(&check);
        Change(&roundtrip);
        CHECK(ModelCompileImport(out, length, &check, &roundtrip, "", &again, &savedsize, &why));
        CHECK(ModelReadSource(again, savedsize, &loaded, &why));
        for (i = 0; i < check.count; i++) { CHECK(loaded.flags[i] == check.flags[i]); }
        ModelFreeSource(&loaded); GltfFreeModelImport(&roundtrip); free(again); again = NULL;
        ModelFreeSource(&check); free(out); out = NULL;
    }
    CHECK(ModelEditsReadSource(project, "Pjungle3_treeZ", &loaded, &revision, &why)); ModelFreeSource(&loaded);
    CHECK(ModelEditsSetProperties(project, "Pjungle3_treeZ", revision, chosen, count, 1, 1, 1, 2, &why));
    CHECK(ModelEditsHasUnsaved());
    CHECK(!ModelEditsSetProperties(project, "Pjungle3_treeZ", revision, chosen, count, 0, 2, -1, -1, &why));
    CHECK(strstr(why, "revision"));
    CHECK(ModelEditsSave(project, &why)); ModelEditsReset();
    saved = ModelEditsGetData(project, "Pjungle3_treeZ", &savedsize, &why); CHECK(saved != NULL);
    CHECK(ModelReadSource(saved, savedsize, &check, &why));
    CHECK((check.flags[chosen[0]] & (BG_RENDER_CULL_BACK | BG_RENDER_ALPHA_TEST))
        == (BG_RENDER_CULL_BACK | BG_RENDER_ALPHA_TEST));
    CHECK(ModelEditsReadReplacement(project, "Pjungle3_treeZ", data, size, &replacement, &replacementsize, &why) == 1);
    CHECK(replacementsize == ModelMaterialsNativeSize(saved,savedsize) && !memcmp(replacement, saved, replacementsize)); free(replacement);
    snprintf(path, sizeof(path), "%s/models/objects/Pjungle3_treeZ.gltf", project);
    CHECK(GltfReadModelImport(path, ModelDataHash(saved, savedsize), &roundtrip, &why));
    ModelMaterialsMatch(&roundtrip.materials,&check.materials);
    for (i=0;i<roundtrip.count;i++) roundtrip.tags[i]=roundtrip.materials.slots[roundtrip.materials.faces[i].slot].texture;
    CHECK(ModelCompileImport(saved, savedsize, &check, &roundtrip, project, &again, &length, &why));
    CHECK(length == ModelMaterialsNativeSize(saved,savedsize) && !memcmp(again, saved, length)); free(again);
    GltfFreeModelImport(&roundtrip); ModelFreeSource(&check); ModelEditsReset();
    printf("PASS selected-face properties, TRI4 splitting, unchanged neighbours, save/reload/ROM and Blender round trip: %s\n", assetpath);
}

/* Per-face wrap edits must retain every other native property, including the
 * other axis, authored repeat alias 3, detail texture IDs and vertex loads. */
static void CheckWrapping(const ModelSource *base, const ModelSource *edited,
    const DWORD *chosen, DWORD count, int u, int v)
{
    CHECK(base->count == edited->count && base->listcount == edited->listcount);
    for (DWORD i = 0; i < base->count; i++)
    {
        BgMaterial expected = base->faces[i].material;
        BOOL selected = FALSE;
        for (DWORD j = 0; j < count; j++) { selected |= chosen[j] == i; }
        if (selected && BgMaterialTextureId(&expected) != BG_TEX_NONE)
        {
            if (u >= 0) { BgMaterialSetWrap(&expected, FALSE, u); }
            if (v >= 0) { BgMaterialSetWrap(&expected, TRUE, v); }
        }
        CHECK(!memcmp(&expected, &edited->faces[i].material, sizeof(expected)));
        CHECK(!memcmp(&base->faces[i].state, &edited->faces[i].state, sizeof(BgRenderState)));
        CHECK(!memcmp(base->vertices + i*3, edited->vertices + i*3, 3*sizeof(BgVertex)));
        CHECK(!memcmp(base->vertexoffsets + i*3, edited->vertexoffsets + i*3, 3*sizeof(DWORD)));
        CHECK(base->tags[i] == edited->tags[i]);
        CHECK(base->faces[i].normalmask == edited->faces[i].normalmask);
        CHECK((base->flags[i] & ~BG_RENDER_WRAP_MASK) == (edited->flags[i] & ~BG_RENDER_WRAP_MASK));
        /* These are the flags used by both the viewport sampler and glTF. */
        if (BgMaterialTextureId(&expected) != BG_TEX_NONE)
        {
            int s = BgMaterialGetWrap(&expected, FALSE), t = BgMaterialGetWrap(&expected, TRUE);
            CHECK(!!(edited->flags[i] & BG_RENDER_CLAMP_S) == (s == 1));
            CHECK(!!(edited->flags[i] & BG_RENDER_MIRROR_S) == (s == 2));
            CHECK(!!(edited->flags[i] & BG_RENDER_CLAMP_T) == (t == 1));
            CHECK(!!(edited->flags[i] & BG_RENDER_MIRROR_T) == (t == 2));
        }
    }
}
static void Wrapping(const unsigned char *data, DWORD size, const ModelSource *source, const char *project)
{
    DWORD chosen[3] = {0, source->count > 3 ? 3 : 1, source->count-1};
    DWORD length = 0, againlength = 0, revision, romsize, stable[3] = {0};
    unsigned char *out = NULL, *again = NULL, *rom = NULL;
    const unsigned char *saved;
    ModelSource check = {0}, loaded = {0};
    char path[MAX_PATH];
    GltfModelImport roundtrip = {0};
    for (int u = -1; u < 3; u++) for (int v = -1; v < 3; v++)
    {
        CHECK(ModelCompileProperties(data,size,source,chosen,3,-1,-1,u,v,&out,&length,&why));
        CHECK(ModelReadSource(out,length,&check,&why));
        CheckWrapping(source,&check,chosen,3,u,v);
        CHECK(ModelCompileProperties(out,length,&check,chosen,3,-1,-1,u,v,&again,&againlength,&why));
        CHECK(againlength == length && !memcmp(again,out,length));
        free(again); again = NULL; free(out); out = NULL; ModelFreeSource(&check);
    }
    CHECK(!ModelCompileProperties(data,size,source,chosen,3,-1,-1,3,0,&out,&length,&why));
    CHECK(!out && !length);
    CHECK(!ModelCompileProperties(data,size,source,chosen,3,-1,-1,0,-2,&out,&length,&why));
    CHECK(!out && !length);
    for (DWORD i=0;i<source->count;i++) if (BgMaterialTextureId(&source->faces[i].material) == BG_TEX_NONE)
    {
        CHECK(ModelCompileProperties(data,size,source,&i,1,-1,-1,1,2,&out,&length,&why));
        CHECK(length == size && !memcmp(out,data,size)); free(out); out = NULL;
    }
    /* Repeated changes must not accumulate obsolete wrap restores/binds. */
    out = malloc(size); CHECK(out); memcpy(out,data,size); length = size;
    for (int pass = 0; pass < 30; pass++)
    {
        int u = pass%3, v = (pass+1)%3;
        CHECK(ModelReadSource(out,length,&check,&why));
        CHECK(ModelCompileProperties(out,length,&check,chosen,3,-1,-1,u,v,&again,&againlength,&why));
        free(out); out = again; length = againlength; again = NULL;
        CHECK(ModelReadSource(out,length,&loaded,&why));
        CheckWrapping(&check,&loaded,chosen,3,u,v);
        ModelFreeSource(&loaded);
        if (pass >= 6) { CHECK(length == stable[u]); }
        stable[u] = length;
        ModelFreeSource(&check);
    }
    free(out); out = NULL;
    CHECK(ModelEditsReadSource(project,"Pjungle3_treeZ",&check,&revision,&why));
    /* Force a change even if this fixture starts mirrored on both axes. */
    int u = (BgMaterialGetWrap(&check.faces[0].material,FALSE)+1)%3;
    int v = (BgMaterialGetWrap(&check.faces[0].material,TRUE)+2)%3;
    CHECK(ModelEditsSetProperties(project,"Pjungle3_treeZ",revision,chosen,3,-1,-1,u,v,&why));
    CHECK(ModelEditsHasUnsaved());
    CHECK(!ModelEditsSetProperties(project,"Pjungle3_treeZ",revision,chosen,3,-1,-1,1,1,&why));
    CHECK(strstr(why,"revision"));
    ModelFreeSource(&check);
    CHECK(ModelEditsReadSource(project,"Pjungle3_treeZ",&check,&revision,&why));
    CheckWrapping(source,&check,chosen,3,u,v);
    ModelUVEdit uv = {0,{.125f,-.125f}};
    CHECK(ModelEditsSetUVs(project,"Pjungle3_treeZ",revision,&uv,1,NULL,&why));
    CHECK(ModelEditsReadSource(project,"Pjungle3_treeZ",&loaded,&revision,&why));
    for (DWORD i=0;i<check.count;i++) { CHECK(check.flags[i] == loaded.flags[i]); }
    CHECK(ModelEditsSetMaterial(project,"Pjungle3_treeZ",revision,loaded.materials.faces[0].slot,0xddd,&why));
    ModelFreeSource(&loaded);
    CHECK(ModelEditsSave(project,&why)); ModelEditsReset();
    saved = ModelEditsGetData(project,"Pjungle3_treeZ",&length,&why); CHECK(saved);
    CHECK(ModelReadSource(saved,length,&loaded,&why));
    for (DWORD i=0;i<check.count;i++)
    {
        CHECK((check.flags[i] & BG_RENDER_WRAP_MASK) == (loaded.flags[i] & BG_RENDER_WRAP_MASK));
        /* Untextured draws need no C0 bind in the UV/image compiler. */
        if (BgMaterialTextureId(&check.faces[i].material) != BG_TEX_NONE)
        { CHECK((check.faces[i].material.textureword0 & 0x00f00000) ==
                (loaded.faces[i].material.textureword0 & 0x00f00000)); }
    }
    CHECK(ModelEditsReadReplacement(project,"Pjungle3_treeZ",data,size,&rom,&romsize,&why) == 1);
    CHECK(romsize == ModelMaterialsNativeSize(saved,length) && !memcmp(rom,saved,romsize)); free(rom);
    snprintf(path,sizeof(path),"%s/models/objects/Pjungle3_treeZ.gltf",project);
    CHECK(GltfReadModelImport(path,ModelDataHash(saved,length),&roundtrip,&why));
    ModelMaterialsMatch(&roundtrip.materials,&loaded.materials);
    for (DWORD i=0;i<roundtrip.count;i++)
    { roundtrip.tags[i]=roundtrip.materials.slots[roundtrip.materials.faces[i].slot].texture; }
    CHECK(ModelCompileImport(saved,length,&loaded,&roundtrip,project,&out,&againlength,&why));
    CHECK(againlength == ModelMaterialsNativeSize(saved,length) && !memcmp(out,saved,againlength));
    free(out); GltfFreeModelImport(&roundtrip);
    ModelFreeSource(&check); ModelFreeSource(&loaded); ModelEditsReset();
    printf("PASS U/V wraps, neighbours, native state, bounded size, UV/image edits, save/reload/ROM/glTF: %s\n",assetpath);
}

static void WrapSizeGuards(const unsigned char *data, DWORD size, const ModelSource *source, const char *project)
{
    DWORD *chosen=malloc(source->count*sizeof(*chosen)), count=0, revision, savedsize, romsize;
    ModelSource check={0}; const unsigned char *saved; unsigned char *rom=NULL;
    CHECK(chosen);
    for(DWORD i=0;i<source->count;i++)
        if(BgMaterialTextureId(&source->faces[i].material)==0x237) chosen[count++]=i;
    CHECK(count);
    CHECK(ModelEditsReadSource(project,"PsevdoormetslideZ",&check,&revision,&why));ModelFreeSource(&check);
    const int invalid[][5]={{32,33,2,0,1},{32,33,2,2,1},{33,32,0,2,0},{33,32,2,2,0},{33,33,2,0,2}};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++)
    {
        wrapwidth=invalid[i][0];wrapheight=invalid[i][1];
        CHECK(!ModelEditsSetProperties(project,"PsevdoormetslideZ",revision,chosen,count,-1,2,
            invalid[i][2],invalid[i][3],&why)); /* Render mode must remain unchanged too. */
        CHECK(strstr(why,"0237") && strstr(why,"power-of-two"));
        CHECK(strstr(why,invalid[i][4]==2?"U and V":invalid[i][4]==1?"V wrapping":"U wrapping"));
        CHECK(!ModelEditsHasUnsaved());
    }
    wrapwidth=wrapheight=0;
    CHECK(!ModelEditsSetProperties(project,"PsevdoormetslideZ",revision,chosen,count,-1,-1,2,1,&why));
    CHECK((strstr(why,"unavailable") || strstr(why,"missing")) && !ModelEditsHasUnsaved());
    /* Correct the reported door: U mirror on its 32 columns, V clamp on its
     * 33 rows. The other material, UVs, and all non-wrap native state survive. */
    wrapwidth=32;wrapheight=33;
    CHECK(ModelEditsSetProperties(project,"PsevdoormetslideZ",revision,chosen,count,-1,-1,2,1,&why));
    CHECK(ModelEditsReadSource(project,"PsevdoormetslideZ",&check,&revision,&why));
    CheckWrapping(source,&check,chosen,count,2,1);ModelFreeSource(&check);
    CHECK(ModelEditsSave(project,&why));ModelEditsReset();
    saved=ModelEditsGetData(project,"PsevdoormetslideZ",&savedsize,&why);CHECK(saved);
    CHECK(ModelEditsReadReplacement(project,"PsevdoormetslideZ",data,size,&rom,&romsize,&why)==1);
    CHECK(romsize==ModelMaterialsNativeSize(saved,savedsize) && !memcmp(rom,saved,romsize));
    CHECK(ModelReadSource(rom,romsize,&check,&why));CheckWrapping(source,&check,chosen,count,2,1);
    ModelFreeSource(&check);free(rom);
    CHECK(ModelEditsReadSource(project,"PsevdoormetslideZ",&check,&revision,&why));ModelFreeSource(&check);
    CHECK(ModelEditsSetProperties(project,"PsevdoormetslideZ",revision,chosen,count,-1,-1,2,-1,&why));
    CHECK(!ModelEditsHasUnsaved()); /* Safe Keep current and no-op. */
    /* A later image replacement can leave a legacy invalid mode. The retained
     * axis must be checked even when only its other axis is being edited. */
    wrapheight=32;
    CHECK(ModelEditsSetProperties(project,"PsevdoormetslideZ",revision,chosen,count,-1,-1,2,0,&why));
    CHECK(ModelEditsReadSource(project,"PsevdoormetslideZ",&check,&revision,&why));ModelFreeSource(&check);
    saved=ModelEditsGetData(project,"PsevdoormetslideZ",&savedsize,&why);CHECK(saved);
    DWORD hash=ModelDataHash(saved,savedsize);
    wrapheight=33;
    CHECK(!ModelEditsSetProperties(project,"PsevdoormetslideZ",revision,chosen,count,-1,-1,1,-1,&why));
    CHECK(strstr(why,"V wrapping"));
    saved=ModelEditsGetData(project,"PsevdoormetslideZ",&savedsize,&why);CHECK(saved);
    CHECK(ModelDataHash(saved,savedsize)==hash);
    CHECK(ModelEditsSetProperties(project,"PsevdoormetslideZ",revision,chosen,count,-1,-1,2,1,&why));
    ModelEditsReset();free(chosen);
    puts("PASS non-power-of-two U/V guards, atomic rejection, retained axes, unavailable images and door repair/save/ROM.");
}

static void MaterialSlots(const unsigned char *base,DWORD basesize,const char *project)
{
    ModelSource original={0},current={0};DWORD revision,i,k,nativesize,size;
    const unsigned char *pending;unsigned char *rom=NULL;DWORD textures[]={0xeee,BG_TEX_NONE,0xeee};
    CHECK(ModelEditsReadSource(project,"Pjungle3_treeZ",&original,&revision,&why));
    CHECK(original.materials.count>0);
    for(k=0;k<3;k++)
    {
        CHECK(ModelEditsSetMaterial(project,"Pjungle3_treeZ",revision,0,textures[k],&why));
        CHECK(!ModelEditsSetMaterial(project,"Pjungle3_treeZ",revision,0,textures[k],&why));
        CHECK(strstr(why,"changed"));
        CHECK(ModelEditsReadSource(project,"Pjungle3_treeZ",&current,&revision,&why));
        CHECK(current.count==original.count && current.materials.count==original.materials.count);
        CHECK(!memcmp(current.materials.faces,original.materials.faces,current.count*sizeof(*current.materials.faces)));
        for(i=0;i<current.count;i++)
        {
            DWORD slot=original.materials.faces[i].slot,j;
            CHECK(BG_TEX_ID(current.tags[i])==(slot==0?textures[k]:BG_TEX_ID(original.tags[i])));
            for(j=0;j<3;j++)
            {
                const BgVertex *a=&original.vertices[i*3+j],*b=&current.vertices[i*3+j];
                CHECK(a->x==b->x && a->y==b->y && a->z==b->z);
                CHECK(a->r==b->r && a->g==b->g && a->b==b->b && a->a==b->a);
                if(slot==0 && textures[k]!=BG_TEX_NONE)
                {
                    CHECK(fabsf(b->s-original.materials.faces[i].uv[j*2]*64)<=1.f/64);
                    CHECK(fabsf(b->t-original.materials.faces[i].uv[j*2+1]*16)<=1.f/64);
                }
                if(slot!=0) CHECK(a->s==b->s && a->t==b->t);
            }
        }
        ModelFreeSource(&current);
    }
    CHECK(ModelEditsSave(project,&why));ModelEditsReset();
    CHECK(ModelEditsReadSource(project,"Pjungle3_treeZ",&current,&revision,&why));
    CHECK(current.materials.slots[0].texture==0xeee && current.count==original.count);
    CHECK(!memcmp(current.materials.faces,original.materials.faces,current.count*sizeof(*current.materials.faces)));
    pending=ModelEditsGetData(project,"Pjungle3_treeZ",&size,&why);CHECK(pending);
    CHECK(ModelEditsReadReplacement(project,"Pjungle3_treeZ",base,basesize,&rom,&nativesize,&why)==1);
    CHECK(nativesize<size && nativesize==ModelMaterialsNativeSize(pending,size) && !memcmp(rom,pending,nativesize));
    free(rom);ModelFreeSource(&current);ModelFreeSource(&original);ModelEditsReset();
    printf("PASS stock material assignment, resize, clear, stale revision, unchanged neighbours, UV retention, save/reload and native ROM export: %s\n",assetpath);
}

static void Brush(const unsigned char *base, DWORD size, const ModelSource *source, const char *project)
{
    ModelSource current = {0}; DWORD revision, length, offset = source->vertexoffsets[1];
    unsigned char rgba[4] = {17, 99, 201, 173}, *painted = NULL, *rom = NULL;
    ModelVertexPaint change;
    const unsigned char *pending;
    CHECK(ModelCompileVertexColor(base, size, source, 1, rgba, &painted, &why));
    for (DWORD i = 0; i < size; i++)
        CHECK(painted[i] == (i >= offset+12 && i < offset+16 ? rgba[i-offset-12] : base[i]));
    CHECK(ModelReadSource(painted, size, &current, &why));
    for (DWORD i = 0; i < current.count*3; i++)
    {
        const BgVertex *a = &source->vertices[i], *b = &current.vertices[i];
        CHECK(a->x==b->x && a->y==b->y && a->z==b->z && a->s==b->s && a->t==b->t);
        if (source->vertexoffsets[i] == offset)
            CHECK(b->r==17 && b->g==99 && b->b==201 && b->a==173);
        else CHECK(!memcmp(a,b,sizeof(*a)));
    }
    ModelFreeSource(&current); free(painted);
    CHECK(!ModelCompileVertexColor(base,size,source,source->count*3,rgba,&painted,&why) && !painted);
    CHECK(ModelEditsReadSource(project,"Pjungle3_treeZ",&current,&revision,&why));
    { const BgVertex *v=&current.vertices[1]; unsigned char same[]={v->r,v->g,v->b,v->a};
      CHECK(ModelEditsSetVertexColor(project,"Pjungle3_treeZ",revision,1,same,NULL,&why));
      CHECK(!ModelEditsHasUnsaved()); }
    ModelFreeSource(&current);
    CHECK(ModelEditsSetVertexColor(project,"Pjungle3_treeZ",revision,1,rgba,&change,&why));
    CHECK(ModelEditsHasUnsaved());
    CHECK(ModelEditsRestoreVertexColor(project,"Pjungle3_treeZ",&change,FALSE,&why));
    pending=ModelEditsGetData(project,"Pjungle3_treeZ",&length,&why);
    CHECK(length==size && !memcmp(pending,base,size));
    CHECK(ModelEditsRestoreVertexColor(project,"Pjungle3_treeZ",&change,TRUE,&why));
    CHECK(!ModelEditsRestoreVertexColor(project,"Pjungle3_treeZ",&change,TRUE,&why));
    CHECK(!ModelEditsSetVertexColor(project,"Pjungle3_treeZ",revision,1,rgba,NULL,&why));
    CHECK(strstr(why,"changed"));
    for (DWORD i=0;i<100;i++)
    {
        CHECK(ModelEditsReadSource(project,"Pjungle3_treeZ",&current,&revision,&why));
        ModelFreeSource(&current); rgba[0]=(unsigned char)i;
        CHECK(ModelEditsSetVertexColor(project,"Pjungle3_treeZ",revision,1,rgba,NULL,&why));
        pending=ModelEditsGetData(project,"Pjungle3_treeZ",&length,&why);
        CHECK(pending && length==size);
    }
    CHECK(ModelEditsSave(project,&why)); ModelEditsReset();
    CHECK(ModelEditsReadSource(project,"Pjungle3_treeZ",&current,&revision,&why));
    CHECK(current.vertices[1].r==99 && current.vertices[1].a==173);
    ModelFreeSource(&current);
    CHECK(ModelEditsReadReplacement(project,"Pjungle3_treeZ",base,size,&rom,&length,&why)==1);
    CHECK(length==size && !memcmp(rom+offset+12,rgba,4)); free(rom);
    { char path[MAX_PATH]; DWORD before,after;
      snprintf(path,sizeof(path),"%s/models/objects/Pjungle3_treeZ.gltf",project);
      CHECK(ModelEditsImport(project,"Pjungle3_treeZ",path,&before,&after,&why));
      CHECK(!ModelEditsHasUnsaved() && before==after); }
    ModelEditsReset();
    puts("PASS brush RGBA, shared vertices, untouched UVs/geometry, no-op/stale edits, no growth, save/reload/ROM and glTF round trip.");
}

static void BrushGuards(const unsigned char *data, DWORD size, const ModelSource *source, const char *kind)
{
    unsigned char *out=NULL, rgba[4]={17,99,201,173};
    /* Shared-normals selects the RGB use of a vertex also loaded as normals. */
    DWORD corner=!strcmp(kind,"shared-normals") ? 3 : 0;
    const BgVertex *v=&source->vertices[corner];
    rgba[3]=v->a;
    if (!strcmp(kind,"constant-alpha"))
    { rgba[0]=v->r; rgba[1]=v->g; rgba[2]=v->b; rgba[3]=v->a^0xff; }
    CHECK(!ModelCompileVertexColor(data,size,source,corner,rgba,&out,&why) && !out);
    CHECK(strstr(why,!strcmp(kind,"dynamic") ? "dynamic" : !strcmp(kind,"constant-alpha") ? "opacity" : "normal"));
}

int main(int argc, char **argv)
{
    unsigned char *data;
    DWORD size;
    ModelSource source = {0};
    CHECK(argc >= 4); assetpath = argv[2];
    realtextures = !strcmp(argv[1], "blender");
    wrapsizes = !strcmp(argv[1], "wrap-size");
    data = Read(assetpath, &size);
    CHECK(ModelReadSource(data, size, &source, &why));
    CHECK(source.count >= 2);
    if (!strcmp(argv[1], "brush")) { Brush(data,size,&source,argv[3]); }
    else if (!strcmp(argv[1], "brush-guards")) { BrushGuards(data,size,&source,argv[3]); }
    else if (!strcmp(argv[1], "property-guards")) { PropertyGuards(data, size, &source); }
    else if (wrapsizes) { WrapSizeGuards(data,size,&source,argv[3]); }
    else if (!strcmp(argv[1], "wrapping")) { Wrapping(data,size,&source,argv[3]); }
    else if (!strcmp(argv[1], "slots")) { MaterialSlots(data,size,argv[3]); }
    else if (!strcmp(argv[1], "properties")) { Properties(data, size, &source, argv[3]); }
    else if (!strcmp(argv[1], "dynamic")) { Dynamic(data, size, &source); }
    else if (!strcmp(argv[1], "inherited")) { Inherited(data, size, &source); }
    else if (!strcmp(argv[1], "special")) { Special(data, size, &source, argv[3]); }
    else if (!strcmp(argv[1], "export"))
    {
        Unit(data, size, &source);
        CHECK(GltfWriteEditableModel(argv[3], "test", &source, ModelDataHash(data, size), &why));
    }
    else if (!strcmp(argv[1], "no-uv"))
    {
        GltfModelImport imported={0};
        CHECK(GltfReadModelImport(argv[3],ModelDataHash(data,size),&imported,&why));
        CHECK(imported.count==source.count);
        for (DWORD i=0;i<imported.count*3;i++) CHECK(imported.vertices[i].s==0 && imported.vertices[i].t==0);
        GltfFreeModelImport(&imported);
    }
    else if (!strcmp(argv[1], "reject"))
    {
        GltfModelImport imported = {0};
        CHECK(!GltfReadModelImport(argv[3], ModelDataHash(data, size), &imported, &why));
        CHECK(argc == 5 && strstr(why, argv[4]));
        GltfFreeModelImport(&imported);
    }
    else
    {
        const unsigned char *pending;
        unsigned char *replacement;
        DWORD before, after, editedsize, replacementsize;
        char path[MAX_PATH];
        GltfModelImport expected = Copy(&source);
        BOOL changed = strcmp(argv[1], "noop") != 0;
        BOOL painted = !strncmp(argv[1], "paint", 5);
        BOOL seams = !strcmp(argv[1], "seams") || realtextures;
        CHECK(argc == 5);
        CHECK(ModelEditsImport(argv[4], "Pjungle3_treeZ", argv[3], &before, &after, &why));
        CHECK(before == source.count && after == before);
        CHECK(ModelEditsHasUnsaved() == changed);
        if (changed)
        {
            if (seams)
            {
                GltfFreeModelImport(&expected);
                CHECK(GltfReadModelImport(argv[3], ModelDataHash(data, size), &expected, &why));
                CHECK(ModelMaterialsEnsure(&source,argv[4],&why));
                ModelMaterialsMatch(&expected.materials,&source.materials);
                for (DWORD i=0;i<expected.count;i++) expected.tags[i]=expected.materials.slots[expected.materials.faces[i].slot].texture;
            }
            else if (painted) { Paint(&expected, !strcmp(argv[1], "paint-rgb")); }
            else { Change(&expected); }
            pending = ModelEditsGetData(argv[4], "Pjungle3_treeZ", &editedsize, &why);
            CHECK(pending && (seams ? ModelMaterialsNativeSize(pending,editedsize) > size : ModelMaterialsNativeSize(pending,editedsize) == size));
            if (seams) { Corners(data, &source, pending, editedsize, &expected); }
            else { Positions(data, &source, pending, editedsize, &expected); }
            if (painted) { Colors(data, &source, pending, editedsize, &expected); }
            /* A stale export fails without replacing the pending result. */
            CHECK(!ModelEditsImport(argv[4], "Pjungle3_treeZ", argv[3], &before, &after, &why));
            CHECK(strstr(why, "revision") && ModelEditsHasUnsaved());
            CHECK(ModelEditsGetData(argv[4], "Pjungle3_treeZ", &editedsize, &why) == pending);
            CHECK(ModelEditsSave(argv[4], &why)); CHECK(!ModelEditsHasUnsaved());
            ModelEditsReset();
            pending = ModelEditsGetData(argv[4], "Pjungle3_treeZ", &editedsize, &why);
            CHECK(pending != NULL);
            if (seams) { Corners(data, &source, pending, editedsize, &expected); }
            else { Positions(data, &source, pending, editedsize, &expected); }
            if (painted) { Colors(data, &source, pending, editedsize, &expected); }
            CHECK(ModelEditsReadReplacement(argv[4], "Pjungle3_treeZ", data, size,
                &replacement, &replacementsize, &why) == 1);
            CHECK(replacementsize == ModelMaterialsNativeSize(pending,editedsize) && !memcmp(replacement, pending, replacementsize));
            free(replacement);
            snprintf(path, sizeof(path), "%s/models/objects/Pjungle3_treeZ.gltf", argv[4]);
            CHECK(ModelEditsImport(argv[4], "Pjungle3_treeZ", path, &before, &after, &why));
            CHECK(!ModelEditsHasUnsaved());
        }
        GltfFreeModelImport(&expected); ModelEditsReset();
        printf("PASS %s glTF/GLB round trip and model store: %s\n", argv[1], assetpath);
    }
    ModelFreeSource(&source); free(data); return 0;
}
