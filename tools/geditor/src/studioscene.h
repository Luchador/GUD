#ifndef GEDITOR_STUDIOSCENE_H
#define GEDITOR_STUDIOSCENE_H
#include <windows.h>

#define STUDIO_SCENE_NAME_MAX 128 /* Includes the terminator, excludes .rnd. */
typedef struct StudioSceneEntry { char filename[MAX_PATH]; } StudioSceneEntry;

/* Create a versioned empty scene, publishing only a complete file and never
 * replacing an existing scene. filename receives its leaf name, including .rnd. */
BOOL StudioSceneCreate(const char *projectdir, const char *name,
    char filename[MAX_PATH], const char **why);
/* List .rnd files (not directories) without modifying or loading their content.
 * The caller frees entries. Failed reads return no partial list. */
BOOL StudioSceneList(const char *projectdir, StudioSceneEntry **entries, DWORD *count, const char **why);

#endif
