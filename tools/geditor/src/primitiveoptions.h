#ifndef GEDITOR_PRIMITIVEOPTIONS_H
#define GEDITOR_PRIMITIVEOPTIONS_H

#include <windows.h>

typedef struct PrimitiveOptions {
    double radius, height; /* Metres, converted to world centimetres by the caller. */
    DWORD sides;
} PrimitiveOptions;

/* Defaults to radius/height 1 metre and 8 sides. Cancel leaves out unchanged. */
BOOL PrimitiveOptionsPrompt(HWND parent, BOOL cylinder, PrimitiveOptions *out);

#endif
