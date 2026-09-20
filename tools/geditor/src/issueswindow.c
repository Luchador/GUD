#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "issueswindow.h"
#include "resource.h"

typedef struct IssuesWindow { LevelIssueReport report; BOOL stale, exported; } IssuesWindow;
static HWND g_IssuesWindow;
static IssuesWindow g_IssuesState;

static IssuesWindow *State(HWND dialog)
{ return (IssuesWindow *)GetWindowLongPtr(dialog, DWLP_USER); }
static int Selected(HWND dialog, const IssuesWindow *state)
{
    int row = ListView_GetNextItem(GetDlgItem(dialog, IDC_ISSUES_LIST), -1, LVNI_SELECTED);
    return !state || state->stale || row < 0 || (DWORD)row >= state->report.count ? -1 : row;
}
static void Details(HWND dialog, IssuesWindow *state)
{
    int row = Selected(dialog, state);
    const LevelIssue *issue = row >= 0 ? &state->report.items[row] : NULL;
    EnableWindow(GetDlgItem(dialog, IDOK), issue && issue->target != LEVEL_ISSUE_NOWHERE);
    SetDlgItemText(dialog, IDC_ISSUES_DETAILS, issue ? issue->description :
        "Double-click a result or choose Go to Issue to focus its location. Reports without a location cannot be selected in the viewport.");
}
static void Clear(HWND dialog, IssuesWindow *state)
{
    LevelIssuesFree(&state->report);
    ListView_DeleteAllItems(GetDlgItem(dialog, IDC_ISSUES_LIST));
    Details(dialog, state);
}
static void Check(HWND dialog, IssuesWindow *state)
{
    IssuesScanRequest request = {0}; char summary[256];
    state->exported = SendDlgItemMessage(dialog, IDC_ISSUES_SCOPE, CB_GETCURSEL, 0, 0) == 1;
    request.exported = state->exported;
    state->stale = TRUE; Clear(dialog, state);
    SetDlgItemText(dialog, IDC_ISSUES_CHECK, state->exported ? "&Reload Report" : "&Check Again");
    SetDlgItemText(dialog, IDC_ISSUES_STATUS, state->exported ? "Loading the last ROM export report..." : "Checking the current level...");
    HCURSOR cursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BOOL ok = (BOOL)SendMessage(GetParent(dialog), ISSUES_WM_SCAN, 0, (LPARAM)&request);
    SetCursor(cursor);
    if (!ok)
    {
        LevelIssuesFree(&request.report);
        SetDlgItemText(dialog, IDC_ISSUES_STATUS, "The report is unavailable.");
        SetDlgItemText(dialog, IDC_ISSUES_DETAILS, request.reason ? request.reason : "The level could not be checked.");
        return;
    }
    state->report = request.report; state->stale = FALSE;
    HWND list = GetDlgItem(dialog, IDC_ISSUES_LIST);
    SendMessage(list, WM_SETREDRAW, FALSE, 0);
    for (DWORD i = 0; i < state->report.count; i++)
    {
        LevelIssue *issue = &state->report.items[i]; LVITEM item = {0};
        item.mask = LVIF_TEXT; item.iItem = (int)i; item.pszText = issue->error ? "Error" : "Warning";
        if (ListView_InsertItem(list, &item) == -1)
        {
            SendMessage(list, WM_SETREDRAW, TRUE, 0); state->stale = TRUE; Clear(dialog, state);
            SetDlgItemText(dialog, IDC_ISSUES_STATUS, "Not enough memory to display the report. Choose Check Again to retry.");
            return;
        }
        ListView_SetItemText(list, i, 1, issue->scope);
        ListView_SetItemText(list, i, 2, issue->subject);
        ListView_SetItemText(list, i, 3, issue->description);
    }
    SendMessage(list, WM_SETREDRAW, TRUE, 0); InvalidateRect(list, NULL, TRUE);
    if (state->report.count)
    {
        snprintf(summary, sizeof(summary), "%lu errors, %lu warnings.%s %s",
            (unsigned long)state->report.errors, (unsigned long)state->report.warnings,
            state->report.truncated ? " Showing the first 10,000 issues." : "",
            state->exported ? "All setups in the last successfully created ROM. Create ROM again to update this snapshot."
                : "Checked current level data, including unsaved edits.");
        ListView_SetItemState(list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    else { strcpy(summary, state->exported ? "No unresolved pads or unchecked setups in the last ROM export. Create ROM again to update this snapshot."
        : "No issues found by these checks. Checked current level data, including unsaved edits."); }
    SetDlgItemText(dialog, IDC_ISSUES_STATUS, summary); Details(dialog, state);
}
static void Locate(HWND dialog, IssuesWindow *state)
{
    int row = Selected(dialog, state);
    if (row < 0 || state->report.items[row].target == LEVEL_ISSUE_NOWHERE) { return; }
    LevelIssue issue = state->report.items[row];
    if (!SendMessage(GetParent(dialog), ISSUES_WM_LOCATE, 0, (LPARAM)&issue))
        SetDlgItemText(dialog, IDC_ISSUES_DETAILS,
            state->exported ? "This item could not be located, the level switch was canceled, or the pad changed since export. Finish active transforms and export again to refresh the snapshot."
                : "This item could not be located. Finish any active transform and choose Check Again to refresh the report.");
}
static void Layout(HWND dialog)
{
    RECT client, units = {8, 8, 84, 20};
    GetClientRect(dialog, &client); MapDialogRect(dialog, &units);
    int margin = units.left, gap = units.top, button = units.right, height = units.bottom;
    int width = client.right, bottom = client.bottom - gap - height;
    int details = bottom - gap - height * 3, top = gap * 2 + height * 3;
    MoveWindow(GetDlgItem(dialog, IDC_ISSUES_SCOPE_LABEL), margin, gap, button, height, TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_ISSUES_SCOPE), margin + button, gap, button * 3, height * 8, TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_ISSUES_STATUS), margin, gap * 2 + height, width - margin * 2, height * 2 - gap, TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_ISSUES_LIST), margin, top, width - margin * 2, details - gap - top, TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_ISSUES_DETAILS), margin, details, width - margin * 2, height * 3, TRUE);
    MoveWindow(GetDlgItem(dialog, IDC_ISSUES_CHECK), margin, bottom, button, height, TRUE);
    MoveWindow(GetDlgItem(dialog, IDOK), width - margin - button * 2 - gap, bottom, button, height, TRUE);
    MoveWindow(GetDlgItem(dialog, IDCANCEL), width - margin - button, bottom, button, height, TRUE);
    RECT columns = {0, 0, 332, 20}; MapDialogRect(dialog, &columns);
    ListView_SetColumnWidth(GetDlgItem(dialog, IDC_ISSUES_LIST), 3, width - margin * 2 - columns.right - 26);
}
static INT_PTR CALLBACK Dialog(HWND dialog, UINT message, WPARAM wp, LPARAM lp)
{
    IssuesWindow *state = State(dialog);
    if (message == WM_INITDIALOG)
    {
        state = (IssuesWindow *)lp; SetWindowLongPtr(dialog, DWLP_USER, (LONG_PTR)state);
        HWND list = GetDlgItem(dialog, IDC_ISSUES_LIST);
        SendDlgItemMessage(dialog, IDC_ISSUES_SCOPE, CB_ADDSTRING, 0, (LPARAM)"Current level");
        SendDlgItemMessage(dialog, IDC_ISSUES_SCOPE, CB_ADDSTRING, 0, (LPARAM)"Last ROM export (all setups)");
        SendDlgItemMessage(dialog, IDC_ISSUES_SCOPE, CB_SETCURSEL, 0, 0);
        ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        const char *names[] = {"Severity", "Level / Setup", "Item", "Issue"};
        const int widths[] = {60, 160, 112, 300};
        for (int i = 0; i < 4; i++)
        {
            RECT units = {0, 0, widths[i], 20}; MapDialogRect(dialog, &units);
            LVCOLUMN column = {0}; column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = (char *)names[i]; column.cx = units.right;
            ListView_InsertColumn(list, i, &column);
        }
        Layout(dialog); Details(dialog, state); return TRUE;
    }
    if (message == WM_GETMINMAXINFO)
    {
        RECT minimum = {0, 0, 640, 290}; MapDialogRect(dialog, &minimum);
        MINMAXINFO *info = (MINMAXINFO *)lp;
        info->ptMinTrackSize.x = minimum.right; info->ptMinTrackSize.y = minimum.bottom; return TRUE;
    }
    if (message == WM_DESTROY)
    {
        if (g_IssuesWindow == dialog) { g_IssuesWindow = NULL; }
        if (state) { LevelIssuesFree(&state->report); }
        SetWindowLongPtr(dialog, DWLP_USER, 0); return TRUE;
    }
    if (!state) { return FALSE; }
    if (message == WM_SIZE && wp != SIZE_MINIMIZED) { Layout(dialog); return TRUE; }
    if (message == WM_CLOSE) { DestroyWindow(dialog); return TRUE; }
    if (message == WM_COMMAND)
    {
        switch (LOWORD(wp))
        {
        case IDC_ISSUES_SCOPE:
            if (HIWORD(wp) == CBN_SELCHANGE) { Check(dialog, state); }
            return TRUE;
        case IDC_ISSUES_CHECK: Check(dialog, state); return TRUE;
        case IDOK: Locate(dialog, state); return TRUE;
        case IDCANCEL: DestroyWindow(dialog); return TRUE;
        }
    }
    if (message == WM_NOTIFY && ((NMHDR *)lp)->idFrom == IDC_ISSUES_LIST)
    {
        switch (((NMHDR *)lp)->code)
        {
        case LVN_ITEMCHANGED: Details(dialog, state); return TRUE;
        case NM_DBLCLK:
            if (((NMITEMACTIVATE *)lp)->iItem >= 0) { Locate(dialog, state); }
            return TRUE;
        }
    }
    return FALSE;
}

BOOL IssuesWindowShow(HWND owner, BOOL exported, const char **why)
{
    *why = "Could not open Check for Issues.";
    if (!g_IssuesWindow)
    {
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES};
        if (!InitCommonControlsEx(&controls)) { return FALSE; }
        memset(&g_IssuesState, 0, sizeof(g_IssuesState));
        g_IssuesWindow = CreateDialogParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_CHECK_ISSUES), owner, Dialog, (LPARAM)&g_IssuesState);
        if (!g_IssuesWindow) { LevelIssuesFree(&g_IssuesState.report); return FALSE; }
    }
    SendDlgItemMessage(g_IssuesWindow, IDC_ISSUES_SCOPE, CB_SETCURSEL, exported ? 1 : 0, 0);
    ShowWindow(g_IssuesWindow, SW_RESTORE); SetForegroundWindow(g_IssuesWindow);
    PostMessage(g_IssuesWindow, WM_COMMAND, IDC_ISSUES_CHECK, 0); *why = ""; return TRUE;
}
void IssuesWindowClose(void) { if (g_IssuesWindow) { DestroyWindow(g_IssuesWindow); } }
void IssuesWindowRefreshExport(void)
{
    IssuesWindow *state = g_IssuesWindow ? State(g_IssuesWindow) : NULL;
    if (state && state->exported) { Check(g_IssuesWindow, state); }
}
void IssuesWindowInvalidate(void)
{
    IssuesWindow *state = g_IssuesWindow ? State(g_IssuesWindow) : NULL;
    if (!state || state->stale || state->exported) { return; }
    state->stale = TRUE; Clear(g_IssuesWindow, state);
    SetDlgItemText(g_IssuesWindow, IDC_ISSUES_STATUS, "The level changed. Choose Check Again to update the report.");
}
BOOL IssuesWindowHandleMessage(MSG *message)
{
    if (!g_IssuesWindow || !message || (message->hwnd != g_IssuesWindow && !IsChild(g_IssuesWindow, message->hwnd))) { return FALSE; }
    if (!IsDialogMessage(g_IssuesWindow, message)) { TranslateMessage(message); DispatchMessage(message); }
    return TRUE;
}
