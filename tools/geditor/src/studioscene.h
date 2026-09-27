#ifndef GEDITOR_STUDIOSCENE_H
#define GEDITOR_STUDIOSCENE_H
#include <windows.h>
#include "studioassets.h"
#include "gltf.h"

#define STUDIO_SCENE_NAME_MAX 128 /* Includes the terminator, excludes .rnd. */
typedef StudioFileEntry StudioSceneEntry;

/* Create a versioned empty scene, publishing only a complete file and never
 * replacing an existing scene. filename receives its leaf name, including .rnd. */
BOOL StudioSceneCreate(const char *projectdir, const char *name,
    char filename[MAX_PATH], const char **why);
/* List .rnd files (not directories) without modifying or loading their content.
 * The caller frees entries. Failed reads return no partial list. */
BOOL StudioSceneList(const char *projectdir, StudioSceneEntry **entries, DWORD *count, const char **why);

typedef struct StudioMaterial {
    char name[128], image[MAX_PATH];
    float base[3], specular[3], intensity, shininess;
} StudioMaterial;

typedef struct StudioModel {
    char filename[MAX_PATH];
    GltfModelImport mesh;
    float (*basecolors)[4];
    double lower[3], upper[3];
    struct StudioModel *next;
} StudioModel;

typedef struct StudioTransform {
    double position[3], rotation[3], scale[3];
} StudioTransform;

typedef struct StudioInstance {
    char model[MAX_PATH];
    StudioTransform transform;
    StudioModel *asset; /* Shared immutable geometry, owned by scene.assets. */
    StudioMaterial *materials; /* Independent overrides for this instance. */
    DWORD materialcount;
} StudioInstance;

typedef struct StudioScene {
    char project[MAX_PATH], filename[MAX_PATH];
    StudioInstance *objects;
    DWORD count;
    StudioModel *assets;
} StudioScene;

void StudioSceneFree(StudioScene *scene);
BOOL StudioSceneLoad(const char *projectdir, const char *filename, StudioScene *scene, const char **why);
BOOL StudioSceneSave(const StudioScene *scene, const char **why);
BOOL StudioSceneAddModel(StudioScene *scene, const char *filename, const double position[3], const char **why);
void StudioSceneRemove(StudioScene *scene, DWORD index);
BOOL StudioTransformValid(const StudioTransform *transform);
BOOL StudioMaterialValid(const StudioMaterial *material);
BOOL StudioAssetFilename(const char *filename, const char *extension);

#endif
