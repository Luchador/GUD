#include "setupmeta.h"
#include "occluders.h"
/*
 * GEditor ROM export.
 *
 * A project stores editable source copies of level resources, but a
 * playable ROM also needs the game's executable and all untouched
 * assets. New projects therefore retain the validated import ROM as
 * base.z64. Export works on an in-memory copy of that base and never
 * alters it.
 *
 * Resources which still fit stay in their original ROM slots. If edited
 * geometry outgrows one, the complete resource segment is repacked as a
 * unit and, when necessary, relocated to padded space at the end of the
 * ROM. File-table data offsets and the OBSG manifest range are updated;
 * executable and other linked segments never move. Imported images are
 * appended to a rebuilt IMGS bank using the manifest's texture configuration.
 */

#include <windows.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "romexport.h"
#include "editorpath.h"
#include "modeledits.h"
#include "newprops.h"
#include "imageedits.h"
#include "bgdocument.h"
#include "setupload.h"
#include "setupstan.h"
#include "actionblocks.h"

#define ROM_EXPORT_FTBL_MAX_ROWS 1024u
#define ROM_EXPORT_CHECKSUM_END  0x101000u
#define ROM_EXPORT_MAX_SIZE      (64u * 1024u * 1024u)

#define ROM_KIND_FTBL 0x4654424cu /* 'FTBL' */
#define ROM_KIND_STGT 0x53544754u /* 'STGT' */
#define ROM_KIND_OBSG 0x4f425347u /* 'OBSG' */
#define ROM_KIND_ENVT 0x454E5654u /* 'ENVT' */

typedef struct RomExportSlot {
    DWORD offset;
    DWORD length;
    DWORD newoffset;
    unsigned char *replacement;
    DWORD replacementlength;
    BOOL model;
    BOOL background;
    BOOL setup;
    BOOL stan;
    char setupname[64];
} RomExportSlot;

static char g_RomExportError[256];
static char g_RomExportCleanupWarning[384];
static LevelIssueReport g_RomExportIssues;
static BOOL g_RomExportHasIssues;

const char *RomExportCleanupWarning(void) { return g_RomExportCleanupWarning; }
const LevelIssueReport *RomExportIssues(void) { return g_RomExportHasIssues ? &g_RomExportIssues : NULL; }
void RomExportClearIssues(void)
{ LevelIssuesFree(&g_RomExportIssues); g_RomExportHasIssues = FALSE; }


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


BOOL RomExportRefreshProjectLevelMetadata(GEditorProject *project, const char **reasonout)
{
    char path[MAX_PATH];
    RomFile rom;
    DWORD i, j;
    BOOL matches;
    if (!RomExportBasePath(project, path, sizeof(path), reasonout)
        || !RomLoad(path, &rom, reasonout)) { return FALSE; }
    matches = RomExportProjectMatchesRom(project, &rom, reasonout);
    if (matches)
    {
        BOOL available = EnvironmentReadRom(&rom, &project->environments, NULL, reasonout);
        if (project->environmentOverrides.count && (!available
            || !EnvironmentValidateOverrides(&project->environments, &project->environmentOverrides, reasonout))) { matches = FALSE; }
        else { *reasonout = ""; }
    }
    if (matches)
    {
        BOOL available = LevelMemoryReadRom(&rom, &project->memory, reasonout);
        if (project->memoryOverrides.count && (!available
            || !LevelMemoryValidateOverrides(&project->memory, &project->memoryOverrides, reasonout))) { matches = FALSE; }
        else { *reasonout = ""; }
    }
    if (matches)
    {
        for (i = 0; i < project->levelcount; i++)
        {
            RomLevel *level = &project->levels[i];
            for (j = 0; j < rom.info.levelcount; j++) if (level->levelID == rom.info.levels[j].levelID)
            {
                const RomLevel *source = &rom.info.levels[j];
                lstrcpyn(level->name, source->name, sizeof(level->name));
                level->hasbackgroundcolor = source->hasbackgroundcolor;
                memcpy(level->backgroundcolor, source->backgroundcolor, sizeof(level->backgroundcolor));
                level->fog = source->fog;
                level->clouds = source->clouds;
                break;
            }
        }
        EnvironmentRefreshLevels(&project->environments, &project->environmentOverrides, project->levels, project->levelcount);
    }
    RomFree(&rom);
    return matches;
}

static BOOL RomExportWriteFile(const char *path, const unsigned char *data, DWORD size, const char **reasonout)
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
    char filename[ROM_EXPORT_NAME_MAX + 4];

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
    snprintf(filename, sizeof(filename), "%s%s", name, extension);
    if (!EditorPathJoin(pathout, pathmax, directory, filename))
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
int RomExportProjectResourcePath(const GEditorProject *project,
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
    if (!strncmp(resource, "Usetup", 6) || !strncmp(resource, "Ump_setup", 9))
    {
        DWORD native, meta;
        if (!SetupMetaSplit(data, size, &native, &meta))
        { free(data); RomExportSetError(reasonout, "%s has invalid editor metadata.", resource); return NULL; }
        size = native;
        if (meta)
        {
            SetupFile setup = {0}; unsigned char *runtime = NULL; DWORD runtimesize;
            setup.data = data; setup.size = native;
            setup.actionmeta = data + native; setup.actionmetasize = meta;
            if (!ActionSetupBuildRuntime(&setup, &runtime, &runtimesize, reasonout))
            { free(data); return NULL; }
            if (runtime) { free(data); data = runtime; size = runtimesize; }
        }
    }
    *sizeout = size;
    return data;
}


static void RomExportFreeSlots(RomExportSlot *slots, DWORD slotcount)
{
    DWORD slot;

    for (slot = 0; slot < slotcount; slot++)
    {
        free(slots[slot].replacement);
    }
}


static int RomExportCompareSlots(const void *left, const void *right)
{
    const RomExportSlot *a = (const RomExportSlot *)left;
    const RomExportSlot *b = (const RomExportSlot *)right;

    return a->offset < b->offset ? -1 : a->offset > b->offset ? 1 : 0;
}


static RomExportSlot *RomExportFindSlot(RomExportSlot *slots,
                                        DWORD slotcount, DWORD offset)
{
    DWORD slot;

    for (slot = 0; slot < slotcount; slot++)
    {
        if (slots[slot].offset == offset)
        {
            return &slots[slot];
        }
    }
    return NULL;
}


static BOOL RomExportAddSize(DWORD *value, DWORD add)
{
    if (add > (DWORD)-1 - *value)
    {
        return FALSE;
    }
    *value += add;
    return TRUE;
}


static BOOL RomExportAlignSize(DWORD *value, DWORD alignment)
{
    DWORD padding = (alignment - (*value % alignment)) % alignment;

    return RomExportAddSize(value, padding);
}


static BOOL RomExportUpdateFileOffsets(RomFile *rom,
                                       const RomManifestEntry *ftbl,
                                       RomExportSlot *slots,
                                       DWORD slotcount,
                                       const char **reasonout)
{
    DWORD rowindex;

    for (rowindex = 0; rowindex < ROM_EXPORT_FTBL_MAX_ROWS; rowindex++)
    {
        DWORD row = ftbl->romstart + rowindex * 12;
        DWORD offset;
        RomExportSlot *slot;

        if (row > rom->size || rom->size - row < 12)
        {
            *reasonout = "the base ROM's file table is incomplete.";
            return FALSE;
        }
        if (RomExportRead32(rom->data + row + 4) == 0)
        {
            return TRUE;
        }

        offset = RomExportRead32(rom->data + row + 8);
        /* Row zero is the intentional { NULLFILE, "", 0 } sentinel-like
           entry. It has a name pointer so it is not the table terminator,
           but it does not describe data inside obseg and must not move. */
        if (offset == 0)
        {
            continue;
        }
        slot = RomExportFindSlot(slots, slotcount, offset);
        if (slot == NULL)
        {
            *reasonout = "the base ROM's file table changed during export.";
            return FALSE;
        }
        RomExportWrite32(rom->data + row + 8, slot->newoffset);
    }

    *reasonout = "the base ROM's file table has no terminator.";
    return FALSE;
}


static DWORD RomExportNextRomSize(DWORD required)
{
    DWORD size = 1024u * 1024u;

    while (size < required && size < ROM_EXPORT_MAX_SIZE)
    {
        size *= 2;
    }
    return size >= required ? size : 0;
}


static BOOL RomExportRepackResources(RomFile *rom,
                                     RomManifestEntry *obsg,
                                     const RomManifestEntry *ftbl,
                                     RomExportSlot *slots, DWORD slotcount,
                                     const char **reasonout)
{
    unsigned char *packed;
    DWORD packedsize;
    DWORD prefixsize;
    DWORD cursor;
    DWORD slotindex;
    DWORD target;
    DWORD oldstart = obsg->romstart;
    DWORD oldend = obsg->romend;
    DWORD inplacecapacity = oldend - oldstart;
    DWORD ftblend = 0;
    DWORD manifestend;

    for (slotindex = 0; slotindex < ROM_EXPORT_FTBL_MAX_ROWS; slotindex++)
    {
        DWORD row = ftbl->romstart + slotindex * 12;

        if (row > rom->size || rom->size - row < 12)
        {
            *reasonout = "the base ROM's file table is incomplete.";
            return FALSE;
        }
        if (RomExportRead32(rom->data + row + 4) == 0)
        {
            ftblend = row + 12;
            break;
        }
    }
    manifestend = rom->info.manifestoffset + 24
                + rom->info.entrycount * 16;
    if (ftblend == 0 || manifestend < rom->info.manifestoffset
        || (ftbl->romstart < oldend && ftblend > oldstart)
        || (rom->info.manifestoffset < oldend && manifestend > oldstart))
    {
        *reasonout = "the base ROM embeds export metadata inside its resource segment.";
        return FALSE;
    }

    qsort(slots, slotcount, sizeof(*slots), RomExportCompareSlots);
    if (slotcount == 0 || slots[0].offset < oldstart)
    {
        *reasonout = "the base ROM's resource table is empty or invalid.";
        return FALSE;
    }

    prefixsize = slots[0].offset - oldstart;
    packedsize = prefixsize;
    for (slotindex = 0; slotindex < slotcount; slotindex++)
    {
        DWORD payload = slots[slotindex].length;

        if (slots[slotindex].offset < oldstart
            || slots[slotindex].offset > oldend
            || slots[slotindex].length > oldend - slots[slotindex].offset)
        {
            *reasonout = "the base ROM contains an invalid resource slot.";
            return FALSE;
        }
        if (slots[slotindex].replacement != NULL
            && (slots[slotindex].setup || slots[slotindex].background || slots[slotindex].stan
                || slots[slotindex].replacementlength > payload))
        {
            payload = slots[slotindex].replacementlength;
        }
        if (!RomExportAddSize(&packedsize, payload)
            || !RomExportAlignSize(&packedsize, 16))
        {
            *reasonout = "the repacked resource segment is too large.";
            return FALSE;
        }
    }

    packed = (unsigned char *)calloc(packedsize, 1);
    if (packed == NULL)
    {
        *reasonout = "out of memory repacking the ROM resource segment.";
        return FALSE;
    }
    memcpy(packed, rom->data + oldstart, prefixsize);

    cursor = prefixsize;
    for (slotindex = 0; slotindex < slotcount; slotindex++)
    {
        RomExportSlot *slot = &slots[slotindex];
        DWORD payload = slot->length;

        slot->newoffset = cursor;
        if ((slot->setup || slot->background || slot->stan) && slot->replacement != NULL)
        {
            payload = slot->replacementlength;
            memcpy(packed + cursor, slot->replacement, payload);
        }
        else
        {
            memcpy(packed + cursor, rom->data + slot->offset, slot->length);
            if (slot->replacement != NULL)
            {
                if (slot->model) { memset(packed + cursor, 0, slot->length); }
                memcpy(packed + cursor, slot->replacement, slot->replacementlength);
                if (slot->replacementlength > payload) { payload = slot->replacementlength; }
            }
        }
        cursor += payload;
        RomExportAlignSize(&cursor, 16);
    }

    /* A previously compacted export can be imported as a new base. Reuse
     * verified zero padding after OBSG before appending another whole bank,
     * especially when the ROM already has the maximum supported capacity.
     * Even an all-zero neighboring manifest segment is occupied space. */
    if (packedsize > inplacecapacity && packedsize <= rom->size - oldstart)
    {
        DWORD end = oldstart + packedsize;
        BOOL padding = !((ftbl->romstart < end && ftblend > oldend)
            || (rom->info.manifestoffset < end && manifestend > oldend));
        for (DWORD i = 0; padding && i < rom->info.entrycount; i++)
        {
            const RomManifestEntry *entry = &rom->info.entries[i];
            if (entry != obsg && entry->romstart < entry->romend
                && entry->romstart < end && entry->romend > oldend) { padding = FALSE; }
        }
        for (DWORD i = oldend; padding && i < end; i++)
        { if (rom->data[i]) { padding = FALSE; } }
        if (padding) { inplacecapacity = packedsize; }
    }
    if (packedsize <= inplacecapacity)
    {
        target = oldstart;
        memcpy(rom->data + target, packed, packedsize);
        if (packedsize < oldend - oldstart)
        { memset(rom->data + target + packedsize, 0, oldend - oldstart - packedsize); }
    }
    else
    {
        unsigned char *grown;
        DWORD targetend;
        DWORD newsize;

        target = rom->size;
        if (!RomExportAlignSize(&target, 16)
            || packedsize > (DWORD)-1 - target)
        {
            free(packed);
            *reasonout = "the repacked resource segment cannot fit in a ROM.";
            return FALSE;
        }
        targetend = target + packedsize;
        newsize = RomExportNextRomSize(targetend);
        if (newsize == 0)
        {
            free(packed);
            *reasonout = "the edited resources exceed the 64 MB ROM limit.";
            return FALSE;
        }

        grown = (unsigned char *)realloc(rom->data, newsize);
        if (grown == NULL)
        {
            free(packed);
            *reasonout = "out of memory growing the output ROM.";
            return FALSE;
        }
        rom->data = grown;
        memset(rom->data + rom->size, 0, newsize - rom->size);
        rom->size = newsize;
        rom->info.size = newsize;
        memcpy(rom->data + target, packed, packedsize);
    }

    for (slotindex = 0; slotindex < slotcount; slotindex++)
    {
        slots[slotindex].newoffset += target;
    }
    free(packed);

    if (!RomExportUpdateFileOffsets(rom, ftbl, slots, slotcount,
                                    reasonout))
    {
        return FALSE;
    }

    if (target != oldstart || packedsize != oldend - oldstart)
    {
        DWORD manifestentry = rom->info.manifestoffset + 24
                            + (DWORD)(obsg - rom->info.entries) * 16;

        if (manifestentry > rom->size || rom->size - manifestentry < 16)
        {
            *reasonout = "the output ROM's manifest is incomplete.";
            return FALSE;
        }
        obsg->romstart = target;
        obsg->romend = target + packedsize;
        RomExportWrite32(rom->data + manifestentry + 4, obsg->romstart);
        RomExportWrite32(rom->data + manifestentry + 8, obsg->romend);
    }

    return TRUE;
}


/* Select the actual post-alias STAN bytes, including levels never opened in
 * this session and the runtime's legacy multiplayer setup-name expansion. */
static const RomLevel *RomExportSetupLevel(const GEditorProject *project,
    const RomFile *rom, RomExportSlot *slot, DWORD *stanoffset, char setupname[64], DWORD *levelindex)
{
    const RomLevel *match = NULL;
    const char *why;
    *levelindex = (DWORD)-1;
    for (DWORD i = 0; i < project->levelcount; i++)
    {
        const RomLevel *level = &project->levels[i];
        for (int multiplayer = 0; multiplayer < 2; multiplayer++)
        {
            char name[64]; DWORD offset, length, stan;
            if (multiplayer && strncmp(level->setupname, "Usetup", 6)) { continue; }
            snprintf(name, sizeof(name), multiplayer ? "Ump_%s" : "U%s", level->setupname + 1);
            if (!RomFindFile(rom, name, &offset, &length, &why) || offset != slot->offset) { continue; }
            if (!RomFindFile(rom, level->stanname, &stan, &length, &why)) { return NULL; }
            /* Shared setups with different collision worlds cannot safely
             * cache a single placement choice. Leave their names authored. */
            if (match && (*stanoffset != stan || match->levelscale != level->levelscale)) { return NULL; }
            if (!match || !multiplayer)
            {
                match = level; strcpy(setupname, name);
                *levelindex = multiplayer ? (DWORD)-1 : i;
            }
            *stanoffset = stan;
        }
    }
    return match;
}

typedef struct RomExportPadContext {
    LevelIssueReport *report;
    const RomLevel *level;
    DWORD levelindex;
    const char *setupname;
} RomExportPadContext;

static BOOL RomExportPadIssue(void *context, SetupPadRef pad, const float position[3], const char *stanname)
{
    RomExportPadContext *c = context; LevelIssue issue = {0};
    if (!c->report) { return TRUE; }
    issue.kind = LEVEL_ISSUE_UNRESOLVED_PAD; issue.pad = pad;
    issue.exportlevel = c->levelindex; issue.exportscale = c->level->levelscale;
    memcpy(issue.exportposition, position, sizeof(issue.exportposition));
    snprintf(issue.exportstan, sizeof(issue.exportstan), "%s", stanname);
    snprintf(issue.exportsetup, sizeof(issue.exportsetup), "%s", c->setupname);
    snprintf(issue.scope, sizeof(issue.scope), "%s / %s", c->level->name, c->setupname);
    snprintf(issue.subject, sizeof(issue.subject), "%s pad %lu", pad.bound ? "Bound" : "Ordinary", (unsigned long)pad.index);
    if (c->levelindex != (DWORD)-1 && isfinite(position[0]) && isfinite(position[1]) && isfinite(position[2]))
        issue.target = LEVEL_ISSUE_PAD;
    snprintf(issue.description, sizeof(issue.description),
        "Unresolved in the last ROM export (STAN reference: %s). Position: %.7g, %.7g, %.7g. Names and positions were retained.%s",
        stanname[0] ? stanname : "none", position[0], position[1], position[2],
        c->levelindex == (DWORD)-1 ? " This setup has no directly openable level entry." : "");
    return LevelIssuesAdd(c->report, &issue);
}

static BOOL RomExportReplaceProjectResources(const GEditorProject *project,
                                             RomFile *rom,
                                             LevelIssueReport *report,
                                             const RomFile *featureRom,
                                             const char **reasonout)
{
    RomExportSlot slots[ROM_EXPORT_FTBL_MAX_ROWS];
    RomManifestEntry *obsg = NULL;
    const RomManifestEntry *ftbl = NULL;
    DWORD slotcount = 0;
    DWORD index;
    BOOL needrepack = FALSE;
    DWORD unresolved = 0, skipped = 0;

    ZeroMemory(slots, sizeof(slots));
    g_RomExportCleanupWarning[0] = '\0';

    for (index = 0; index < rom->info.entrycount; index++)
    {
        if (rom->info.entries[index].kind == ROM_KIND_FTBL)
        {
            ftbl = &rom->info.entries[index];
        }
        if (rom->info.entries[index].kind == ROM_KIND_OBSG)
        {
            obsg = &rom->info.entries[index];
        }
    }

    if (ftbl == NULL || obsg == NULL || obsg->romstart >= obsg->romend
        || obsg->romend > rom->size)
    {
        *reasonout = "the base ROM's resource segment is invalid.";
        return FALSE;
    }

    for (index = 0; index < ROM_EXPORT_FTBL_MAX_ROWS; index++)
    {
        char resource[64];
        char path[MAX_PATH];
        DWORD offset;
        DWORD maxlen;
        DWORD attrs;
        DWORD length;
        unsigned char *data;
        RomExportSlot *slot;
        int managed;

        if (!RomGetFileByIndex(rom, index, resource, sizeof(resource),
                               NULL, NULL))
        {
            break;
        }

        /* file_resource_table[0] is { NULLFILE, "", 0 }. Its non-NULL
           empty-string pointer keeps it distinct from the real terminator,
           but it has no ROM slot to collect or replace. */
        if (resource[0] == '\0')
        {
            continue;
        }

        if (!RomGetFileByIndex(rom, index, resource, sizeof(resource),
                               &offset, &maxlen))
        {
            RomExportSetError(reasonout,
                              "the ROM slot for %s is invalid.", resource);
            goto fail;
        }

        slot = RomExportFindSlot(slots, slotcount, offset);
        if (slot == NULL)
        {
            if (slotcount == ROM_EXPORT_FTBL_MAX_ROWS)
            {
                *reasonout = "the base ROM has too many resource slots.";
                goto fail;
            }
            slot = &slots[slotcount++];
            slot->offset = offset;
            slot->length = maxlen;
        }

        slot->background |= strncmp(resource, "bg/", 3) == 0;
        slot->setup |= strncmp(resource, "Usetup", 6) == 0 || strncmp(resource, "Ump_setup", 9) == 0;
        if (slot->setup && !slot->setupname[0]) { strcpy(slot->setupname, resource); }
        slot->stan |= strncmp(resource, "Tbg_", 4) == 0 && strstr(resource, "_stanZ") != NULL;
        managed = ModelEditsReadReplacement(project->dir, resource, rom->data + offset,
            maxlen, &data, &length, reasonout);
        if (managed < 0) { goto fail; }
        if (managed > 0) { slot->model = TRUE; goto have_replacement; }

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

        data = RomExportReadResource(path, resource, &length, reasonout);
        if (data == NULL)
        {
            goto fail;
        }

        if (strncmp(resource, "bg/", 3) == 0)
        {
            BgFile bg;
            const char *why = "";

            ZeroMemory(&bg, sizeof(bg));
            bg.data = data;
            bg.size = length;
            lstrcpyn(bg.name, resource, sizeof(bg.name));
            if (!BgFileValidateVertexBatches(&bg, &why))
            {
                free(data);
                RomExportSetError(reasonout, "%s: %s", resource, why);
                goto fail;
            }
        }

have_replacement:
        /* Most aliases are untouched duplicate project copies. Ignore
         * those so one genuinely edited alias can supply the shared
         * slot without conflicting with its original siblings. */
        if (length <= maxlen
            && memcmp(rom->data + offset, data, length) == 0)
        {
            free(data);
            continue;
        }

        if (slot->replacement != NULL)
        {
            /* Rare aliases several placeholder names to one resource.
             * Identical project copies are harmless; divergent edits
             * would otherwise make table order decide which one wins. */
            if (slot->replacementlength != length
                || memcmp(slot->replacement, data, length) != 0)
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
            slot->replacement = data;
            slot->replacementlength = length;
            data = NULL;
            if (length > maxlen)
            {
                needrepack = TRUE;
            }
        }

        free(data);
    }

    /* Resolve resource aliases before cleanup so unchanged copies do not
     * conflict with an edited alias. Also clean levels not opened this session. */
    for (index = 0; index < slotcount; index++) if (slots[index].background)
    {
        RomExportSlot *slot = &slots[index];
        BgFile source = {0}, cleaned = {0};
        source.data = slot->replacement ? slot->replacement : rom->data + slot->offset;
        source.size = slot->replacement ? slot->replacementlength : slot->length;
        if (!BgFileRemoveUnusedVertices(&source, &cleaned, reasonout)) { goto fail; }
        if (cleaned.data)
        {
            free(slot->replacement);
            slot->replacement = cleaned.data;
            slot->replacementlength = cleaned.size;
        }
        source.data = slot->replacement ? slot->replacement : rom->data + slot->offset;
        source.size = slot->replacement ? slot->replacementlength : slot->length;
        if (!BgFileOptimize(&source, &cleaned, reasonout)) { goto fail; }
        if (cleaned.data)
        {
            free(slot->replacement);
            slot->replacement = cleaned.data;
            slot->replacementlength = cleaned.size;
        }
        source.data = slot->replacement ? slot->replacement : rom->data + slot->offset;
        source.size = slot->replacement ? slot->replacementlength : slot->length;
        if (!BgFileCompact(&source, &cleaned, reasonout)) { goto fail; }
        free(slot->replacement);
        slot->replacement = cleaned.data;
        slot->replacementlength = cleaned.size;
        if (((cleaned.size + 15) & ~15u) != slot->length) { needrepack = TRUE; }
    }

    for (index = 0; index < slotcount; index++) if (slots[index].stan)
    {
        RomExportSlot *slot = &slots[index];
        const unsigned char *source = slot->replacement ? slot->replacement : rom->data + slot->offset;
        DWORD size = slot->replacement ? slot->replacementlength : slot->length, length;
        unsigned char *copy;
        if (!StanMeasureNative(source, size, &length, reasonout))
        {
            /* Unimplemented stock maps can label a small opaque linker
             * placeholder as STAN. Preserve it; edited invalid data fails. */
            if (!slot->replacement) { *reasonout = ""; continue; }
            goto fail;
        }
        copy = malloc(length);
        if (!copy) { *reasonout = "Out of memory compacting stan."; goto fail; }
        memcpy(copy, source, length); free(slot->replacement);
        slot->replacement = copy; slot->replacementlength = length;
        if (((length + 15) & ~15u) != slot->length) { needrepack = TRUE; }
    }

    /* Resolve aliases first, then compact the chosen setup. The runtime loads
     * the whole FTBL slot, so shortening bytes inside an old large slot does
     * not save RAM: its following resource address must move as well. */
    for (index = 0; index < slotcount; index++) if (slots[index].setup)
    {
        RomExportSlot *slot = &slots[index];
        const unsigned char *source = slot->replacement ? slot->replacement : rom->data + slot->offset;
        DWORD size = slot->replacement ? slot->replacementlength : slot->length;
        unsigned char *packed;
        DWORD packedsize;
        DWORD stanoffset = 0;
        DWORD levelindex;
        char setupname[64];
        const RomLevel *level;
        /* Unused stock setup placeholders can share a tiny, opaque slot. */
        if (size < 40) { continue; }
        level = RomExportSetupLevel(project, rom, slot, &stanoffset, setupname, &levelindex);
        if (level && (RomExportRead32(source + 24) || RomExportRead32(source + 28)))
        {
            RomExportSlot *stanslot = RomExportFindSlot(slots, slotcount, stanoffset);
            StanFile stan = {0}; SetupStanRefresh stats;
            BOOL ok;
            if (!stanslot) { *reasonout = "The setup's stan resource is missing."; goto fail; }
            if (!StanLoadNative(stanslot->replacement ? stanslot->replacement : rom->data + stanslot->offset,
                stanslot->replacement ? stanslot->replacementlength : stanslot->length,
                level->levelscale, &stan, reasonout)) { goto fail; }
            RomExportPadContext context = {report, level, levelindex, setupname};
            ok = SetupRefreshPadStanNativeReport(source, size, &stan, &packed, &packedsize, &stats,
                RomExportPadIssue, &context, reasonout);
            StanFileFree(&stan);
            if (!ok) { goto fail; }
            unresolved += stats.unresolved;
        }
        else
        {
            if (RomExportRead32(source + 24) || RomExportRead32(source + 28))
            {
                skipped++;
                if (report)
                {
                    LevelIssue issue = {0}; issue.kind = LEVEL_ISSUE_MISSING_STAN;
                    snprintf(issue.scope, sizeof(issue.scope), "%s", slot->setupname);
                    strcpy(issue.subject, "Pad references not checked");
                    strcpy(issue.description, "This setup has no unique level/STAN pairing. Its authored pad references were retained; no unresolved-pad count is available for it.");
                    if (!LevelIssuesAdd(report, &issue))
                    { *reasonout = "Out of memory recording unchecked setups."; goto fail; }
                }
            }
            if (!SetupCompactNative(source, size, &packed, &packedsize, reasonout)) { goto fail; }
        }
        if (!OccludersValidateNative(packed, packedsize, featureRom ? featureRom : rom, reasonout))
        { free(packed); goto fail; }
        free(slot->replacement);
        slot->replacement = packed;
        slot->replacementlength = packedsize;
        if (packedsize != slot->length) { needrepack = TRUE; }
    }

    if (unresolved || skipped)
    {
        snprintf(g_RomExportCleanupWarning, sizeof(g_RomExportCleanupWarning),
            "Across all ROM setups: %lu pads could not be resolved; their names and positions were retained. "
            "%lu setups have no unique level/STAN pairing. "
            "See Check for Issues > Last ROM export for the level, setup and pad list.",
            (unsigned long)unresolved, (unsigned long)skipped);
    }

    if (needrepack)
    {
        BOOL ok = RomExportRepackResources(rom, obsg, ftbl, slots,
                                           slotcount, reasonout);
        RomExportFreeSlots(slots, slotcount);
        return ok;
    }

    for (index = 0; index < slotcount; index++)
    {
        if (slots[index].replacement != NULL)
        {
            if (slots[index].model) { memset(rom->data + slots[index].offset, 0, slots[index].length); }
            memcpy(rom->data + slots[index].offset,
                   slots[index].replacement,
                   slots[index].replacementlength);
        }
    }
    RomExportFreeSlots(slots, slotcount);
    return TRUE;

fail:
    RomExportFreeSlots(slots, slotcount);
    return FALSE;
}


static BOOL RomExportUpdateLevelTable(const GEditorProject *project,
                                      RomFile *rom,
                                      const char **reasonout)
{
    const RomManifestEntry *stgt = NULL;
    DWORD rowSize, layoutShift;
    DWORD i;

    for (i = 0; i < rom->info.entrycount; i++)
    {
        if (rom->info.entries[i].kind == ROM_KIND_STGT)
        {
            stgt = &rom->info.entries[i];
            break;
        }
    }

    if (!RomLevelTableIsValid(stgt, rom->size))
    {
        *reasonout = "the base ROM's level table is invalid.";
        return FALSE;
    }

    rowSize = (stgt->romend - stgt->romstart) / stgt->flags;
    /* Preserve the allocation string pointer when writing the new layout. */
    layoutShift = rowSize - ROM_LEVEL_ROW_LEGACY_SIZE;

    for (i = 0; i < project->levelcount; i++)
    {
        const RomLevel *level = &project->levels[i];
        unsigned char *row = NULL;
        DWORD j;

        if (!RomScaleIsValid(level->levelscale) || !RomScaleIsValid(level->renderScale))
        {
            RomExportSetError(reasonout,
                              "level %s has an invalid scale.", level->name);
            return FALSE;
        }

        for (j = 0; j < stgt->flags; j++)
        {
            unsigned char *candidate = rom->data + stgt->romstart + j * rowSize;

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
            RomExportWrite32(row + 20 + layoutShift, bits.u);
            bits.f = level->renderScale;
            RomExportWrite32(row + 24 + layoutShift, bits.u);
        }

        RomExportWrite16(row + 28 + layoutShift, (unsigned short)level->music);
        RomExportWrite16(row + 30 + layoutShift, (unsigned short)level->bgsound);
        RomExportWrite16(row + 32 + layoutShift, (unsigned short)level->xtrack);
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


static BOOL RomExportBuild(const GEditorProject *project, RomFile *rom,
                            LevelIssueReport *report, const RomFile *featureRom,
                            const char **reasonout)
{
    char basepath[MAX_PATH];
    ZeroMemory(rom, sizeof(*rom));
    *reasonout = "";
    if (!RomExportBasePath(project, basepath, sizeof(basepath), reasonout)
        || !RomLoad(basepath, rom, reasonout))
    {
        return FALSE;
    }
    return RomExportProjectMatchesRom(project, rom, reasonout)
        && EnvironmentApplyRom(rom, &project->environmentOverrides, reasonout)
        && LevelMemoryApplyRom(rom, &project->memoryOverrides, reasonout)
        && RomExportReplaceProjectResources(project, rom, report, featureRom, reasonout)
        && RomExportUpdateLevelTable(project, rom, reasonout)
        && NewPropsExportToRom(project->dir, rom, reasonout)
        && ImageEditsExportToRom(project->dir, rom, reasonout)
        && RomExportUpdateChecksum(rom, reasonout);
}

BOOL RomExportValidateProject(const GEditorProject *project, const char **reasonout)
{
    RomFile rom;
    BOOL ok = RomExportBuild(project, &rom, NULL, NULL, reasonout);
    RomFree(&rom);
    return ok;
}

/* Rebase validates assets against the old base but new runtime capabilities
 * against the destination. This lets an authored occluder project upgrade. */
BOOL RomExportValidateRebaseSource(const GEditorProject *project, const RomFile *target, const char **reasonout)
{
    RomFile rom;
    BOOL ok = RomExportBuild(project, &rom, NULL, target, reasonout);
    RomFree(&rom);
    return ok;
}

BOOL RomExportCreate(const GEditorProject *project,
                     const char *name, const char *directory,
                     char *pathout, size_t pathmax,
                     const char **reasonout)
{
    RomFile rom;
    BOOL ok;
    LevelIssueReport report = {0};
    if (!RomExportDestinationIsValid(project, directory, name,
                                     pathout, pathmax, reasonout)) { return FALSE; }
    ok = RomExportBuild(project, &rom, &report, NULL, reasonout)
        && RomExportWriteFile(pathout, rom.data, rom.size, reasonout);
    RomFree(&rom);
    if (ok)
    {
        RomExportClearIssues(); g_RomExportIssues = report; g_RomExportHasIssues = TRUE;
    }
    else { LevelIssuesFree(&report); }
    return ok;
}
