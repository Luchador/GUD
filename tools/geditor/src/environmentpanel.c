#include <stdlib.h>
#include <string.h>
#include "environmentpanel.h"

enum { ENV_VARIANT_LABEL = 3000, ENV_VARIANT, ENV_STATUS, ENV_APPLY, ENV_RESET, ENV_REVERT,
       ENV_GROUP_FIRST = 3010, ENV_LABEL_FIRST = 3020, ENV_FIELD_FIRST = 3050 };
typedef struct EnvironmentPanel {
    const GEditorProject *project;
    DWORD level;
    char projectpath[MAX_PATH];
    EnvironmentChoice choices[ENVIRONMENT_MAX_CHOICES];
    int count, selected;
    BOOL loading, draft, visible;
    EditorEnvironment committed;
} EnvironmentPanel;
static EnvironmentPanel *State(HWND hwnd) { return (EnvironmentPanel *)GetWindowLongPtr(hwnd, DWLP_USER); }
static HWND Owner(HWND hwnd) { return GetWindow(GetParent(hwnd), GW_OWNER); }
static BOOL Checkbox(int field)
{ return g_EnvironmentFields[field].type == ENV_BOOL32 || g_EnvironmentFields[field].type == ENV_BOOL8; }
static BOOL Ready(EnvironmentPanel *s) { return s && s->project && s->selected >= 0 && s->selected < s->count; }
static void Preview(HWND hwnd)
{
    EnvironmentPanel *s = State(hwnd);
    SendMessage(Owner(hwnd), ENVIRONMENT_WM_PREVIEW, Ready(s) ? s->choices[s->selected].id : 0, s && s->visible && Ready(s));
}
static void Status(HWND hwnd)
{
    EnvironmentPanel *s = State(hwnd);
    BOOL ready = Ready(s);
    const char *text = "Open a level to edit its environment.";
    if (s->project && s->level < s->project->levelcount && !ready)
    { text = "No editable environment table is available in this base ROM."; }
    if (ready)
    {
        text = s->draft ? "Unapplied changes." : "Using ROM defaults.";
        if (!s->draft)
            for (DWORD i = 0; i < s->project->environmentOverrides.count; i++)
                if (EnvironmentId(&s->project->environmentOverrides.rows[i].value) == s->choices[s->selected].id)
                { text = "Project overrides applied."; break; }
        if (s->choices[s->selected].shared)
        { text = s->draft ? "Unapplied changes to a shared default: affects every level using it." : "Shared default: changes affect every level using this environment."; }
    }
    SetDlgItemText(hwnd, ENV_STATUS, text);
    EnableWindow(GetDlgItem(hwnd, ENV_VARIANT), ready);
    EnableWindow(GetDlgItem(hwnd, ENV_APPLY), ready && s->draft);
    EnableWindow(GetDlgItem(hwnd, ENV_REVERT), ready && s->draft);
    EnableWindow(GetDlgItem(hwnd, ENV_RESET), ready);
    for (int i = 0; i < ENVIRONMENT_FIELD_COUNT; i++)
        EnableWindow(GetDlgItem(hwnd, ENV_FIELD_FIRST + i), ready);
}
static void Load(HWND hwnd)
{
    EnvironmentPanel *s = State(hwnd);
    s->loading = TRUE;
    memset(&s->committed, 0, sizeof(s->committed));
    if (Ready(s)) { EnvironmentGet(&s->project->environments, &s->project->environmentOverrides, s->choices[s->selected].id, &s->committed); }
    for (int i = 0; i < ENVIRONMENT_FIELD_COUNT; i++)
    {
        char text[64] = "";
        if (Ready(s)) { EnvironmentFormatField(&s->committed, i, text, sizeof(text)); }
        if (Checkbox(i)) { CheckDlgButton(hwnd, ENV_FIELD_FIRST + i, text[0] == '1' ? BST_CHECKED : BST_UNCHECKED); }
        else { SetDlgItemText(hwnd, ENV_FIELD_FIRST + i, text); }
    }
    s->draft = FALSE; s->loading = FALSE; Status(hwnd);
    SendMessage(Owner(hwnd), ENVIRONMENT_WM_DRAFT, 0, 0);
}
static BOOL Commit(HWND hwnd, const EditorEnvironment *value)
{
    EnvironmentEditRequest request = {*value, ""};
    if (!SendMessage(Owner(hwnd), ENVIRONMENT_WM_APPLY, 0, (LPARAM)&request))
    { MessageBox(hwnd, request.why[0] ? request.why : "The environment could not be applied.", "GEditor", MB_ICONERROR); return FALSE; }
    Load(hwnd); Preview(hwnd); return TRUE;
}
static BOOL Reset(HWND hwnd)
{
    EnvironmentPanel *s = State(hwnd);
    const EditorEnvironment *base = EnvironmentFind(&s->project->environments, s->choices[s->selected].id);
    return Commit(hwnd, base);
}
BOOL EnvironmentPanelApply(HWND hwnd)
{
    EnvironmentPanel *s = hwnd ? State(hwnd) : NULL;
    const char *why = "";
    if (!Ready(s) || !s->draft) { return TRUE; }
    EditorEnvironment value = s->committed;
    for (int i = 0; i < ENVIRONMENT_FIELD_COUNT; i++)
    {
        char text[64];
        if (Checkbox(i)) { strcpy(text, IsDlgButtonChecked(hwnd, ENV_FIELD_FIRST + i) == BST_CHECKED ? "1" : "0"); }
        else { GetDlgItemText(hwnd, ENV_FIELD_FIRST + i, text, sizeof(text)); }
        if (!EnvironmentParseField(&value, i, text, &why))
        {
            MessageBox(hwnd, why, "GEditor", MB_ICONWARNING);
            SetFocus(GetDlgItem(hwnd, ENV_FIELD_FIRST + i));
            SendDlgItemMessage(hwnd, ENV_FIELD_FIRST + i, EM_SETSEL, 0, -1); return FALSE;
        }
    }
    return Commit(hwnd, &value);
}
BOOL EnvironmentPanelHasDraft(HWND hwnd) { EnvironmentPanel *s = hwnd ? State(hwnd) : NULL; return s && s->draft; }
void EnvironmentPanelRefresh(HWND hwnd, const GEditorProject *project, DWORD level)
{
    EnvironmentPanel *s = hwnd ? State(hwnd) : NULL;
    if (!s) { return; }
    BOOL valid = project && project->name[0] && level < project->levelcount;
    if (valid && s->project == project && s->level == level && !strcmp(s->projectpath, project->geppath) && s->count)
    {
        EditorEnvironment value;
        if (s->draft || (EnvironmentGet(&project->environments, &project->environmentOverrides,
            s->choices[s->selected].id, &value) && !memcmp(&value, &s->committed, sizeof(value)))) { return; }
        Load(hwnd); return;
    }
    s->project = valid ? project : NULL; s->level = level; s->count = 0; s->selected = -1;
    lstrcpyn(s->projectpath, valid ? project->geppath : "", sizeof(s->projectpath));
    SendDlgItemMessage(hwnd, ENV_VARIANT, CB_RESETCONTENT, 0, 0);
    if (valid)
    {
        s->count = EnvironmentChoices(&project->environments, project->levels[level].levelID, s->choices);
        for (int i = 0; i < s->count; i++)
        {
            char label[80]; snprintf(label, sizeof(label), "%s%s", s->choices[i].label, s->choices[i].shared ? " (shared default)" : "");
            SendDlgItemMessage(hwnd, ENV_VARIANT, CB_ADDSTRING, 0, (LPARAM)label);
        }
        if (s->count) { s->selected = 0; SendDlgItemMessage(hwnd, ENV_VARIANT, CB_SETCURSEL, 0, 0); }
    }
    Load(hwnd);
    if (s->visible) { Preview(hwnd); }
}
void EnvironmentPanelShow(HWND hwnd, BOOL visible)
{
    EnvironmentPanel *s = hwnd ? State(hwnd) : NULL;
    if (!s) { return; }
    BOOL changed = s->visible != visible;
    s->visible = visible; ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
    if (changed) { Preview(hwnd); }
}
static void Place(HWND hwnd, int id, int x, int y, int width, int height)
{
    RECT r = {x, y, x + width, y + height}; MapDialogRect(hwnd, &r);
    MoveWindow(GetDlgItem(hwnd, id), r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
}
static void Layout(HWND hwnd)
{
    /* Three columns fit the Level Settings minimum size. Keep the form
     * compact as the parent grows; controls retain their tab order. */
    Place(hwnd, ENV_VARIANT_LABEL, 0, 3, 60, 14); Place(hwnd, ENV_VARIANT, 62, 0, 220, 120);
    static const int first[] = {0, 6, 15}, last[] = {6, 15, ENVIRONMENT_FIELD_COUNT};
    for (int column = 0; column < 3; column++)
    {
        int x = column * 170;
        Place(hwnd, ENV_GROUP_FIRST + column, x, 24, 162, 14);
        for (int i = first[column]; i < last[column]; i++)
        {
            int y = 42 + (i - first[column]) * 18;
            if (Checkbox(i)) { Place(hwnd, ENV_FIELD_FIRST + i, x, y, 162, 15); }
            else
            { Place(hwnd, ENV_LABEL_FIRST + i, x, y + 2, 93, 14); Place(hwnd, ENV_FIELD_FIRST + i, x + 94, y, 68, 15); }
        }
    }
    Place(hwnd, ENV_STATUS, 0, 226, 504, 20);
    Place(hwnd, ENV_APPLY, 0, 249, 64, 18);
    Place(hwnd, ENV_REVERT, 72, 249, 78, 18);
    Place(hwnd, ENV_RESET, 158, 249, 122, 18);
}
static BOOL Control(HWND hwnd, const char *type, const char *text, int id, DWORD style, DWORD exstyle)
{
    HWND child = CreateWindowEx(exstyle, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
        hwnd, (HMENU)(INT_PTR)id, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    if (child) { SendMessage(child, WM_SETFONT, SendMessage(hwnd, WM_GETFONT, 0, 0), TRUE); }
    return child != NULL;
}
static INT_PTR CALLBACK Dialog(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    EnvironmentPanel *s = State(hwnd);
    switch (message)
    {
    case WM_INITDIALOG:
    {
        s = calloc(1, sizeof(*s)); if (!s) { return FALSE; }
        s->selected = -1; SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)s);
        BOOL ok = Control(hwnd, "STATIC", "Environment", ENV_VARIANT_LABEL, 0, 0)
            && Control(hwnd, "COMBOBOX", "", ENV_VARIANT, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0);
        static const char *groups[] = {"Clipping and fog", "Sky and colors", "Water and prop fade"};
        for (int i = 0; ok && i < 3; i++) { ok = Control(hwnd, "STATIC", groups[i], ENV_GROUP_FIRST + i, 0, 0); }
        for (int i = 0; ok && i < ENVIRONMENT_FIELD_COUNT; i++)
        {
            if (!Checkbox(i)) { ok = Control(hwnd, "STATIC", g_EnvironmentFields[i].label, ENV_LABEL_FIRST + i, 0, 0); }
            ok = ok && Control(hwnd, Checkbox(i) ? "BUTTON" : "EDIT", Checkbox(i) ? g_EnvironmentFields[i].label : "",
                ENV_FIELD_FIRST + i, WS_TABSTOP | (Checkbox(i) ? BS_AUTOCHECKBOX : ES_AUTOHSCROLL), Checkbox(i) ? 0 : WS_EX_CLIENTEDGE);
            if (!Checkbox(i)) { SendDlgItemMessage(hwnd, ENV_FIELD_FIRST + i, EM_SETLIMITTEXT, 63, 0); }
        }
        ok = ok && Control(hwnd, "STATIC", "", ENV_STATUS, 0, 0)
            && Control(hwnd, "BUTTON", "&Apply", ENV_APPLY, BS_PUSHBUTTON | WS_TABSTOP, 0)
            && Control(hwnd, "BUTTON", "&Revert edits", ENV_REVERT, BS_PUSHBUTTON | WS_TABSTOP, 0)
            && Control(hwnd, "BUTTON", "Use ROM &defaults", ENV_RESET, BS_PUSHBUTTON | WS_TABSTOP, 0);
        if (!ok) { DestroyWindow(hwnd); return FALSE; }
        Layout(hwnd); Load(hwnd); return TRUE;
    }
    case WM_COMMAND:
    {
        if (!s || s->loading) { return TRUE; }
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == ENV_APPLY || id == IDOK) { EnvironmentPanelApply(hwnd); return TRUE; }
        if (id == ENV_REVERT) { Load(hwnd); return TRUE; }
        if (id == ENV_RESET && Ready(s))
        { Reset(hwnd); return TRUE; }
        if (id == ENV_VARIANT && code == CBN_SELCHANGE)
        {
            int selected = (int)SendDlgItemMessage(hwnd, ENV_VARIANT, CB_GETCURSEL, 0, 0);
            if (!EnvironmentPanelApply(hwnd)) { SendDlgItemMessage(hwnd, ENV_VARIANT, CB_SETCURSEL, s->selected, 0); return TRUE; }
            if (selected >= 0 && selected < s->count) { s->selected = selected; Load(hwnd); Preview(hwnd); }
            return TRUE;
        }
        if (id >= ENV_FIELD_FIRST && id < ENV_FIELD_FIRST + ENVIRONMENT_FIELD_COUNT && Ready(s)
            && (code == EN_CHANGE || (Checkbox(id - ENV_FIELD_FIRST) && code == BN_CLICKED)))
        { s->draft = TRUE; Status(hwnd); SendMessage(Owner(hwnd), ENVIRONMENT_WM_DRAFT, 0, 0); return TRUE; }
        break;
    }
    case WM_NCDESTROY: SetWindowLongPtr(hwnd, DWLP_USER, 0); free(s); return TRUE;
    }
    return FALSE;
}
HWND EnvironmentPanelCreate(HWND parent, HINSTANCE instance)
{
    HWND panel = CreateDialog(instance, MAKEINTRESOURCE(IDD_ENVIRONMENT), parent, Dialog);
    if (panel && !State(panel)) { DestroyWindow(panel); panel = NULL; }
    return panel;
}
