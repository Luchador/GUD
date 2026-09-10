#ifndef GEDITOR_TEXROM_H
#define GEDITOR_TEXROM_H
#include "texload.h"

#define TEX_IMAGE_CAPACITY 4096u
typedef struct TexRomBank {
    DWORD images, imagebytes, table, config;
    DWORD count, capacity, hash;
    DWORD manifestentry; /* IMGS entry in the on-disk manifest */
} TexRomBank;

DWORD TexDataHash(const unsigned char *data, DWORD size);
BOOL TexRomReadBank(const RomFile *rom, TexRomBank *bank, const char **reasonout);
/* Rebuild IMGS + its table after all other ROM edits. Original records and
 * surface/detail flags are retained byte-for-byte. Input base.z64 is untouched. */
BOOL TexRomAppendImages(RomFile *rom, const TexRomBank *bank,
    const unsigned char *const *records, const DWORD *sizes,
    const unsigned char *surfaces, DWORD count, const char **reasonout);
/* Indexed by final texture ID: NULL preserves an original, non-NULL replaces
 * it. Appended IDs must all have records. No existing IDs are shifted. */
BOOL TexRomUpdateImages(RomFile *rom, const TexRomBank *bank,
    const unsigned char *const *records, const DWORD *sizes,
    const unsigned char *surfaces, DWORD count, const char **reasonout);
#endif
