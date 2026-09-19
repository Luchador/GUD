#ifndef GEDITOR_LEVELMANAGER_H
#define GEDITOR_LEVELMANAGER_H

#include <windows.h>
#include "setupload.h"

#define LEVELMANAGER_WM_INTRO_EDIT (WM_APP + 0x250)
#define LEVELMANAGER_WM_HISTORY (WM_APP + 0x251)
#define LEVELMANAGER_WM_SAVE (WM_APP + 0x252)

BOOL LevelManagerShow(HWND owner, HINSTANCE instance, const SetupFile *setup, const char *levelname);
void LevelManagerRefresh(const SetupFile *setup, const char *levelname);
BOOL LevelManagerHandleMessage(MSG *message);

#endif
