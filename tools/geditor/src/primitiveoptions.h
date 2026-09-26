#ifndef GEDITOR_PRIMITIVEOPTIONS_H
#define GEDITOR_PRIMITIVEOPTIONS_H

#include <windows.h>

typedef struct PrimitiveOptions {
    double radius, height; /* World units, converted from the chosen editor units by the dialog. */
    DWORD sides;
} PrimitiveOptions;

/* Defaults to radius/height 100 editor units and 8 sides. Cancel leaves out unchanged. */
BOOL PrimitiveOptionsPrompt(HWND parent, BOOL cylinder, double factor, PrimitiveOptions *out);

#endif
