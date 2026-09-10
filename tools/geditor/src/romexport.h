#ifndef GEDITOR_ROMEXPORT_H
#define GEDITOR_ROMEXPORT_H

#include <windows.h>
#include <stddef.h>

#include "project.h"
#include "rom.h"

#define ROM_EXPORT_NAME_MAX 128
#define ROM_EXPORT_BASE_FILENAME "base.z64"

/* Every project retains its validated import ROM under this fixed name. */
BOOL RomExportStoreProjectBase(const GEditorProject *project,
                               const RomFile *rom,
                               const char **reasonout);

/* Validate the required base ROM and refresh names/environment metadata. */
BOOL RomExportRefreshProjectLevelMetadata(GEditorProject *project, const char **reasonout);

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
 * updated before the output is written. Saved imported images are appended
 * to IMGS with their formats, mipmaps, hit sounds and bullet-hole settings. */
BOOL RomExportCreate(const GEditorProject *project,
                     const char *name, const char *directory,
                     char *pathout, size_t pathmax,
                     const char **reasonout);


#endif /* GEDITOR_ROMEXPORT_H */
