#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include "primitiveoptions.h"
#include "resource.h"
#include "bgdocument.h"

typedef struct PrimitiveOptionsDialog {
    BOOL cylinder;
    PrimitiveOptions options;
} PrimitiveOptionsDialog;

static BOOL PrimitiveOptionsNumber(const char *text, BOOL sides, double *out)
{
    char *end;
    double value;
    errno = 0;
    value = strtod(text, &end);
    if (end == text || errno || !isfinite(value) || value <= 0) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    if (*end || (sides && (value != floor(value) || value < 3 || value > BG_PRIMITIVE_MAX_SIDES))
        || (!sides && !isfinite(value * 100))) { return FALSE; }
    *out = value;
    return TRUE;
}

static INT_PTR CALLBACK PrimitiveOptionsProc(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    PrimitiveOptionsDialog *state = (PrimitiveOptionsDialog *)GetWindowLongPtr(dialog, DWLP_USER);
    switch (message)
    {
    case WM_INITDIALOG:
        state = (PrimitiveOptionsDialog *)lparam;
        SetWindowLongPtr(dialog, DWLP_USER, (LONG_PTR)state);
        SetDlgItemText(dialog, IDC_PRIMITIVE_RADIUS, "1");
        SetDlgItemText(dialog, IDC_PRIMITIVE_SIDES, "8");
        if (state->cylinder) { SetDlgItemText(dialog, IDC_PRIMITIVE_HEIGHT, "1"); }
        SendDlgItemMessage(dialog, IDC_PRIMITIVE_RADIUS, EM_LIMITTEXT, 63, 0);
        SendDlgItemMessage(dialog, IDC_PRIMITIVE_SIDES, EM_LIMITTEXT, 63, 0);
        if (state->cylinder) { SendDlgItemMessage(dialog, IDC_PRIMITIVE_HEIGHT, EM_LIMITTEXT, 63, 0); }
        SetFocus(GetDlgItem(dialog, IDC_PRIMITIVE_RADIUS));
        SendDlgItemMessage(dialog, IDC_PRIMITIVE_RADIUS, EM_SETSEL, 0, -1);
        return FALSE;
    case WM_CLOSE:
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDCANCEL)
        { EndDialog(dialog, IDCANCEL); return TRUE; }
        if (LOWORD(wparam) == IDOK && state)
        {
            const int controls[] = {IDC_PRIMITIVE_RADIUS, IDC_PRIMITIVE_SIDES, IDC_PRIMITIVE_HEIGHT};
            const char *errors[] = {"Enter a positive radius in meters.",
                "Enter a whole number of sides from 3 to 64.", "Enter a positive height in meters."};
            double values[3] = {1, 8, 1};
            for (int i = 0; i < (state->cylinder ? 3 : 2); i++)
            {
                char text[64];
                GetDlgItemText(dialog, controls[i], text, sizeof(text));
                if (!PrimitiveOptionsNumber(text, i == 1, &values[i]))
                {
                    MessageBox(dialog, errors[i], "Primitive size", MB_OK | MB_ICONINFORMATION);
                    SetFocus(GetDlgItem(dialog, controls[i]));
                    SendDlgItemMessage(dialog, controls[i], EM_SETSEL, 0, -1);
                    return TRUE;
                }
            }
            state->options = (PrimitiveOptions){values[0], values[2], (DWORD)values[1]};
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL PrimitiveOptionsPrompt(HWND parent, BOOL cylinder, PrimitiveOptions *out)
{
    PrimitiveOptionsDialog state = {cylinder, {1, 1, 8}};
    INT_PTR result = DialogBoxParam(GetModuleHandle(NULL),
        MAKEINTRESOURCE(cylinder ? IDD_ADD_CYLINDER : IDD_ADD_CIRCLE), parent,
        PrimitiveOptionsProc, (LPARAM)&state);
    if (result == -1)
    { MessageBox(parent, "Could not open the primitive size dialog.", "GEditor", MB_OK | MB_ICONERROR); }
    if (result != IDOK) { return FALSE; }
    *out = state.options;
    return TRUE;
}
