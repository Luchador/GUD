#ifndef GEDITOR_CHARACTERLOAD_H
#define GEDITOR_CHARACTERLOAD_H

#include "objectload.h"

typedef struct CharacterModelDefinition {
    const char *filename;
    float scale;
    BOOL ismale;
    BOOL hashead;
} CharacterModelDefinition;

BOOL CharacterGetModelDefinition(int modelid, CharacterModelDefinition *out);
/* Resolves only the preview. A body with an integrated head returns -1. */
BOOL CharacterResolveModels(const SetupCharacter *character,
                             int *bodyid, int *headid);

/* Returns the feet position used by the preview. Pad height selects a stan
   floor; it does not suspend a character above that floor. Without stan data
   the preview uses the pad's world position directly. */
BOOL CharacterGetPadPosition(const SetupPad *pad, const StanFile *stan,
                              float levelscale, float position[3]);

/* Loads project character glTFs, attaches separate heads using the authored
   body node, and places a stationary preview on the setup's stan floor.
   Authored setup bytes and random head IDs are never changed. */
BOOL CharacterLoadSetupGeometry(const char *projectdir, const SetupFile *setup,
                                 const StanFile *stan, const RomFile *rom,
                                 float levelscale, SetupObjectGeometry *out,
                                 const char **reasonout);

#endif
