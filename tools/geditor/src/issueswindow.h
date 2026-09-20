#ifndef GEDITOR_ISSUESWINDOW_H
#define GEDITOR_ISSUESWINDOW_H
#include <windows.h>
#include "levelissues.h"
#define ISSUES_WM_SCAN (WM_APP + 89)
#define ISSUES_WM_LOCATE (WM_APP + 90)
typedef struct IssuesScanRequest { LevelIssueReport report; const char *reason; BOOL exported; } IssuesScanRequest;
BOOL IssuesWindowShow(HWND owner, BOOL exported, const char **why);
void IssuesWindowClose(void);
void IssuesWindowInvalidate(void);
void IssuesWindowRefreshExport(void);
BOOL IssuesWindowHandleMessage(MSG *message);
#endif
