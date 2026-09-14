#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "portaloptions.h"
#include "resource.h"

typedef struct PortalOptionsDialog {
    DWORD roomcount;
    BgPortalPlacement placement;
} PortalOptionsDialog;

static BOOL PortalOptionsNumber(const char *text, BOOL dimension, double *out)
{
    char *end;
    double value;
    errno = 0;
    value = strtod(text, &end);
    if (end == text || errno || !isfinite(value) || (dimension && value <= 0)) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    if (*end || (dimension && !isfinite(value * 100))) { return FALSE; }
    *out = dimension ? value * 100 : value;
    return TRUE;
}

static INT_PTR CALLBACK PortalOptionsProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    PortalOptionsDialog *state = (PortalOptionsDialog *)GetWindowLongPtr(dialog, DWLP_USER);
    static const int numbers[] = {IDC_PORTAL_WIDTH, IDC_PORTAL_HEIGHT,
        IDC_PORTAL_X, IDC_PORTAL_Y, IDC_PORTAL_Z};
    switch (message)
    {
    case WM_INITDIALOG:
    {
        char text[64];
        state = (PortalOptionsDialog *)lparam;
        SetWindowLongPtr(dialog, DWLP_USER, (LONG_PTR)state);
        for (int side = 0; side < 2; side++)
        {
            int control = side ? IDC_PORTAL_ROOM2 : IDC_PORTAL_ROOM1, choice = -1;
            DWORD selected = side ? state->placement.room2 : state->placement.room1;
            for (DWORD room = 1; room <= state->roomcount && room <= 255; room++)
            {
                int entry;
                snprintf(text, sizeof(text), "%lu", (unsigned long)room);
                entry = (int)SendDlgItemMessage(dialog, control, CB_ADDSTRING, 0, (LPARAM)text);
                if (entry >= 0)
                {
                    SendDlgItemMessage(dialog, control, CB_SETITEMDATA, entry, room);
                    if (room == selected) { choice = entry; }
                }
            }
            SendDlgItemMessage(dialog, control, CB_SETCURSEL, choice, 0);
        }
        SendDlgItemMessage(dialog, IDC_PORTAL_PLANE, CB_ADDSTRING, 0, (LPARAM)"XY (vertical, facing Z)");
        SendDlgItemMessage(dialog, IDC_PORTAL_PLANE, CB_ADDSTRING, 0, (LPARAM)"YZ (vertical, facing X)");
        SendDlgItemMessage(dialog, IDC_PORTAL_PLANE, CB_ADDSTRING, 0, (LPARAM)"XZ (horizontal)");
        SendDlgItemMessage(dialog, IDC_PORTAL_PLANE, CB_SETCURSEL, state->placement.plane, 0);
        for (int i = 0; i < 5; i++)
        {
            double value = i == 0 ? state->placement.width / 100
                : i == 1 ? state->placement.height / 100 : state->placement.center[i - 2];
            snprintf(text, sizeof(text), "%.9g", value);
            SetDlgItemText(dialog, numbers[i], text);
            SendDlgItemMessage(dialog, numbers[i], EM_LIMITTEXT, 63, 0);
        }
        return TRUE;
    }
    case WM_CLOSE:
        EndDialog(dialog, IDCANCEL); return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDCANCEL) { EndDialog(dialog, IDCANCEL); return TRUE; }
        if (LOWORD(wparam) == IDOK && state)
        {
            BgPortalPlacement placement = state->placement;
            DWORD rooms[2];
            for (int side = 0; side < 2; side++)
            {
                int control = side ? IDC_PORTAL_ROOM2 : IDC_PORTAL_ROOM1;
                int choice = (int)SendDlgItemMessage(dialog, control, CB_GETCURSEL, 0, 0);
                rooms[side] = choice == CB_ERR ? 0
                    : (DWORD)SendDlgItemMessage(dialog, control, CB_GETITEMDATA, choice, 0);
            }
            if (!rooms[0] || !rooms[1] || rooms[0] == rooms[1]
                || rooms[0] > state->roomcount || rooms[1] > state->roomcount)
            {
                MessageBox(dialog, "Choose two different rooms to connect.", "Add Portal", MB_ICONINFORMATION);
                return TRUE;
            }
            placement.room1 = rooms[0]; placement.room2 = rooms[1];
            placement.plane = (BgPortalPlane)SendDlgItemMessage(dialog, IDC_PORTAL_PLANE, CB_GETCURSEL, 0, 0);
            for (int i = 0; i < 5; i++)
            {
                char text[64]; double value;
                GetDlgItemText(dialog, numbers[i], text, sizeof(text));
                if (!PortalOptionsNumber(text, i < 2, &value))
                {
                    MessageBox(dialog, i < 2 ? "Enter a positive size in meters."
                        : "Enter a finite world coordinate.", "Add Portal", MB_ICONINFORMATION);
                    SetFocus(GetDlgItem(dialog, numbers[i]));
                    SendDlgItemMessage(dialog, numbers[i], EM_SETSEL, 0, -1);
                    return TRUE;
                }
                if (i == 0) { placement.width = value; }
                else if (i == 1) { placement.height = value; }
                else { placement.center[i - 2] = value; }
            }
            state->placement = placement;
            EndDialog(dialog, IDOK); return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL PortalOptionsPrompt(HWND parent, DWORD roomcount, BgPortalPlacement *out)
{
    PortalOptionsDialog state = {roomcount, *out};
    INT_PTR result = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ADD_PORTAL),
        parent, PortalOptionsProc, (LPARAM)&state);
    if (result == -1)
    { MessageBox(parent, "Could not open the portal dialog.", "GEditor", MB_ICONERROR); }
    if (result != IDOK) { return FALSE; }
    *out = state.placement;
    return TRUE;
}
