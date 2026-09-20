#ifndef GEDITOR_PATROLEDITOR_H
#define GEDITOR_PATROLEDITOR_H
#include "patrolpaths.h"
/* Synchronous PatrolDocument payload. The frame owns the current setup and history. */
#define PATROLEDITOR_WM_APPLY (WM_APP + 87)
#define PATROLEDITOR_WM_SAVE (WM_APP + 88)
BOOL PatrolEditorShow(HWND owner, HWND viewport, const SetupFile *source,
    DWORD selectedpad, const char **why);
BOOL PatrolEditorApply(void);
/* Save when replacing the current level/project; ordinary Close only applies. */
BOOL PatrolEditorConfirmClose(BOOL save);
void PatrolEditorClose(void);
void PatrolEditorRefresh(void);
void PatrolEditorSetPicking(BOOL enabled);
BOOL PatrolEditorPickPad(const SetupPadRef *pad);
BOOL PatrolEditorHandleMessage(MSG *message);
#endif
