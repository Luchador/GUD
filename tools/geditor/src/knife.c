#include "knife.h"
#include "viewport.h"
#include "resource.h"
#include <windowsx.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HWND g_Knife, g_KnifeViewport;
static BgKnifePlane g_KnifePlane, g_KnifeBeforePick;
static double g_KnifeRadius;
static BOOL g_KnifePick;
static BOOL g_KnifeUpdating;

static BOOL KnifeParseNumber(const char *text, double *out)
{
    char *end;
    double value;
    errno = 0;
    value = strtod(text, &end);
    if (end == text || errno == ERANGE || !isfinite(value) || fabs(value) > 1e20) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    if (*end) { return FALSE; }
    *out = value;
    return TRUE;
}

static BOOL KnifeReadPlane(BgKnifePlane *plane)
{
    char text[128];
    BgKnifePlane normalized;
    int i;
    for (i = 0; i < 6; i++)
    {
        double *out = i < 3 ? plane->position + i : plane->normal + i - 3;
        GetDlgItemText(g_Knife, IDC_KNIFE_PX + i, text, sizeof(text));
        if (!KnifeParseNumber(text, out)) { return FALSE; }
    }
    return BgKnifeNormalizePlane(plane, &normalized);
}

static void KnifeUpdatePreview(void)
{
    BgKnifePlane plane;
    BOOL valid = KnifeReadPlane(&plane);
    if (valid) { g_KnifePlane = plane; }
    EnableWindow(GetDlgItem(g_Knife, IDOK), valid && !g_KnifePick);
    g_KnifeUpdating = TRUE;
    ViewportSetKnifePlane(g_KnifeViewport, valid ? &plane : NULL, g_KnifeRadius);
    g_KnifeUpdating = FALSE;
    SetDlgItemText(g_Knife, IDC_KNIFE_STATUS, g_KnifePick ? "Click a background surface for the plane position."
        : valid ? "Move/Rotate adjusts the plane. Both sides of each cut are kept."
        : "Enter numbers and a nonzero normal direction.");
}

static void KnifeWriteFields(void)
{
    char text[64];
    int i;
    g_KnifeUpdating = TRUE;
    for (i = 0; i < 6; i++)
    {
        snprintf(text, sizeof(text), "%.12g", i < 3 ? g_KnifePlane.position[i] : g_KnifePlane.normal[i - 3]);
        SetDlgItemText(g_Knife, IDC_KNIFE_PX + i, text);
    }
    g_KnifeUpdating = FALSE;
}

static void KnifeWritePlane(void)
{
    KnifeWriteFields();
    KnifeUpdatePreview();
}

void KnifeDialogSyncPreview(void)
{
    if (!g_Knife || g_KnifeUpdating || g_KnifePick
        || !ViewportGetKnifePlane(g_KnifeViewport, &g_KnifePlane)) { return; }
    KnifeWriteFields();
}

static void KnifeCancelPick(void)
{
    g_KnifePick = 0;
    g_KnifePlane = g_KnifeBeforePick;
    for (int i = 0; i < 6; i++) { EnableWindow(GetDlgItem(g_Knife, IDC_KNIFE_PX + i), TRUE); }
    SetDlgItemText(g_Knife, IDC_KNIFE_PICK, "Pick Point");
    KnifeWritePlane();
}

static INT_PTR CALLBACK KnifeDialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    (void)lparam;
    switch (msg)
    {
    case WM_INITDIALOG:
        g_Knife = hwnd;
        for (int i = 0; i < 6; i++) { SendDlgItemMessage(hwnd, IDC_KNIFE_PX + i, EM_SETLIMITTEXT, 100, 0); }
        KnifeWritePlane();
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) >= IDC_KNIFE_PX && LOWORD(wparam) <= IDC_KNIFE_NZ
            && HIWORD(wparam) == EN_CHANGE && !g_KnifeUpdating) { KnifeUpdatePreview(); return TRUE; }
        if (LOWORD(wparam) == IDC_KNIFE_PICK)
        {
            if (g_KnifePick) { KnifeCancelPick(); return TRUE; }
            g_KnifeBeforePick = g_KnifePlane;
            g_KnifePick = 1;
            for (int i = 0; i < 6; i++) { EnableWindow(GetDlgItem(hwnd, IDC_KNIFE_PX + i), FALSE); }
            SetDlgItemText(hwnd, IDC_KNIFE_PICK, "Cancel Pick");
            KnifeUpdatePreview();
            SetFocus(g_KnifeViewport);
            return TRUE;
        }
        if (LOWORD(wparam) == IDOK)
        {
            BgKnifePlane plane;
            HWND owner = GetWindow(hwnd, GW_OWNER);
            if (g_KnifePick || !KnifeReadPlane(&plane)) { return TRUE; }
            /* Close before the synchronous edit/rebuild; no stale dialog
             * state survives a failed edit, undo, or document replacement. */
            KnifeDialogClose();
            SendMessage(owner, KNIFE_WM_APPLY, 0, (LPARAM)&plane);
            return TRUE;
        }
        if (LOWORD(wparam) == IDCANCEL)
        { if (g_KnifePick) { KnifeCancelPick(); } else { KnifeDialogClose(); } return TRUE; }
        break;
    case WM_CLOSE:
        KnifeDialogClose(); return TRUE;
    case WM_DESTROY:
        ViewportSetKnifePlane(g_KnifeViewport, NULL, 0);
        g_Knife = NULL; g_KnifeViewport = NULL; g_KnifePick = 0;
        return TRUE;
    }
    return FALSE;
}

BOOL KnifeDialogShow(HWND owner, HINSTANCE instance, HWND viewport, const double center[3], double radius)
{
    if (g_Knife) { SetForegroundWindow(g_Knife); return TRUE; }
    memset(&g_KnifePlane, 0, sizeof(g_KnifePlane));
    memcpy(g_KnifePlane.position, center, sizeof(g_KnifePlane.position));
    g_KnifePlane.normal[1] = 1;
    g_KnifeRadius = radius;
    g_KnifeViewport = viewport;
    if (!CreateDialog(instance, MAKEINTRESOURCE(IDD_KNIFE), owner, KnifeDialogProc))
    { g_KnifeViewport = NULL; return FALSE; }
    ShowWindow(g_Knife, SW_SHOW); SetForegroundWindow(g_Knife);
    return TRUE;
}

void KnifeDialogClose(void)
{
    HWND viewport = g_KnifeViewport;
    if (g_Knife) { ViewportCancelTransform(viewport); DestroyWindow(g_Knife); }
    if (IsWindow(viewport)) { SetFocus(viewport); }
}

BOOL KnifeDialogHandleMessage(MSG *message)
{
    if (!g_Knife || !message) { return FALSE; }
    if (message->hwnd == g_KnifeViewport)
    {
        if (message->message == WM_KEYDOWN && message->wParam == VK_ESCAPE)
        {
            if (ViewportIsTransforming(g_KnifeViewport)) { return FALSE; }
            if (g_KnifePick) { KnifeCancelPick(); } else { KnifeDialogClose(); }
            return TRUE;
        }
        if (message->message == WM_LBUTTONDOWN || message->message == WM_LBUTTONDBLCLK)
        {
            double point[3];
            SetFocus(g_KnifeViewport);
            if (g_KnifePick && ViewportGetKnifePoint(g_KnifeViewport,
                GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam), point))
            {
                memcpy(g_KnifePlane.position, point, sizeof(point));
                g_KnifePick = FALSE;
                for (int i = 0; i < 6; i++) { EnableWindow(GetDlgItem(g_Knife, IDC_KNIFE_PX + i), TRUE); }
                SetDlgItemText(g_Knife, IDC_KNIFE_PICK, "Pick Point");
                KnifeWritePlane();
            }
            else if (g_KnifePick)
            { SetDlgItemText(g_Knife, IDC_KNIFE_STATUS, "No background surface hit. Click a visible background face."); }
            else
            { ViewportBeginKnifeTransform(g_KnifeViewport, GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam)); }
            return TRUE; /* Keep the original face selection while positioning the knife. */
        }
        if (message->message == WM_LBUTTONUP) { return !ViewportIsTransforming(g_KnifeViewport); }
    }
    if (message->hwnd != g_Knife && !IsChild(g_Knife, message->hwnd)) { return FALSE; }
    if (!IsDialogMessage(g_Knife, message))
    { TranslateMessage(message); DispatchMessage(message); }
    return TRUE;
}
