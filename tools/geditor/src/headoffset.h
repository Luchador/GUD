#ifndef GEDITOR_HEADOFFSET_H
#define GEDITOR_HEADOFFSET_H
#include <windows.h>
BOOL HeadOffsetShow(HWND owner,const char *project,const char *name,const char **why);
void HeadOffsetClose(void);
BOOL HeadOffsetHandleMessage(MSG *message);
#endif
