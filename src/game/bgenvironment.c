#include <ultra64.h>
#include "bgtransparency.h"
#include "bg.h"
#include "image.h"

extern Lights1 GlobalLight;
static Lights1 g_BgEnvironmentLight = gdSPDefLights1(255, 255, 255, 0, 0, 0, 0, 0, 127);

/* Resolve editor packets once when a room loads. The compiler gives every
 * generated triangle a private, contiguous three-vertex load. ROM/project
 * vertices retain painted RGB and UVs; only their RAM copies become normals.
 * The RSP then generates coordinates using the current player's LookAt axes.
 * No vertex rewriting or display-list allocation is needed per frame. */
void bgApplyEnvironmentMapping(Gfx *start, Gfx *end, Vtx *vertices, u32 vertexBytes)
{
    Gfx *cmd;
    u32 policy = BG_ENV_AUTO;
    u32 geometry = 0;
    u32 texture0 = 0xbb000001u, texture1 = 0xffffffffu;
    u32 normal = 0;
    u32 normalPending = FALSE;
    struct tex *texture = NULL;
    const u32 geometryMask = G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR;

    for (cmd = start; cmd < end; cmd++)
    {
        u32 op = cmd->words.w0 >> 24;
        if (BG_ENV_IS_NORMAL(cmd->words.w0, cmd->words.w1))
        {
            normal = cmd->words.w1;
            normalPending = TRUE;
            continue;
        }
        if (BG_ENV_IS_MARKER(cmd->words.w0, cmd->words.w1))
        {
            u32 kind = cmd->words.w1 & 255u;
            Lights1 *lights = BG_ENV_GENERATED(policy) ? &g_BgEnvironmentLight : &GlobalLight;
            if (kind <= BG_ENV_LINEAR) { policy = kind; normalPending = FALSE; }
            else if (kind == BG_ENV_CLEAR) { gSPClearGeometryMode(cmd, geometryMask); }
            else if (kind == BG_ENV_SET)
            {
                u32 bits = BG_ENV_GENERATED(policy)
                    ? G_LIGHTING | G_TEXTURE_GEN | (policy == BG_ENV_LINEAR ? G_TEXTURE_GEN_LINEAR : 0)
                    : geometry & (policy == BG_ENV_OFF ? G_LIGHTING : geometryMask);
                gSPSetGeometryMode(cmd, bits);
            }
            else if (kind == BG_ENV_LIGHTS) { gSPNumLights(cmd, NUMLIGHTS_1); }
            else if (kind == BG_ENV_LIGHT_DIRECTION) { gSPLight(cmd, &lights->l[0], 1); }
            else if (kind == BG_ENV_LIGHT_AMBIENT) { gSPLight(cmd, &lights->a, 2); }
            else if (kind == BG_ENV_SCALE)
            {
                cmd->words.w0 = texture0;
                cmd->words.w1 = BG_ENV_GENERATED(policy) && texture
                    ? ((u32)texture->width * 64u << 16) | ((u32)texture->height * 64u)
                    : texture1;
            }
            continue; /* Generated commands never change the authored baseline. */
        }
        if (op == (u8)G_SETGEOMETRYMODE) { geometry |= cmd->words.w1; }
        else if (op == (u8)G_CLEARGEOMETRYMODE) { geometry &= ~cmd->words.w1; }
        else if (op == (u8)G_TEXTURE)
        { texture0 = cmd->words.w0; texture1 = cmd->words.w1; }
        else if (op == (u8)G_SETTIMG)
        {
            struct tex *found = texFindByData(cmd->words.w1);
            if (found) { texture = found; }
        }
        else if (op == (u8)G_VTX)
        {
            u32 offset = cmd->words.w1 & 0xffffffu;
            if (normalPending && BG_ENV_GENERATED(policy) && vertices
                && cmd->words.w0 == 0x04200030u && (cmd->words.w1 >> 24) == 0x0e
                && !(offset & 15u) && offset <= vertexBytes && vertexBytes - offset >= 48)
            {
                u32 i;
                u8 *data = (u8 *)vertices + offset;
                for (i = 0; i < 3; i++, data += 16)
                {
                    data[12] = normal >> 16;
                    data[13] = normal >> 8;
                    data[14] = normal;
                }
            }
            normalPending = FALSE;
        }
    }
}
