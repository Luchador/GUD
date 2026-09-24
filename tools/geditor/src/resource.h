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
#define IDD_REBASE_PROJECT    109
#define IDD_ADD_PORTAL        110
#define IDD_ACTION_BLOCKS     111
#define IDD_KNIFE             112
#define IDD_ADD_PROP_MODEL    113
#define IDD_LEVEL_MANAGER     114
#define IDD_INTRO_EQUIPMENT   115
#define IDD_PROJECT_SETTINGS  116
#define IDD_PATROL_PATHS      117
#define IDD_CHECK_ISSUES      118
#define IDD_ENVIRONMENT       119
#define IDD_STAGE_OPTIONS     120
#define IDD_TEXT_EDITOR       121
#define IDD_BRIEFING          122
#define IDD_BRIEFING_CONDITION 123
#define IDC_PROJECT_SETTINGS_TABS 1270
#define IDC_PROJECT_SFX_FILTER_LABEL 1271
#define IDC_PROJECT_SFX_FILTER 1272
#define IDC_PROJECT_SFX_LIST   1273
#define IDC_PROJECT_SFX_STATUS 1274
#define IDC_PROJECT_SFX_PLAY   1275
#define IDC_PROJECT_SFX_STOP   1276
#define IDC_ISSUES_LIST       1260
#define IDC_ISSUES_STATUS     1261
#define IDC_ISSUES_DETAILS    1262
#define IDC_ISSUES_CHECK      1263
#define IDC_ISSUES_SCOPE      1264
#define IDC_ISSUES_SCOPE_LABEL 1265

#define IDC_PATROL_PATHS      1240
#define IDC_PATROL_NEW        1241
#define IDC_PATROL_DELETE     1242
#define IDC_PATROL_POINTS     1243
#define IDC_PATROL_PAD        1244
#define IDC_PATROL_ADD        1245
#define IDC_PATROL_INSERT     1246
#define IDC_PATROL_REMOVE     1247
#define IDC_PATROL_UP         1248
#define IDC_PATROL_DOWN       1249
#define IDC_PATROL_LOOP       1250
#define IDC_PATROL_HELP       1251
#define IDC_PATROL_PICK       1252

/* Level Settings tabs. */
#define IDC_LEVEL_MANAGER_TABS 1210
#define IDC_INTRO_STATUS       1211
#define IDC_INTRO_WEAPONS_LABEL 1212
#define IDC_INTRO_WEAPONS_HELP 1213
#define IDC_INTRO_WEAPONS      1214
#define IDC_INTRO_WEAPON_ADD   1215
#define IDC_INTRO_WEAPON_EDIT  1216
#define IDC_INTRO_WEAPON_REMOVE 1217
#define IDC_INTRO_WEAPON_UP    1218
#define IDC_INTRO_WEAPON_DOWN  1219
#define IDC_INTRO_AMMO_LABEL   1220
#define IDC_INTRO_AMMO_HELP    1221
#define IDC_INTRO_AMMO         1222
#define IDC_INTRO_AMMO_ADD     1223
#define IDC_INTRO_AMMO_EDIT    1224
#define IDC_INTRO_AMMO_REMOVE  1225
#define IDC_INTRO_AMMO_UP      1226
#define IDC_INTRO_AMMO_DOWN    1227
#define IDC_INTRO_FIRST_LABEL  1230
#define IDC_INTRO_FIRST        1231
#define IDC_INTRO_SECOND_LABEL 1232
#define IDC_INTRO_SECOND       1233
#define IDC_INTRO_QUANTITY     1234
#define IDC_LEVEL_ROOMS_STATUS 1235
#define IDC_LEVEL_ROOMS_LIST   1236
#define IDC_LEVEL_ROOMS_TOTALS 1237

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
#define IDR_STARTUP_BOX           340
#define IDR_VERTEX_COLOR_BRUSH    341
#define IDR_OBJECT_PAD            342
#define IDR_EYEDROPPER            343
#define IDR_OBJECT_OCCLUDER       344

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
#define IDC_UV_PROJECT_CYLINDER 1036
#define IDC_UV_ROTATE         1034
#define IDC_UV_SCALE          1035
#define IDC_UV_TEXTURE_LABEL  1037
#define IDC_UV_TEXTURE_STATUS 1038
#define IDC_UV_TEXTURE_OPACITY_LABEL 1039
#define IDC_UV_TEXTURE_OPACITY 1099

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
#define IDC_MODEL_ADD              1049
#define IDC_NEW_PROP_NAME          1190

#define IDC_MODEL_PROPERTIES       1050
#define IDC_MODEL_SELECTION        1051
#define IDC_MODEL_GROUP_LABEL      1052
#define IDC_MODEL_GROUP            1053
#define IDC_MODEL_SELECT_GROUP     1054
#define IDC_MODEL_SELECT_ALL       1055
#define IDC_MODEL_CURRENT          1056
#define IDC_MODEL_CULL_LABEL       1057
#define IDC_MODEL_CULL             1058
#define IDC_MODEL_SURFACE_LABEL    1059
#define IDC_MODEL_SURFACE          1060
#define IDC_MODEL_APPLY            1061
#define IDC_MODEL_LODS             1062
#define IDC_MODEL_HINT             1063
#define IDC_MODEL_MATERIALS        1064
#define IDC_MODEL_MATERIAL_LIST    1065
#define IDC_MODEL_COLORS           1066
#define IDC_MODEL_UV               1067
#define IDC_MODEL_WRAP_U_LABEL     1068
#define IDC_MODEL_WRAP_U           1069
#define IDC_MODEL_WRAP_V_LABEL     1070
#define IDC_MODEL_WRAP_V           1071
#define IDC_MODEL_LOD_LABEL        1072
#define IDC_MODEL_UNTEXTURED       1073
#define IDC_MODEL_SHARED           1074
#define IDC_MODEL_SEPARATE_LODS    1075

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

#define IDC_PORTAL_ROOM1      1180
#define IDC_PORTAL_ROOM2      1181
#define IDC_PORTAL_PLANE      1182

/* Rebase Project dialog. */
#define IDC_REBASE_ROM        1160
#define IDC_REBASE_BROWSE_ROM 1161
#define IDC_REBASE_NAME       1162
#define IDC_REBASE_PARENT     1163
#define IDC_REBASE_BROWSE_DIR 1164
#define IDC_REBASE_REPORT     1165
#define IDC_REBASE_CHECK      1166
#define IDC_REBASE_CREATE     1167
#define IDC_REBASE_KEEP_IMAGES 1168

/* Knife plane fields are contiguous, position followed by normal. */
#define IDC_KNIFE_PX          1200
#define IDC_KNIFE_PY          1201
#define IDC_KNIFE_PZ          1202
#define IDC_KNIFE_NX          1203
#define IDC_KNIFE_NY          1204
#define IDC_KNIFE_NZ          1205
#define IDC_KNIFE_PICK        1206
#define IDC_KNIFE_STATUS      1207

#define IDC_UV_CYLINDER_AXIS_LABEL 1230
#define IDC_UV_CYLINDER_AXIS       1231
#define IDC_UV_CYLINDER_SIZE_LABEL 1232
#define IDC_UV_CYLINDER_SIZE       1233

/* Longest project name we accept, including the terminator. */
#define GEDITOR_NAME_MAX       64

#endif /* GEDITOR_RESOURCE_H */
