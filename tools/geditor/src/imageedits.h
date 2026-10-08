#ifndef GEDITOR_IMAGEEDITS_H
#define GEDITOR_IMAGEEDITS_H
#include "texencode.h"
#include "projectrebase.h"

void ImageEditsReset(void);
BOOL ImageEditsHasUnsaved(void);
BOOL ImageEditsNextId(const char *projectdir, DWORD *id, const char **reasonout);
/* sourcepath is the original BMP path, or NULL for images without a source. */
BOOL ImageEditsImport(const char *projectdir, const TexPixel *pixels, int width, int height,
    const TexImportOptions *options, const char *sourcepath, DWORD *id, const char **reasonout);
BOOL ImageEditsCanEdit(const char *projectdir, DWORD id, const char **reasonout);
BOOL ImageEditsReplace(const char *projectdir, DWORD id, const TexPixel *pixels, int width, int height,
    const TexImportOptions *options, const char *sourcepath, const char **reasonout);
/* sourceout receives the remembered path (when available), including on failure.
 * Conversion must succeed before replacing any saved or pending image. */
BOOL ImageEditsReimport(const char *projectdir, DWORD id, char sourceout[MAX_PATH], const char **reasonout);
BOOL ImageEditsDelete(const char *projectdir, DWORD id, const char **reasonout);
/* Flip current pixels at the same ID, retaining format, mips, surface settings
 * and remembered source. Changes are pending until Save Project. */
BOOL ImageEditsFlip(const char *projectdir, DWORD id, BOOL horizontal, const char **reasonout);
/* Change one HIT_TYPE (0..12), preserving the other, pixels, native mip chain,
 * palette, detail flags and remembered source. TRUE selects the sound field;
 * FALSE selects bullet holes. Pending until Save Project; same value is a no-op. */
BOOL ImageEditsSetSurface(const char *projectdir, DWORD id, BOOL sound,
    unsigned int type, BOOL *changed, const char **reasonout);
BOOL ImageEditsGetDeletedPixels(const char *projectdir, DWORD id, TexPixel *out, int *width, int *height);
BOOL ImageEditsSave(const char *projectdir, const char **reasonout);
BOOL ImageEditsExportToRom(const char *projectdir, RomFile *rom, const char **reasonout);
/* Rebase already-validated base banks without changing image IDs.
 * FALSE checks only; TRUE updates metadata/previews in a private staging copy.
 * The caller must discard that copy on failure, never pass the live project.
 * Report may be NULL when applying a previously reported choice. */
BOOL ImageEditsRebase(const char *projectdir, const RomFile *oldrom,
    const RomFile *newrom, ProjectRebaseChoice choice, ProjectRebaseReport *report,
    BOOL write, const char **reasonout);
/* Pending imports participate in every existing project-image consumer. */
BOOL ImageEditsGetPixels(const char *projectdir, DWORD id, TexPixel *out, int *width, int *height);
void ImageEditsUpdateThumbnails(const char *projectdir, TexThumb **items, unsigned char **pixels, DWORD *count);
#endif
