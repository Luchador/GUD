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
    const DWORD *faces, DWORD count, int culling, int surface, int wrapu, int wrapv, const char **reasonout);
BOOL ModelEditsSetMaterial(const char *project, const char *name, DWORD revision,
    DWORD slot, DWORD texture, const char **reasonout);
typedef struct ModelVertexPaint {
    DWORD offset, beforeRevision, afterRevision;
    unsigned char before[4], after[4]; /* Native bytes, including unrounded alpha. */
} ModelVertexPaint;
BOOL ModelEditsSetVertexColor(const char *project, const char *name, DWORD revision,
    DWORD corner, const unsigned char rgba[4], ModelVertexPaint *change, const char **reasonout);
BOOL ModelEditsRestoreVertexColor(const char *project, const char *name,
    const ModelVertexPaint *change, BOOL redo, const char **reasonout);
typedef struct ModelUVEdit {
    DWORD corner; /* face * 3 + corner, in ModelSource order */
    float uv[2];  /* normalized authored coordinates */
} ModelUVEdit;
typedef struct ModelUVChange {
    unsigned char *before, *after; /* Owned exact snapshots, including material UVs. */
    DWORD beforeSize, afterSize, beforeRevision, afterRevision;
} ModelUVChange;
/* Atomic corner edits. Conflicting shared UVs split native vertices; all other
 * attributes, unselected faces, model parts and material assignments survive. */
BOOL ModelEditsSetUVs(const char *project, const char *name, DWORD revision,
    const ModelUVEdit *edits, DWORD count, ModelUVChange *change, const char **reasonout);
BOOL ModelEditsRestoreUVs(const char *project, const char *name,
    const ModelUVChange *change, BOOL redo, const char **reasonout);
void ModelEditsFreeUVChange(ModelUVChange *change);
BOOL ModelEditsSave(const char *projectdir, const char **reasonout);
/* Returns 1 for a replacement, 0 if absent, -1 on a corrupt/mismatched edit.
   ROM builds read saved overrides only; save-before-build is owned by GEditor. */
int ModelEditsReadReplacement(const char *projectdir, const char *name,
    const unsigned char *base, DWORD basesize, unsigned char **data, DWORD *size,
    const char **reasonout);
#endif
