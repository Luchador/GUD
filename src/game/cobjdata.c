#include <ultra64.h>
#include <bondgame.h>
#include "chrobjdata.h"
#include "gedmanifest.h"
#include <custompropformat.h>
#define CHARACTER_MODEL_TABLE_SIZE (CUSTOM_CHARACTER_LIMIT + 1)

//D:8003D410
#include <assets/obseg/chr/chrModelFileHeaders.inc.c>

//struct ChrModelFileRecord CitemZ_entries[] = {
#include <assets/obseg/chr/chrModelFileRecords.inc.c>
GEDM_TABLE(g_GedCharacterModels, CitemZ_entries, 1);

//D:8003E464
#include <assets/obseg/chr/chrHeadHats.inc.c>
