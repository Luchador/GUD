#include <stdio.h>
#include "portaloptions.h"
#include "resource.h"

typedef struct PortalOptionsDialog {
    DWORD roomcount;
    BgPortalPlacement placement;
} PortalOptionsDialog;

static INT_PTR CALLBACK PortalOptionsProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    PortalOptionsDialog *state = (PortalOptionsDialog *)GetWindowLongPtr(dialog, DWLP_USER);
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
