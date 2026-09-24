#ifndef GEDITOR_BGCOMMANDSWINDOW_H
#define GEDITOR_BGCOMMANDSWINDOW_H
#include "bgcommands.h"
#include "bgdocument.h"
#define BGCOMMANDS_WM_LOCATE (WM_APP + 0x290)
BOOL BgCommandsWindowShow(HWND owner,const BgFile *bg,const BgDocument *document,const char *level,const char **why);
void BgCommandsWindowRefresh(const BgFile *bg,const BgDocument *document,const char *level);
void BgCommandsWindowClose(void);
BOOL BgCommandsWindowHandleMessage(MSG *message);
#endif
