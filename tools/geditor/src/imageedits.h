#ifndef GEDITOR_IMAGEEDITS_H
#define GEDITOR_IMAGEEDITS_H
#include "texencode.h"

void ImageEditsReset(void);
BOOL ImageEditsHasUnsaved(void);
BOOL ImageEditsNextId(const char *projectdir, DWORD *id, const char **reasonout);
BOOL ImageEditsImport(const char *projectdir, const TexPixel *pixels, int width, int height,
    const TexImportOptions *options, DWORD *id, const char **reasonout);
BOOL ImageEditsCanEdit(const char *projectdir, DWORD id, const char **reasonout);
BOOL ImageEditsReplace(const char *projectdir, DWORD id, const TexPixel *pixels, int width, int height,
    const TexImportOptions *options, const char **reasonout);
BOOL ImageEditsDelete(const char *projectdir, DWORD id, const char **reasonout);
BOOL ImageEditsGetDeletedPixels(const char *projectdir, DWORD id, TexPixel *out, int *width, int *height);
BOOL ImageEditsSave(const char *projectdir, const char **reasonout);
BOOL ImageEditsExportToRom(const char *projectdir, RomFile *rom, const char **reasonout);
/* Pending imports participate in every existing project-image consumer. */
BOOL ImageEditsGetPixels(const char *projectdir, DWORD id, TexPixel *out, int *width, int *height);
void ImageEditsUpdateThumbnails(const char *projectdir, TexThumb **items, unsigned char **pixels, DWORD *count);
#endif
