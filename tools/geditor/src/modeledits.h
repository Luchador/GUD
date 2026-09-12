#ifndef GEDITOR_MODELEDITS_H
#define GEDITOR_MODELEDITS_H
#include "rom.h"
#include "modelload.h"

void ModelEditsReset(void);
BOOL ModelEditsHasUnsaved(void);
/* NULL with an empty reason means there is no native override. Borrowed data
   remains owned by this module until replacement or project close. */
const unsigned char *ModelEditsGetData(const char *projectdir, const char *name,
                                      DWORD *size, const char **reasonout);
BOOL ModelEditsExport(const char *projectdir, const char *name, const char *path,
                       const char **reasonout);
BOOL ModelEditsImport(const char *projectdir, const char *name, const char *path,
                       DWORD *before, DWORD *after, const char **reasonout);
BOOL ModelEditsReadSource(const char *project, const char *name, ModelSource *source,
    DWORD *revision, const char **reasonout);
BOOL ModelEditsSetProperties(const char *project, const char *name, DWORD revision,
    const DWORD *faces, DWORD count, int culling, int surface, const char **reasonout);
BOOL ModelEditsSave(const char *projectdir, const char **reasonout);
/* Returns 1 for a replacement, 0 if absent, -1 on a corrupt/mismatched edit.
   ROM builds read saved overrides only; save-before-build is owned by GEditor. */
int ModelEditsReadReplacement(const char *projectdir, const char *name,
    const unsigned char *base, DWORD basesize, unsigned char **data, DWORD *size,
    const char **reasonout);
#endif
