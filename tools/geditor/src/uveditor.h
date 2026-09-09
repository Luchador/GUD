#ifndef GEDITOR_UVEDITOR_H
#define GEDITOR_UVEDITOR_H

#include <windows.h>

/* Open the modeless owned window, or activate it if it is already open.
   Closing it leaves GEditor running; closing its owner destroys it too. */
BOOL UVEditorShow(HWND owner, HINSTANCE instance);

/* Route wheel input by hover position, and this window's other messages
   before the main editor's keyboard shortcuts.
   Returns TRUE when the message has been handled or dispatched here. */
BOOL UVEditorHandleMessage(MSG *message);

#endif /* GEDITOR_UVEDITOR_H */
