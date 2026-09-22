#ifndef GEDITOR_SFXCATALOG_H
#define GEDITOR_SFXCATALOG_H

#include "rom.h"

typedef struct SfxEntry {
    DWORD id; /* GUD IDs are one-based; zero means no sound. */
    DWORD sampleOffset, sampleBytes;
    DWORD definitionOffset;
    unsigned char format;
} SfxEntry;

typedef struct SfxCatalog {
    SfxEntry *entries;
    DWORD count;
    DWORD controlOffset, controlBytes, sampleRate;
} SfxCatalog;

/* Initialize to zero. Owns metadata only, never retains the RomFile buffer. */
BOOL SfxCatalogReadRom(const RomFile *rom, SfxCatalog *catalog, const char **reason);
void SfxCatalogFree(SfxCatalog *catalog);
const char *SfxCatalogName(DWORD id);
BOOL SfxCatalogMatches(const SfxEntry *entry, const char *filter);

#endif
