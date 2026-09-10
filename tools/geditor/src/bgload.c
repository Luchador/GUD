/*
 * GoldenEye bg file parser.
 *
 * The file layout mirrors what bgLoadFile in the game does:
 *   header word 1 -> room record list (24-byte records, index 0 unused)
 *   header word 2 -> portal table (8-byte records pointing to polygons)
 *   each record  -> vertex blob, primary GDL, secondary GDL, room pos
 * Every block is preceded by a 4-byte size word. Internal pointers are
 * 0x0F-segment addresses; their low 24 bits are file offsets.
 *
 * The display lists are Fast3D: 8-byte commands, G_VTX (0x04) loads a
 * vertex batch, G_TRI1 (0xBF) indexes it with bytes scaled by 10,
 * G_TRI4 (0xB1) packs four nibble-indexed triangles, G_ENDDL (0xB8)
 * ends, and geometry-mode commands carry the authored culling state.
 * The remaining Fast3D state is not needed by the current preview.
 *
 * Malformed data is handled by stopping the current room, never by
 * reading outside the file: the parser must survive any input.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bgload.h"
#include "bgmaterial.h"

#define BG_ROOM_RECORD_SIZE 24
#define BG_MAX_ROOMS        256
#define BG_PORTAL_RECORD_SIZE 8

#define G_VTX   0x04
#define G_TRI4  0xB1
#define G_CLEARGEOMETRYMODE 0xB6
#define G_SETGEOMETRYMODE   0xB7
#define G_ENDDL 0xB8
#define G_TRI1  0xBF

#define G_CULL_BACK 0x00002000

static const char *BgBaseName(const char *name)
{
    const char *slash = strrchr(name, '/');
    const char *backslash = strrchr(name, '\\');

    if (backslash != NULL && (slash == NULL || backslash > slash))
    {
        slash = backslash;
    }

    return slash != NULL ? slash + 1 : name;
}

static BOOL BgProjectPath(char *path, size_t pathsize,
                          const char *projectdir, const char *bgname)
{
    const char *base = BgBaseName(bgname);
    int written;

    if (base[0] == '\0')
    {
        return FALSE;
    }

    written = snprintf(path, pathsize, "%s\\bg\\%s", projectdir, base);
    return written >= 0 && written < (int)pathsize;
}

typedef struct BgBuilder {
    BgVertex       *verts;
    unsigned short *tags;       /* one per triangle */
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
        unsigned short *grownt = (unsigned short *)realloc(b->tags,
                                     (next / 3) * sizeof(unsigned short));

        if (grown != NULL) { b->verts = grown; }
        if (grownt != NULL) { b->tags = grownt; }
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
 * GUD's uncompressed BG converter prefixes every room stream with its
 * byte length and aligns the completed segment to 16 bytes. Portal data
 * sits before those streams, so measuring the stream end preserves the
 * complete room and portal asset while still finding the real .seg
 * boundary when file-table aliases or an unreferenced linker blob make
 * the next table address only an upper bound.
 */
static DWORD BgMeasureFileLength(const unsigned char *data, DWORD maxlen)
{
    DWORD roomlist;
    DWORD end = 0;
    DWORD i;

    if (maxlen < 0x40 || bg32(data) != 0)
    {
        return maxlen;
    }

    roomlist = bg32(data + 4) & 0x00FFFFFF;
    if (roomlist >= maxlen)
    {
        return maxlen;
    }

    for (i = 1; i < BG_MAX_ROOMS; i++)
    {
        DWORD rec = roomlist + i * BG_ROOM_RECORD_SIZE;
        int field;

        if (rec + BG_ROOM_RECORD_SIZE > maxlen || bg32(data + rec + 4) == 0)
        {
            break;
        }

        for (field = 0; field < 3; field++)
        {
            DWORD offset = bg32(data + rec + field * 4) & 0x00FFFFFF;

            if (offset != 0)
            {
                DWORD size = BgBlockSize(data, maxlen, offset);

                if (size == 0)
                {
                    return maxlen;
                }

                if (offset + size > end)
                {
                    end = offset + size;
                }
            }
        }
    }

    if (end == 0 || end > maxlen - 15)
    {
        return maxlen;
    }

    end = (end + 15) & ~(DWORD)15;
    return end <= maxlen ? end : maxlen;
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
    const unsigned char *vertexcache[16];
    BgMaterial material;
    /* Valid room streams establish this state before their first tri.
       False is the safest fallback for malformed or future data. */
    BOOL cullbackfaces = FALSE;

    BgMaterialInit(&material);
    ZeroMemory(vertexcache, sizeof(vertexcache));
    /* every triangle this walk emits carries the layer flag */

    for (pc = gdloffset; pc + 8 <= gdloffset + gdlsize && pc + 8 <= maxlen; pc += 8)
    {
        const unsigned char *cmd = data + pc;

        if (cmd[0] == G_ENDDL)
        {
            return;
        }

        if (BgMaterialReadCommand(&material, bg32(cmd), bg32(cmd + 4)))
        {
            continue;
        }

        if (cmd[0] == G_SETGEOMETRYMODE)
        {
            if (bg32(cmd + 4) & G_CULL_BACK)
            {
                cullbackfaces = TRUE;
            }
            continue;
        }

        if (cmd[0] == G_CLEARGEOMETRYMODE)
        {
            if (bg32(cmd + 4) & G_CULL_BACK)
            {
                cullbackfaces = FALSE;
            }
            continue;
        }

        if (cmd[0] == G_VTX)
        {
            DWORD addr = bg32(cmd + 4) & 0x00FFFFFF;
            DWORD batchcount = ((cmd[1] >> 4) & 0xF) + 1;
            DWORD batchv0 = cmd[1] & 0xF;
            DWORD vertex;

            if (batchv0 + batchcount > 16)
            {
                continue;
            }
            for (vertex = 0; vertex < batchcount; vertex++)
            {
                vertexcache[batchv0 + vertex] = NULL;
            }
            if (addr > vtxsize || batchcount * 16 > vtxsize - addr)
            {
                continue;
            }
            for (vertex = 0; vertex < batchcount; vertex++)
            {
                vertexcache[batchv0 + vertex] = vtxblob + addr + vertex * 16;
            }
            continue;
        }

        if (cmd[0] == G_TRI1 || cmd[0] == G_TRI4)
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

                /* Only 0,0,0 in a TRI4 is an unused packed slot. */
                if (cmd[0] == G_TRI4
                    && idx[0] == 0 && idx[1] == 0 && idx[2] == 0)
                {
                    continue;
                }

                if (idx[0] < 0 || idx[1] < 0 || idx[2] < 0
                    || idx[0] >= 16 || idx[1] >= 16 || idx[2] >= 16
                    || vertexcache[idx[0]] == NULL
                    || vertexcache[idx[1]] == NULL
                    || vertexcache[idx[2]] == NULL)
                {
                    continue; /* index escaped the cache: drop the tri */
                }

                for (k = 0; k < 3; k++)
                {
                    const unsigned char *v = vertexcache[idx[k]];
                    BgVertex out = {0};

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

                /* Record the render state after the pushes: growth has
                   already resized the tag array to match. */
                if (!b->failed)
                {
                    unsigned short cullflag = cullbackfaces ? BG_TRI_CULL_BACK : 0;

                    b->tags[b->count / 3 - 1] =
                        (unsigned short)(BgMaterialTextureId(&material) | layerflag | cullflag);
                }
            }
        }
    }
}

BgVertex *BgLoadGeometry(const unsigned char *data, DWORD maxlen,
                         float levelscale,
                         DWORD *tricount, unsigned short **tritags,
                         const char **reasonout)
{
    BgBuilder b;
    float worldscale;
    DWORD roomlist;
    DWORD rooms;
    DWORD i;

    *tricount = 0;
    *tritags = NULL;
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
        free(b.tags);
        *reasonout = b.failed ? "out of memory building bg geometry."
                              : "bg file produced no triangles.";
        return NULL;
    }

    *tricount = b.count / 3;
    *tritags = b.tags;
    return b.verts;
}


BOOL BgLoadPortals(const unsigned char *data, DWORD maxlen,
                   float levelscale, BgPortalFile *out,
                   const char **reasonout)
{
    DWORD tableoffset;
    DWORD portalcount = 0;
    DWORD i;
    float worldscale;

    ZeroMemory(out, sizeof(*out));
    *reasonout = "";

    if (!(levelscale > 0.0f))
    {
        *reasonout = "level scale must be greater than zero.";
        return FALSE;
    }

    if (maxlen < 0x14)
    {
        *reasonout = "bg file is too small to have a portal table.";
        return FALSE;
    }

    if (bg32(data) != 0)
    {
        *reasonout = "bg file is a single display list and has no portal table.";
        return FALSE;
    }

    tableoffset = bg32(data + 8) & 0x00ffffffu;
    if (tableoffset > maxlen - BG_PORTAL_RECORD_SIZE)
    {
        *reasonout = "bg portal table offset is outside the file.";
        return FALSE;
    }

    /* The game stops on the first null portal pointer. Count with the
       same rule, but cap the walk at its PORTMAX-sized runtime tables. */
    for (portalcount = 0; portalcount < BG_MAX_PORTALS; portalcount++)
    {
        DWORD recordoffset = tableoffset
                           + portalcount * BG_PORTAL_RECORD_SIZE;

        if (recordoffset > maxlen - BG_PORTAL_RECORD_SIZE)
        {
            *reasonout = "bg portal table has no terminator.";
            return FALSE;
        }

        if (bg32(data + recordoffset) == 0)
        {
            break;
        }
    }

    if (portalcount == BG_MAX_PORTALS)
    {
        *reasonout = "bg portal table exceeds the game's portal limit.";
        return FALSE;
    }

    if (portalcount == 0)
    {
        return TRUE;
    }

    out->portals = (BgPortal *)calloc(portalcount, sizeof(*out->portals));
    if (out->portals == NULL)
    {
        *reasonout = "out of memory decoding the bg portals.";
        return FALSE;
    }

    out->portalcount = portalcount;
    worldscale = 1.0f / levelscale;

    for (i = 0; i < portalcount; i++)
    {
        DWORD recordoffset = tableoffset + i * BG_PORTAL_RECORD_SIZE;
        DWORD geometryoffset = bg32(data + recordoffset) & 0x00ffffffu;
        unsigned int pointcount;
        DWORD pointbytes;
        unsigned int point;
        BgPortal *portal = &out->portals[i];

        if (geometryoffset > maxlen - 4)
        {
            BgPortalFileFree(out);
            *reasonout = "a bg portal geometry pointer is outside the file.";
            return FALSE;
        }

        pointcount = data[geometryoffset];
        if (pointcount < 3 || pointcount > BG_PORTAL_MAX_POINTS)
        {
            BgPortalFileFree(out);
            *reasonout = "a bg portal has an invalid point count.";
            return FALSE;
        }

        pointbytes = pointcount * 12;
        if (pointbytes > maxlen - geometryoffset - 4)
        {
            BgPortalFileFree(out);
            *reasonout = "a bg portal extends beyond the file.";
            return FALSE;
        }

        portal->geometryoffset = geometryoffset;
        portal->connectedroom1 = data[recordoffset + 4];
        portal->connectedroom2 = data[recordoffset + 5];
        portal->controlbytes1 = data[recordoffset + 6];
        portal->controlbytes2 = data[recordoffset + 7];
        portal->pointcount = (unsigned char)pointcount;

        for (point = 0; point < pointcount; point++)
        {
            const unsigned char *src = data + geometryoffset + 4 + point * 12;

            portal->points[point].x = bgf32(src + 0) * worldscale;
            portal->points[point].y = bgf32(src + 4) * worldscale;
            portal->points[point].z = bgf32(src + 8) * worldscale;
        }
    }

    return TRUE;
}


DWORD BgExtractAll(const RomFile *rom, const char *projectdir,
                   const char **reasonout)
{
    char dir[MAX_PATH];
    DWORD count = 0;
    DWORD i;
    int written;

    *reasonout = "";

    written = snprintf(dir, sizeof(dir), "%s\\bg", projectdir);
    if (written < 0 || written >= (int)sizeof(dir))
    {
        *reasonout = "the bg folder path is too long.";
        return 0;
    }

    if (!CreateDirectory(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        *reasonout = "the bg folder could not be created.";
        return 0;
    }

    for (i = 0; ; i++)
    {
        char bgname[64];
        DWORD offset;
        DWORD length;
        char path[MAX_PATH];
        HANDLE file;
        DWORD output;
        BOOL ok;
        size_t namelen;

        if (!RomGetFileByIndex(rom, i, bgname, sizeof(bgname), NULL, NULL))
        {
            break;
        }

        namelen = strlen(bgname);
        if (strncmp(bgname, "bg/", 3) != 0
            || namelen < 4 || strcmp(bgname + namelen - 4, ".seg") != 0)
        {
            continue;
        }

        if (!RomGetFileByIndex(rom, i, bgname, sizeof(bgname),
                               &offset, &length))
        {
            *reasonout = "a background file-table entry is invalid.";
            return 0;
        }

        length = BgMeasureFileLength(rom->data + offset, length);

        if (!BgProjectPath(path, sizeof(path), projectdir, bgname))
        {
            *reasonout = "a background output path is too long.";
            return 0;
        }

        file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE)
        {
            *reasonout = "a background file could not be created.";
            return 0;
        }

        ok = WriteFile(file, rom->data + offset, length, &output, NULL)
          && output == length;
        CloseHandle(file);

        if (!ok)
        {
            *reasonout = "a background file could not be fully written.";
            return 0;
        }

        count++;
    }

    if (count == 0)
    {
        *reasonout = "the ROM file table contains no backgrounds.";
    }

    return count;
}


static unsigned char *BgReadProjectFile(const char *projectdir,
                                        const char *bgname,
                                        DWORD *lengthout,
                                        const char **reasonout)
{
    char path[MAX_PATH];
    HANDLE file;
    DWORD length;
    DWORD got;
    unsigned char *data;
    *lengthout = 0;
    *reasonout = "";

    if (!BgProjectPath(path, sizeof(path), projectdir, bgname))
    {
        *reasonout = "the background path is too long.";
        return NULL;
    }

    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        *reasonout = "the background .seg file is missing from this project.";
        return NULL;
    }

    length = GetFileSize(file, NULL);
    if (length == INVALID_FILE_SIZE || length == 0)
    {
        CloseHandle(file);
        *reasonout = "the project background file is empty or unreadable.";
        return NULL;
    }

    data = (unsigned char *)malloc(length);
    if (data == NULL)
    {
        CloseHandle(file);
        *reasonout = "out of memory reading the project background.";
        return NULL;
    }

    if (!ReadFile(file, data, length, &got, NULL) || got != length)
    {
        free(data);
        CloseHandle(file);
        *reasonout = "the project background file could not be fully read.";
        return NULL;
    }

    CloseHandle(file);
    *lengthout = length;
    return data;
}


BOOL BgLoadProjectFile(const char *projectdir, const char *bgname,
                       BgFile *out, const char **reasonout)
{
    ZeroMemory(out, sizeof(*out));
    out->data = BgReadProjectFile(projectdir, bgname, &out->size,
                                  reasonout);
    if (out->data == NULL)
    {
        return FALSE;
    }

    strncpy(out->name, bgname, sizeof(out->name) - 1);
    return TRUE;
}


BOOL BgSaveProjectFile(const char *projectdir, const BgFile *bg,
                       const char **reasonout)
{
    char path[MAX_PATH];
    HANDLE file;
    DWORD written;
    BOOL ok;

    *reasonout = "";

    if (bg == NULL || bg->data == NULL || bg->size == 0
        || !BgProjectPath(path, sizeof(path), projectdir, bg->name))
    {
        *reasonout = "there is no valid background loaded to save.";
        return FALSE;
    }

    file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        *reasonout = "the project background could not be opened for writing.";
        return FALSE;
    }

    ok = WriteFile(file, bg->data, bg->size, &written, NULL)
      && written == bg->size;
    if (!CloseHandle(file))
    {
        ok = FALSE;
    }

    if (!ok)
    {
        *reasonout = "the project background could not be fully written.";
    }

    return ok;
}


void BgFileFree(BgFile *bg)
{
    free(bg->data);
    ZeroMemory(bg, sizeof(*bg));
}


BgVertex *BgLoadProjectGeometry(const char *projectdir, const char *bgname,
                                float levelscale,
                                DWORD *tricount, unsigned short **tritags,
                                const char **reasonout)
{
    BgFile bg;
    BgVertex *vertices;

    *tricount = 0;
    *tritags = NULL;

    if (!BgLoadProjectFile(projectdir, bgname, &bg, reasonout))
    {
        return NULL;
    }

    vertices = BgLoadGeometry(bg.data, bg.size, levelscale,
                              tricount, tritags, reasonout);
    BgFileFree(&bg);
    return vertices;
}


BOOL BgLoadProjectPortals(const char *projectdir, const char *bgname,
                          float levelscale, BgPortalFile *out,
                          const char **reasonout)
{
    BgFile bg;
    BOOL ok;

    ZeroMemory(out, sizeof(*out));
    if (!BgLoadProjectFile(projectdir, bgname, &bg, reasonout))
    {
        return FALSE;
    }

    ok = BgLoadPortals(bg.data, bg.size, levelscale, out, reasonout);
    BgFileFree(&bg);
    return ok;
}


void BgPortalFileFree(BgPortalFile *portals)
{
    free(portals->portals);
    ZeroMemory(portals, sizeof(*portals));
}

float BgPortalGetMargin(const BgPortal *portal)
{
    unsigned int value = portal->controlbytes2;
    return (value & 15) * .25f * (1u << (value >> 4));
}
