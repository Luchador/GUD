#ifndef GEDITOR_ROMEXPORTSETTINGS_H
#define GEDITOR_ROMEXPORTSETTINGS_H

#include "romexport.h"

/* Application preferences: fall back to the current project on first use. */
void RomExportSettingsLoad(const GEditorProject *project,
    char name[ROM_EXPORT_NAME_MAX], char directory[MAX_PATH]);
/* Call only after a ROM was successfully written. */
void RomExportSettingsRemember(const char *name, const char *directory);

#endif
