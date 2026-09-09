#ifndef GEDITOR_EDITTOOL_H
#define GEDITOR_EDITTOOL_H

#include <windows.h>

/* Shared by the toolbar, frame, and viewport. */
typedef enum EditorTool {
    EDITOR_TOOL_VERTEX_SELECT,
    EDITOR_TOOL_EDGE_SELECT,
    EDITOR_TOOL_FACE_SELECT,
    EDITOR_TOOL_VERTEX_PAINT,
    EDITOR_TOOL_COUNT
} EditorTool;

/* Request a tool change from the frame; wparam is an EditorTool. */
#define EDITTOOL_WM_SELECT (WM_APP + 6)
/* Snapping is a toggle within vertex selection, not a separate selection tool. */
#define EDITTOOL_WM_TOGGLE_VERTEX_SNAP (WM_APP + 21)

#endif
