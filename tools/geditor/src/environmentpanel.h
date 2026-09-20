#ifndef GEDITOR_ENVIRONMENTPANEL_H
#define GEDITOR_ENVIRONMENTPANEL_H
#include "project.h"

#define ENVIRONMENT_WM_APPLY (WM_APP + 0x260)
#define ENVIRONMENT_WM_PREVIEW (WM_APP + 0x261)
#define ENVIRONMENT_WM_DRAFT (WM_APP + 0x262)
typedef struct EnvironmentEditRequest {
    EditorEnvironment value;
    const char *why;
} EnvironmentEditRequest;

HWND EnvironmentPanelCreate(HWND parent, HINSTANCE instance);
void EnvironmentPanelRefresh(HWND panel, const GEditorProject *project, DWORD level);
void EnvironmentPanelShow(HWND panel, BOOL visible);
BOOL EnvironmentPanelApply(HWND panel);
BOOL EnvironmentPanelHasDraft(HWND panel);
#endif
