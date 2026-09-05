/*
 * Model file layout (bondtypes.h is the spec):
 *   header (40 bytes): RootNode*, Skeleton*, Switches**, counts,
 *   bounding radius, texture table - all pointers are 0x05-segment
 *   addresses whose low 24 bits are file offsets.
 *   Nodes are 24 bytes: u16 opcode, Data*, Parent*, Next*, Prev*,
 *   Child*. The tree walks Child-first, then Next.
 *   Mesh data lives in two opcodes:
 *     0x04 DisplayListRecord: Primary@0 Secondary@4 Vertices@0xC
 *     0x18 DisplayList_Collision: Primary@0 Secondary@4 Vertices@8
 *   Display lists are the same Fast3D dialect as bg files, with the
 *   same G_NOOP texture markers - but G_VTX addresses are absolute
 *   file offsets, not blob-relative ones.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modelload.h"

#define MDL_G_NOOP  0xC0
#define MDL_G_VTX   0x04
#define MDL_G_TRI4  0xB1
#define MDL_G_ENDDL 0xB8
#define MDL_G_TRI1  0xBF

#define MDL_MAX_NODES 512

typedef struct MdlBuilder {
    BgVertex       *verts;
    unsigned short *texids;
    DWORD           count;
    DWORD           capacity;
    BOOL            failed;
} MdlBuilder;

static DWORD md32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16)
         | ((DWORD)p[2] << 8)  |  (DWORD)p[3];
}

static short md16(const unsigned char *p)
{
    return (short)(((int)p[0] << 8) | p[1]);
}

static DWORD mdoff(DWORD segptr)
{
    return segptr & 0x00FFFFFF;
}

static void MdlPush(MdlBuilder *b, const BgVertex *v)
{
    if (b->failed)
    {
        return;
    }

    if (b->count == b->capacity)
    {
        DWORD next = b->capacity ? b->capacity * 2 : 1024;
        BgVertex *g = (BgVertex *)realloc(b->verts, next * sizeof(BgVertex));
        unsigned short *gt = (unsigned short *)realloc(b->texids,
                                 (next / 3) * sizeof(unsigned short));

        if (g != NULL) { b->verts = g; }
        if (gt != NULL) { b->texids = gt; }
        if (g == NULL || gt == NULL)
        {
            b->failed = TRUE;
            return;
        }

        b->capacity = next;
    }

    b->verts[b->count++] = *v;
}

/*
 * Same command walk as bgload. Vertex addressing differs by segment:
 * segment 0x05 is a file-absolute offset, while segment 0x04 (the
 * model-vertex segment the renderer maps to the record's Vertices
 * pointer) is RELATIVE to vtxbase - Pbook1Z's own display list is the
 * proof, loading 0x04000000 then 0x040000F0 against Vertices=0x98.
 */
static void MdlWalkGdl(MdlBuilder *b, const unsigned char *data, DWORD maxlen,
                       DWORD gdloffset, DWORD vtxbase, unsigned short layerflag)
{
    DWORD pc;
    DWORD batchoff = 0;
    DWORD batchcount = 0;
    int batchv0 = 0;
    unsigned short curtex = BG_TEX_NONE;

    for (pc = gdloffset; pc + 8 <= maxlen; pc += 8)
    {
        const unsigned char *cmd = data + pc;

        if (cmd[0] == MDL_G_ENDDL)
        {
            return;
        }

        if (cmd[0] == MDL_G_NOOP)
        {
            curtex = (unsigned short)(md32(cmd + 4) & 0xFFF);
            continue;
        }

        if (cmd[0] == MDL_G_VTX)
        {
            DWORD raw = md32(cmd + 4);
            DWORD addr = (raw >> 24) == 0x05 ? mdoff(raw)
                                             : vtxbase + mdoff(raw);

            batchcount = ((cmd[1] >> 4) & 0xF) + 1;
            batchv0 = cmd[1] & 0xF;

            if (addr + batchcount * 16 > maxlen)
            {
                batchoff = 0;
                batchcount = 0;
            }
            else
            {
                batchoff = addr;
            }
            continue;
        }

        if ((cmd[0] == MDL_G_TRI1 || cmd[0] == MDL_G_TRI4) && batchoff != 0)
        {
            int tri;
            int tricount = (cmd[0] == MDL_G_TRI1) ? 1 : 4;

            for (tri = 0; tri < tricount; tri++)
            {
                int idx[3];
                int k;

                if (cmd[0] == MDL_G_TRI1)
                {
                    idx[0] = cmd[5] / 10;
                    idx[1] = cmd[6] / 10;
                    idx[2] = cmd[7] / 10;
                }
                else if (tri == 0)
                {
                    idx[0] = cmd[7] & 0xF; idx[1] = cmd[7] >> 4; idx[2] = cmd[3] & 0xF;
                }
                else if (tri == 1)
                {
                    idx[0] = cmd[6] & 0xF; idx[1] = cmd[6] >> 4; idx[2] = cmd[3] >> 4;
                }
                else if (tri == 2)
                {
                    idx[0] = cmd[5] & 0xF; idx[1] = cmd[5] >> 4; idx[2] = cmd[2] & 0xF;
                }
                else
                {
                    idx[0] = cmd[4] & 0xF; idx[1] = cmd[4] >> 4; idx[2] = cmd[2] >> 4;
                }

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
                    continue;
                }

                for (k = 0; k < 3; k++)
                {
                    const unsigned char *v = data + batchoff + idx[k] * 16;
                    BgVertex out;

                    out.x = md16(v + 0);
                    out.y = md16(v + 2);
                    out.z = md16(v + 4);
                    out.s = (float)md16(v + 8) / 32.0f;
                    out.t = (float)md16(v + 10) / 32.0f;
                    out.r = v[12];
                    out.g = v[13];
                    out.b = v[14];
                    out.a = v[15];

                    MdlPush(b, &out);
                }

                if (!b->failed)
                {
                    b->texids[b->count / 3 - 1] = (unsigned short)(curtex | layerflag);
                }
            }
        }
    }
}

/* Emits the mesh data of one node, if it has any. */
static void MdlNodeMeshes(MdlBuilder *b, const unsigned char *data,
                          DWORD maxlen, DWORD opcode, DWORD dataoff)
{
    DWORD prioff = 0;
    DWORD secoff = 0;

    if (dataoff == 0 || dataoff + 0x14 > maxlen)
    {
        return;
    }

    DWORD vtxbase = 0;

    if (opcode == 0x04 || opcode == 0x18)
    {
        prioff = mdoff(md32(data + dataoff + 0));
        secoff = mdoff(md32(data + dataoff + 4));

        /* Vertices: op 0x04 keeps them at +0xC, op 0x18 at +0x8. */
        vtxbase = mdoff(md32(data + dataoff + (opcode == 0x04 ? 0xC : 0x8)));
    }

    if (prioff != 0 && prioff < maxlen)
    {
        MdlWalkGdl(b, data, maxlen, prioff, vtxbase, 0);
    }

    if (secoff != 0 && secoff < maxlen)
    {
        MdlWalkGdl(b, data, maxlen, secoff, vtxbase, BG_TRI_SECONDARY);
    }
}

BgVertex *ModelLoadGeometry(const unsigned char *data, DWORD maxlen,
                            DWORD *tricount, unsigned short **texids,
                            const char **reasonout)
{
    MdlBuilder b;
    DWORD stack[MDL_MAX_NODES];
    int sp = 0;
    int visited = 0;
    DWORD rootoff;

    *tricount = 0;
    *texids = NULL;
    *reasonout = "";

    if (maxlen < 40)
    {
        *reasonout = "model file too small for a header.";
        return NULL;
    }

    /*
     * GE model files carry no header - it lives in the game's static
     * per-model tables (load_object_fill_header is the spec). The file
     * is [switch ptrs][texture table][RootNode][...], and the counts
     * that locate RootNode are in code we don't have here. So: scan
     * the first bytes for the first thing shaped like a node - small
     * u16 opcode, a segment-5 Data pointer, and a NULL Parent, which
     * the root uniquely has.
     */
    rootoff = 0;

    {
        DWORD probe;

        for (probe = 0; probe + 24 <= maxlen && probe < 0x200; probe += 4)
        {
            DWORD op = md16(data + probe) & 0xFFFF;
            DWORD dp = md32(data + probe + 4);
            DWORD parent = md32(data + probe + 8);

            if (op >= 1 && op <= 0x20
                && (dp >> 24) == 0x05 && mdoff(dp) < maxlen
                && parent == 0)
            {
                rootoff = probe;
                break;
            }
        }
    }

    if (rootoff == 0)
    {
        *reasonout = "no root node found in model file.";
        return NULL;
    }

    ZeroMemory(&b, sizeof(b));
    stack[sp++] = rootoff;

    while (sp > 0 && visited < MDL_MAX_NODES)
    {
        DWORD node = stack[--sp];
        DWORD opcode;
        DWORD dataoff;
        DWORD childoff;
        DWORD nextoff;

        if (node == 0 || node + 24 > maxlen)
        {
            continue;
        }

        visited++;

        opcode = md16(data + node) & 0xFFFF;
        dataoff = mdoff(md32(data + node + 4));
        nextoff = mdoff(md32(data + node + 12));
        childoff = mdoff(md32(data + node + 20));

        MdlNodeMeshes(&b, data, maxlen, opcode, dataoff);

        if (sp + 2 <= MDL_MAX_NODES)
        {
            if (nextoff != 0)  { stack[sp++] = nextoff; }
            if (childoff != 0) { stack[sp++] = childoff; }
        }
    }

    if (b.failed || b.count == 0)
    {
        free(b.verts);
        free(b.texids);
        *reasonout = b.failed ? "out of memory building model geometry."
                              : "model produced no triangles.";
        return NULL;
    }

    *tricount = b.count / 3;
    *texids = b.texids;
    return b.verts;
}

/* --- extraction to PLY ------------------------------------------- */

static BOOL MdlWritePly(const char *path, const BgVertex *v, DWORD tricount)
{
    FILE *f = fopen(path, "w");
    DWORD i;

    if (f == NULL)
    {
        return FALSE;
    }

    fprintf(f,
        "ply\nformat ascii 1.0\ncomment GEditor model export\n"
        "element vertex %lu\n"
        "property float x\nproperty float y\nproperty float z\n"
        "property uchar red\nproperty uchar green\nproperty uchar blue\n"
        "property uchar alpha\n"
        "property float s\nproperty float t\n"
        "element face %lu\n"
        "property list uchar int vertex_indices\nend_header\n",
        (unsigned long)(tricount * 3), (unsigned long)tricount);

    for (i = 0; i < tricount * 3; i++)
    {
        fprintf(f, "%g %g %g %u %u %u %u %g %g\n",
                v[i].x, v[i].y, v[i].z, v[i].r, v[i].g, v[i].b, v[i].a,
                v[i].s, v[i].t);
    }

    for (i = 0; i < tricount; i++)
    {
        fprintf(f, "3 %lu %lu %lu\n", (unsigned long)(i * 3),
                (unsigned long)(i * 3 + 1), (unsigned long)(i * 3 + 2));
    }

    fclose(f);
    return TRUE;
}

static const char *MdlClassFolder(const char *name)
{
    if (strcmp(name, "GcartblueZ") == 0 || strcmp(name, "GcartridgeZ") == 0
        || strcmp(name, "GcartrifleZ") == 0 || strcmp(name, "GcartshellZ") == 0)
    {
        return "casings";
    }

    switch (name[0])
    {
    case 'C': return "characters";
    case 'G': return "guns";
    case 'P': return "objects";
    }

    return NULL; /* setups, stans, text, bg - not models */
}

DWORD ModelExtractAll(const RomFile *rom, const char *projectdir,
                      const char **reasonout)
{
    char dir[MAX_PATH];
    char path[MAX_PATH];
    char name[64];
    DWORD written = 0;
    DWORD i;
    static const char *classes[] = { "characters", "guns", "objects", "casings" };

    *reasonout = "";

    wsprintf(dir, "%s\\models", projectdir);
    if (!CreateDirectory(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        *reasonout = "the models folder could not be created.";
        return 0;
    }

    for (i = 0; i < 4; i++)
    {
        wsprintf(path, "%s\\models\\%s", projectdir, classes[i]);
        CreateDirectory(path, NULL);
    }

    for (i = 0; RomGetFileByIndex(rom, i, name, sizeof(name), NULL, NULL); i++)
    {
        const char *cls = MdlClassFolder(name);
        DWORD offset;
        DWORD maxlen;
        DWORD tricount = 0;
        unsigned short *texids = NULL;
        BgVertex *tris;
        const char *why = "";

        if (cls == NULL || strchr(name, '/') != NULL)
        {
            continue;
        }

        if (!RomGetFileByIndex(rom, i, name, sizeof(name), &offset, &maxlen))
        {
            continue;
        }

        tris = ModelLoadGeometry(rom->data + offset, maxlen,
                                 &tricount, &texids, &why);

        if (tris != NULL)
        {
            wsprintf(path, "%s\\models\\%s\\%s.ply", projectdir, cls, name);

            if (MdlWritePly(path, tris, tricount))
            {
                written++;
            }

            free(tris);
            free(texids);
        }
    }

    if (written == 0)
    {
        *reasonout = "no models could be extracted.";
    }

    return written;
}