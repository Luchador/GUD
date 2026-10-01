#ifndef GEDITOR_SCENEOUTLINER_H
#define GEDITOR_SCENEOUTLINER_H

#include <windows.h>
#include "setupload.h"
#include "bgload.h"

typedef enum SceneOutlinerKind {
    SCENE_OUTLINER_NONE, SCENE_OUTLINER_CHARACTER,
    SCENE_OUTLINER_OBJECT, SCENE_OUTLINER_PORTAL
} SceneOutlinerKind;

/* Synchronous request; index is the setup array index or native portal ID.
 * Character labels use chrnum, which is not their selection index. */
#define SCENEOUTLINER_WM_SELECT (WM_APP + 120)
typedef struct SceneOutlinerSelection {
    SceneOutlinerKind kind;
    DWORD index;
    BOOL frame;
} SceneOutlinerSelection;

BOOL SceneOutlinerRegisterClass(HINSTANCE instance);
HWND SceneOutlinerCreate(HWND parent, HINSTANCE instance);
void SceneOutlinerRefresh(HWND outliner, const SetupFile *setup, const BgPortalFile *portals);
void SceneOutlinerSelect(HWND outliner, SceneOutlinerKind kind, DWORD index);

#endif
