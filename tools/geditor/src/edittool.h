#ifndef GEDITOR_EDITTOOL_H
#define GEDITOR_EDITTOOL_H

#include <windows.h>

/* Shared by the toolbar, frame, and viewport. */
typedef enum TransformMode {
    TRANSFORM_MOVE, TRANSFORM_ROTATE, TRANSFORM_SCALE
} TransformMode;

typedef enum EditorTool {
    EDITOR_TOOL_VERTEX_SELECT,
    EDITOR_TOOL_EDGE_SELECT,
    EDITOR_TOOL_FACE_SELECT,
    EDITOR_TOOL_ROOM_SELECT,
    EDITOR_TOOL_VERTEX_PAINT,
    EDITOR_TOOL_COUNT
} EditorTool;

/* Request a tool change from the frame; wparam is an EditorTool. */
#define EDITTOOL_WM_SELECT (WM_APP + 6)
/* Toggle snapping within vertex selection; no separate toolbar mode. */
#define EDITTOOL_WM_TOGGLE_VERTEX_SNAP (WM_APP + 21)

#endif
