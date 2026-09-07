#ifndef GEDITOR_COLORPICKER_H
#define GEDITOR_COLORPICKER_H

#include <windows.h>

/* Persistent RGBA brush color; changing it never edits the document. */
BOOL ColorPickerRegisterClass(HINSTANCE instance);
HWND ColorPickerCreate(HWND parent, HINSTANCE instance);
void ColorPickerGetColor(HWND picker, unsigned char rgba[4]);
BOOL ColorPickerHandleMessage(HWND picker, MSG *message);

#endif
