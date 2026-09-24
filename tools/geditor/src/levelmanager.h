#ifndef GEDITOR_LEVELMANAGER_H
#define GEDITOR_LEVELMANAGER_H

#include <windows.h>
#include "setupload.h"
#include "roomstats.h"
#include "environmentpanel.h"
#include "stageoptions.h"
#include "briefingpanel.h"

#define LEVELMANAGER_WM_INTRO_EDIT (WM_APP + 0x250)
#define LEVELMANAGER_WM_HISTORY (WM_APP + 0x251)
#define LEVELMANAGER_WM_SAVE (WM_APP + 0x252)
#define LEVELMANAGER_WM_FRAME_ROOM (WM_APP + 0x253)

BOOL LevelManagerShow(HWND owner, HINSTANCE instance, const SetupFile *setup, const char *levelname);
void LevelManagerRefresh(const SetupFile *setup, const char *levelname);
void LevelManagerRefreshRooms(const BgDocument *bg, const SetupFile *setup, const StanFile *stan);
void LevelManagerRefreshSettings(const GEditorProject *project, DWORD level);
BOOL LevelManagerApplySettings(void);
BOOL LevelManagerHasSettingsDraft(void);
BOOL LevelManagerHandleMessage(MSG *message);

#endif
