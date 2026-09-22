#ifndef GEDITOR_UVEDITOR_H
#define GEDITOR_UVEDITOR_H

#include <windows.h>
#include "bgdocument.h"
#include "uvcanvas.h"

/* Open the modeless owned window, or activate it if it is already open.
   Closing it leaves GEditor running; closing its owner destroys it too. */
BOOL UVEditorShow(HWND owner, HINSTANCE instance);

/* Snapshot the selected BG faces' authored UVs while in face mode and load
   their shared image from the project (including pending replacements).
   No document/viewport pointers are retained; a closed window is a no-op. */
void UVEditorRefreshSelection(HWND viewport, const BgDocument *document, const char *projectdir);
/* Model adapter: takes ownership of triangles. Only the current owner can
 * replace the overlay; background selection refreshes cannot overwrite it. */
void UVEditorSetOverlay(HWND owner, UVCanvasTriangle *triangles, int count,
    const char *projectdir, unsigned short texture, BOOL shared, const char *title);

/* Synchronous owner requests. APPLY lparam points to a UVCanvasEdit;
   HISTORY wparam is TRUE for redo. */
#define UVEDITOR_WM_APPLY (WM_APP + 42)
#define UVEDITOR_WM_HISTORY (WM_APP + 43)
#define UVEDITOR_WM_SELECTION_CHANGED (WM_APP + 44)
/* APPLY_FACES lparam points to a UVCanvasFaceEdit, with per-corner UVs. */
#define UVEDITOR_WM_APPLY_FACES (WM_APP + 61)
#define UVEDITOR_WM_VISIBILITY (WM_APP + 79) /* wparam: show seam guides */
BOOL UVEditorIsOpen(HWND owner);
BOOL UVEditorCaptureSelection(HWND owner, void **data, size_t *size);
BOOL UVEditorRestoreSelection(HWND owner, const void *data, size_t size);
void UVEditorCancelInteraction(HWND owner);

/* Route wheel input by hover position, and this window's other messages
   before the main editor's keyboard shortcuts.
   Returns TRUE when the message has been handled or dispatched here. */
BOOL UVEditorHandleMessage(MSG *message);

#endif /* GEDITOR_UVEDITOR_H */
