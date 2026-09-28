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
    float emission[3], metalness; /* Black emission, nonmetallic by default. */
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

/* Fixed slots: one spotlight followed by two point lights. */
#define STUDIO_LIGHT_COUNT 3
typedef struct StudioLight {
    BOOL enabled;
    double position[3], direction[3];
    float color[3];
    double intensity, radius, inner, outer;
} StudioLight;

/* Permanent lighting has no position, radius, enabled flag, or Add/Delete slot. */
typedef struct StudioGlobalLight {
    float color[3];
    double intensity, direction[3]; /* direction is only used by the directional light. */
} StudioGlobalLight;
extern const StudioGlobalLight g_StudioDefaultAmbientLight, g_StudioDefaultDirectionalLight;
#define STUDIO_SELECT_AMBIENT (-5)
#define STUDIO_SELECT_DIRECTIONAL (-6)

typedef struct StudioScene {
    char project[MAX_PATH], filename[MAX_PATH];
    StudioInstance *objects;
    DWORD count;
    StudioModel *assets;
    StudioLight lights[STUDIO_LIGHT_COUNT];
    StudioGlobalLight ambient, directional;
} StudioScene;

/* Shared viewport/outliner selection IDs: models >= 0, none/root -1. */
#define STUDIO_LIGHT_SELECTION(slot) (-2-(slot))
static inline int StudioSceneLightIndex(const StudioScene *scene,int selection)
{
    if (!scene || selection>-2 || selection<STUDIO_LIGHT_SELECTION(STUDIO_LIGHT_COUNT-1)) { return -1; }
    int slot=-2-selection;
    return scene->lights[slot].enabled ? slot : -1;
}

/* Initialize lighting when constructing a scene directly; Load also supplies legacy defaults. */
void StudioSceneDefaultLighting(StudioScene *scene);
BOOL StudioGlobalLightValid(const StudioGlobalLight *light,BOOL directional);
void StudioSceneFree(StudioScene *scene);
BOOL StudioSceneLoad(const char *projectdir, const char *filename, StudioScene *scene, const char **why);
BOOL StudioSceneSave(const StudioScene *scene, const char **why);
BOOL StudioSceneAddModel(StudioScene *scene, const char *filename, const double position[3], const char **why);
void StudioSceneRemove(StudioScene *scene, DWORD index);
BOOL StudioTransformValid(const StudioTransform *transform);
BOOL StudioMaterialValid(const StudioMaterial *material);
BOOL StudioLightValid(const StudioLight *light, BOOL spotlight);
int StudioSceneLightSlot(const StudioScene *scene, BOOL spotlight);
int StudioSceneAddLight(StudioScene *scene, BOOL spotlight, const char **why);
BOOL StudioAssetFilename(const char *filename, const char *extension);

#endif
