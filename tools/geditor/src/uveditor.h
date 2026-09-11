#ifndef GEDITOR_UVEDITOR_H
#define GEDITOR_UVEDITOR_H

#include <windows.h>
#include "bgdocument.h"

/* Open the modeless owned window, or activate it if it is already open.
   Closing it leaves GEditor running; closing its owner destroys it too. */
BOOL UVEditorShow(HWND owner, HINSTANCE instance);

/* Snapshot the selected BG faces' authored UVs while in face mode. No
   document/viewport pointers are retained, and a closed window is a no-op. */
void UVEditorRefreshSelection(HWND viewport, const BgDocument *document);

/* Synchronous owner requests. APPLY lparam points to a UVCanvasEdit;
   HISTORY wparam is TRUE for redo. */
#define UVEDITOR_WM_APPLY (WM_APP + 42)
#define UVEDITOR_WM_HISTORY (WM_APP + 43)
#define UVEDITOR_WM_SELECTION_CHANGED (WM_APP + 44)
BOOL UVEditorIsOpen(void);
BOOL UVEditorCaptureSelection(void **data, size_t *size);
BOOL UVEditorRestoreSelection(const void *data, size_t size);
void UVEditorCancelInteraction(void);

/* Route wheel input by hover position, and this window's other messages
   before the main editor's keyboard shortcuts.
   Returns TRUE when the message has been handled or dispatched here. */
BOOL UVEditorHandleMessage(MSG *message);

#endif /* GEDITOR_UVEDITOR_H */
