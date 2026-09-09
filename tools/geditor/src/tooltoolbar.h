#ifndef GEDITOR_TOOLTOOLBAR_H
#define GEDITOR_TOOLTOOLBAR_H

#include "edittool.h"

/* The icons are 32px squares, with a 4px margin on each side. */
#define TOOLTOOLBAR_HEIGHT 40

BOOL ToolToolbarRegisterClass(HINSTANCE hinstance);
HWND ToolToolbarCreate(HWND parent, HINSTANCE hinstance);
void ToolToolbarSetTool(HWND toolbar, EditorTool tool);
void ToolToolbarSetVertexSnap(HWND toolbar, BOOL enabled);
/* Call from both frame message loops. Tool shortcuts leave text input
   alone; Tab/Space/Enter also support the toolbar's native buttons. */
BOOL ToolToolbarHandleMessage(HWND toolbar, MSG *message);

#endif
