#include <windows.h>
#include <stdio.h>
#include <src/propconstants.h>
#include "safelinkdialog.h"
#include "modelload.h"
#include "resource.h"

typedef struct SafeLinkDialog {
    const SetupFile *setup;
    LONG body, door;
    const char *warning;
} SafeLinkDialog;

static void Fill(HWND combo, const SetupFile *setup, unsigned char type, LONG selected)
{
    int row = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)"None");
    SendMessage(combo, CB_SETITEMDATA, row, (LPARAM)-1);
    SendMessage(combo, CB_SETCURSEL, row, 0);
    for (DWORD i = 0; i < setup->objectcount; i++)
    {
        const SetupObject *obj = &setup->objects[i];
        if (obj->deleted || obj->type != type) continue;
        char label[160]; const char *model = "Unknown model";
        ModelGetPropDefinition(obj->modelid, &model, NULL);
        snprintf(label, sizeof(label), "Object %lu - %s", (unsigned long)i, model);
        row = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)label);
        if (row >= 0)
        {
            SendMessage(combo, CB_SETITEMDATA, row, i);
            if ((LONG)i == selected) SendMessage(combo, CB_SETCURSEL, row, 0);
        }
    }
}

static LONG Choice(HWND dialog, int id)
{
    HWND combo = GetDlgItem(dialog, id);
    LRESULT row = SendMessage(combo, CB_GETCURSEL, 0, 0);
    return row < 0 ? -1 : (LONG)SendMessage(combo, CB_GETITEMDATA, row, 0);
}

static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    SafeLinkDialog *s = (SafeLinkDialog *)GetWindowLongPtr(hwnd, DWLP_USER);
    if (msg == WM_INITDIALOG)
    {
        s = (SafeLinkDialog *)lp; SetWindowLongPtr(hwnd, DWLP_USER, lp);
        Fill(GetDlgItem(hwnd, IDC_SAFE_BODY), s->setup, PROPDEF_SAFE, s->body);
        Fill(GetDlgItem(hwnd, IDC_SAFE_DOOR), s->setup, PROPDEF_DOOR, s->door);
        SetDlgItemText(hwnd, IDC_SAFE_STATUS, s->warning);
        return TRUE;
    }
    if (msg == WM_CLOSE) { EndDialog(hwnd, IDCANCEL); return TRUE; }
    if (msg == WM_COMMAND && s)
    {
        if (LOWORD(wp) == IDCANCEL) { EndDialog(hwnd, IDCANCEL); return TRUE; }
        if (LOWORD(wp) == IDC_SAFE_BODY && HIWORD(wp) == CBN_SELCHANGE)
        {
            if (Choice(hwnd, IDC_SAFE_BODY) < 0) SendDlgItemMessage(hwnd, IDC_SAFE_DOOR, CB_SETCURSEL, 0, 0);
            SetDlgItemText(hwnd, IDC_SAFE_STATUS, "");
            return TRUE;
        }
        if (LOWORD(wp) == IDOK)
        {
            s->body = Choice(hwnd, IDC_SAFE_BODY); s->door = Choice(hwnd, IDC_SAFE_DOOR);
            if ((s->body < 0) != (s->door < 0))
            { SetDlgItemText(hwnd, IDC_SAFE_STATUS, "Choose both a safe body and its door, or None for both to remove the link."); return TRUE; }
            EndDialog(hwnd, IDOK); return TRUE;
        }
    }
    return FALSE;
}

BOOL SafeLinkDialogShow(HWND parent, const SetupFile *setup, DWORD item, LONG *body, LONG *door)
{
    SafeLinkDialog state = { .setup = setup, .body = -1, .door = -1, .warning = "" };
    if (!SetupFileCanBeSafeItem(setup, item)) return FALSE;
    if (!SetupFileGetSafeLink(setup, item, &state.body, &state.door, &state.warning))
    { state.body = state.door = -1; }
    INT_PTR result = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SAFE_CONTENTS),
        parent, DialogProc, (LPARAM)&state);
    if (result == -1) MessageBox(parent, "The Safe Contents window could not be opened.", "GEditor", MB_ICONERROR);
    if (result != IDOK) return FALSE;
    *body = state.body; *door = state.door;
    return TRUE;
}
