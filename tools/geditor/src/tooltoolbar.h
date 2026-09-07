#ifndef GEDITOR_TOOLTOOLBAR_H
#define GEDITOR_TOOLTOOLBAR_H

#include "edittool.h"

/* The supplied artwork is 48px square, with a 4px margin on each side. */
#define TOOLTOOLBAR_HEIGHT 56

BOOL ToolToolbarRegisterClass(HINSTANCE hinstance);
HWND ToolToolbarCreate(HWND parent, HINSTANCE hinstance);
void ToolToolbarSetTool(HWND toolbar, EditorTool tool);
/* Call from both frame message loops. Number shortcuts leave text input
   alone; Tab/Space/Enter also support the toolbar's native buttons. */
BOOL ToolToolbarHandleMessage(HWND toolbar, MSG *message);

#endif
