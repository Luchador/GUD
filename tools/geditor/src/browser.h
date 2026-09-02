#ifndef GEDITOR_BROWSER_H
#define GEDITOR_BROWSER_H

#include <windows.h>


BOOL BrowserRegisterClass(HINSTANCE hinstance);
HWND BrowserCreate(HWND parent, HINSTANCE hinstance);

#endif