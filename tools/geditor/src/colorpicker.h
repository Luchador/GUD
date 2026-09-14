#ifndef GEDITOR_COLORPICKER_H
#define GEDITOR_COLORPICKER_H

#include <windows.h>

/* Ask the parent to arm one viewport color sample. */
#define COLORPICKER_WM_PICK_COLOR (WM_APP + 54)

/* Persistent RGBA brush color; changing it never edits the document. */
BOOL ColorPickerRegisterClass(HINSTANCE instance);
HWND ColorPickerCreate(HWND parent, HINSTANCE instance);
void ColorPickerGetColor(HWND picker, unsigned char rgba[4]);
void ColorPickerSetColor(HWND picker, const unsigned char rgba[4]);
void ColorPickerSetSampling(HWND picker, BOOL enabled);
BOOL ColorPickerHandleMessage(HWND picker, MSG *message);

#endif
