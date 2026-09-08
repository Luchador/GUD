#ifndef GEDITOR_RESOURCE_H
#define GEDITOR_RESOURCE_H

/* Dialog templates */
#define IDD_NEW_PROJECT       101
#define IDD_CREATE_ROM        102

/* Icons */
#define IDI_GEDITOR           201

/* Viewport tool buttons (embedded PNGs). */
#define IDR_VERTEX_SELECT_OFF 301
#define IDR_VERTEX_SELECT_ON  302
#define IDR_EDGE_SELECT_OFF   303
#define IDR_EDGE_SELECT_ON    304
#define IDR_FACE_SELECT_OFF   305
#define IDR_FACE_SELECT_ON    306
#define IDR_VERTEX_PAINT_OFF  307
#define IDR_VERTEX_PAINT_ON   308
#define IDR_GIZMO_ARROW       309
#define IDR_NO_TEXTURE        310

/* Controls inside IDD_NEW_PROJECT */
#define IDC_PROJECT_NAME     1001
#define IDC_NAME_WARNING     1002
#define IDC_CREATE_PROJECT   1003
#define IDC_PROJECT_LOCATION 1004
#define IDC_BROWSE_LOCATION  1005
#define IDC_PROJECT_ROM      1006
#define IDC_BROWSE_ROM       1007

/* Controls inside IDD_CREATE_ROM */
#define IDC_ROM_NAME          1010
#define IDC_ROM_OUTPUT_DIR    1011
#define IDC_BROWSE_ROM_OUTPUT 1012
#define IDC_ROM_WARNING       1013
#define IDC_CREATE_ROM        1014

/* Longest project name we accept, including the terminator. */
#define GEDITOR_NAME_MAX       64

#endif /* GEDITOR_RESOURCE_H */
