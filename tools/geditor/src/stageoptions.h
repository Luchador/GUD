#ifndef GEDITOR_STAGEOPTIONS_H
#define GEDITOR_STAGEOPTIONS_H
#include "project.h"

#define STAGEOPTIONS_WM_APPLY (WM_APP + 0x270)
#define STAGEOPTIONS_WM_DRAFT (WM_APP + 0x271)
typedef struct StageOptionsEditRequest {
    LevelMemory value;
    const char *why;
    float levelscale, renderScale;
    BOOL defaults;
} StageOptionsEditRequest;
HWND StageOptionsCreate(HWND parent, HINSTANCE instance);
void StageOptionsRefresh(HWND panel, const GEditorProject *project, DWORD level);
BOOL StageOptionsApply(HWND panel);
BOOL StageOptionsHasDraft(HWND panel);
#endif
