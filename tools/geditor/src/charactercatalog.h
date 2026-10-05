#ifndef GEDITOR_CHARACTERCATALOG_H
#define GEDITOR_CHARACTERCATALOG_H
#include <src/custompropformat.h>
/* Reuse the game's complete CitemZ_entries order and metadata. The included
   model headers supply switch counts through local header placeholders;
   no N64 structs, skeletons or runtime symbols enter the editor build. */
typedef struct CharacterSourceHeader {
    int switchcount;
    const int *skeleton;
} CharacterSourceHeader;
static const int g_EditorSkeleton_guard = 1, g_EditorSkeleton_suit_lf_hand = 2;
#define SKELETON(NAME) g_EditorSkeleton_ ## NAME
#define MODELFILEHEADER(NAME, ROOT, SKELETON, SWITCHES, NUMSWITCHES, NUMMATRICES, RADIUS, RECORDS, TEXTURES) \
    static const CharacterSourceHeader NAME ## _header = {NUMSWITCHES, SKELETON};
#include <assets/obseg/chr/chrModelFileHeaders.inc.c>
#undef MODELFILEHEADER
#undef SKELETON

typedef struct CharacterSourceDefinition {
    const CharacterSourceHeader *header;
    const char *filename;
    float scale;
    float pov;
    unsigned char ismale, hashead, pad1, pad2;
} CharacterSourceDefinition;

#define ChrModelFileRecord CharacterSourceDefinition
#define CitemZ_entries g_CharacterModels
static const
#include <assets/obseg/chr/chrModelFileRecords.inc.c>
#undef CitemZ_entries
#undef ChrModelFileRecord


/* Stock IDs are fixed: bodies have the guard skeleton; standalone heads
 * have no skeleton. The watch hand is neither. */
static inline int CharacterCatalogKind(int id)
{
    if (id < 0 || id >= CUSTOM_CHARACTER_BASE) return 0;
    const CharacterSourceHeader *header = g_CharacterModels[id].header;
    return header->skeleton == &g_EditorSkeleton_guard ? CUSTOM_CHARACTER_BODY
        : header->skeleton == NULL ? CUSTOM_CHARACTER_HEAD : 0;
}
#endif
