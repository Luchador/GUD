#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sfxcatalog.h"
#include "sfxnames.h"

static DWORD SfxRead32(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }
static DWORD SfxRead16(const unsigned char *p)
{ return (DWORD)p[0] << 8 | p[1]; }
static BOOL SfxSpan(DWORD offset, DWORD size, DWORD length)
{ return offset <= length && size <= length - offset; }

void SfxCatalogFree(SfxCatalog *catalog)
{
    free(catalog->entries);
    memset(catalog, 0, sizeof(*catalog));
}

BOOL SfxCatalogReadRom(const RomFile *rom, SfxCatalog *catalog, const char **reason)
{
    const RomManifestEntry *ctl = NULL, *tbl = NULL;
    const char *why = "The ROM sound-effect bank is malformed.";
    SfxEntry *entries = NULL;
    SfxCatalogFree(catalog);
    if (!rom || !rom->data || rom->info.entrycount > ROM_MAX_ENTRIES) { goto fail; }
    for (DWORD i = 0; i < rom->info.entrycount; i++)
    {
        const RomManifestEntry *entry = &rom->info.entries[i];
        if (entry->kind == 0x53465843u) { if (ctl) { goto fail; } ctl = entry; }
        if (entry->kind == 0x53465854u) { if (tbl) { goto fail; } tbl = entry; }
    }
    if (!ctl || !tbl)
    {
        why = "This project's ROM has no sound-effect catalog. Rebase the project onto a current GUD ROM.";
        goto fail;
    }
    if (ctl->romstart >= ctl->romend || ctl->romend > rom->size || ctl->flags
        || tbl->romstart >= tbl->romend || tbl->romend > rom->size || tbl->flags) { goto fail; }
    const unsigned char *data = rom->data + ctl->romstart;
    DWORD length = ctl->romend - ctl->romstart, samples = tbl->romend - tbl->romstart;
    if (!SfxSpan(0, 8, length) || SfxRead16(data) != 0x4231u) { goto fail; }
    DWORD banks = SfxRead16(data + 2), bank = SfxRead32(data + 4);
    if (!banks || banks > 32767 || !SfxSpan(4, banks * 4, length)
        || !bank || !SfxSpan(bank, 16, length) || data[bank + 2]) { goto fail; }
    DWORD instruments = SfxRead16(data + bank), instrument = SfxRead32(data + bank + 12);
    if (!instruments || instruments > 32767 || !SfxSpan(bank + 12, instruments * 4, length)
        || !instrument || !SfxSpan(instrument, 16, length) || data[instrument + 3]) { goto fail; }
    DWORD count = SfxRead16(data + instrument + 14);
    if (count > 32767 || !SfxSpan(instrument + 16, count * 4, length)) { goto fail; }
    if (count)
    {
        entries = calloc(count, sizeof(*entries));
        if (!entries) { why = "Out of memory reading sound effects."; goto fail; }
    }
    /* sndPlaySfx uses bank 0/instrument 0 through ALInstrumentAlt, whose sound
     * array begins four bytes earlier than ALInstrument's. Thus ID 1 reads
     * native soundArray[0]. Do not deduplicate shared sounds or sample data. */
    for (DWORD i = 0; i < count; i++)
    {
        DWORD sound = SfxRead32(data + instrument + 16 + i * 4);
        if (!sound || !SfxSpan(sound, 16, length) || data[sound + 14]) { goto fail; }
        DWORD wave = SfxRead32(data + sound + 8);
        if (!wave || !SfxSpan(wave, 20, length) || data[wave + 9]) { goto fail; }
        DWORD offset = SfxRead32(data + wave), bytes = SfxRead32(data + wave + 4);
        if (bytes > 0x7fffffffu || !SfxSpan(offset, bytes, samples)) { goto fail; }
        entries[i].id = i + 1;
        entries[i].sampleOffset = tbl->romstart + offset;
        entries[i].sampleBytes = bytes;
        entries[i].format = data[wave + 8];
    }
    catalog->entries = entries;
    catalog->count = count;
    if (reason) { *reason = ""; }
    return TRUE;
fail:
    free(entries);
    if (reason) { *reason = why; }
    return FALSE;
}

const char *SfxCatalogName(DWORD id)
{
    return id < sizeof(g_SfxNames) / sizeof(g_SfxNames[0]) && g_SfxNames[id]
        ? g_SfxNames[id] : "Unnamed sound effect";
}

static BOOL SfxContains(const char *text, const char *filter)
{
    for (; *text; text++)
    {
        const char *a = text, *b = filter;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
        if (!*b) { return TRUE; }
    }
    return FALSE;
}

BOOL SfxCatalogMatches(const SfxEntry *entry, const char *filter)
{
    char id[32];
    if (!filter || !filter[0]) { return TRUE; }
    snprintf(id, sizeof(id), "0x%04lX %lu", (unsigned long)entry->id, (unsigned long)entry->id);
    return SfxContains(id, filter) || SfxContains(SfxCatalogName(entry->id), filter);
}
