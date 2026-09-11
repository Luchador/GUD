#ifndef GUD_GEDMANIFEST_H
#define GUD_GEDMANIFEST_H

#include <ultra64.h>

/* New catalog entries point to this descriptor in CMAP. All fields are
 * big-endian; records is a 32-bit linked RAM address, not a ROM offset.
 * See tools/geditor/docs/MANIFEST.md for per-kind record layouts and IDs. */
#define GEDM_F_TABLE 0x80000000u
#define GEDM_TABLE_VERSION 1u

typedef struct GedManifestTable {
    const void *records;
    u32 count;       /* slots, including any documented unused IDs */
    u32 stride;     /* native N64 bytes per record */
    u32 version;    /* descriptor and per-kind record layout version */
} GedManifestTable;

typedef char GedManifestTableMustBe16Bytes[sizeof(GedManifestTable) == 16 ? 1 : -1];

/* Define beside the actual array so additions and native record sizes are
 * reflected automatically. Exclude trailing terminators, never interior holes. */
#define GEDM_TABLE(name, array, terminators) \
    const GedManifestTable name = { \
        (array), sizeof(array) / sizeof((array)[0]) - (terminators), \
        sizeof((array)[0]), GEDM_TABLE_VERSION \
    }

#endif
