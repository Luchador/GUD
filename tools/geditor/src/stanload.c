/*
 * GoldenEye stan extraction and project loading.
 *
 * A stan starts with two big-endian pointers. The second points to the
 * first tile, after an optional draw-order table in the header. Tiles
 * have an eight-byte header followed by 3-10 eight-byte points; an all
 * zero tile header terminates the list. This is the same walk used by
 * stanBuildRoomData in the game.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "stanload.h"

#define STAN_FILE_MAX  (16u * 1024u * 1024u)
#define STAN_HEADER_MIN 8u
#define STAN_TILE_HEADER_SIZE 8u
#define STAN_POINT_SIZE 8u
#define STAN_TILE_MAX 65536u

static DWORD StanRead32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16)
         | ((DWORD)p[2] << 8)  |  (DWORD)p[3];
}

static unsigned short StanRead16(const unsigned char *p)
{
    return (unsigned short)(((unsigned int)p[0] << 8) | p[1]);
}

static short StanReadS16(const unsigned char *p)
{
    return (short)StanRead16(p);
}

static BOOL StanResourceNameIsValid(const char *name)
{
    static const char suffix[] = "_stanZ";
    size_t length = strlen(name);
    size_t suffixlength = sizeof(suffix) - 1;
    size_t i;

    if (strncmp(name, "Tbg_", 4) != 0
        || length <= 4 + suffixlength
        || strcmp(name + length - suffixlength, suffix) != 0)
    {
        return FALSE;
    }

    /* stanname is project metadata and can be hand-edited. Only allow
       the resource-name alphabet, keeping paths inside the stan folder. */
    for (i = 0; i < length; i++)
    {
        char ch = name[i];

        if (!((ch >= 'A' && ch <= 'Z')
              || (ch >= 'a' && ch <= 'z')
              || (ch >= '0' && ch <= '9')
              || ch == '_'))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL StanProjectPath(char *path, size_t pathsize,
                            const char *projectdir,
                            const char *stanname)
{
    int written;

    if (!StanResourceNameIsValid(stanname))
    {
        return FALSE;
    }

    written = snprintf(path, pathsize, "%s\\stan\\%s.stan",
                       projectdir, stanname);
    return written >= 0 && written < (int)pathsize;
}

/* First pass: validate every record and count it without trusting the
   file-table length or a malformed point count. */
static BOOL StanCountTiles(const unsigned char *data, DWORD size,
                           DWORD *firstoffsetout, DWORD *countout,
                           const char **reasonout)
{
    DWORD offset;
    DWORD count = 0;

    if (size < STAN_HEADER_MIN)
    {
        *reasonout = "the project stan is too small to have a header.";
        return FALSE;
    }

    offset = StanRead32(data + 4) & 0x00ffffffu;
    if (offset < STAN_HEADER_MIN || offset > size - STAN_TILE_HEADER_SIZE)
    {
        *reasonout = "the stan's first-tile pointer is outside the file.";
        return FALSE;
    }

    *firstoffsetout = offset;

    while (count <= STAN_TILE_MAX)
    {
        DWORD word;
        unsigned int pointcount;
        DWORD recordsize;

        if (offset > size - STAN_TILE_HEADER_SIZE)
        {
            *reasonout = "the stan tile list has no terminator.";
            return FALSE;
        }

        word = StanRead32(data + offset);
        if (word == 0)
        {
            if (count == 0)
            {
                *reasonout = "the stan contains no tiles.";
                return FALSE;
            }

            *countout = count;
            return TRUE;
        }

        pointcount = StanRead16(data + offset + 6) >> 12;
        if (pointcount < 3 || pointcount > STAN_TILE_MAX_POINTS)
        {
            *reasonout = "a stan tile has an invalid point count.";
            return FALSE;
        }

        recordsize = STAN_TILE_HEADER_SIZE + pointcount * STAN_POINT_SIZE;
        if (recordsize > size - offset)
        {
            *reasonout = "a stan tile extends beyond the file.";
            return FALSE;
        }

        offset += recordsize;
        count++;
    }

    *reasonout = "the stan contains too many tiles.";
    return FALSE;
}

static BOOL StanParseTiles(StanFile *stan, float levelscale,
                           const char **reasonout)
{
    DWORD firstoffset;
    DWORD offset;
    DWORD i;
    float worldscale;

    if (!(levelscale > 0.0f) || !isfinite(levelscale))
    {
        *reasonout = "level scale must be greater than zero.";
        return FALSE;
    }

    if (!StanCountTiles(stan->data, stan->size, &firstoffset,
                        &stan->tilecount, reasonout))
    {
        return FALSE;
    }

    stan->tiles = (StanTile *)calloc(stan->tilecount, sizeof(*stan->tiles));
    if (stan->tiles == NULL)
    {
        *reasonout = "out of memory decoding the stan tiles.";
        return FALSE;
    }

    worldscale = 1.0f / levelscale;
    stan->levelscale = levelscale;
    offset = firstoffset;

    for (i = 0; i < stan->tilecount; i++)
    {
        StanTile *tile = &stan->tiles[i];
        DWORD idroom = StanRead32(stan->data + offset);
        unsigned short mid = StanRead16(stan->data + offset + 4);
        unsigned short tail = StanRead16(stan->data + offset + 6);
        unsigned int point;

        tile->id = idroom >> 8;
        tile->editorid = i + 1;
        tile->sourceoffset = offset;
        tile->room = (unsigned char)idroom;
        tile->special = (unsigned char)(mid >> 12);
        tile->red = (unsigned char)(((mid >> 8) & 0xf) * 17);
        tile->green = (unsigned char)(((mid >> 4) & 0xf) * 17);
        tile->blue = (unsigned char)((mid & 0xf) * 17);
        tile->pointcount = (unsigned char)(tail >> 12);
        tile->extreme[0] = (unsigned char)((tail >> 8) & 0xf);
        tile->extreme[1] = (unsigned char)((tail >> 4) & 0xf);
        tile->extreme[2] = (unsigned char)(tail & 0xf);

        if (tile->extreme[0] >= tile->pointcount
            || tile->extreme[1] >= tile->pointcount
            || tile->extreme[2] >= tile->pointcount)
        {
            *reasonout = "a stan tile has an invalid representative triangle.";
            return FALSE;
        }

        for (point = 0; point < tile->pointcount; point++)
        {
            const unsigned char *src = stan->data + offset
                                     + STAN_TILE_HEADER_SIZE
                                     + point * STAN_POINT_SIZE;
            StanPoint *dst = &tile->points[point];

            dst->x = StanReadS16(src + 0) * worldscale;
            dst->y = StanReadS16(src + 2) * worldscale;
            dst->z = StanReadS16(src + 4) * worldscale;
            dst->link = StanRead16(src + 6);
        }

        offset += STAN_TILE_HEADER_SIZE
                + tile->pointcount * STAN_POINT_SIZE;
    }

    return TRUE;
}

DWORD StanExtractAll(const RomFile *rom, const char *projectdir,
                     const char **reasonout)
{
    char dir[MAX_PATH];
    DWORD count = 0;
    DWORD index;
    int written;

    *reasonout = "";

    written = snprintf(dir, sizeof(dir), "%s\\stan", projectdir);
    if (written < 0 || written >= (int)sizeof(dir))
    {
        *reasonout = "the stan folder path is too long.";
        return 0;
    }

    if (!CreateDirectory(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        *reasonout = "the stan folder could not be created.";
        return 0;
    }

    for (index = 0; ; index++)
    {
        char name[64];
        char path[MAX_PATH];
        DWORD offset;
        DWORD length;
        DWORD output;
        HANDLE file;
        BOOL ok;

        if (!RomGetFileByIndex(rom, index, name, sizeof(name), NULL, NULL))
        {
            break;
        }

        if (!StanResourceNameIsValid(name))
        {
            continue;
        }

        if (!RomGetFileByIndex(rom, index, name, sizeof(name),
                               &offset, &length))
        {
            *reasonout = "a stan file-table entry is invalid.";
            return 0;
        }

        if (!StanProjectPath(path, sizeof(path), projectdir, name))
        {
            *reasonout = "a stan output path is too long.";
            return 0;
        }

        file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE)
        {
            *reasonout = "a stan file could not be created.";
            return 0;
        }

        ok = WriteFile(file, rom->data + offset, length, &output, NULL)
          && output == length;
        CloseHandle(file);

        if (!ok)
        {
            *reasonout = "a stan file could not be fully written.";
            return 0;
        }

        count++;
    }

    if (count == 0)
    {
        *reasonout = "the ROM file table contains no stan files.";
    }

    return count;
}

BOOL StanLoadProjectFile(const char *projectdir, const char *stanname,
                         float levelscale, StanFile *out,
                         const char **reasonout)
{
    char path[MAX_PATH];
    HANDLE file;
    DWORD got;

    ZeroMemory(out, sizeof(*out));
    *reasonout = "";

    if (!StanProjectPath(path, sizeof(path), projectdir, stanname))
    {
        *reasonout = "the stan resource name or path is invalid.";
        return FALSE;
    }

    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        *reasonout = "the stan .stan file is missing from this project.";
        return FALSE;
    }

    out->size = GetFileSize(file, NULL);
    if (out->size == INVALID_FILE_SIZE || out->size == 0
        || out->size > STAN_FILE_MAX)
    {
        CloseHandle(file);
        ZeroMemory(out, sizeof(*out));
        *reasonout = "the project stan is empty, too large, or unreadable.";
        return FALSE;
    }

    out->data = (unsigned char *)malloc(out->size);
    if (out->data == NULL)
    {
        CloseHandle(file);
        ZeroMemory(out, sizeof(*out));
        *reasonout = "out of memory reading the project stan.";
        return FALSE;
    }

    if (!ReadFile(file, out->data, out->size, &got, NULL)
        || got != out->size)
    {
        CloseHandle(file);
        StanFileFree(out);
        *reasonout = "the project stan file could not be fully read.";
        return FALSE;
    }

    CloseHandle(file);

    if (!StanParseTiles(out, levelscale, reasonout))
    {
        StanFileFree(out);
        return FALSE;
    }

    strncpy(out->name, stanname, sizeof(out->name) - 1);
    return TRUE;
}


static void StanWrite32(unsigned char *p, DWORD value)
{
    p[0] = (unsigned char)(value >> 24); p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8); p[3] = (unsigned char)value;
}

static DWORD StanTileAtOffset(const StanFile *stan, DWORD offset)
{
    DWORD low = 0, high = stan->tilecount;
    while (low < high)
    {
        DWORD mid = low + (high - low) / 2;
        if (stan->tiles[mid].sourceoffset < offset) { low = mid + 1; }
        else { high = mid; }
    }
    return low < stan->tilecount && stan->tiles[low].sourceoffset == offset ? low : STAN_TILE_NONE;
}

/* stanBuildRoomData and stanFindTileBelowPos require one contiguous run per
 * room. Reorder only the saved copy: live indices, selection and undo remain
 * stable. Preserve room encounter order and tile order within each room. */
static BOOL StanGroupRoomsForSave(const StanFile *stan, unsigned char **out, const char **reasonout)
{
    BOOL seen[STAN_MAX_ROOM + 1] = {0}, grouped = TRUE;
    DWORD first, end, previous = STAN_MAX_ROOM + 1, *offsets = NULL;
    unsigned char *data = NULL;
    const char *why = "The stan tile records are inconsistent.";
    *out = NULL;
    /* Extraction and native fixture writers can supply an undecoded file. */
    if (!stan->tiles || !stan->tilecount) { return TRUE; }
    first = stan->tiles[0].sourceoffset;
    if (stan->size < 12 || first < 12 || first > stan->size - 8 || (first & 3)
        || (StanRead32(stan->data + 4) & 0xffffffu) != first) { goto fail; }
    end = first;
    for (DWORD i = 0; i < stan->tilecount; i++)
    {
        const StanTile *tile = stan->tiles + i;
        DWORD size = 8u + tile->pointcount * 8u;
        if (tile->room > STAN_MAX_ROOM || tile->sourceoffset != end
            || tile->pointcount < 3 || tile->pointcount > STAN_TILE_MAX_POINTS
            || end > stan->size || size > stan->size - end
            || StanRead32(stan->data + end) != (tile->id << 8 | tile->room)
            || stan->data[end + 6] >> 4 != tile->pointcount) { goto fail; }
        if (tile->room != previous && seen[tile->room]) { grouped = FALSE; }
        seen[tile->room] = TRUE; previous = tile->room;
        end += size;
    }
    if (end > stan->size - 8 || StanRead32(stan->data + end) != 0) { goto fail; }
    if (grouped) { return TRUE; }
    offsets = malloc((size_t)stan->tilecount * sizeof(*offsets));
    data = malloc(stan->size);
    if (!offsets || !data) { why = "Out of memory grouping stan tiles by room."; goto fail; }
    memcpy(data, stan->data, stan->size);
    memset(seen, 0, sizeof(seen));
    end = first;
    for (DWORD i = 0; i < stan->tilecount; i++)
    {
        unsigned int room = stan->tiles[i].room;
        if (seen[room]) { continue; }
        seen[room] = TRUE;
        for (DWORD j = i; j < stan->tilecount; j++)
        {
            const StanTile *tile = stan->tiles + j;
            DWORD size = 8u + tile->pointcount * 8u;
            if (tile->room != room) { continue; }
            offsets[j] = end;
            memcpy(data + end, stan->data + tile->sourceoffset, size);
            end += size;
        }
    }
    for (DWORD i = 0; i < stan->tilecount; i++)
    {
        const StanTile *tile = stan->tiles + i;
        for (unsigned int p = 0; p < tile->pointcount; p++)
        {
            unsigned char *raw = data + offsets[i] + 8u + p * 8u + 6u;
            unsigned int link = StanRead16(raw);
            DWORD target, relocated;
            if (link < 0x10) { continue; }
            target = StanTileAtOffset(stan, first + (link - 0x10u) * 8u);
            if (target == STAN_TILE_NONE)
            { why = "A stan edge link points to a missing tile."; goto fail; }
            relocated = (offsets[target] - first) / 8u + 0x10u;
            if (relocated > 0xffffu)
            { why = "A relocated stan edge link exceeds the native range."; goto fail; }
            raw[0] = (unsigned char)(relocated >> 8); raw[1] = (unsigned char)relocated;
        }
    }
    /* Relocate all header tile pointers, keeping their segment bytes. The
     * first tile stays first, preserving the base used by every edge link. */
    for (DWORD i = 4; i < first; i += 4)
    {
        DWORD pointer = StanRead32(stan->data + i), target;
        if (!pointer)
        {
            if (i != first - 4) { goto fail; }
            break;
        }
        if (i == first - 4) { goto fail; }
        target = StanTileAtOffset(stan, pointer & 0xffffffu);
        if (target == STAN_TILE_NONE) { goto fail; }
        StanWrite32(data + i, (pointer & 0xff000000u) | offsets[target]);
    }
    free(offsets); *out = data;
    return TRUE;
fail:
    free(offsets); free(data); *reasonout = why;
    return FALSE;
}

BOOL StanSaveProjectFile(const char *projectdir, const StanFile *stan,
                         const char **reasonout)
{
    char path[MAX_PATH];
    HANDLE file;
    DWORD written;
    BOOL ok;
    unsigned char *grouped = NULL;

    *reasonout = "";

    if (stan == NULL || stan->data == NULL || stan->size == 0
        || !StanProjectPath(path, sizeof(path), projectdir, stan->name))
    {
        *reasonout = "there is no valid stan loaded to save.";
        return FALSE;
    }

    if (!StanGroupRoomsForSave(stan, &grouped, reasonout)) { return FALSE; }
    file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        free(grouped);
        *reasonout = "the project stan could not be opened for writing.";
        return FALSE;
    }

    ok = WriteFile(file, grouped ? grouped : stan->data, stan->size, &written, NULL)
      && written == stan->size;
    free(grouped);
    if (!CloseHandle(file))
    {
        ok = FALSE;
    }

    if (!ok)
    {
        *reasonout = "the project stan could not be fully written.";
    }

    return ok;
}

void StanFileFree(StanFile *stan)
{
    free(stan->tiles);
    free(stan->data);
    ZeroMemory(stan, sizeof(*stan));
}


BOOL StanFileClone(const StanFile *source, StanFile *out, const char **reasonout)
{
    ZeroMemory(out, sizeof(*out));
    *reasonout = "";
    if (source == NULL || source->data == NULL || source->tiles == NULL)
    {
        *reasonout = "there is no stan document to copy.";
        return FALSE;
    }
    *out = *source;
    out->data = malloc(source->size);
    out->tiles = malloc((size_t)source->tilecount * sizeof(*out->tiles));
    if (out->data == NULL || out->tiles == NULL)
    {
        StanFileFree(out);
        *reasonout = "out of memory copying the stan document.";
        return FALSE;
    }
    memcpy(out->data, source->data, source->size);
    memcpy(out->tiles, source->tiles, (size_t)source->tilecount * sizeof(*out->tiles));
    return TRUE;
}
