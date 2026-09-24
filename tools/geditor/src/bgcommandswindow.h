#ifndef GEDITOR_BGCOMMANDSWINDOW_H
#define GEDITOR_BGCOMMANDSWINDOW_H
#include "bgcommands.h"
#include "bgdocument.h"
#define BGCOMMANDS_WM_LOCATE (WM_APP + 0x290)
#define BGCOMMANDS_WM_EDIT (WM_APP + 0x291)
#define BGCOMMANDS_WM_HISTORY (WM_APP + 0x292)
#define BGCOMMANDS_WM_CANHISTORY (WM_APP + 0x293)
BOOL BgCommandsWindowShow(HWND owner,const BgFile *bg,const BgDocument *document,const char *level,const char **why);
void BgCommandsWindowRefresh(const BgFile *bg,const BgDocument *document,const char *level);
void BgCommandsWindowClose(void);
BOOL BgCommandsWindowHandleMessage(MSG *message);
#endif
