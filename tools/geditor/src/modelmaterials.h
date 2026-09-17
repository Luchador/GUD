#ifndef GEDITOR_MODELMATERIALS_H
#define GEDITOR_MODELMATERIALS_H
#include <windows.h>

/* Editor-only material slots. UVs remain normalized even with No Texture.
 * Stored with the native override, but removed when building a ROM. */
typedef struct ModelMaterialSlot {
    char name[128];
    DWORD texture;
} ModelMaterialSlot;
typedef struct ModelMaterialFace {
    DWORD slot;
    float uv[6];
} ModelMaterialFace;
typedef struct ModelMaterials {
    ModelMaterialSlot *slots;
    ModelMaterialFace *faces;
    DWORD count, facecount;
} ModelMaterials;
struct ModelSource;
void ModelMaterialsFree(ModelMaterials *materials);
BOOL ModelMaterialsCopy(ModelMaterials *out, const ModelMaterials *in, const char **why);
BOOL ModelMaterialsEnsure(struct ModelSource *source, const char *project, const char **why);
/* Reimports retain assignments only for unambiguous matching slot names. */
void ModelMaterialsMatch(ModelMaterials *materials, const ModelMaterials *previous);
BOOL ModelMaterialsRead(const unsigned char *data, DWORD size, DWORD faces,
    ModelMaterials *materials, const char **why);
BOOL ModelMaterialsAttach(unsigned char **data, DWORD *size,
    const ModelMaterials *materials, const char **why);
/* Native byte length excludes the editor trailer, including its alignment. */
DWORD ModelMaterialsNativeSize(const unsigned char *data, DWORD size);
#endif
