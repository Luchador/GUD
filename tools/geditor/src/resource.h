#ifndef GEDITOR_RESOURCE_H
#define GEDITOR_RESOURCE_H

/* Dialog templates */
#define IDD_NEW_PROJECT       101
#define IDD_CREATE_ROM        102
#define IDD_UV_EDITOR         103
#define IDD_EXIT_UNSAVED      104
#define IDD_MODEL_EDITOR      105
#define IDD_IMPORT_IMAGE      106
#define IDD_ADD_CIRCLE        107
#define IDD_ADD_CYLINDER      108

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
#define IDR_GIZMO_CYLINDER    311
#define IDR_VERTEX_SNAP_OFF   312
#define IDR_VERTEX_SNAP_ON    313
#define IDR_GIZMO_SCALE       314

/* Object palette icons (embedded PNGs). */
#define IDR_OBJECT_TRIANGLE     315
#define IDR_OBJECT_QUAD         316
#define IDR_OBJECT_SPAWN        317
#define IDR_OBJECT_INTRO_SPLINE 318
#define IDR_OBJECT_INTRO        319
#define IDR_OBJECT_OUTRO        320
#define IDR_OBJECT_DOOR         321
#define IDR_OBJECT_GLASS        322
#define IDR_OBJECT_WEAPON       323
#define IDR_OBJECT_AMMO         324
#define IDR_OBJECT_CCTV         325
#define IDR_OBJECT_ALARM        326
#define IDR_OBJECT_DRONE_GUN    331
#define IDR_OBJECT_TANK         332
#define IDR_OBJECT_PORTAL       333
#define IDR_OBJECT_KEY          334
#define IDR_OBJECT_SAFE         335
#define IDR_OBJECT_CIRCLE       336
#define IDR_OBJECT_CYLINDER     337
#define IDR_OBJECT_ARMOR        338
#define IDR_OBJECT_MONITOR      339

/* Viewport-only setup marker models. */
#define IDR_MARKER_START          327
#define IDR_MARKER_INTRO_CAMERA   328
#define IDR_MARKER_OUTRO_CAMERA   329
#define IDR_MARKER_INTRO_SPLINE   330

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

/* UV Editor toolbar and transform panel. */
#define IDC_UV_MOVE           1020
#define IDC_UV_TOOL_HINT      1021
#define IDC_UV_TRANSFORM      1022
#define IDC_UV_SELECTION      1023
#define IDC_UV_U_LABEL        1024
#define IDC_UV_V_LABEL        1025
#define IDC_UV_U              1026
#define IDC_UV_V              1027
#define IDC_UV_HINT           1028
#define IDC_UV_PROJECT_LABEL  1029
#define IDC_UV_PROJECT_X      1030
#define IDC_UV_PROJECT_Y      1031
#define IDC_UV_PROJECT_Z      1032
#define IDC_UV_PROJECT_BEST   1033
#define IDC_UV_ROTATE         1034
#define IDC_UV_SCALE          1035

/* Model Editor selectors and footer. */
#define IDC_MODEL_CHARACTERS       1040
#define IDC_MODEL_ITEMS            1041
#define IDC_MODEL_PROPS            1042
#define IDC_MODEL_CHARACTERS_LABEL 1043
#define IDC_MODEL_ITEMS_LABEL      1044
#define IDC_MODEL_PROPS_LABEL      1045
#define IDC_MODEL_STATUS           1046
#define IDC_MODEL_EXPORT           1047
#define IDC_MODEL_IMPORT           1048

/* Image import settings. The thirteen texture types form one radio group. */
#define IDC_IMAGE_SUMMARY     1100
#define IDC_IMAGE_FORMAT_FIRST 1110
#define IDC_IMAGE_FORMAT_LAST  1122
#define IDC_IMAGE_MIPMAPS     1130
#define IDC_IMAGE_SOUND       1131
#define IDC_IMAGE_BULLET      1132
#define IDC_IMAGE_TMEM        1133
#define IDC_IMAGE_WARNING     1134
#define IDC_IMAGE_IMPORT      1135

/* Circle/cylinder creation settings. */
#define IDC_PRIMITIVE_RADIUS  1140
#define IDC_PRIMITIVE_SIDES   1141
#define IDC_PRIMITIVE_HEIGHT  1142

/* Longest project name we accept, including the terminator. */
#define GEDITOR_NAME_MAX       64

#endif /* GEDITOR_RESOURCE_H */
