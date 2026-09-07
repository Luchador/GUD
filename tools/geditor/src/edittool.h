#ifndef GEDITOR_EDITTOOL_H
#define GEDITOR_EDITTOOL_H

#include <windows.h>

/* Shared by the toolbar, frame, and viewport. Vertex and edge selection
   reserve input modes for future editing tools. */
typedef enum EditorTool {
    EDITOR_TOOL_VERTEX_SELECT,
    EDITOR_TOOL_EDGE_SELECT,
    EDITOR_TOOL_FACE_SELECT,
    EDITOR_TOOL_VERTEX_PAINT,
    EDITOR_TOOL_COUNT
} EditorTool;

/* Request a tool change from the frame; wparam is an EditorTool. */
#define EDITTOOL_WM_SELECT (WM_APP + 6)

#endif
