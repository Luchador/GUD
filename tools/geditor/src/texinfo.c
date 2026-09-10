/* Read-only image information for the content browser. The game keeps surface
 * categories in g_Textures, separately from the GUTX pixel/mipmap records. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "texload.h"

#define TEXINFO_RECORD_HEADER 100u
#define TEXINFO_IMAGE_ENTRY_SIZE 8u

typedef struct TexInfoRecord {
    DWORD size;
    TexImageInfo info;
} TexInfoRecord;

static DWORD TexInfoBe32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3];
}

static DWORD TexInfoBe16(const unsigned char *p)
{
    return ((DWORD)p[0] << 8) | p[1];
}

static const RomManifestEntry *TexInfoSegment(const RomFile *rom, DWORD kind)
{
    DWORD i;
    for (i = 0; i < rom->info.entrycount && i < ROM_MAX_ENTRIES; i++)
    {
        const RomManifestEntry *entry = &rom->info.entries[i];
        if (entry->kind == kind && entry->romstart < entry->romend
            && entry->romend <= rom->size) { return entry; }
    }
    return NULL;
}

static DWORD TexInfoRowBytes(unsigned int format, DWORD width)
{
    if (format == 0 || format == 2) { return ((width + 3) & ~3u) * 4; }
    if (format == 1 || format == 3 || format == 4) { return ((width + 3) & ~3u) * 2; }
    if (format == 5 || format == 7 || format == 9 || format == 11) { return (width + 7) & ~7u; }
    if (format == 6 || format == 8 || format == 10 || format == 12) { return ((width + 15) & ~15u) / 2; }
    return 0;
}

static BOOL TexInfoReadRecord(const unsigned char *rec, DWORD available, TexInfoRecord *out)
{
    DWORD size, header, colours, i, payload = 0;
    unsigned int levels, images;
    TexImageInfo *info = &out->info;
    ZeroMemory(out, sizeof(*out));
    if (available < TEXINFO_RECORD_HEADER || memcmp(rec, "GUTX", 4) != 0) { return FALSE; }
    size = TexInfoBe32(rec + 12);
    header = TexInfoBe16(rec + 10);
    colours = TexInfoBe16(rec + 8);
    levels = rec[5];
    images = rec[6];
    if (size < TEXINFO_RECORD_HEADER || size > available || (size & 15)
        || colours > 256 || header < TEXINFO_RECORD_HEADER + colours * 2
        || header > size || (header & 15) || rec[4] > 1 || levels > 7
        || images != (rec[4] && levels ? levels : 1)) { return FALSE; }
    for (i = 0; i < images; i++)
    {
        const unsigned char *desc = rec + 16 + i * 12;
        DWORD offset = TexInfoBe32(desc + 4), bytes = TexInfoBe32(desc + 8);
        DWORD rowbytes = TexInfoRowBytes(desc[0], desc[1]);
        if (rowbytes == 0 || desc[1] == 0 || desc[2] == 0
            || offset != header + payload || offset > size || bytes > size - offset
            || bytes != rowbytes * desc[2]
            || (desc[0] >= 9 && colours == 0)) { return FALSE; }
        payload += bytes;
    }
    /* The loader generates implicit levels, but caps paletted pixel data at
     * 0x800 bytes. Match texLoadRaw rather than counting stored descriptors. */
    if (!rec[4] && levels >= 2 && colours)
    {
        DWORD width = rec[17], height = rec[18], total = payload;
        for (i = 1; i < levels; i++)
        {
            DWORD bytes;
            width = (width + 1) / 2;
            height = (height + 1) / 2;
            bytes = TexInfoRowBytes(rec[16], width) * height;
            if (total + bytes > 0x800) { levels = i; break; }
            total += bytes;
        }
    }
    out->size = size;
    info->valid = TRUE;
    info->format = rec[16];
    info->mipmaps = levels > 0 ? levels - 1 : 0;
    info->generatedmipmaps = !rec[4] && info->mipmaps > 0;
    return TRUE;
}

/* Existing ROM manifests do not expose g_Textures. Identify it inside CMAP
 * by matching EVERY GUTX record size in order and the terminating image_entry.
 * Sizes occupy the low 24 bits; the first byte holds independent sound/decal
 * nibbles (src/game/image.h). Never substitute compiled-in images.def values,
 * and reject ambiguous matches instead of reporting another ROM's settings. */
static const unsigned char *TexInfoFindSurfaceTable(const RomFile *rom,
                                                    const TexInfoRecord *records, DWORD count)
{
    const RomManifestEntry *cmap = TexInfoSegment(rom, 0x434D4150u);
    const unsigned char *found = NULL;
    DWORD pos, i, bytes;
    if (cmap == NULL || count < 16 || count > (cmap->romend - cmap->romstart) / TEXINFO_IMAGE_ENTRY_SIZE)
    { return NULL; }
    bytes = (count + 1) * TEXINFO_IMAGE_ENTRY_SIZE;
    if (bytes > cmap->romend - cmap->romstart) { return NULL; }
    for (pos = (cmap->romstart + 3) & ~3u; pos <= cmap->romend - bytes; pos += 4)
    {
        const unsigned char *table = rom->data + pos;
        for (i = 0; i < count; i++)
        {
            if ((TexInfoBe32(table + i * TEXINFO_IMAGE_ENTRY_SIZE) & 0xFFFFFFu) != records[i].size)
            { break; }
        }
        if (i == count && TexInfoBe32(table + count * TEXINFO_IMAGE_ENTRY_SIZE) == 0xFFFFu
            && TexInfoBe32(table + count * TEXINFO_IMAGE_ENTRY_SIZE + 4) == 0)
        {
            if (found != NULL) { return NULL; }
            found = table;
        }
    }
    return found;
}

void TexSetRomThumbnailInfo(const RomFile *rom, TexThumb *items, DWORD count)
{
    const RomManifestEntry *images;
    const unsigned char *surfaces = NULL;
    TexInfoRecord *records = NULL;
    DWORD capacity = 0, recordcount = 0, pos, i;
    BOOL complete = TRUE;
    if (items == NULL) { return; }
    for (i = 0; i < count; i++) { ZeroMemory(&items[i].info, sizeof(items[i].info)); }
    if (rom == NULL || rom->data == NULL
        || (images = TexInfoSegment(rom, 0x494D4753u)) == NULL) { return; }
    pos = images->romstart;
    while (pos < images->romend)
    {
        TexInfoRecord record;
        if (!TexInfoReadRecord(rom->data + pos, images->romend - pos, &record))
        {
            /* Linker alignment may leave zero padding after the last record. */
            for (i = pos; i < images->romend && rom->data[i] == 0; i++) { }
            complete = i == images->romend;
            break;
        }
        if (recordcount == capacity)
        {
            DWORD next = capacity ? capacity * 2 : 512;
            TexInfoRecord *grown = (TexInfoRecord *)realloc(records, (size_t)next * sizeof(*records));
            if (grown == NULL) { complete = FALSE; break; }
            records = grown;
            capacity = next;
        }
        records[recordcount++] = record;
        pos += record.size;
    }
    if (complete) { surfaces = TexInfoFindSurfaceTable(rom, records, recordcount); }
    for (i = 0; i < count; i++)
    {
        char *end;
        unsigned long id = strtoul(items[i].label, &end, 16);
        if (end == items[i].label || *end != '\0' || id >= recordcount) { continue; }
        items[i].info = records[id].info;
        if (surfaces)
        {
            unsigned char types = surfaces[id * TEXINFO_IMAGE_ENTRY_SIZE];
            items[i].info.surfacevalid = TRUE;
            items[i].info.hitsound = types >> 4;
            items[i].info.hittexture = types & 15;
        }
    }
    free(records);
}

static const char *TexInfoSurfaceName(unsigned int type)
{
    /* HIT_TYPE order from src/bondconstants.h; both fields use this enum. */
    static const char *names[] = {"Default", "Stone", "Wood", "Metal", "Glass",
        "Water", "Snow", "Dirt", "Mud", "Tile", "Metal object", "Flesh", "Translucent glass"};
    return type < sizeof(names) / sizeof(names[0]) ? names[type] : "Unknown";
}

void TexFormatThumbnailInfo(const TexThumb *thumb, char *text, DWORD capacity)
{
    static const char *formats[] = {"RGBA32 (color + alpha)", "RGBA16 (color + alpha)",
        "RGB24 (color)", "RGB15 (color)", "IA16 (grayscale + alpha)",
        "IA8 (grayscale + alpha)", "IA4 (grayscale + alpha)", "I8 (grayscale)",
        "I4 (grayscale)", "CI8 (RGBA16 palette)", "CI4 (RGBA16 palette)",
        "CI8 (IA16 palette)", "CI4 (IA16 palette)"};
    const TexImageInfo *info = &thumb->info;
    char mipmaps[96];
    if (capacity == 0) { return; }
    if (!info->valid) { snprintf(mipmaps, sizeof(mipmaps), "Unavailable"); }
    else if (info->mipmaps == 0) { snprintf(mipmaps, sizeof(mipmaps), "0 (base image only)"); }
    else
    {
        snprintf(mipmaps, sizeof(mipmaps), "%u (%s; %u levels including base)",
            (unsigned int)info->mipmaps, info->generatedmipmaps ? "generated" : "stored",
            (unsigned int)info->mipmaps + 1);
    }
    snprintf(text, capacity, "%s\r\nDimensions: %d x %d pixels\r\nType: %s\r\nMipmaps: %s\r\n"
        "Hit sound: %s\r\nBullet hole: %s", thumb->label, thumb->imagewidth, thumb->imageheight,
        info->valid && info->format < sizeof(formats) / sizeof(formats[0]) ? formats[info->format] : "Unavailable",
        mipmaps, info->surfacevalid ? TexInfoSurfaceName(info->hitsound) : "Unavailable",
        info->surfacevalid ? TexInfoSurfaceName(info->hittexture) : "Unavailable");
}
