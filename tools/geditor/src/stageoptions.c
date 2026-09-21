#include <stdlib.h>
#include <string.h>
#include "stageoptions.h"

enum { MEMORY_HEADING = 3200, MEMORY_HELP, MEMORY_TOTAL, MEMORY_STATUS, MEMORY_APPLY, MEMORY_REVERT, MEMORY_RESET,
       MEMORY_LABEL_FIRST = 3220, MEMORY_FIELD_FIRST = 3230, MEMORY_UNIT_FIRST = 3240 };
typedef struct StageOptions {
    const GEditorProject *project;
    DWORD level;
    char projectpath[MAX_PATH];
    BOOL ready, loading, draft;
    LevelMemory committed;
} StageOptions;
static StageOptions *State(HWND hwnd) { return (StageOptions *)GetWindowLongPtr(hwnd, DWLP_USER); }
static HWND Owner(HWND hwnd) { return GetWindow(GetParent(hwnd), GW_OWNER); }
static BOOL ReadFields(HWND hwnd, LevelMemory *value, int *failed, const char **why)
{
    StageOptions *s = State(hwnd); *value = s->committed;
    for (int f = 0; f < LEVEL_MEMORY_FIELDS; f++)
    {
        char text[32]; GetDlgItemText(hwnd, MEMORY_FIELD_FIRST + f, text, sizeof(text));
        if (!LevelMemoryParseField(value, f, text, why)) { *failed = f; return FALSE; }
    }
    return TRUE;
}
static void Status(HWND hwnd)
{
    StageOptions *s = State(hwnd);
    const char *text = "Open a level to edit its memory allocations.";
    if (s->project && !s->ready) { text = "Rebase onto a rebuilt GUD ROM to enable memory allocation editing."; }
    if (s->ready)
    {
        text = s->draft ? "Unapplied changes." : "Using ROM defaults.";
        if (!s->draft)
            for (DWORD i = 0; i < s->project->memoryOverrides.count; i++)
                if (s->project->memoryOverrides.rows[i].value.id == s->committed.id) { text = "Project overrides applied."; break; }
    }
    SetDlgItemText(hwnd, MEMORY_STATUS, text);
    EnableWindow(GetDlgItem(hwnd, MEMORY_APPLY), s->ready && s->draft);
    EnableWindow(GetDlgItem(hwnd, MEMORY_REVERT), s->ready && s->draft);
    EnableWindow(GetDlgItem(hwnd, MEMORY_RESET), s->ready);
    for (int f = 0; f < LEVEL_MEMORY_FIELDS; f++) { EnableWindow(GetDlgItem(hwnd, MEMORY_FIELD_FIRST + f), s->ready); }
    char total[128] = ""; LevelMemory value; int failed; const char *why;
    if (s->ready && ReadFields(hwnd, &value, &failed, &why))
        snprintf(total, sizeof(total), "Reserved by these pools: %lu KiB", (unsigned long)(2 * value.kib[0] + 2 * value.kib[1] + value.kib[2] + value.kib[3]));
    SetDlgItemText(hwnd, MEMORY_TOTAL, total);
}
static void Load(HWND hwnd)
{
    StageOptions *s = State(hwnd); s->loading = TRUE;
    memset(&s->committed, 0, sizeof(s->committed));
    s->ready = s->project && s->level < s->project->levelcount
        && LevelMemoryGet(&s->project->memory, &s->project->memoryOverrides, s->project->levels[s->level].levelID, &s->committed);
    for (int f = 0; f < LEVEL_MEMORY_FIELDS; f++)
    {
        char text[32] = "";
        if (s->ready) { snprintf(text, sizeof(text), "%lu", (unsigned long)s->committed.kib[f]); }
        SetDlgItemText(hwnd, MEMORY_FIELD_FIRST + f, text);
    }
    s->draft = FALSE; s->loading = FALSE; Status(hwnd);
    SendMessage(Owner(hwnd), STAGEOPTIONS_WM_DRAFT, 0, 0);
}
static BOOL Commit(HWND hwnd, const LevelMemory *value)
{
    StageOptionsEditRequest request = {*value, ""};
    if (!SendMessage(Owner(hwnd), STAGEOPTIONS_WM_APPLY, 0, (LPARAM)&request))
    { MessageBox(hwnd, request.why[0] ? request.why : "The memory allocations could not be applied.", "GEditor", MB_ICONERROR); return FALSE; }
    Load(hwnd); return TRUE;
}
BOOL StageOptionsApply(HWND hwnd)
{
    StageOptions *s = hwnd ? State(hwnd) : NULL;
    if (!s || !s->ready || !s->draft) { return TRUE; }
    LevelMemory value; int failed; const char *why;
    if (!ReadFields(hwnd, &value, &failed, &why))
    {
        MessageBox(hwnd, why, "GEditor", MB_ICONWARNING);
        SetFocus(GetDlgItem(hwnd, MEMORY_FIELD_FIRST + failed));
        SendDlgItemMessage(hwnd, MEMORY_FIELD_FIRST + failed, EM_SETSEL, 0, -1); return FALSE;
    }
    return Commit(hwnd, &value);
}
BOOL StageOptionsHasDraft(HWND hwnd) { StageOptions *s = hwnd ? State(hwnd) : NULL; return s && s->draft; }
void StageOptionsRefresh(HWND hwnd, const GEditorProject *project, DWORD level)
{
    StageOptions *s = hwnd ? State(hwnd) : NULL;
    if (!s) { return; }
    BOOL valid = project && project->name[0] && level < project->levelcount;
    if (valid && s->project == project && s->level == level && !strcmp(s->projectpath, project->geppath) && s->ready)
    {
        LevelMemory value;
        if (s->draft || (LevelMemoryGet(&project->memory, &project->memoryOverrides, project->levels[level].levelID, &value)
            && !memcmp(&value, &s->committed, sizeof(value)))) { return; }
    }
    s->project = valid ? project : NULL; s->level = level;
    lstrcpyn(s->projectpath, valid ? project->geppath : "", sizeof(s->projectpath)); Load(hwnd);
}
static BOOL Control(HWND hwnd, const char *type, const char *text, int id, DWORD style,
                    int x, int y, int width, int height)
{
    RECT r = {x, y, x + width, y + height}; MapDialogRect(hwnd, &r);
    HWND child = CreateWindowEx(!strcmp(type, "EDIT") ? WS_EX_CLIENTEDGE : 0, type, text,
        WS_CHILD | WS_VISIBLE | style, r.left, r.top, r.right - r.left, r.bottom - r.top,
        hwnd, (HMENU)(INT_PTR)id, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    if (child) { SendMessage(child, WM_SETFONT, SendMessage(hwnd, WM_GETFONT, 0, 0), TRUE); }
    return child != NULL;
}
static INT_PTR CALLBACK Dialog(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    StageOptions *s = State(hwnd);
    switch (message)
    {
    case WM_INITDIALOG:
    {
        s = calloc(1, sizeof(*s)); if (!s) { return FALSE; }
        SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)s);
        BOOL ok = Control(hwnd, "STATIC", "Memory allocations", MEMORY_HEADING, 0, 0, 2, 490, 14)
            && Control(hwnd, "STATIC", "Display lists and vertex/matrix data each use two buffers. Enter the size of one buffer.\r\nThe total excludes other engine allocations and loaded level data. 1 KiB = 1024 bytes.",
                MEMORY_HELP, 0, 0, 23, 490, 32);
        for (int f = 0; ok && f < LEVEL_MEMORY_FIELDS; f++)
        {
            int y = 65 + f * 25;
            ok = Control(hwnd, "STATIC", g_LevelMemoryLabels[f], MEMORY_LABEL_FIRST + f, 0, 0, y + 2, 125, 14)
                && Control(hwnd, "EDIT", "", MEMORY_FIELD_FIRST + f, WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, 128, y, 70, 16)
                && Control(hwnd, "STATIC", f < 2 ? "KiB per buffer" : "KiB", MEMORY_UNIT_FIRST + f, 0, 206, y + 2, 100, 14);
            SendDlgItemMessage(hwnd, MEMORY_FIELD_FIRST + f, EM_SETLIMITTEXT, 16, 0);
        }
        ok = ok && Control(hwnd, "STATIC", "", MEMORY_TOTAL, 0, 0, 175, 490, 14)
            && Control(hwnd, "STATIC", "", MEMORY_STATUS, 0, 0, 207, 490, 24)
            && Control(hwnd, "BUTTON", "&Apply", MEMORY_APPLY, WS_TABSTOP | BS_PUSHBUTTON, 0, 249, 64, 18)
            && Control(hwnd, "BUTTON", "&Revert edits", MEMORY_REVERT, WS_TABSTOP | BS_PUSHBUTTON, 72, 249, 78, 18)
            && Control(hwnd, "BUTTON", "Use ROM &defaults", MEMORY_RESET, WS_TABSTOP | BS_PUSHBUTTON, 158, 249, 122, 18);
        if (!ok) { DestroyWindow(hwnd); return FALSE; }
        Load(hwnd); return TRUE;
    }
    case WM_COMMAND:
    {
        if (!s || s->loading) { return TRUE; }
        int id = LOWORD(wp);
        if (id == MEMORY_APPLY || id == IDOK) { StageOptionsApply(hwnd); return TRUE; }
        if (id == MEMORY_REVERT) { Load(hwnd); return TRUE; }
        if (id == MEMORY_RESET && s->ready) { Commit(hwnd, LevelMemoryFind(&s->project->memory, s->committed.id)); return TRUE; }
        if (id >= MEMORY_FIELD_FIRST && id < MEMORY_FIELD_FIRST + LEVEL_MEMORY_FIELDS && s->ready && HIWORD(wp) == EN_CHANGE)
        { s->draft = TRUE; Status(hwnd); SendMessage(Owner(hwnd), STAGEOPTIONS_WM_DRAFT, 0, 0); return TRUE; }
        break;
    }
    case WM_NCDESTROY: SetWindowLongPtr(hwnd, DWLP_USER, 0); free(s); return TRUE;
    }
    return FALSE;
}
HWND StageOptionsCreate(HWND parent, HINSTANCE instance)
{
    HWND panel = CreateDialog(instance, MAKEINTRESOURCE(IDD_STAGE_OPTIONS), parent, Dialog);
    if (panel && !State(panel)) { DestroyWindow(panel); panel = NULL; }
    return panel;
}
