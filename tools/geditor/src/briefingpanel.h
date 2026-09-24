#ifndef GEDITOR_BRIEFINGPANEL_H
#define GEDITOR_BRIEFINGPANEL_H
#include "briefing.h"
#define BRIEFING_WM_APPLY (WM_APP + 0x280)
#define BRIEFING_WM_DRAFT (WM_APP + 0x281)
typedef struct BriefingEditRequest { const BriefingDocument *document; const TextBank *bank; const char *why; } BriefingEditRequest;
HWND BriefingPanelCreate(HWND parent,HINSTANCE instance);
void BriefingPanelRefresh(HWND panel,const GEditorProject *project,DWORD level,const SetupFile *setup);
BOOL BriefingPanelApply(HWND panel);
BOOL BriefingPanelHasDraft(HWND panel);
#endif
