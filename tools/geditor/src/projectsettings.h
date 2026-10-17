#ifndef GEDITOR_PROJECTSETTINGS_H
#define GEDITOR_PROJECTSETTINGS_H

#include <windows.h>
#include "project.h"
#define PROJECTSETTINGS_WM_LEVEL_SLOT (WM_APP + 0x2f0)
#define PROJECTSETTINGS_WM_LEVEL_ASSETS (WM_APP + 0x2f1)
#define PROJECTSETTINGS_WM_DRAFT (WM_APP + 0x2f2)
typedef struct ProjectLevelAssetEdit { unsigned slot; LevelMenuAsset value; const char *why; } ProjectLevelAssetEdit;
BOOL ProjectSettingsApply(void);
BOOL ProjectSettingsHasDraft(void);
typedef struct ProjectLevelSlotEdit { unsigned slot; int stage; const char *why; } ProjectLevelSlotEdit;

BOOL ProjectSettingsShow(HWND owner, HINSTANCE instance, const GEditorProject *project);
void ProjectSettingsRefresh(const GEditorProject *project);
BOOL ProjectSettingsHandleMessage(MSG *message);

#endif
