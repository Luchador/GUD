#ifndef GEDITOR_KNIFE_H
#define GEDITOR_KNIFE_H
#include <windows.h>
#include "bgknife.h"
/* Synchronous request, lparam points to a BgKnifePlane valid for the call. */
#define KNIFE_WM_APPLY (WM_APP + 70)
BOOL KnifeDialogShow(HWND owner, HINSTANCE instance, HWND viewport,
    const double center[3], double radius);
void KnifeDialogClose(void);
void KnifeDialogSyncPreview(void);
BOOL KnifeDialogHandleMessage(MSG *message);
#endif
