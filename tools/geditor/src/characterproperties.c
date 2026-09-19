#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include "characterproperties.h"
#include "characterload.h"

#define CHARACTERPROPERTIES_CLASS "GEditorCharacterProperties"
enum { CHARACTER_RIGHT_LABEL, CHARACTER_RIGHT, CHARACTER_LEFT_LABEL, CHARACTER_LEFT,
       CHARACTER_HAT_LABEL, CHARACTER_HAT, CHARACTER_DETAILS, CHARACTER_CONTROLS };
typedef struct CharacterPropertiesState {
    HWND controls[CHARACTER_CONTROLS];
    SetupCharacterWeaponEdit binding;
    BOOL selected, updating, committing;
    int scroll, wheel;
} CharacterPropertiesState;
static CharacterPropertiesState *CharacterPropertiesGetState(HWND hwnd)
{ return (CharacterPropertiesState *)GetWindowLongPtr(hwnd, GWLP_USERDATA); }
static int CharacterPropertiesTextHeight(HWND hwnd, int width)
{
    char text[2048]; RECT rect = {0,0,width,0};
    HDC dc = GetDC(hwnd);
    HFONT old = SelectObject(dc, (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0));
    GetWindowText(hwnd, text, sizeof(text));
    DrawText(dc, text, -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old); ReleaseDC(hwnd, dc);
    return rect.bottom + 6;
}
static void CharacterPropertiesLayout(HWND hwnd, CharacterPropertiesState *state)
{
    RECT rect; GetClientRect(hwnd, &rect);
    int width = max(1, rect.right - 8);
    int details = CharacterPropertiesTextHeight(state->controls[CHARACTER_DETAILS], width);
    int height = 168 + details;
    state->scroll = max(0, min(state->scroll, max(0, height - rect.bottom)));
    SCROLLINFO si = {sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS, 0, height - 1, (UINT)max(0,rect.bottom), state->scroll, 0};
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    for (int slot = 0; slot < 3; slot++)
    {
        int label = slot * 2;
        MoveWindow(state->controls[label], 4, 4 + slot * 54 - state->scroll, width, 18, TRUE);
        MoveWindow(state->controls[label+1], 4, 24 + slot * 54 - state->scroll, width, 320, TRUE);
    }
    MoveWindow(state->controls[CHARACTER_DETAILS], 4, 168 - state->scroll, width, details, TRUE);
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}
static void CharacterPropertiesChoices(HWND combo, int item)
{
    DWORD count; const SetupWeaponChoice *choices = SetupWeaponChoices(&count);
    int selected = -1;
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (DWORD i = 0; i < count; i++)
    {
        int row = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)choices[i].name);
        if (row < 0) { continue; }
        SendMessage(combo, CB_SETITEMDATA, row, choices[i].item);
        if (choices[i].item == item) { selected = row; }
    }
    if (selected < 0)
    {
        char text[80];
        if (item == SETUP_WEAPON_MIXED) { snprintf(text, sizeof(text), "Multiple setup variants"); }
        else { snprintf(text, sizeof(text), "Item %d (current)", item); }
        selected = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)text);
        if (selected >= 0) { SendMessage(combo, CB_SETITEMDATA, selected, item); }
    }
    SendMessage(combo, CB_SETCURSEL, selected, 0);
}
static void CharacterPropertiesApply(HWND hwnd, CharacterPropertiesState *state, int hand)
{
    if (!state || !state->selected || state->updating || state->committing || hand < 0 || hand > 1) { return; }
    HWND combo = state->controls[hand ? CHARACTER_LEFT : CHARACTER_RIGHT];
    int row = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (row == CB_ERR) { return; }
    int item = (int)SendMessage(combo, CB_GETITEMDATA, row, 0);
    if (!SetupWeaponChoiceForItem(item)) { return; }
    SetupCharacterWeaponEdit edit = state->binding;
    edit.hand = hand; edit.item = item;
    state->committing = TRUE;
    SendMessage(GetParent(hwnd), CHARACTERPROPERTIES_WM_WEAPON_CHANGED, 0, (LPARAM)&edit);
    state->committing = FALSE;
}
static void CharacterPropertiesHatChoices(HWND combo, int model)
{
    DWORD count; const SetupHatChoice *choices = SetupHatChoices(&count);
    int selected = -1;
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (DWORD i = 0; i < count; i++)
    {
        int row = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)choices[i].name);
        if (row < 0) { continue; }
        SendMessage(combo, CB_SETITEMDATA, row, choices[i].model);
        if (choices[i].model == model) { selected = row; }
    }
    if (selected < 0)
    {
        char text[80];
        if (model == SETUP_HAT_MIXED) { snprintf(text, sizeof(text), "Multiple setup variants"); }
        else { snprintf(text, sizeof(text), "Model %d (current)", model); }
        selected = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)text);
        if (selected >= 0) { SendMessage(combo, CB_SETITEMDATA, selected, model); }
    }
    SendMessage(combo, CB_SETCURSEL, selected, 0);
}
static void CharacterPropertiesApplyHat(HWND hwnd, CharacterPropertiesState *state)
{
    if (!state || !state->selected || state->updating || state->committing) { return; }
    HWND combo = state->controls[CHARACTER_HAT];
    int row = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (row == CB_ERR) { return; }
    int model = (int)SendMessage(combo, CB_GETITEMDATA, row, 0);
    if (!SetupHatChoiceForModel(model)) { return; }
    SetupCharacterHatEdit edit = {state->binding.characterindex, state->binding.sourceoffset,
        state->binding.chrnum, model};
    state->committing = TRUE;
    SendMessage(GetParent(hwnd), CHARACTERPROPERTIES_WM_HAT_CHANGED, 0, (LPARAM)&edit);
    state->committing = FALSE;
}
BOOL CharacterPropertiesSetSelection(HWND panel, const SetupFile *setup, DWORD index)
{
    CharacterPropertiesState *state = CharacterPropertiesGetState(panel);
    SetupCharacterWeapons weapons;
    SetupCharacterHat hat;
    if (!state) { return FALSE; }
    state->updating = TRUE;
    if (!SetupFileGetCharacterWeapons(setup, index, &weapons)
        || !SetupFileGetCharacterHat(setup, index, &hat))
    {
        state->selected = FALSE; state->scroll = 0;
        for (int i = 0; i < CHARACTER_CONTROLS; i++) { EnableWindow(state->controls[i], FALSE); }
        state->updating = FALSE; return FALSE;
    }
    const SetupCharacter *chr = &setup->characters[index];
    if (!state->selected || state->binding.characterindex != index || state->binding.chrnum != chr->chrnum) { state->scroll = 0; }
    state->binding = (SetupCharacterWeaponEdit){.characterindex=index, .sourceoffset=chr->sourceoffset, .chrnum=chr->chrnum};
    state->selected = TRUE;
    for (int i = 0; i < CHARACTER_CONTROLS; i++) { EnableWindow(state->controls[i], TRUE); }
    CharacterPropertiesChoices(state->controls[CHARACTER_RIGHT], weapons.item[0]);
    CharacterPropertiesChoices(state->controls[CHARACTER_LEFT], weapons.item[1]);
    CharacterPropertiesHatChoices(state->controls[CHARACTER_HAT], hat.model);
    int bodyid, headid; CharacterModelDefinition body, head;
    BOOL randomhead = FALSE;
    const char *bodyname = "Unknown", *headname = "Included in body";
    if (CharacterResolveModels(chr, &bodyid, &headid))
    {
        if (CharacterGetModelDefinition(bodyid, &body)) { bodyname = body.filename; }
        if (headid >= 0 && CharacterGetModelDefinition(headid, &head))
        { headname = head.filename; randomhead = chr->headid < 0; }
    }
    char details[2048];
    snprintf(details, sizeof(details),
        "Character ID: %u\r\nBody: %s\r\nHead: %s%s\r\nPad: %u\r\nAI list: 0x%04X\r\nFlags: 0x%04X\r\n\r\n"
        "Drag an arrow or enter a world position.\r\nCharacters settle onto a stan floor when placed.",
        chr->chrnum, bodyname, headname, randomhead ? " (random preview)" : "",
        chr->pad, chr->ailistid, chr->flags);
    SetWindowText(state->controls[CHARACTER_DETAILS], details);
    state->updating = FALSE;
    CharacterPropertiesLayout(panel, state);
    return TRUE;
}
static LRESULT CALLBACK CharacterPropertiesWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    CharacterPropertiesState *state = CharacterPropertiesGetState(hwnd);
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT *)lp;
        state = calloc(1, sizeof(*state)); if (!state) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        const char *labels[CHARACTER_CONTROLS] = {"Right-hand weapon", "", "Left-hand weapon", "",
            "Hat", "", ""};
        for (int i = 0; i < CHARACTER_CONTROLS; i++)
        {
            BOOL combo = i == CHARACTER_RIGHT || i == CHARACTER_LEFT || i == CHARACTER_HAT;
            state->controls[i] = CreateWindowEx(0, combo ? "COMBOBOX" : "STATIC", labels[i],
                WS_CHILD | WS_VISIBLE | (combo ? WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST : SS_NOPREFIX),
                0,0,1,1, hwnd, (HMENU)(INT_PTR)(i+1), cs->hInstance, NULL);
            if (!state->controls[i]) { return -1; }
            SendMessage(state->controls[i], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
            if (combo) { SendMessage(state->controls[i], CB_SETMINVISIBLE, 12, 0); }
        }
        return 0;
    }
    case WM_SIZE: if (state) { CharacterPropertiesLayout(hwnd, state); } return 0;
    case WM_COMMAND:
        if (HIWORD(wp) == CBN_SELCHANGE)
        {
            if (LOWORD(wp) == CHARACTER_RIGHT+1) { CharacterPropertiesApply(hwnd, state, 0); }
            if (LOWORD(wp) == CHARACTER_LEFT+1) { CharacterPropertiesApply(hwnd, state, 1); }
            if (LOWORD(wp) == CHARACTER_HAT+1) { CharacterPropertiesApplyHat(hwnd, state); }
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (state)
        {
            state->wheel += GET_WHEEL_DELTA_WPARAM(wp);
            state->scroll -= state->wheel / WHEEL_DELTA * 48; state->wheel %= WHEEL_DELTA;
            CharacterPropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_VSCROLL:
        if (state)
        {
            SCROLLINFO si = {0}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            switch (LOWORD(wp))
            {
            case SB_LINEUP: state->scroll -= 24; break;
            case SB_LINEDOWN: state->scroll += 24; break;
            case SB_PAGEUP: state->scroll -= si.nPage; break;
            case SB_PAGEDOWN: state->scroll += si.nPage; break;
            case SB_THUMBTRACK: state->scroll = si.nTrackPos; break;
            case SB_TOP: state->scroll = 0; break;
            case SB_BOTTOM: state->scroll = si.nMax; break;
            }
            CharacterPropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_DESTROY: free(state); SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
BOOL CharacterPropertiesRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc = {0}; wc.lpfnWndProc = CharacterPropertiesWndProc; wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName = CHARACTERPROPERTIES_CLASS;
    return RegisterClass(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
HWND CharacterPropertiesCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, CHARACTERPROPERTIES_CLASS, "", WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL,
        0,0,1,1, parent, NULL, instance, NULL);
}
BOOL CharacterPropertiesHandleMessage(HWND panel, MSG *message)
{
    CharacterPropertiesState *state = CharacterPropertiesGetState(panel);
    return state && state->selected && IsWindowVisible(panel) && IsChild(panel, message->hwnd)
        && IsDialogMessage(panel, message);
}
