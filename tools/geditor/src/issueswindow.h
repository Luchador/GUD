#ifndef GEDITOR_ISSUESWINDOW_H
#define GEDITOR_ISSUESWINDOW_H
#include <windows.h>
#include "levelissues.h"
#define ISSUES_WM_SCAN (WM_APP + 89)
#define ISSUES_WM_LOCATE (WM_APP + 90)
typedef struct IssuesScanRequest { LevelIssueReport report; const char *reason; } IssuesScanRequest;
BOOL IssuesWindowShow(HWND owner, const char *level, const char **why);
void IssuesWindowClose(void);
void IssuesWindowInvalidate(void);
BOOL IssuesWindowHandleMessage(MSG *message);
#endif
