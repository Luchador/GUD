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
#define ROM_MANIFEST_VERSION   2

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

/* Bounded copy that always terminates. */
static void RomCopyString(char *dst, DWORD dstmax, const unsigned char *src, DWORD srcmax)
{
    DWORD i;

    for (i = 0; i + 1 < dstmax && i < srcmax && src[i] != '\0'; i++)
    {
        dst[i] = (char)src[i];
    }

    dst[i] = '\0';
}

/* "UsetupsevbunkerZ" -> "sevbunker". Unrecognized shapes copy through. */
static void RomSetupStem(char *dst, DWORD dstmax, const char *setup)
{
    DWORD len;

    if (strncmp(setup, "Usetup", 6) == 0)
    {
        setup += 6;
    }

    RomCopyString(dst, dstmax, (const unsigned char *)setup, dstmax);

    len = strlen(dst);
    if (len > 0 && dst[len - 1] == 'Z')
    {
        dst[len - 1] = '\0';
    }
}

/* "bg/bg_sev_all_p.seg" -> "sev". */
static void RomWorldStem(char *dst, DWORD dstmax, const char *bg)
{
    const char *slash = strrchr(bg, '/');
    char *cut;

    if (slash != NULL)
    {
        bg = slash + 1;
    }
    if (strncmp(bg, "bg_", 3) == 0)
    {
        bg += 3;
    }

    RomCopyString(dst, dstmax, (const unsigned char *)bg, dstmax);

    cut = strstr(dst, "_all_p.seg");
    if (cut == NULL)
    {
        cut = strstr(dst, ".seg");
    }
    if (cut != NULL)
    {
        *cut = '\0';
    }
}

DWORD RomLevelTableRowSize(const RomManifestEntry *stgt, DWORD romsize)
{
    DWORD bytes, stride;
    if (stgt == NULL || stgt->flags == 0 || stgt->flags > ROM_MAX_LEVELS
        || stgt->romstart >= stgt->romend || stgt->romend > romsize) { return 0; }
    bytes = stgt->romend - stgt->romstart;
    if (bytes % stgt->flags != 0) { return 0; }
    stride = bytes / stgt->flags;
    return stride == 32 || stride == 36 ? stride : 0;
}

/*
 * Parses the STGT level table, chasing its name pointers through the
 * CMAP virtual-address mapping. Fills info->levels/levelcount.
 * Returns FALSE with *reasonout set if the table is malformed.
 */
static BOOL RomParseLevelTable(const unsigned char *data, DWORD size,
                               const RomManifestEntry *stgt,
                               const RomManifestEntry *cmap,
                               RomInfo *info, const char **reasonout)
{
    DWORD rows = stgt->flags;
    DWORD stride = RomLevelTableRowSize(stgt, size);
    DWORD nameshift = stride == 36 ? 4 : 0;
    DWORD i;

    if (stride == 0)
    {
        *reasonout = "GUD level table is malformed.";
        return FALSE;
    }

    for (i = 0; i < rows; i++)
    {
        const unsigned char *row = data + stgt->romstart + i * stride;
        RomLevel *lvl = &info->levels[info->levelcount];
        DWORD strs[4];
        char *dsts[4];
        DWORD dstmax[4];
        DWORD s;

        lvl->levelID = (LONG)be32(row + 0);
        strs[0] = be32(row + 4 + nameshift);  dsts[0] = lvl->setupname; dstmax[0] = sizeof(lvl->setupname);
        strs[1] = be32(row + 8 + nameshift);  dsts[1] = lvl->bgname;    dstmax[1] = sizeof(lvl->bgname);
        strs[2] = be32(row + 12 + nameshift); dsts[2] = lvl->stanname;  dstmax[2] = sizeof(lvl->stanname);
        strs[3] = nameshift ? be32(row + 4) : 0;
        dsts[3] = lvl->name; dstmax[3] = sizeof(lvl->name);

        for (s = 0; s < 4; s++)
        {
            LONGLONG off;

            if (strs[s] == 0)
            {
                /* NULL is a legal value here: the table's sentinel row
                   carries no name or setup file. Resolve it to an empty string
                   instead of refusing the whole ROM. */
                dsts[s][0] = '\0';
                continue;
            }

            /* virtual address -> ROM offset, per the CMAP contract.
               Signed 64-bit so an out-of-segment address becomes a
               plainly negative offset rather than an unsigned wrap. */
            off = (LONGLONG)strs[s] - cmap->flags + cmap->romstart;

            if (off < 0 || off >= (LONGLONG)size)
            {
                *reasonout = "GUD level table points outside the ROM (corrupt build?).";
                return FALSE;
            }

            RomCopyString(dsts[s], dstmax[s], data + (DWORD)off, size - (DWORD)off);
        }

        /* The table ends with a placeholder row; it is not a level. */
        if (lvl->bgname[0] == '\0' || strcmp(lvl->bgname, "bg/bgx.seg") == 0)
        {
            continue;
        }

        {
            /* floats arrive as big-endian bit patterns */
            union { DWORD u; float f; } cvt;

            cvt.u = be32(row + 16 + nameshift); lvl->levelscale = cvt.f;
            cvt.u = be32(row + 20 + nameshift); lvl->renderScale = cvt.f;
        }

        lvl->music   = (short)((row[24 + nameshift] << 8) | row[25 + nameshift]);
        lvl->bgsound = (short)((row[26 + nameshift] << 8) | row[27 + nameshift]);
        lvl->xtrack  = (short)((row[28 + nameshift] << 8) | row[29 + nameshift]);

        if (lvl->name[0] == '\0') { RomSetupStem(lvl->name, sizeof(lvl->name), lvl->setupname); }
        RomWorldStem(lvl->world, sizeof(lvl->world), lvl->bgname);

        info->levelcount++;
    }

    return TRUE;
}

/*
 * Validates an in-memory ROM image and fills info. Never frees data.
 */
static BOOL RomValidateBuffer(unsigned char *data, DWORD size,
                              RomInfo *info, const char **reasonout)
{
    static const unsigned char z64magic[4] = { 0x80, 0x37, 0x12, 0x40 };
    DWORD m;
    DWORD i;

    ZeroMemory(info, sizeof(*info));
    *reasonout = "";

    if (size < ROM_MIN_SIZE || size > ROM_MAX_SIZE)
    {
        *reasonout = "That file is not a plausible ROM size.";
        return FALSE;
    }

    if (memcmp(data, z64magic, 4) != 0)
    {
        *reasonout = "Not a big-endian .z64 ROM (byte-swapped dump?).";
        return FALSE;
    }

    /* Header title: GoldenEye-engine ROMs say so at 0x20. */
    memcpy(info->internalname, data + 0x20, 20);
    info->internalname[20] = '\0';
    if (strstr(info->internalname, "GOLDENEYE") == NULL)
    {
        *reasonout = "Not a GoldenEye-engine ROM (header title mismatch).";
        return FALSE;
    }

    m = RomFindManifest(data, size);
    if (m == 0)
    {
        *reasonout = "GoldenEye ROM, but not a GUD build - no GEditor manifest.";
        return FALSE;
    }

    if (m + 24 > size)
    {
        *reasonout = "GUD manifest is truncated.";
        return FALSE;
    }

    info->size = size;
    info->manifestoffset = m;
    info->manifestversion = be32(data + m + 16);
    info->entrycount = be32(data + m + 20);

    if (info->manifestversion < 1 || info->manifestversion > ROM_MANIFEST_VERSION)
    {
        *reasonout = "GUD manifest is from a newer GEditor - update GEditor.";
        return FALSE;
    }

    if (info->entrycount < 1 || info->entrycount > ROM_MAX_ENTRIES
        || m + 24 + info->entrycount * 16 > size)
    {
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

        /* romend == 0 marks a self-terminating entry (FTBL ends at a
           NULL row), so only its start needs to be inside the ROM.
           Every other entry must be a well-formed range. */
        if (out->romend == 0)
        {
            if (out->romstart >= size)
            {
                *reasonout = "GUD manifest points outside the ROM (corrupt build?).";
                return FALSE;
            }
        }
        else if (out->romstart >= out->romend || out->romend > size)
        {
            *reasonout = "GUD manifest points outside the ROM (corrupt build?).";
            return FALSE;
        }
    }

    {
        const RomManifestEntry *stgt = NULL;
        const RomManifestEntry *cmap = NULL;

        for (i = 0; i < info->entrycount; i++)
        {
            if (info->entries[i].kind == 0x53544754)      /* 'STGT' */
            {
                stgt = &info->entries[i];
            }
            else if (info->entries[i].kind == 0x434D4150) /* 'CMAP' */
            {
                cmap = &info->entries[i];
            }
        }

        if (stgt == NULL || cmap == NULL)
        {
            *reasonout = "GUD build predates the level manifest - rebuild GUD.";
            return FALSE;
        }

        if (!RomParseLevelTable(data, size, stgt, cmap, info, reasonout))
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL RomValidate(const char *path, RomInfo *info, const char **reasonout)
{
    unsigned char *data;
    DWORD size = 0;
    BOOL ok;

    data = RomReadAll(path, &size);
    if (data == NULL)
    {
        ZeroMemory(info, sizeof(*info));
        *reasonout = "The ROM file could not be read.";
        return FALSE;
    }

    ok = RomValidateBuffer(data, size, info, reasonout);
    free(data);
    return ok;
}

BOOL RomLoad(const char *path, RomFile *rom, const char **reasonout)
{
    ZeroMemory(rom, sizeof(*rom));

    rom->data = RomReadAll(path, &rom->size);
    if (rom->data == NULL)
    {
        *reasonout = "The ROM file could not be read.";
        return FALSE;
    }

    if (!RomValidateBuffer(rom->data, rom->size, &rom->info, reasonout))
    {
        RomFree(rom);
        return FALSE;
    }

    return TRUE;
}

void RomFree(RomFile *rom)
{
    free(rom->data);
    ZeroMemory(rom, sizeof(*rom));
}

#define ROM_KIND_FTBL 0x4654424C  /* 'FTBL' */
#define ROM_KIND_CMAP 0x434D4150  /* 'CMAP' */
#define ROM_KIND_OBSG 0x4F425347  /* 'OBSG' */
#define ROM_KIND_ENVT 0x454E5654u /* 'ENVT' */
#define ROM_FTBL_MAX_ROWS 1024

BOOL RomGetLevelBackgroundColor(const RomFile *rom, LONG levelid, unsigned char rgb[3])
{
    const RomManifestEntry *envt = NULL;
    const RomManifestEntry *cmap = NULL;
    const unsigned char *selected = NULL;
    DWORD i, offset;
    int bestpriority = 0;

    if (rom == NULL || rom->data == NULL || rgb == NULL || rom->info.entrycount > ROM_MAX_ENTRIES) 
    { 
        return FALSE; 
    }

    for (i = 0; i < rom->info.entrycount; i++)
    {
        const RomManifestEntry *entry = &rom->info.entries[i];

        if (entry->kind == ROM_KIND_ENVT) 
        { 
            envt = entry; 
        }
    
        if (entry->kind == ROM_KIND_CMAP) 
        { 
            cmap = entry; 
        }
    }

    /* Current N64 EnvironmentRecord: 104 bytes; Sky RGB starts at 44. */
    if (envt == NULL || cmap == NULL || envt->flags != 104u || envt->romend != 0 || cmap->romstart >= cmap->romend || cmap->romend > rom->size || envt->romstart < cmap->romstart || envt->romstart >= cmap->romend) 
    { 
        return FALSE; 
    }

    offset = envt->romstart;

    while (cmap->romend - offset >= envt->flags)
    {
        const unsigned char *row = rom->data + offset;
        DWORD id = be32(row);
        int priority = 0;

        if (id == 0) /* ENVIRONMENTDATA_END */
        {
            if (selected == NULL) { return FALSE; }
            memcpy(rgb, selected + 44, 3);
            return TRUE;
        }

        /* Prefer solo, then a two-player preview for MP-only maps,
         * then LEVELID_NONE (-1), the game's fallback environment. */
        if (id == (DWORD)levelid) 
        { 
            priority = 3; 
        }
        else if (id == (DWORD)levelid + 200u) 
        { 
            priority = 2; 
        }
        else if (id == 0xFFFFFFFFu) 
        { 
            priority = 1; 
        }

        if (priority > bestpriority)
        {
            selected = row;
            bestpriority = priority;
        }

        offset += envt->flags;
    }

    return FALSE; /* Missing terminator or incomplete final record. */
}

/* File-table entries can alias the same data (several multiplayer
   names do). The tightest table-derived upper bound is therefore the
   smallest DISTINCT data address after the start, not necessarily the
   following row. */
static DWORD RomFindFileUpperBound(const RomFile *rom, const RomManifestEntry *ftbl, const RomManifestEntry *obsg, DWORD start)
{
    DWORD end = obsg != NULL ? obsg->romend : rom->size;
    DWORD i;

    for (i = 0; i < ROM_FTBL_MAX_ROWS; i++)
    {
        DWORD row = ftbl->romstart + i * 12;
        DWORD candidate;

        if (row + 12 > rom->size || be32(rom->data + row + 4) == 0)
        {
            break;
        }

        candidate = be32(rom->data + row + 8);

        if (candidate > start && candidate < end)
        {
            end = candidate;
        }
    }

    return end;
}

BOOL RomFindFile(const RomFile *rom, const char *name, DWORD *offset, DWORD *maxlen, const char **reasonout)
{
    const RomManifestEntry *ftbl = NULL;
    const RomManifestEntry *cmap = NULL;
    const RomManifestEntry *obsg = NULL;
    DWORD i;

    *offset = 0;
    *maxlen = 0;
    *reasonout = "";

    for (i = 0; i < rom->info.entrycount; i++)
    {
        const RomManifestEntry *e = &rom->info.entries[i];

        if (e->kind == ROM_KIND_FTBL) { ftbl = e; }
        if (e->kind == ROM_KIND_CMAP) { cmap = e; }
        if (e->kind == ROM_KIND_OBSG) { obsg = e; }
    }

    if (ftbl == NULL || cmap == NULL)
    {
        *reasonout = "GUD build predates the file table - rebuild GUD.";
        return FALSE;
    }

    /* fileentry rows: {id, name vaddr, data} - 12 bytes each, ending
       at a row whose name pointer is NULL. Data pointers are plain
       ROM offsets: the obseg segment is linked at its own ROM start. */
    for (i = 0; i < ROM_FTBL_MAX_ROWS; i++)
    {
        DWORD row = ftbl->romstart + i * 12;
        DWORD namev;
        DWORD datav;
        LONGLONG nameoff;

        if (row + 12 > rom->size)
        {
            break;
        }

        namev = be32(rom->data + row + 4);
        if (namev == 0)
        {
            break; /* terminator */
        }

        datav = be32(rom->data + row + 8);

        nameoff = (LONGLONG)namev - cmap->flags + cmap->romstart;
        if (nameoff < 0 || nameoff >= (LONGLONG)rom->size)
        {
            continue; /* unresolvable name: not ours to match */
        }

        if (strncmp((const char *)rom->data + (DWORD)nameoff, name, rom->size - (DWORD)nameoff) == 0)
        {
            DWORD end = RomFindFileUpperBound(rom, ftbl, obsg, datav);

            if (datav >= end || (obsg != NULL && datav < obsg->romstart))
            {
                *reasonout = "file table entry points outside the ROM.";
                return FALSE;
            }

            *offset = datav;
            *maxlen = end - datav;
            return TRUE;
        }
    }

    *reasonout = "file not present in the ROM's file table.";
    return FALSE;
}


BOOL RomGetFileByIndex(const RomFile *rom, DWORD index, char *nameout, DWORD namemax, DWORD *offset, DWORD *maxlen)
{
    const RomManifestEntry *ftbl = NULL;
    const RomManifestEntry *cmap = NULL;
    const RomManifestEntry *obsg = NULL;
    DWORD row;
    DWORD namev;
    DWORD datav;
    LONGLONG nameoff;
    DWORD i;

    nameout[0] = '\0';

    for (i = 0; i < rom->info.entrycount; i++)
    {
        const RomManifestEntry *e = &rom->info.entries[i];

        if (e->kind == ROM_KIND_FTBL) { ftbl = e; }
        if (e->kind == ROM_KIND_CMAP) { cmap = e; }
        if (e->kind == ROM_KIND_OBSG) { obsg = e; }
    }

    if (ftbl == NULL || cmap == NULL || index >= ROM_FTBL_MAX_ROWS)
    {
        return FALSE;
    }

    row = ftbl->romstart + index * 12;
    if (row + 12 > rom->size)
    {
        return FALSE;
    }

    namev = be32(rom->data + row + 4);
    if (namev == 0)
    {
        return FALSE; /* terminator */
    }

    nameoff = (LONGLONG)namev - cmap->flags + cmap->romstart;
    if (nameoff < 0 || nameoff >= (LONGLONG)rom->size)
    {
        return FALSE;
    }

    for (i = 0; i + 1 < namemax && (DWORD)nameoff + i < rom->size; i++)
    {
        char ch = (char)rom->data[(DWORD)nameoff + i];

        if (ch == '\0')
        {
            break;
        }

        nameout[i] = ch;
    }

    nameout[i] = '\0';

    if (offset != NULL && maxlen != NULL)
    {
        datav = be32(rom->data + row + 8);

        {
            DWORD end = RomFindFileUpperBound(rom, ftbl, obsg, datav);

            if (datav >= end || (obsg != NULL && datav < obsg->romstart))
            {
                return FALSE;
            }

            *offset = datav;
            *maxlen = end - datav;
        }
    }

    return TRUE;
}
