/*
 * GUD ROM validation.
 *
 * The checks run cheapest-first, and every failure names its reason:
 * a refused ROM should teach the user what was wrong, not make them
 * guess. The manifest is the decisive test - it is what makes a ROM a
 * GUD ROM rather than any GoldenEye ROM.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rom.h"

#define ROM_MIN_SIZE (1024u * 1024u)        /* smaller than 1 MB is not a ROM */
#define ROM_MAX_SIZE (64u * 1024u * 1024u)  /* larger than the biggest cart  */

#define ROM_MANIFEST_MAGIC     "GUDGEDITORMANIF"  /* + implicit NUL = 16 bytes */
#define ROM_MANIFEST_MAGIC_LEN 16
#define ROM_MANIFEST_VERSION   1

/* The ROM is big-endian; the PC is not. All multi-byte reads go
   through this. */
static DWORD be32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16)
         | ((DWORD)p[2] << 8)  |  (DWORD)p[3];
}

/* Reads the whole file. Returns NULL on any failure. */
static unsigned char *RomReadAll(const char *path, DWORD *sizeout)
{
    HANDLE f;
    DWORD size;
    DWORD got = 0;
    unsigned char *data;

    f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                   OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }

    size = GetFileSize(f, NULL);
    if (size == INVALID_FILE_SIZE || size == 0)
    {
        CloseHandle(f);
        return NULL;
    }

    data = (unsigned char *)malloc(size);
    if (data == NULL)
    {
        CloseHandle(f);
        return NULL;
    }

    if (!ReadFile(f, data, size, &got, NULL) || got != size)
    {
        free(data);
        CloseHandle(f);
        return NULL;
    }

    CloseHandle(f);
    *sizeout = size;
    return data;
}

/* Offset of the manifest magic, or 0 if absent. Struct data is at
   least 4-aligned, so the scan strides 4. */
static DWORD RomFindManifest(const unsigned char *data, DWORD size)
{
    DWORD i;

    if (size < ROM_MANIFEST_MAGIC_LEN)
    {
        return 0;
    }

    for (i = 0; i <= size - ROM_MANIFEST_MAGIC_LEN; i += 4)
    {
        if (data[i] == 'G'
            && memcmp(data + i, ROM_MANIFEST_MAGIC, ROM_MANIFEST_MAGIC_LEN) == 0)
        {
            return i;
        }
    }

    return 0;
}

BOOL RomValidate(const char *path, RomInfo *info, const char **reasonout)
{
    static const unsigned char z64magic[4] = { 0x80, 0x37, 0x12, 0x40 };
    unsigned char *data;
    DWORD size = 0;
    DWORD m;
    DWORD i;

    ZeroMemory(info, sizeof(*info));
    *reasonout = "";

    data = RomReadAll(path, &size);
    if (data == NULL)
    {
        *reasonout = "The ROM file could not be read.";
        return FALSE;
    }

    /* Everything below this point must free data before returning. */

    if (size < ROM_MIN_SIZE || size > ROM_MAX_SIZE)
    {
        free(data);
        *reasonout = "That file is not a plausible ROM size.";
        return FALSE;
    }

    if (memcmp(data, z64magic, 4) != 0)
    {
        free(data);
        *reasonout = "Not a big-endian .z64 ROM (byte-swapped dump?).";
        return FALSE;
    }

    /* Header title: GoldenEye-engine ROMs say so at 0x20. */
    memcpy(info->internalname, data + 0x20, 20);
    info->internalname[20] = '\0';
    if (strstr(info->internalname, "GOLDENEYE") == NULL)
    {
        free(data);
        *reasonout = "Not a GoldenEye-engine ROM (header title mismatch).";
        return FALSE;
    }

    m = RomFindManifest(data, size);
    if (m == 0)
    {
        free(data);
        *reasonout = "GoldenEye ROM, but not a GUD build - no GEditor manifest.";
        return FALSE;
    }

    if (m + 24 > size)
    {
        free(data);
        *reasonout = "GUD manifest is truncated.";
        return FALSE;
    }

    info->size = size;
    info->manifestoffset = m;
    info->manifestversion = be32(data + m + 16);
    info->entrycount = be32(data + m + 20);

    if (info->manifestversion < 1 || info->manifestversion > ROM_MANIFEST_VERSION)
    {
        free(data);
        *reasonout = "GUD manifest is from a newer GEditor - update GEditor.";
        return FALSE;
    }

    if (info->entrycount < 1 || info->entrycount > ROM_MAX_ENTRIES
        || m + 24 + info->entrycount * 16 > size)
    {
        free(data);
        *reasonout = "GUD manifest entry table is malformed.";
        return FALSE;
    }

    for (i = 0; i < info->entrycount; i++)
    {
        const unsigned char *e = data + m + 24 + i * 16;
        RomManifestEntry *out = &info->entries[i];

        out->kind     = be32(e + 0);
        out->romstart = be32(e + 4);
        out->romend   = be32(e + 8);
        out->flags    = be32(e + 12);

        if (out->romstart >= out->romend || out->romend > size)
        {
            free(data);
            *reasonout = "GUD manifest points outside the ROM (corrupt build?).";
            return FALSE;
        }
    }

    free(data);
    return TRUE;
}