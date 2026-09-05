/*
 * GoldenEye bg file parser.
 *
 * The file layout mirrors what bgLoadFile in the game does:
 *   header word 1 -> room record list (24-byte records, index 0 unused)
 *   each record  -> vertex blob, primary GDL, secondary GDL, room pos
 * Every block is preceded by a 4-byte size word. Internal pointers are
 * 0x0F-segment addresses; their low 24 bits are file offsets.
 *
 * The display lists are Fast3D: 8-byte commands, G_VTX (0x04) loads a
 * vertex batch, G_TRI1 (0xBF) indexes it with bytes scaled by 10,
 * G_TRI4 (0xB1) packs four nibble-indexed triangles, G_ENDDL (0xB8)
 * ends. Everything else is state we can ignore while untextured.
 *
 * Malformed data is handled by stopping the current room, never by
 * reading outside the file: the parser must survive any input.
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "bgload.h"

#define BG_ROOM_RECORD_SIZE 24
#define BG_MAX_ROOMS        256
#define BG_MAX_BATCH        64      /* vertices a G_VTX may load */

#define G_NOOP  0xC0   /* F3DEX G_NOOP; in raw GE DLs a texture reference, ID in w1 & 0xFFF */
#define G_VTX   0x04
#define G_TRI4  0xB1
#define G_ENDDL 0xB8
#define G_TRI1  0xBF

typedef struct BgBuilder {
    BgVertex       *verts;
    unsigned short *texids;     /* one per triangle */
    DWORD           count;      /* vertices */
    DWORD           capacity;
    BOOL            failed;
} BgBuilder;

static DWORD bg32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16)
         | ((DWORD)p[2] << 8)  |  (DWORD)p[3];
}

static float bgf32(const unsigned char *p)
{
    union { DWORD u; float f; } cvt;

    cvt.u = bg32(p);
    return cvt.f;
}

static short bg16(const unsigned char *p)
{
    return (short)(((int)p[0] << 8) | p[1]);
}

static void BgBuilderPush(BgBuilder *b, const BgVertex *v)
{
    if (b->failed)
    {
        return;
    }

    if (b->count == b->capacity)
    {
        DWORD next = b->capacity ? b->capacity * 2 : 4096;
        BgVertex *grown = (BgVertex *)realloc(b->verts, next * sizeof(BgVertex));
        unsigned short *grownt = (unsigned short *)realloc(b->texids,
                                     (next / 3) * sizeof(unsigned short));

        if (grown != NULL) { b->verts = grown; }
        if (grownt != NULL) { b->texids = grownt; }
        if (grown == NULL || grownt == NULL)
        {
            b->failed = TRUE;
            return;
        }

        b->capacity = next;
    }

    b->verts[b->count++] = *v;
}

/*
 * Reads the size word stored just before a block and bounds it.
 * Returns 0 for anything implausible.
 */
static DWORD BgBlockSize(const unsigned char *data, DWORD maxlen, DWORD offset)
{
    DWORD size;

    if (offset < 4 || offset >= maxlen)
    {
        return 0;
    }

    size = bg32(data + offset - 4);

    if (size == 0 || size > maxlen - offset)
    {
        return 0;
    }

    return size;
}

/*
 * Walks one display list, emitting triangles from the room's vertex
 * blob. The room origin and local vertices are both stored in scaled
 * bg coordinates, so worldscale reproduces the game's per-room matrix.
 */
static void BgWalkGdl(BgBuilder *b,
                      const unsigned char *data, DWORD maxlen,
                      DWORD gdloffset, DWORD gdlsize,
                      const unsigned char *vtxblob, DWORD vtxsize,
                      float roomx, float roomy, float roomz,
                      float worldscale,
                      unsigned short layerflag)
{
    DWORD pc;
    const unsigned char *batch = NULL;
    DWORD batchcount = 0;
    int batchv0 = 0;
    unsigned short curtex = BG_TEX_NONE;

    /* every triangle this walk emits carries the layer flag */

    for (pc = gdloffset; pc + 8 <= gdloffset + gdlsize && pc + 8 <= maxlen; pc += 8)
    {
        const unsigned char *cmd = data + pc;

        if (cmd[0] == G_ENDDL)
        {
            return;
        }

        if (cmd[0] == G_NOOP)
        {
            /* The raw file marks texture changes with G_NOOP commands;
               the game rewrites them into SETTIMG/SETTILE sequences at
               load (texLoadFromGdl). The ID is the low 12 bits. */
            curtex = (unsigned short)(bg32(cmd + 4) & 0xFFF);
            continue;
        }

        if (cmd[0] == G_VTX)
        {
            DWORD addr = bg32(cmd + 4) & 0x00FFFFFF;

            batchcount = ((cmd[1] >> 4) & 0xF) + 1;
            batchv0 = cmd[1] & 0xF;

            if (addr + batchcount * 16 > vtxsize)
            {
                batch = NULL; /* points outside the blob: poison it */
                batchcount = 0;
            }
            else
            {
                batch = vtxblob + addr;
            }
            continue;
        }

        if ((cmd[0] == G_TRI1 || cmd[0] == G_TRI4) && batch != NULL)
        {
            int tri;
            int tricount = (cmd[0] == G_TRI1) ? 1 : 4;

            for (tri = 0; tri < tricount; tri++)
            {
                int idx[3];
                int k;

                if (cmd[0] == G_TRI1)
                {
                    idx[0] = cmd[5] / 10;
                    idx[1] = cmd[6] / 10;
                    idx[2] = cmd[7] / 10;
                }
                else if (tri == 0)
                {
                    idx[0] = cmd[7] & 0xF;
                    idx[1] = cmd[7] >> 4;
                    idx[2] = cmd[3] & 0xF;
                }
                else if (tri == 1)
                {
                    idx[0] = cmd[6] & 0xF;
                    idx[1] = cmd[6] >> 4;
                    idx[2] = cmd[3] >> 4;
                }
                else if (tri == 2)
                {
                    idx[0] = cmd[5] & 0xF;
                    idx[1] = cmd[5] >> 4;
                    idx[2] = cmd[2] & 0xF;
                }
                else
                {
                    idx[0] = cmd[4] & 0xF;
                    idx[1] = cmd[4] >> 4;
                    idx[2] = cmd[2] >> 4;
                }

                /* A TRI4 slot holding no triangle packs as 0,0,0. */
                if (idx[0] == idx[1] && idx[1] == idx[2])
                {
                    continue;
                }

                for (k = 0; k < 3; k++)
                {
                    idx[k] -= batchv0;
                }

                if (idx[0] < 0 || idx[1] < 0 || idx[2] < 0
                    || idx[0] >= (int)batchcount
                    || idx[1] >= (int)batchcount
                    || idx[2] >= (int)batchcount)
                {
                    continue; /* index escaped the batch: drop the tri */
                }

                for (k = 0; k < 3; k++)
                {
                    const unsigned char *v = batch + idx[k] * 16;
                    BgVertex out;

                    out.x = (roomx + bg16(v + 0)) * worldscale;
                    out.y = (roomy + bg16(v + 2)) * worldscale;
                    out.z = (roomz + bg16(v + 4)) * worldscale;
                    out.s = (float)bg16(v + 8) / 32.0f;   /* s10.5 -> texels */
                    out.t = (float)bg16(v + 10) / 32.0f;
                    out.r = v[12];
                    out.g = v[13];
                    out.b = v[14];
                    out.a = v[15];

                    BgBuilderPush(b, &out);
                }

                /* Record the texture after the pushes: growth has
                   already resized the texid array to match. */
                if (!b->failed)
                {
                    b->texids[b->count / 3 - 1] = (unsigned short)(curtex | layerflag);
                }
            }
        }
    }
}

BgVertex *BgLoadGeometry(const unsigned char *data, DWORD maxlen,
                         float levelscale,
                         DWORD *tricount, unsigned short **texids,
                         const char **reasonout)
{
    BgBuilder b;
    float worldscale;
    DWORD roomlist;
    DWORD rooms;
    DWORD i;

    *tricount = 0;
    *texids = NULL;
    *reasonout = "";

    if (!(levelscale > 0.0f))
    {
        *reasonout = "level scale must be greater than zero.";
        return NULL;
    }

    worldscale = 1.0f / levelscale;

    if (maxlen < 0x40)
    {
        *reasonout = "bg file is too small to have a header.";
        return NULL;
    }

    if (bg32(data + 0) != 0)
    {
        *reasonout = "bg file is a single display list (not a room-based level).";
        return NULL;
    }

    roomlist = bg32(data + 4) & 0x00FFFFFF;

    if (roomlist >= maxlen)
    {
        *reasonout = "bg room list offset is outside the file.";
        return NULL;
    }

    /* Count rooms the way the game does: from record 1 until a record
       with no primary graphics. Record 0 is unused. */
    rooms = 0;
    for (i = 1; i < BG_MAX_ROOMS; i++)
    {
        DWORD rec = roomlist + i * BG_ROOM_RECORD_SIZE;

        if (rec + BG_ROOM_RECORD_SIZE > maxlen || bg32(data + rec + 4) == 0)
        {
            break;
        }

        rooms = i;
    }

    if (rooms == 0)
    {
        *reasonout = "bg file contains no rooms.";
        return NULL;
    }

    ZeroMemory(&b, sizeof(b));

    for (i = 1; i <= rooms; i++)
    {
        DWORD rec = roomlist + i * BG_ROOM_RECORD_SIZE;
        DWORD vtxoff = bg32(data + rec + 0) & 0x00FFFFFF;
        DWORD prioff = bg32(data + rec + 4) & 0x00FFFFFF;
        DWORD secoff = bg32(data + rec + 8) & 0x00FFFFFF;
        float rx = bgf32(data + rec + 12);
        float ry = bgf32(data + rec + 16);
        float rz = bgf32(data + rec + 20);
        DWORD vtxsize = BgBlockSize(data, maxlen, vtxoff);
        DWORD prisize;
        DWORD secsize;

        if (vtxoff == 0 || vtxsize == 0)
        {
            continue; /* portal-only or empty room */
        }

        prisize = BgBlockSize(data, maxlen, prioff);
        if (prioff != 0 && prisize != 0)
        {
            BgWalkGdl(&b, data, maxlen, prioff, prisize,
                      data + vtxoff, vtxsize, rx, ry, rz, worldscale, 0);
        }

        secsize = BgBlockSize(data, maxlen, secoff);
        if (secoff != 0 && secsize != 0)
        {
            BgWalkGdl(&b, data, maxlen, secoff, secsize,
                      data + vtxoff, vtxsize, rx, ry, rz, worldscale,
                      BG_TRI_SECONDARY);
        }
    }

    if (b.failed || b.count == 0)
    {
        free(b.verts);
        free(b.texids);
        *reasonout = b.failed ? "out of memory building bg geometry."
                              : "bg file produced no triangles.";
        return NULL;
    }

    *tricount = b.count / 3;
    *texids = b.texids;
    return b.verts;
}
