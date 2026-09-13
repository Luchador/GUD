#ifndef GEDITOR_IMAGEIMPORT_H
#define GEDITOR_IMAGEIMPORT_H
#include <windows.h>
BOOL ImageImportShow(HWND owner, const char *projectdir, DWORD *id);
BOOL ImageReplaceShow(HWND owner, const char *projectdir, DWORD id);
BOOL ImageReimportShow(HWND owner, const char *projectdir, DWORD id);
#endif
