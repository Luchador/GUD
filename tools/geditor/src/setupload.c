/*
 * GEditor setup-file extraction and project loading.
 *
 * GUD stores setup binaries as ordinary obseg resources whose table
 * names omit the on-disk extension. A project gives those raw binaries
 * the .set extension used by the editor; their contents are otherwise
 * copied byte-for-byte.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "setupload.h"

#define SETUP_FILE_MAX (16u * 1024u * 1024u)
#define SETUP_HEADER_SIZE       40u
#define SETUP_PAD_POINTER       24u
#define SETUP_BOUNDPAD_POINTER  28u
#define SETUP_PAD_SIZE          44u
#define SETUP_BOUNDPAD_SIZE     68u
#define SETUP_PAD_LINK          36u
#define SETUP_BOUNDPAD_BBOX     44u
#define SETUP_PAD_MAX        65536u

static DWORD SetupRead32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16)
         | ((DWORD)p[2] << 8)  |  (DWORD)p[3];
}

/* Returns FALSE for NaN and infinity as well as decoding big endian. */
static BOOL SetupReadFloat(const unsigned char *p, float *out)
{
    union { DWORD u; float f; } value;

    value.u = SetupRead32(p);
    if ((value.u & 0x7f800000u) == 0x7f800000u)
    {
        return FALSE;
    }

    *out = value.f;
    return TRUE;
}

/* Counts records using the same plink==NULL terminator as the game. */
static BOOL SetupCountPadList(const SetupFile *setup, DWORD offset,
                              DWORD recordsize, DWORD *countout)
{
    DWORD count;

    if (offset < SETUP_HEADER_SIZE || offset > setup->size)
    {
        return FALSE;
    }

    for (count = 0; count <= SETUP_PAD_MAX; count++)
    {
        DWORD record;

        if (count > (setup->size - offset) / recordsize)
        {
            return FALSE;
        }

        record = offset + count * recordsize;
        if (recordsize > setup->size - record)
        {
            return FALSE;
        }

        if (SetupRead32(setup->data + record + SETUP_PAD_LINK) == 0)
        {
            *countout = count;
            return TRUE;
        }
    }

    return FALSE;
}

static BOOL SetupReadPad(const unsigned char *record, SetupPad *pad)
{
    int axis;

    for (axis = 0; axis < 3; axis++)
    {
        if (!SetupReadFloat(record + axis * 4, &pad->pos[axis])
            || !SetupReadFloat(record + 12 + axis * 4, &pad->up[axis])
            || !SetupReadFloat(record + 24 + axis * 4, &pad->look[axis]))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL SetupParsePads(SetupFile *setup, const char **reasonout)
{
    DWORD padoffset;
    DWORD boundoffset;
    DWORD i;

    if (setup->size < SETUP_HEADER_SIZE)
    {
        *reasonout = "the project setup is too small to have a header.";
        return FALSE;
    }

    padoffset = SetupRead32(setup->data + SETUP_PAD_POINTER);
    boundoffset = SetupRead32(setup->data + SETUP_BOUNDPAD_POINTER);

    if (!SetupCountPadList(setup, padoffset, SETUP_PAD_SIZE,
                           &setup->padcount))
    {
        *reasonout = "the setup's PadRecord list is malformed.";
        return FALSE;
    }

    if (!SetupCountPadList(setup, boundoffset, SETUP_BOUNDPAD_SIZE,
                           &setup->boundpadcount))
    {
        *reasonout = "the setup's BoundPadRecord list is malformed.";
        return FALSE;
    }

    if (setup->padcount > 0)
    {
        setup->pads = (SetupPad *)malloc(setup->padcount * sizeof(*setup->pads));
    }
    if (setup->boundpadcount > 0)
    {
        setup->boundpads = (SetupBoundPad *)malloc(
            setup->boundpadcount * sizeof(*setup->boundpads));
    }

    if ((setup->padcount > 0 && setup->pads == NULL)
        || (setup->boundpadcount > 0 && setup->boundpads == NULL))
    {
        *reasonout = "out of memory decoding the setup's pads.";
        return FALSE;
    }

    for (i = 0; i < setup->padcount; i++)
    {
        const unsigned char *record = setup->data + padoffset
                                    + i * SETUP_PAD_SIZE;

        if (!SetupReadPad(record, &setup->pads[i]))
        {
            *reasonout = "a PadRecord contains an invalid coordinate.";
            return FALSE;
        }
    }

    for (i = 0; i < setup->boundpadcount; i++)
    {
        const unsigned char *record = setup->data + boundoffset
                                    + i * SETUP_BOUNDPAD_SIZE;
        SetupBoundPad *pad = &setup->boundpads[i];

        if (!SetupReadPad(record, &pad->pad)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 0, &pad->xmin)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 4, &pad->xmax)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 8, &pad->ymin)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 12, &pad->ymax)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 16, &pad->zmin)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 20, &pad->zmax))
        {
            *reasonout = "a BoundPadRecord contains an invalid coordinate.";
            return FALSE;
        }
    }

    return TRUE;
}

void SetupPadGetBoxCorners(const SetupPad *pad,
                           float xmin, float xmax,
                           float ymin, float ymax,
                           float zmin, float zmax,
                           float worldscale,
                           float corners[8][3])
{
    float side[3];
    int corner;

    side[0] = pad->up[1] * pad->look[2] - pad->up[2] * pad->look[1];
    side[1] = pad->up[2] * pad->look[0] - pad->up[0] * pad->look[2];
    side[2] = pad->up[0] * pad->look[1] - pad->up[1] * pad->look[0];

    for (corner = 0; corner < 8; corner++)
    {
        float x = (corner & 1) ? xmax : xmin;
        float y = (corner & 2) ? ymax : ymin;
        float z = (corner & 4) ? zmax : zmin;
        int axis;

        for (axis = 0; axis < 3; axis++)
        {
            corners[corner][axis] = (pad->pos[axis]
                + side[axis] * x
                + pad->up[axis] * y
                + pad->look[axis] * z) * worldscale;
        }
    }
}

static BOOL SetupResourceNameIsValid(const char *name)
{
    size_t length = strlen(name);
    size_t i;

    if ((strncmp(name, "Usetup", 6) != 0
         && strncmp(name, "Ump_setup", 9) != 0)
        || length == 0 || name[length - 1] != 'Z')
    {
        return FALSE;
    }

    /* Project metadata is editable text. Restrict the filename to the
       resource-name alphabet so it can never escape the setup folder. */
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

static BOOL SetupProjectPath(char *path, size_t pathsize,
                             const char *projectdir,
                             const char *setupname)
{
    int written;

    if (!SetupResourceNameIsValid(setupname))
    {
        return FALSE;
    }

    written = snprintf(path, pathsize, "%s\\setup\\%s.set",
                       projectdir, setupname);
    return written >= 0 && written < (int)pathsize;
}

DWORD SetupExtractAll(const RomFile *rom, const char *projectdir,
                      const char **reasonout)
{
    char dir[MAX_PATH];
    DWORD count = 0;
    DWORD index;
    int written;

    *reasonout = "";

    written = snprintf(dir, sizeof(dir), "%s\\setup", projectdir);
    if (written < 0 || written >= (int)sizeof(dir))
    {
        *reasonout = "the setup folder path is too long.";
        return 0;
    }

    if (!CreateDirectory(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        *reasonout = "the setup folder could not be created.";
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

        if (!SetupResourceNameIsValid(name))
        {
            continue;
        }

        if (!RomGetFileByIndex(rom, index, name, sizeof(name),
                               &offset, &length))
        {
            *reasonout = "a setup file-table entry is invalid.";
            return 0;
        }

        if (!SetupProjectPath(path, sizeof(path), projectdir, name))
        {
            *reasonout = "a setup output path is too long.";
            return 0;
        }

        file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE)
        {
            *reasonout = "a setup file could not be created.";
            return 0;
        }

        ok = WriteFile(file, rom->data + offset, length, &output, NULL)
          && output == length;
        CloseHandle(file);

        if (!ok)
        {
            *reasonout = "a setup file could not be fully written.";
            return 0;
        }

        count++;
    }

    if (count == 0)
    {
        *reasonout = "the ROM file table contains no setup files.";
    }

    return count;
}

BOOL SetupLoadProjectFile(const char *projectdir, const char *setupname,
                          SetupFile *out, const char **reasonout)
{
    char path[MAX_PATH];
    HANDLE file;
    DWORD got;

    ZeroMemory(out, sizeof(*out));
    *reasonout = "";

    if (!SetupProjectPath(path, sizeof(path), projectdir, setupname))
    {
        *reasonout = "the setup resource name or path is invalid.";
        return FALSE;
    }

    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        *reasonout = "the setup .set file is missing from this project.";
        return FALSE;
    }

    out->size = GetFileSize(file, NULL);
    if (out->size == INVALID_FILE_SIZE || out->size == 0
        || out->size > SETUP_FILE_MAX)
    {
        CloseHandle(file);
        ZeroMemory(out, sizeof(*out));
        *reasonout = "the project setup file is empty, too large, or unreadable.";
        return FALSE;
    }

    out->data = (unsigned char *)malloc(out->size);
    if (out->data == NULL)
    {
        CloseHandle(file);
        ZeroMemory(out, sizeof(*out));
        *reasonout = "out of memory reading the project setup.";
        return FALSE;
    }

    if (!ReadFile(file, out->data, out->size, &got, NULL)
        || got != out->size)
    {
        CloseHandle(file);
        SetupFileFree(out);
        *reasonout = "the project setup file could not be fully read.";
        return FALSE;
    }

    CloseHandle(file);

    if (!SetupParsePads(out, reasonout))
    {
        SetupFileFree(out);
        return FALSE;
    }

    strncpy(out->name, setupname, sizeof(out->name) - 1);
    return TRUE;
}

void SetupFileFree(SetupFile *setup)
{
    free(setup->boundpads);
    free(setup->pads);
    free(setup->data);
    ZeroMemory(setup, sizeof(*setup));
}
