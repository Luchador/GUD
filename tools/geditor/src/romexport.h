#ifndef GEDITOR_ROMEXPORT_H
#define GEDITOR_ROMEXPORT_H

#include <windows.h>
#include <stddef.h>

#include "project.h"
#include "rom.h"

#define ROM_EXPORT_NAME_MAX 128
#define ROM_EXPORT_BASE_FILENAME "base.z64"

/* New projects retain their import ROM under this fixed name. Existing
 * projects can acquire it once when Create ROM is first used. */
BOOL RomExportHasProjectBase(const GEditorProject *project);
BOOL RomExportStoreProjectBase(const GEditorProject *project,
                               const RomFile *rom,
                               const char **reasonout);

/* Refresh cached display names from a matching base ROM with levelName.
 * Optional: old projects still open using their .gep names without a ROM. */
void RomExportRefreshProjectLevelMetadata(GEditorProject *project);

/* Shared by the dialog's live validation and the exporter itself. */
BOOL RomExportNameIsValid(const char *name, const char **reasonout);
BOOL RomExportBuildOutputPath(const char *directory, const char *name,
                              char *pathout, size_t pathmax,
                              const char **reasonout);
BOOL RomExportDestinationIsValid(const GEditorProject *project,
                                 const char *directory, const char *name,
                                 char *pathout, size_t pathmax,
                                 const char **reasonout);

/* Creates a playable .z64 from the retained base ROM. Every recognized
 * BG, setup (including multiplayer), and stan project file is copied
 * into its original ROM slot; level metadata and the N64 checksum are
 * updated before the output is written. */
BOOL RomExportCreate(const GEditorProject *project,
                     const char *name, const char *directory,
                     char *pathout, size_t pathmax,
                     const char **reasonout);


#endif /* GEDITOR_ROMEXPORT_H */
