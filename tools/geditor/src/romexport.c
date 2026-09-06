/*
 * GEditor ROM export.
 *
 * A project stores editable source copies of level resources, but a
 * playable ROM also needs the game's executable and all untouched
 * assets. New projects therefore retain the validated import ROM as
 * base.z64. Export works on an in-memory copy of that base and never
 * alters it.
 *
 * Resources stay in their original ROM slots. This is lossless for the
 * current editor operations and avoids relocating the later images and
 * csegment. A future format-changing editor can add a full obseg repack;
 * until then an oversized file is rejected rather than corrupting the
 * resource which follows it.
 */

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "romexport.h"

#define ROM_EXPORT_FTBL_MAX_ROWS 1024u
#define ROM_EXPORT_CHECKSUM_END  0x101000u

#define ROM_KIND_STGT 0x53544754u /* 'STGT' */
#define ROM_KIND_OBSG 0x4f425347u /* 'OBSG' */

typedef struct RomExportSlot {
    DWORD offset;
    DWORD length;
} RomExportSlot;

static char g_RomExportError[256];


static void RomExportSetError(const char **reasonout, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vsnprintf(g_RomExportError, sizeof(g_RomExportError), format, args);
    va_end(args);
    g_RomExportError[sizeof(g_RomExportError) - 1] = '\0';
    *reasonout = g_RomExportError;
}


static DWORD RomExportRead32(const unsigned char *src)
{
    return ((DWORD)src[0] << 24) | ((DWORD)src[1] << 16)
         | ((DWORD)src[2] << 8) | (DWORD)src[3];
}


static void RomExportWrite32(unsigned char *dst, DWORD value)
{
    dst[0] = (unsigned char)(value >> 24);
    dst[1] = (unsigned char)(value >> 16);
    dst[2] = (unsigned char)(value >> 8);
    dst[3] = (unsigned char)value;
}


static void RomExportWrite16(unsigned char *dst, unsigned short value)
{
    dst[0] = (unsigned char)(value >> 8);
    dst[1] = (unsigned char)value;
}


static int RomExportAsciiLower(int ch)
{
    if (ch >= 'A' && ch <= 'Z')
    {
        return ch + ('a' - 'A');
    }

    return ch;
}


static BOOL RomExportAsciiEquals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0')
    {
        int l = *left == '/' ? '\\' : RomExportAsciiLower((unsigned char)*left);
        int r = *right == '/' ? '\\' : RomExportAsciiLower((unsigned char)*right);

        if (l != r)
        {
            return FALSE;
        }

        left++;
        right++;
    }

    return *left == *right;
}


static BOOL RomExportEndsWith(const char *text, const char *suffix)
{
    size_t textlength = strlen(text);
    size_t suffixlength = strlen(suffix);

    return textlength >= suffixlength
        && strcmp(text + textlength - suffixlength, suffix) == 0;
}


static BOOL RomExportEndsWithNoCase(const char *text, const char *suffix)
{
    size_t textlength = strlen(text);
    size_t suffixlength = strlen(suffix);

    if (textlength < suffixlength)
    {
        return FALSE;
    }

    return RomExportAsciiEquals(text + textlength - suffixlength, suffix);
}


static BOOL RomExportSimpleResourceName(const char *name)
{
    const unsigned char *p = (const unsigned char *)name;

    if (*p == '\0')
    {
        return FALSE;
    }

    for (; *p != '\0'; p++)
    {
        if (!((*p >= 'A' && *p <= 'Z')
              || (*p >= 'a' && *p <= 'z')
              || (*p >= '0' && *p <= '9')
              || *p == '_'))
        {
            return FALSE;
        }
    }

    return TRUE;
}


static BOOL RomExportBasePath(const GEditorProject *project,
                              char *pathout, size_t pathmax,
                              const char **reasonout)
{
    int written;

    if (project == NULL || project->dir[0] == '\0')
    {
        *reasonout = "there is no valid project open.";
        return FALSE;
    }

    written = snprintf(pathout, pathmax, "%s\\%s", project->dir,
                       ROM_EXPORT_BASE_FILENAME);
    if (written < 0 || written >= (int)pathmax)
    {
        *reasonout = "the project base-ROM path is too long.";
        return FALSE;
    }

    return TRUE;
}


BOOL RomExportHasProjectBase(const GEditorProject *project)
{
    char path[MAX_PATH];
    const char *why = "";
    DWORD attrs;

    if (!RomExportBasePath(project, path, sizeof(path), &why))
    {
        return FALSE;
    }

    attrs = GetFileAttributes(path);
    return attrs != INVALID_FILE_ATTRIBUTES
        && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}


static BOOL RomExportProjectMatchesRom(const GEditorProject *project,
                                       const RomFile *rom,
                                       const char **reasonout)
{
    DWORD i;

    if (project == NULL || project->levelcount == 0
        || project->levelcount != rom->info.levelcount)
    {
        *reasonout = "the base ROM's level table does not match this project.";
        return FALSE;
    }

    for (i = 0; i < project->levelcount; i++)
    {
        const RomLevel *projectlevel = &project->levels[i];
        const RomLevel *romlevel = NULL;
        DWORD matchcount = 0;
        DWORD j;

        for (j = 0; j < rom->info.levelcount; j++)
        {
            if (rom->info.levels[j].levelID == projectlevel->levelID)
            {
                romlevel = &rom->info.levels[j];
                matchcount++;
            }
        }

        if (matchcount != 1
            || strcmp(projectlevel->setupname, romlevel->setupname) != 0
            || strcmp(projectlevel->bgname, romlevel->bgname) != 0
            || strcmp(projectlevel->stanname, romlevel->stanname) != 0)
        {
            *reasonout = "the base ROM's level resources do not match this project.";
            return FALSE;
        }
    }

    return TRUE;
}


static BOOL RomExportWriteFile(const char *path, const unsigned char *data,
                               DWORD size, const char **reasonout)
{
    HANDLE file;
    DWORD written;
    BOOL ok;

    file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        *reasonout = "the ROM output file could not be opened for writing.";
        return FALSE;
    }

    ok = WriteFile(file, data, size, &written, NULL) && written == size;
    if (!CloseHandle(file))
    {
        ok = FALSE;
    }

    if (!ok)
    {
        *reasonout = "the ROM output file could not be fully written.";
    }

    return ok;
}


BOOL RomExportStoreProjectBase(const GEditorProject *project,
                               const RomFile *rom,
                               const char **reasonout)
{
    char path[MAX_PATH];

    *reasonout = "";

    if (rom == NULL || rom->data == NULL || rom->size == 0
        || !RomExportProjectMatchesRom(project, rom, reasonout)
        || !RomExportBasePath(project, path, sizeof(path), reasonout))
    {
        return FALSE;
    }

    if (!RomExportWriteFile(path, rom->data, rom->size, reasonout))
    {
        *reasonout = "the project's base.z64 could not be fully written.";
        return FALSE;
    }

    return TRUE;
}


static BOOL RomExportReservedDeviceName(const char *name)
{
    char stem[8];
    size_t length = 0;
    const char *reserved[] = { "con", "prn", "aux", "nul" };
    size_t i;

    while (name[length] != '\0' && name[length] != '.'
           && length + 1 < sizeof(stem))
    {
        stem[length] = (char)RomExportAsciiLower((unsigned char)name[length]);
        length++;
    }
    stem[length] = '\0';

    for (i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
    {
        if (strcmp(stem, reserved[i]) == 0)
        {
            return TRUE;
        }
    }

    return length == 4
        && ((strncmp(stem, "com", 3) == 0)
            || (strncmp(stem, "lpt", 3) == 0))
        && stem[3] >= '1' && stem[3] <= '9';
}


BOOL RomExportNameIsValid(const char *name, const char **reasonout)
{
    static const char reserved[] = "\\/:*?\"<>|";
    size_t length;
    const unsigned char *p;

    *reasonout = "";

    if (name == NULL || name[0] == '\0')
    {
        *reasonout = "Enter a ROM name.";
        return FALSE;
    }

    length = strlen(name);
    if (length >= ROM_EXPORT_NAME_MAX)
    {
        *reasonout = "The ROM name is too long.";
        return FALSE;
    }

    if (name[0] == ' ' || name[length - 1] == ' '
        || name[length - 1] == '.')
    {
        *reasonout = "A ROM name cannot begin or end with a space or period.";
        return FALSE;
    }

    for (p = (const unsigned char *)name; *p != '\0'; p++)
    {
        if (*p < 32 || strchr(reserved, *p) != NULL)
        {
            *reasonout = "A ROM name cannot contain \\ / : * ? \" < > or |";
            return FALSE;
        }
    }

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0
        || RomExportReservedDeviceName(name))
    {
        *reasonout = "That name is reserved by Windows.";
        return FALSE;
    }

    if (RomExportAsciiEquals(name, ".z64"))
    {
        *reasonout = "Enter a name before the .z64 extension.";
        return FALSE;
    }

    return TRUE;
}


BOOL RomExportBuildOutputPath(const char *directory, const char *name,
                              char *pathout, size_t pathmax,
                              const char **reasonout)
{
    const char *extension;
    int written;

    *reasonout = "";

    if (directory == NULL || directory[0] == '\0')
    {
        *reasonout = "Choose an output directory.";
        return FALSE;
    }

    if (!RomExportNameIsValid(name, reasonout))
    {
        return FALSE;
    }

    extension = RomExportEndsWithNoCase(name, ".z64") ? "" : ".z64";
    written = snprintf(pathout, pathmax, "%s\\%s%s", directory, name,
                       extension);
    if (written < 0 || written >= (int)pathmax)
    {
        *reasonout = "The ROM output path is too long.";
        return FALSE;
    }

    return TRUE;
}


BOOL RomExportDestinationIsValid(const GEditorProject *project,
                                 const char *directory, const char *name,
                                 char *pathout, size_t pathmax,
                                 const char **reasonout)
{
    char basepath[MAX_PATH];
    char fulloutput[MAX_PATH];
    char fullbase[MAX_PATH];
    DWORD attrs;
    DWORD outputlength;
    DWORD baselength;

    *reasonout = "";

    if (!RomExportNameIsValid(name, reasonout))
    {
        return FALSE;
    }

    if (directory == NULL || directory[0] == '\0')
    {
        *reasonout = "Choose an output directory.";
        return FALSE;
    }

    attrs = GetFileAttributes(directory);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        *reasonout = "The output directory does not exist.";
        return FALSE;
    }

    if (!RomExportBuildOutputPath(directory, name, pathout, pathmax, reasonout)
        || !RomExportBasePath(project, basepath, sizeof(basepath), reasonout))
    {
        return FALSE;
    }

    /* Resolve relative components before comparing. Otherwise a typed
     * destination ending in "subfolder\\.." could spell base.z64 by a
     * different path and overwrite the project's only source ROM. */
    outputlength = GetFullPathName(pathout, sizeof(fulloutput), fulloutput,
                                   NULL);
    baselength = GetFullPathName(basepath, sizeof(fullbase), fullbase, NULL);
    if (outputlength == 0 || outputlength >= sizeof(fulloutput)
        || baselength == 0 || baselength >= sizeof(fullbase))
    {
        *reasonout = "The ROM output path could not be resolved.";
        return FALSE;
    }

    if (RomExportAsciiEquals(fulloutput, fullbase))
    {
        *reasonout = "Choose a different name; base.z64 is the project's source ROM.";
        return FALSE;
    }

    return TRUE;
}


/* Returns 1 for a managed project resource, 0 for another file-table
 * kind, and -1 if a recognized name cannot form a safe project path. */
static int RomExportProjectResourcePath(const GEditorProject *project,
                                        const char *resource,
                                        char *pathout, size_t pathmax)
{
    const char *folder;
    const char *filename;
    const char *extension;
    char simple[64];
    size_t length;
    int written;

    if (strncmp(resource, "bg/", 3) == 0
        && RomExportEndsWith(resource, ".seg"))
    {
        filename = resource + 3;
        length = strlen(filename);
        if (length <= 4 || length - 4 >= sizeof(simple))
        {
            return -1;
        }

        memcpy(simple, filename, length - 4);
        simple[length - 4] = '\0';
        if (!RomExportSimpleResourceName(simple))
        {
            return -1;
        }

        folder = "bg";
        extension = "";
    }
    else if ((strncmp(resource, "Usetup", 6) == 0
              || strncmp(resource, "Ump_setup", 9) == 0)
             && RomExportSimpleResourceName(resource))
    {
        folder = "setup";
        filename = resource;
        extension = ".set";
    }
    else if (strncmp(resource, "Tbg_", 4) == 0
             && RomExportEndsWith(resource, "_stanZ")
             && RomExportSimpleResourceName(resource))
    {
        folder = "stan";
        filename = resource;
        extension = ".stan";
    }
    else
    {
        return 0;
    }

    written = snprintf(pathout, pathmax, "%s\\%s\\%s%s", project->dir,
                       folder, filename, extension);
    return written >= 0 && written < (int)pathmax ? 1 : -1;
}


static unsigned char *RomExportReadResource(const char *path,
                                            const char *resource,
                                            DWORD maxsize,
                                            DWORD *sizeout,
                                            const char **reasonout)
{
    HANDLE file;
    unsigned char *data;
    DWORD size;
    DWORD got;

    *sizeout = 0;
    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        RomExportSetError(reasonout,
                          "%s could not be opened for reading.", resource);
        return NULL;
    }

    size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0)
    {
        CloseHandle(file);
        RomExportSetError(reasonout,
                          "%s is empty or unreadable.", resource);
        return NULL;
    }

    if (size > maxsize)
    {
        CloseHandle(file);
        RomExportSetError(reasonout,
                          "%s is larger than its %lu-byte ROM slot.",
                          resource, (unsigned long)maxsize);
        return NULL;
    }

    data = (unsigned char *)malloc(size);
    if (data == NULL)
    {
        CloseHandle(file);
        RomExportSetError(reasonout,
                          "out of memory reading %s.", resource);
        return NULL;
    }

    if (!ReadFile(file, data, size, &got, NULL) || got != size)
    {
        free(data);
        CloseHandle(file);
        RomExportSetError(reasonout,
                          "%s could not be fully read.", resource);
        return NULL;
    }

    CloseHandle(file);
    *sizeout = size;
    return data;
}


static BOOL RomExportReplaceProjectResources(const GEditorProject *project,
                                             RomFile *rom,
                                             const char **reasonout)
{
    RomExportSlot slots[ROM_EXPORT_FTBL_MAX_ROWS];
    const RomManifestEntry *obsg = NULL;
    unsigned char *original;
    DWORD slotcount = 0;
    DWORD index;

    for (index = 0; index < rom->info.entrycount; index++)
    {
        if (rom->info.entries[index].kind == ROM_KIND_OBSG)
        {
            obsg = &rom->info.entries[index];
            break;
        }
    }

    if (obsg == NULL || obsg->romstart >= obsg->romend
        || obsg->romend > rom->size)
    {
        *reasonout = "the base ROM's resource segment is invalid.";
        return FALSE;
    }

    /* Alias decisions must compare every project file with the same
     * pristine base bytes even after another name has replaced a slot. */
    original = (unsigned char *)malloc(obsg->romend - obsg->romstart);
    if (original == NULL)
    {
        *reasonout = "out of memory preparing the ROM resource segment.";
        return FALSE;
    }
    memcpy(original, rom->data + obsg->romstart,
           obsg->romend - obsg->romstart);

    for (index = 0; index < ROM_EXPORT_FTBL_MAX_ROWS; index++)
    {
        char resource[64];
        char path[MAX_PATH];
        DWORD offset;
        DWORD maxlen;
        DWORD attrs;
        DWORD length;
        unsigned char *data;
        DWORD slot;
        int managed;

        if (!RomGetFileByIndex(rom, index, resource, sizeof(resource),
                               NULL, NULL))
        {
            break;
        }

        managed = RomExportProjectResourcePath(project, resource, path,
                                               sizeof(path));
        if (managed == 0)
        {
            continue;
        }
        if (managed < 0)
        {
            RomExportSetError(reasonout,
                              "project path is invalid for resource %s.",
                              resource);
            goto fail;
        }

        attrs = GetFileAttributes(path);
        if (attrs == INVALID_FILE_ATTRIBUTES)
        {
            /* Some unfinished level-table rows have no editable setup.
             * A missing project copy simply leaves the base resource as-is. */
            continue;
        }
        if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        {
            RomExportSetError(reasonout, "%s is not a file.", path);
            goto fail;
        }

        if (!RomGetFileByIndex(rom, index, resource, sizeof(resource),
                               &offset, &maxlen))
        {
            RomExportSetError(reasonout,
                              "the ROM slot for %s is invalid.", resource);
            goto fail;
        }

        data = RomExportReadResource(path, resource, maxlen, &length,
                                     reasonout);
        if (data == NULL)
        {
            goto fail;
        }

        /* Most aliases are untouched duplicate project copies. Ignore
         * those so one genuinely edited alias can supply the shared
         * slot without conflicting with its original siblings. */
        if (memcmp(original + offset - obsg->romstart, data, length) == 0)
        {
            free(data);
            continue;
        }

        for (slot = 0; slot < slotcount; slot++)
        {
            if (slots[slot].offset == offset)
            {
                break;
            }
        }

        if (slot < slotcount)
        {
            /* Rare aliases several placeholder names to one resource.
             * Identical project copies are harmless; divergent edits
             * would otherwise make table order decide which one wins. */
            if (slots[slot].length != length
                || memcmp(rom->data + offset, data, length) != 0)
            {
                free(data);
                RomExportSetError(reasonout,
                    "%s shares a ROM slot with a differently edited resource.",
                    resource);
                goto fail;
            }
        }
        else
        {
            /* Keep any bytes after the editable file intact. BG slots
             * can contain linker padding or data not named by FTBL. */
            memcpy(rom->data + offset, data, length);
            slots[slotcount].offset = offset;
            slots[slotcount].length = length;
            slotcount++;
        }

        free(data);
    }

    free(original);
    return TRUE;

fail:
    free(original);
    return FALSE;
}


static BOOL RomExportUpdateLevelTable(const GEditorProject *project,
                                      RomFile *rom,
                                      const char **reasonout)
{
    const RomManifestEntry *stgt = NULL;
    DWORD i;

    for (i = 0; i < rom->info.entrycount; i++)
    {
        if (rom->info.entries[i].kind == ROM_KIND_STGT)
        {
            stgt = &rom->info.entries[i];
            break;
        }
    }

    if (stgt == NULL || stgt->flags == 0
        || stgt->romstart + stgt->flags * 32 > rom->size)
    {
        *reasonout = "the base ROM's level table is invalid.";
        return FALSE;
    }

    for (i = 0; i < project->levelcount; i++)
    {
        const RomLevel *level = &project->levels[i];
        unsigned char *row = NULL;
        DWORD j;

        if (!(level->levelscale > 0.0f) || !(level->renderScale > 0.0f))
        {
            RomExportSetError(reasonout,
                              "level %s has an invalid scale.", level->name);
            return FALSE;
        }

        for (j = 0; j < stgt->flags; j++)
        {
            unsigned char *candidate = rom->data + stgt->romstart + j * 32;

            if ((LONG)RomExportRead32(candidate) == level->levelID)
            {
                if (row != NULL)
                {
                    *reasonout = "the base ROM contains a duplicate level ID.";
                    return FALSE;
                }
                row = candidate;
            }
        }

        if (row == NULL)
        {
            RomExportSetError(reasonout,
                              "level %s is absent from the base ROM.",
                              level->name);
            return FALSE;
        }

        {
            union { DWORD u; float f; } bits;

            bits.f = level->levelscale;
            RomExportWrite32(row + 16, bits.u);
            bits.f = level->renderScale;
            RomExportWrite32(row + 20, bits.u);
        }

        RomExportWrite16(row + 24, (unsigned short)level->music);
        RomExportWrite16(row + 26, (unsigned short)level->bgsound);
        RomExportWrite16(row + 28, (unsigned short)level->xtrack);
    }

    return TRUE;
}


static DWORD RomExportRotateLeft(DWORD value, unsigned int shift)
{
    shift &= 31;
    return shift == 0 ? value
                      : (value << shift) | (value >> (32 - shift));
}


/* GoldenEye uses CIC-NUS-6102. This is the same checksum performed by
 * the repository's n64cksum tool, written without its CLI dependencies. */
static BOOL RomExportUpdateChecksum(RomFile *rom, const char **reasonout)
{
    DWORD seed = 0xf8ca4ddcu;
    DWORD t2 = seed;
    DWORD t3 = seed;
    DWORD t4 = seed;
    DWORD rotatedsum = seed;
    DWORD a2 = seed;
    DWORD a3 = seed;
    DWORD offset;

    if (rom->size < ROM_EXPORT_CHECKSUM_END)
    {
        *reasonout = "the base ROM is too small for an N64 checksum.";
        return FALSE;
    }

    for (offset = 0x1000; offset < ROM_EXPORT_CHECKSUM_END; offset += 4)
    {
        DWORD value = RomExportRead32(rom->data + offset);
        DWORD sum = a3 + value;
        DWORD rotated = RomExportRotateLeft(value, value & 31);

        if (sum < a3)
        {
            t2++;
        }

        a3 = sum;
        t3 ^= value;
        rotatedsum += rotated;

        if (a2 < value)
        {
            a2 ^= a3 ^ value;
        }
        else
        {
            a2 ^= rotated;
        }

        t4 += value ^ rotatedsum;
    }

    RomExportWrite32(rom->data + 0x10, (a3 ^ t2) ^ t3);
    RomExportWrite32(rom->data + 0x14, (rotatedsum ^ a2) ^ t4);
    return TRUE;
}


BOOL RomExportCreate(const GEditorProject *project,
                     const char *name, const char *directory,
                     char *pathout, size_t pathmax,
                     const char **reasonout)
{
    char basepath[MAX_PATH];
    RomFile rom;
    BOOL ok;

    *reasonout = "";

    if (!RomExportDestinationIsValid(project, directory, name,
                                     pathout, pathmax, reasonout)
        || !RomExportBasePath(project, basepath, sizeof(basepath), reasonout))
    {
        return FALSE;
    }

    if (!RomLoad(basepath, &rom, reasonout))
    {
        return FALSE;
    }

    ok = RomExportProjectMatchesRom(project, &rom, reasonout)
      && RomExportReplaceProjectResources(project, &rom, reasonout)
      && RomExportUpdateLevelTable(project, &rom, reasonout)
      && RomExportUpdateChecksum(&rom, reasonout)
      && RomExportWriteFile(pathout, rom.data, rom.size, reasonout);

    RomFree(&rom);
    return ok;
}
