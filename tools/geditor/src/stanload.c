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

    if (!(levelscale > 0.0f))
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
    offset = firstoffset;

    for (i = 0; i < stan->tilecount; i++)
    {
        StanTile *tile = &stan->tiles[i];
        DWORD idroom = StanRead32(stan->data + offset);
        unsigned short mid = StanRead16(stan->data + offset + 4);
        unsigned short tail = StanRead16(stan->data + offset + 6);
        unsigned int point;

        tile->id = idroom >> 8;
        tile->room = (unsigned char)idroom;
        tile->special = (unsigned char)(mid >> 12);
        tile->red = (unsigned char)(((mid >> 8) & 0xf) * 17);
        tile->green = (unsigned char)(((mid >> 4) & 0xf) * 17);
        tile->blue = (unsigned char)((mid & 0xf) * 17);
        tile->pointcount = (unsigned char)(tail >> 12);
        tile->extreme[0] = (unsigned char)((tail >> 8) & 0xf);
        tile->extreme[1] = (unsigned char)((tail >> 4) & 0xf);
        tile->extreme[2] = (unsigned char)(tail & 0xf);

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


BOOL StanSaveProjectFile(const char *projectdir, const StanFile *stan,
                         const char **reasonout)
{
    char path[MAX_PATH];
    HANDLE file;
    DWORD written;
    BOOL ok;

    *reasonout = "";

    if (stan == NULL || stan->data == NULL || stan->size == 0
        || !StanProjectPath(path, sizeof(path), projectdir, stan->name))
    {
        *reasonout = "there is no valid stan loaded to save.";
        return FALSE;
    }

    file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        *reasonout = "the project stan could not be opened for writing.";
        return FALSE;
    }

    ok = WriteFile(file, stan->data, stan->size, &written, NULL)
      && written == stan->size;
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
