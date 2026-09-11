/* Shared inspector controls. The setup API owns native encodings and edits;
 * this child window owns only its current values and uncommitted text. */
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <src/propconstants.h>
#include "objectproperties.h"
#include "modelload.h"

#define OBJECTPROPERTIES_CLASS "GEditorObjectProperties"
enum { OBJECT_TYPE, OBJECT_MODEL_LABEL, OBJECT_MODEL, OBJECT_MODEL_HELP,
       OBJECT_HEALTH_LABEL, OBJECT_HEALTH, OBJECT_HEALTH_HELP,
       OBJECT_IDENTITY, OBJECT_STATUS, OBJECT_CONTROL_COUNT };

/* New specialized sections can reuse these controls and the property-edit
 * request, while complex fields remain free to supply their own UI. */
static const struct {
    SetupObjectProperty property;
    int label, input, help;
    const char *name, *description;
} g_SharedProperties[] = {
    {SETUP_OBJECT_MODEL, OBJECT_MODEL_LABEL, OBJECT_MODEL, OBJECT_MODEL_HELP,
     "Model", "Appearance only; the object keeps its type and behavior."},
    {SETUP_OBJECT_HEALTH, OBJECT_HEALTH_LABEL, OBJECT_HEALTH, OBJECT_HEALTH_HELP,
     "Health", "Damage needed to destroy the object. Its type and flags also affect destructibility."}
};

typedef struct ObjectPropertiesState {
    HWND controls[OBJECT_CONTROL_COUNT];
    DWORD objectindex;
    ULONG_PTR document;
    SetupObjectProperties properties;
    BOOL selected, updating, edited, committing;
    int scroll, wheelremainder;
    char projectdir[MAX_PATH];
} ObjectPropertiesState;

static ObjectPropertiesState *ObjectPropertiesGetState(HWND hwnd)
{ return (ObjectPropertiesState *)GetWindowLongPtr(hwnd, GWLP_USERDATA); }

static void ObjectPropertiesLayout(HWND hwnd, ObjectPropertiesState *state)
{
    RECT client, bounds[OBJECT_CONTROL_COUNT];
    SCROLLINFO info = {0};
    int width, y = 4;
    HDC dc = GetDC(hwnd);
    HFONT old = (HFONT)SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    GetClientRect(hwnd, &client);
    width = max(1, client.right - 8);
    for (int i = 0; i < OBJECT_CONTROL_COUNT; i++)
    {
        int height = 24;
        if (i != OBJECT_MODEL && i != OBJECT_HEALTH)
        {
            char text[512]; RECT rect = {0, 0, width, 0};
            GetWindowText(state->controls[i], text, sizeof(text));
            DrawText(dc, text, -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            height = rect.bottom;
        }
        SetRect(&bounds[i], 4, y, width + 4, y + height);
        y += height + (i == OBJECT_MODEL_HELP || i == OBJECT_HEALTH_HELP ? 16 : 6);
    }
    SelectObject(dc, old); ReleaseDC(hwnd, dc);
    state->scroll = max(0, min(state->scroll, y - client.bottom));
    info.cbSize = sizeof(info); info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    info.nMax = y - 1; info.nPage = max(0, client.bottom); info.nPos = state->scroll;
    SetScrollInfo(hwnd, SB_VERT, &info, FALSE);
    for (int i = 0; i < OBJECT_CONTROL_COUNT; i++)
    {
        const RECT *r = &bounds[i];
        /* Do not copy stale child pixels while scrolling or resizing. */
        SetWindowPos(state->controls[i], NULL, r->left, r->top - state->scroll,
            r->right - r->left, i == OBJECT_MODEL ? 320 : r->bottom - r->top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
    }
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

static void ObjectPropertiesStatus(HWND hwnd, ObjectPropertiesState *state, const char *text)
{
    SetWindowText(state->controls[OBJECT_STATUS], text);
    ObjectPropertiesLayout(hwnd, state);
}

static void ObjectPropertiesResetHealth(ObjectPropertiesState *state)
{
    char text[64] = "";
    if (state->selected) { snprintf(text, sizeof(text), "%.15g", state->properties.health); }
    state->updating = TRUE;
    SetWindowText(state->controls[OBJECT_HEALTH], text);
    SendMessage(state->controls[OBJECT_HEALTH], EM_EMPTYUNDOBUFFER, 0, 0);
    state->updating = FALSE;
    state->edited = FALSE;
}

static BOOL ObjectPropertiesParseHealth(const char *text, double *value)
{
    char *end;
    errno = 0;
    *value = strtod(text, &end);
    if (end == text || errno == ERANGE || !isfinite(*value)) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    return !*end && *value >= 0 && *value <= 2147483647.0 / 65536.0;
}

static void ObjectPropertiesApply(HWND hwnd, ObjectPropertiesState *state,
                                   SetupObjectProperty property, double value)
{
    SetupObjectPropertyEdit edit = {0};
    if (!state->selected || state->updating || state->committing) { return; }
    edit.objectindex = state->objectindex;
    edit.sourceoffset = state->properties.object.sourceoffset;
    edit.type = state->properties.object.type;
    edit.property = property;
    edit.value = value;
    state->committing = TRUE;
    SendMessage(GetParent(hwnd), OBJECTPROPERTIES_WM_CHANGED, 0, (LPARAM)&edit);
    state->committing = FALSE;
}

static void ObjectPropertiesApplyHealth(HWND hwnd, ObjectPropertiesState *state)
{
    char text[64]; double value;
    if (!state->selected || !state->edited || state->updating || state->committing) { return; }
    GetWindowText(state->controls[OBJECT_HEALTH], text, sizeof(text));
    if (!ObjectPropertiesParseHealth(text, &value))
    {
        ObjectPropertiesStatus(hwnd, state, "Enter health from 0 to 32767.99998474121. Health has not changed.");
        return;
    }
    ObjectPropertiesApply(hwnd, state, SETUP_OBJECT_HEALTH, value);
    /* Selection refresh supplies the actual quantized value, even on a no-op. */
    ObjectPropertiesResetHealth(state);
}

static int ObjectPropertiesModelChoice(HWND combo, int modelid)
{
    int count = (int)SendMessage(combo, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; i++)
    { if ((int)SendMessage(combo, CB_GETITEMDATA, i, 0) == modelid) { return i; } }
    return -1;
}

static void ObjectPropertiesApplyModel(HWND hwnd, ObjectPropertiesState *state)
{
    HWND combo = state->controls[OBJECT_MODEL];
    int choice = (int)SendMessage(combo, CB_GETCURSEL, 0, 0), model;
    if (choice == CB_ERR || !state->selected) { return; }
    model = (int)SendMessage(combo, CB_GETITEMDATA, choice, 0);
    if (model != state->properties.object.modelid)
    { ObjectPropertiesApply(hwnd, state, SETUP_OBJECT_MODEL, model); }
}

static BOOL ObjectPropertiesLoadModels(ObjectPropertiesState *state, const char *projectdir)
{
    HWND combo = state->controls[OBJECT_MODEL];
    if (!projectdir) { projectdir = ""; }
    if (!lstrcmpi(projectdir, state->projectdir)) { return TRUE; }
    state->updating = TRUE;
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (int model = 0; *projectdir; model++)
    {
        const char *name;
        char path[MAX_PATH];
        DWORD attributes;
        int choice;
        if (!ModelGetPropDefinition(model, &name, NULL)) { break; }
        if (!name || !*name) { continue; }
        if (snprintf(path, sizeof(path), "%s\\models\\objects\\%s.gltf", projectdir, name) >= (int)sizeof(path)) { continue; }
        attributes = GetFileAttributes(path);
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) { continue; }
        choice = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)name);
        if (choice < 0) { state->updating = FALSE; state->projectdir[0] = '\0'; return FALSE; }
        SendMessage(combo, CB_SETITEMDATA, choice, model);
    }
    lstrcpyn(state->projectdir, projectdir, sizeof(state->projectdir));
    state->updating = FALSE;
    return TRUE;
}

static void ObjectPropertiesRevealControl(HWND hwnd, ObjectPropertiesState *state, HWND control)
{
    RECT client, rect;
    GetClientRect(hwnd, &client);
    GetWindowRect(control, &rect); MapWindowPoints(NULL, hwnd, (POINT *)&rect, 2);
    if (rect.top < 0) { state->scroll += rect.top - 4; }
    else if (rect.bottom > client.bottom) { state->scroll += rect.bottom - client.bottom + 4; }
    else { return; }
    ObjectPropertiesLayout(hwnd, state);
}

static LRESULT CALLBACK ObjectPropertiesWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ObjectPropertiesState *state = ObjectPropertiesGetState(hwnd);
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        state = calloc(1, sizeof(*state));
        if (!state) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        for (int i = 0; i < OBJECT_CONTROL_COUNT; i++)
        {
            BOOL combo = i == OBJECT_MODEL, edit = i == OBJECT_HEALTH;
            state->controls[i] = CreateWindowEx(edit ? WS_EX_CLIENTEDGE : 0,
                combo ? "COMBOBOX" : edit ? "EDIT" : "STATIC", "",
                WS_CHILD | WS_VISIBLE | (combo ? WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL
                    : edit ? WS_TABSTOP | ES_AUTOHSCROLL : SS_NOPREFIX),
                0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)(100 + i), cs->hInstance, NULL);
            if (!state->controls[i]) { return -1; }
            SendMessage(state->controls[i], WM_SETFONT, (WPARAM)font, FALSE);
        }
        for (unsigned int i = 0; i < sizeof(g_SharedProperties) / sizeof(g_SharedProperties[0]); i++)
        {
            SetWindowText(state->controls[g_SharedProperties[i].label], g_SharedProperties[i].name);
            SetWindowText(state->controls[g_SharedProperties[i].help], g_SharedProperties[i].description);
        }
        SendMessage(state->controls[OBJECT_MODEL], CB_SETDROPPEDWIDTH, 360, 0);
        SendMessage(state->controls[OBJECT_HEALTH], EM_SETLIMITTEXT, 63, 0);
        return 0;
    }
    case WM_SIZE:
        if (state) { ObjectPropertiesLayout(hwnd, state); }
        return 0;
    case WM_COMMAND:
        if (!state || state->updating) { return 0; }
        if ((HWND)lparam == state->controls[OBJECT_HEALTH])
        {
            if (HIWORD(wparam) == EN_CHANGE) { state->edited = TRUE; }
            if (HIWORD(wparam) == EN_KILLFOCUS) { ObjectPropertiesApplyHealth(hwnd, state); }
            if (HIWORD(wparam) == EN_SETFOCUS) { ObjectPropertiesRevealControl(hwnd, state, (HWND)lparam); }
        }
        if ((HWND)lparam == state->controls[OBJECT_MODEL])
        {
            if (HIWORD(wparam) == CBN_SELENDOK || (HIWORD(wparam) == CBN_SELCHANGE
                && !SendMessage((HWND)lparam, CB_GETDROPPEDSTATE, 0, 0)))
            { ObjectPropertiesApplyModel(hwnd, state); }
            if (HIWORD(wparam) == CBN_SELENDCANCEL)
            { SendMessage((HWND)lparam, CB_SETCURSEL, ObjectPropertiesModelChoice((HWND)lparam, state->properties.object.modelid), 0); }
            if (HIWORD(wparam) == CBN_SETFOCUS) { ObjectPropertiesRevealControl(hwnd, state, (HWND)lparam); }
        }
        return 0;
    case WM_VSCROLL:
        if (state)
        {
            SCROLLINFO info = {0};
            info.cbSize = sizeof(info); info.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &info);
            switch (LOWORD(wparam))
            {
            case SB_LINEUP: state->scroll -= 24; break;
            case SB_LINEDOWN: state->scroll += 24; break;
            case SB_PAGEUP: state->scroll -= (int)info.nPage; break;
            case SB_PAGEDOWN: state->scroll += (int)info.nPage; break;
            case SB_TOP: state->scroll = 0; break;
            case SB_BOTTOM: state->scroll = info.nMax; break;
            case SB_THUMBTRACK: case SB_THUMBPOSITION: state->scroll = info.nTrackPos; break;
            }
            ObjectPropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (state)
        {
            state->wheelremainder += GET_WHEEL_DELTA_WPARAM(wparam);
            state->scroll -= 72 * (state->wheelremainder / WHEEL_DELTA);
            state->wheelremainder %= WHEEL_DELTA;
            ObjectPropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wparam, GetSysColor(COLOR_WINDOW));
        SetBkMode((HDC)wparam, TRANSPARENT);
        if (state && (HWND)lparam == state->controls[OBJECT_STATUS])
        { SetTextColor((HDC)wparam, GetSysColor(COLOR_GRAYTEXT)); }
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_NCDESTROY:
        free(state); SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); break;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

BOOL ObjectPropertiesRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = ObjectPropertiesWndProc; wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = OBJECTPROPERTIES_CLASS;
    return RegisterClass(&wc) != 0;
}

HWND ObjectPropertiesCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, OBJECTPROPERTIES_CLASS, NULL,
        WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 1, 1, parent, NULL, instance, NULL);
}

BOOL ObjectPropertiesSetSelection(HWND panel, const SetupFile *setup, DWORD index, const char *projectdir)
{
    ObjectPropertiesState *state = ObjectPropertiesGetState(panel);
    SetupObjectProperties properties;
    const char *why;
    char text[512], placement[64];
    BOOL same;
    int choice;
    if (!state) { return FALSE; }
    if (!setup || !SetupFileGetObjectProperties(setup, index, &properties, &why))
    {
        state->selected = FALSE; state->edited = FALSE; state->document = 0;
        ObjectPropertiesResetHealth(state);
        return FALSE;
    }
    same = state->selected && state->objectindex == index && state->document == (ULONG_PTR)setup->data
        && state->properties.object.sourceoffset == properties.object.sourceoffset
        && state->properties.object.type == properties.object.type;
    if (!same) { state->scroll = 0; state->edited = FALSE; }
    if (state->properties.health != properties.health) { state->edited = FALSE; }
    state->selected = TRUE; state->objectindex = index; state->document = (ULONG_PTR)setup->data;
    state->properties = properties;
    if (!ObjectPropertiesLoadModels(state, projectdir)) { return FALSE; }
    snprintf(text, sizeof(text), "Type: %s", SetupObjectTypeName(properties.object.type));
    SetWindowText(state->controls[OBJECT_TYPE], text);
    state->updating = TRUE;
    choice = ObjectPropertiesModelChoice(state->controls[OBJECT_MODEL], properties.object.modelid);
    if (choice < 0)
    {
        const char *name = NULL;
        ModelGetPropDefinition(properties.object.modelid, &name, NULL);
        if (name) { snprintf(text, sizeof(text), "%s (unavailable)", name); }
        else { snprintf(text, sizeof(text), "Unknown model %d", properties.object.modelid); }
        choice = (int)SendMessage(state->controls[OBJECT_MODEL], CB_ADDSTRING, 0, (LPARAM)text);
        if (choice >= 0) { SendMessage(state->controls[OBJECT_MODEL], CB_SETITEMDATA, choice, properties.object.modelid); }
    }
    SendMessage(state->controls[OBJECT_MODEL], CB_SETCURSEL, choice, 0);
    state->updating = FALSE;
    if (!state->edited) { ObjectPropertiesResetHealth(state); }
    if (properties.object.flags & PROPFLAG_ASSIGNEDTOCHR)
    { snprintf(placement, sizeof(placement), "Character: %d", properties.object.pad); }
    else if (properties.object.pad < 0)
    { snprintf(placement, sizeof(placement), "Placement reference: %d", properties.object.pad); }
    else if (properties.object.type == PROPDEF_DOOR || properties.object.pad >= 10000)
    { snprintf(placement, sizeof(placement), "Bound pad: %d", properties.object.pad >= 10000 ? properties.object.pad - 10000 : properties.object.pad); }
    else { snprintf(placement, sizeof(placement), "Pad: %d", properties.object.pad); }
    snprintf(text, sizeof(text), "Object index: %lu\r\nModel ID: %d\r\n%s\r\nExtra scale: %.6g",
        (unsigned long)index, properties.object.modelid, placement, properties.object.extrascale / 256.0);
    SetWindowText(state->controls[OBJECT_IDENTITY], text);
    if (!state->edited) { SetWindowText(state->controls[OBJECT_STATUS], "Enter or leave a field to apply. Escape cancels typing."); }
    ObjectPropertiesLayout(panel, state);
    return TRUE;
}

BOOL ObjectPropertiesHandleMessage(HWND panel, MSG *message)
{
    ObjectPropertiesState *state = ObjectPropertiesGetState(panel);
    HWND focus = GetFocus();
    if (!state || !state->selected || message->message != WM_KEYDOWN
        || focus != state->controls[OBJECT_HEALTH]) { return FALSE; }
    if (message->wParam == VK_RETURN)
    { ObjectPropertiesApplyHealth(panel, state); return TRUE; }
    if (message->wParam == VK_ESCAPE)
    {
        ObjectPropertiesResetHealth(state);
        ObjectPropertiesStatus(panel, state, "Enter or leave a field to apply. Escape cancels typing.");
        return TRUE;
    }
    if (message->wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)
        && SendMessage(focus, EM_CANUNDO, 0, 0))
    { SendMessage(focus, WM_UNDO, 0, 0); return TRUE; }
    return FALSE;
}
