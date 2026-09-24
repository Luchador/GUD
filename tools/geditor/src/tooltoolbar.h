#ifndef GEDITOR_TOOLTOOLBAR_H
#define GEDITOR_TOOLTOOLBAR_H

#include "edittool.h"

/* The icons are 32px squares, with a 4px margin on each side. */
#define TOOLTOOLBAR_HEIGHT 40

typedef enum ToolToolbarMenu {
    TOOLTOOLBAR_MENU_VERTEX,
    TOOLTOOLBAR_MENU_EDGE,
    TOOLTOOLBAR_MENU_FACE,
    TOOLTOOLBAR_MENU_COUNT
} ToolToolbarMenu;
/* Synchronous request: wparam is the menu, lparam its anchor button HWND. */
#define TOOLTOOLBAR_WM_MENU (WM_APP + 53)

/* Wrap controls when the viewport is narrowed by its side panels. */
int ToolToolbarGetHeight(int width);
BOOL ToolToolbarRegisterClass(HINSTANCE hinstance);
HWND ToolToolbarCreate(HWND parent, HINSTANCE hinstance);
/* Model Editor's single paint toggle; shares the main toolbar's icons. */
HWND ToolToolbarCreatePaint(HWND parent, HINSTANCE hinstance);
void ToolToolbarSetTool(HWND toolbar, EditorTool tool);
BOOL ToolToolbarCorrectFaceAttributes(HWND toolbar);
/* Call from both frame message loops. Tool shortcuts leave text input
   alone; Tab/Space/Enter also support the toolbar's native buttons. */
BOOL ToolToolbarHandleMessage(HWND toolbar, MSG *message);

#endif
