#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "characterproperties.h"
#include "characterload.h"
#include "patrolpaths.h"

#define CHARACTERPROPERTIES_CLASS "GEditorCharacterProperties"
enum { CHARACTER_RIGHT_LABEL, CHARACTER_RIGHT, CHARACTER_LEFT_LABEL, CHARACTER_LEFT,
       CHARACTER_HAT_LABEL, CHARACTER_HAT, CHARACTER_BEHAVIOR_LABEL, CHARACTER_BEHAVIOR,
       CHARACTER_PATROL_LABEL, CHARACTER_PATROL,
       CHARACTER_CUSTOM_HEALTH, CHARACTER_HEALTH_LABEL, CHARACTER_HEALTH,
       CHARACTER_ARMOR_LABEL, CHARACTER_ARMOR,
       CHARACTER_DETAILS, CHARACTER_CONTROLS };
typedef struct CharacterPropertiesState {
    HWND controls[CHARACTER_CONTROLS];
    SetupCharacterWeaponEdit binding;
    unsigned short ailistid;
    SetupCharacterHealth health;
    BOOL healthvalid, healthedited;
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
    int height = 410 + details;
    state->scroll = max(0, min(state->scroll, max(0, height - rect.bottom)));
    SCROLLINFO si = {sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS, 0, height - 1, (UINT)max(0,rect.bottom), state->scroll, 0};
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    for (int slot = 0; slot < 5; slot++)
    {
        int label = slot * 2;
        MoveWindow(state->controls[label], 4, 4 + slot * 54 - state->scroll, width, 18, TRUE);
        MoveWindow(state->controls[label+1], 4, 24 + slot * 54 - state->scroll, width, 320, TRUE);
    }
    MoveWindow(state->controls[CHARACTER_CUSTOM_HEALTH],4,276-state->scroll,width,22,TRUE);
    MoveWindow(state->controls[CHARACTER_HEALTH_LABEL],4,304-state->scroll,width,18,TRUE);
    MoveWindow(state->controls[CHARACTER_HEALTH],4,324-state->scroll,width,23,TRUE);
    MoveWindow(state->controls[CHARACTER_ARMOR_LABEL],4,356-state->scroll,width,18,TRUE);
    MoveWindow(state->controls[CHARACTER_ARMOR],4,376-state->scroll,width,23,TRUE);
    MoveWindow(state->controls[CHARACTER_DETAILS], 4, 410 - state->scroll, width, details, TRUE);
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
static void CharacterPropertiesBehaviorChoices(HWND combo, unsigned short id)
{
    DWORD count; const SetupBehaviorChoice *choices = SetupCharacterBehaviorChoices(&count);
    int selected = -1;
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (DWORD i = 0; i < count; i++)
    {
        int row = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)choices[i].name);
        if (row < 0) { continue; }
        SendMessage(combo, CB_SETITEMDATA, row, choices[i].id);
        if (choices[i].id == id) { selected = row; }
    }
    if (selected < 0)
    {
        char text[80];
        snprintf(text, sizeof(text), "Action Block 0x%04X (current)", id);
        selected = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)text);
        if (selected >= 0) { SendMessage(combo, CB_SETITEMDATA, selected, id); }
    }
    SendMessage(combo, CB_SETCURSEL, selected, 0);
}
static void CharacterPropertiesApplyBehavior(HWND hwnd, CharacterPropertiesState *state)
{
    if (!state || !state->selected || state->updating || state->committing) { return; }
    HWND combo = state->controls[CHARACTER_BEHAVIOR];
    int row = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (row == CB_ERR) { return; }
    int id = (int)SendMessage(combo, CB_GETITEMDATA, row, 0);
    if (!SetupCharacterBehaviorChoiceForId(id)) { return; }
    SetupCharacterBehaviorEdit edit = {state->binding.characterindex, state->binding.sourceoffset,
        state->binding.chrnum, state->ailistid, id};
    state->committing = TRUE;
    SendMessage(GetParent(hwnd), CHARACTERPROPERTIES_WM_BEHAVIOR_CHANGED, 0, (LPARAM)&edit);
    state->committing = FALSE;
}
static void CharacterPropertiesPatrolChoices(HWND combo, const SetupFile *setup, DWORD character)
{
    PatrolDocument paths={0}; const char *why;
    int current=PatrolCharacterPath(setup,character), selected=-1;
    SendMessage(combo,CB_RESETCONTENT,0,0);
    int row=(int)SendMessage(combo,CB_ADDSTRING,0,(LPARAM)"None");
    SendMessage(combo,CB_SETITEMDATA,row,PATROL_NONE);
    if (current==PATROL_NONE) { selected=row; }
    if (PatrolDocumentLoad(setup,&paths,&why))
    {
        for (DWORD i=0;i<paths.count;i++)
        {
            char text[96]; PatrolPath *p=&paths.paths[i];
            snprintf(text,sizeof(text),"Patrol %u (%lu points, %s)",p->id,(unsigned long)p->count,
                (p->flags&1) ? "loop" : "back and forth");
            row=(int)SendMessage(combo,CB_ADDSTRING,0,(LPARAM)text);
            SendMessage(combo,CB_SETITEMDATA,row,p->id);
            if (current==p->id) { selected=row; }
        }
    }
    PatrolDocumentFree(&paths);
    if (selected<0)
    {
        char text[96];
        if (current>=0) { snprintf(text,sizeof(text),"Patrol %d (missing)",current); }
        else { snprintf(text,sizeof(text),"Custom / disabled Action Block"); }
        selected=(int)SendMessage(combo,CB_ADDSTRING,0,(LPARAM)text);
        SendMessage(combo,CB_SETITEMDATA,selected,PATROL_CUSTOM);
    }
    SendMessage(combo,CB_SETCURSEL,selected,0);
}
static void CharacterPropertiesApplyPatrol(HWND hwnd, CharacterPropertiesState *state)
{
    if (!state || !state->selected || state->updating || state->committing) { return; }
    HWND combo=state->controls[CHARACTER_PATROL];
    int row=(int)SendMessage(combo,CB_GETCURSEL,0,0);
    if (row==CB_ERR) { return; }
    int path=(int)SendMessage(combo,CB_GETITEMDATA,row,0);
    if (path<PATROL_NONE || path>255) { return; }
    PatrolAssignment edit={state->binding.characterindex,state->binding.sourceoffset,
        state->binding.chrnum,state->ailistid,path};
    state->committing=TRUE;
    SendMessage(GetParent(hwnd),CHARACTERPROPERTIES_WM_PATROL_CHANGED,0,(LPARAM)&edit);
    state->committing=FALSE;
}
static void CharacterPropertiesResetHealth(CharacterPropertiesState *state)
{
    char text[32];BOOL updating=state->updating;state->updating=TRUE;
    SendMessage(state->controls[CHARACTER_CUSTOM_HEALTH],BM_SETCHECK,state->health.custom ? BST_CHECKED : BST_UNCHECKED,0);
    snprintf(text,sizeof(text),"%u",state->health.health);SetWindowText(state->controls[CHARACTER_HEALTH],text);
    snprintf(text,sizeof(text),"%u",state->health.armor);SetWindowText(state->controls[CHARACTER_ARMOR],text);
    EnableWindow(state->controls[CHARACTER_CUSTOM_HEALTH],state->healthvalid);
    for(int i=CHARACTER_HEALTH_LABEL;i<=CHARACTER_ARMOR;i++)
        EnableWindow(state->controls[i],state->healthvalid && state->health.custom);
    state->healthedited=FALSE;state->updating=updating;
}
static BOOL CharacterPropertiesHealthValue(HWND control,DWORD minimum,DWORD *out)
{
    char text[64],*end;GetWindowText(control,text,sizeof(text));
    const char *start=text;while(isspace((unsigned char)*start)) start++;
    if(!isdigit((unsigned char)*start)) return FALSE;
    errno=0;unsigned long value=strtoul(start,&end,10);
    while(isspace((unsigned char)*end)) end++;
    if(errno || *end || value<minimum || value>65535) return FALSE;
    *out=(DWORD)value;return TRUE;
}
static void CharacterPropertiesApplyHealth(HWND hwnd,CharacterPropertiesState *state,BOOL toggle)
{
    if(!state || !state->selected || !state->healthvalid || state->updating || state->committing
        || (!toggle && !state->healthedited)) return;
    SetupCharacterHealthEdit edit={state->binding.characterindex,state->binding.sourceoffset,
        state->binding.chrnum,state->ailistid,
        SendMessage(state->controls[CHARACTER_CUSTOM_HEALTH],BM_GETCHECK,0,0)==BST_CHECKED,
        state->health.health,state->health.armor};
    if(edit.custom && (!CharacterPropertiesHealthValue(state->controls[CHARACTER_HEALTH],1,&edit.health)
        || !CharacterPropertiesHealthValue(state->controls[CHARACTER_ARMOR],0,&edit.armor)))
    {
        CharacterPropertiesResetHealth(state);
        MessageBox(hwnd,"Health must be a whole number from 1 to 65535. Armor must be a whole number from 0 to 65535.",
            "Character Properties",MB_ICONERROR);
        return;
    }
    state->committing=TRUE;state->healthedited=FALSE;
    SendMessage(GetParent(hwnd),CHARACTERPROPERTIES_WM_HEALTH_CHANGED,0,(LPARAM)&edit);
    state->committing=FALSE;
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
    if (!state->selected || state->binding.characterindex != index || state->binding.chrnum != chr->chrnum)
    { state->scroll = 0;state->healthedited=FALSE; }
    state->binding = (SetupCharacterWeaponEdit){.characterindex=index, .sourceoffset=chr->sourceoffset, .chrnum=chr->chrnum};
    state->ailistid = chr->ailistid;
    state->selected = TRUE;
    for (int i = 0; i < CHARACTER_CONTROLS; i++) { EnableWindow(state->controls[i], TRUE); }
    CharacterPropertiesChoices(state->controls[CHARACTER_RIGHT], weapons.item[0]);
    CharacterPropertiesChoices(state->controls[CHARACTER_LEFT], weapons.item[1]);
    CharacterPropertiesHatChoices(state->controls[CHARACTER_HAT], hat.model);
    const char *why;
    state->healthvalid=SetupFileGetCharacterHealth(setup,index,&state->health,&why);
    if(!state->healthvalid) state->health=(SetupCharacterHealth){FALSE,40,0,chr->ailistid};
    if(!state->healthedited) CharacterPropertiesResetHealth(state);
    CharacterPropertiesBehaviorChoices(state->controls[CHARACTER_BEHAVIOR], state->health.behavior);
    CharacterPropertiesPatrolChoices(state->controls[CHARACTER_PATROL], setup, index);
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
        "Create routes in Tools > Patrol Paths. Choosing a patrol replaces the starting behavior. None returns a patrolling guard to Standard guard.",
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
            "Hat", "", "Starting behavior", "", "Patrol", "", "Custom health and armor",
            "Starting health (40 = standard)", "", "Starting armor", "", ""};
        for (int i = 0; i < CHARACTER_CONTROLS; i++)
        {
            BOOL combo = i == CHARACTER_RIGHT || i == CHARACTER_LEFT || i == CHARACTER_HAT || i == CHARACTER_BEHAVIOR || i == CHARACTER_PATROL;
            BOOL edit=i==CHARACTER_HEALTH || i==CHARACTER_ARMOR,check=i==CHARACTER_CUSTOM_HEALTH;
            state->controls[i] = CreateWindowEx(edit ? WS_EX_CLIENTEDGE : 0, combo ? "COMBOBOX" : edit ? "EDIT" : check ? "BUTTON" : "STATIC", labels[i],
                WS_CHILD | WS_VISIBLE | (combo ? WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST
                    : edit ? WS_TABSTOP | ES_AUTOHSCROLL : check ? WS_TABSTOP | BS_AUTOCHECKBOX : SS_NOPREFIX),
                0,0,1,1, hwnd, (HMENU)(INT_PTR)(i+1), cs->hInstance, NULL);
            if (!state->controls[i]) { return -1; }
            SendMessage(state->controls[i], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
            if (combo) { SendMessage(state->controls[i], CB_SETMINVISIBLE, 12, 0); }
            if (edit) { SendMessage(state->controls[i],EM_SETLIMITTEXT,5,0); }
        }
        return 0;
    }
    case WM_SIZE: if (state) { CharacterPropertiesLayout(hwnd, state); } return 0;
    case WM_COMMAND:
        if(LOWORD(wp)==CHARACTER_CUSTOM_HEALTH+1 && HIWORD(wp)==BN_CLICKED)
            CharacterPropertiesApplyHealth(hwnd,state,TRUE);
        if(state && (LOWORD(wp)==CHARACTER_HEALTH+1 || LOWORD(wp)==CHARACTER_ARMOR+1))
        {
            if(HIWORD(wp)==EN_CHANGE && !state->updating) state->healthedited=TRUE;
            if(HIWORD(wp)==EN_KILLFOCUS) CharacterPropertiesApplyHealth(hwnd,state,FALSE);
        }
        if (HIWORD(wp) == CBN_SELCHANGE)
        {
            if (LOWORD(wp) == CHARACTER_RIGHT+1) { CharacterPropertiesApply(hwnd, state, 0); }
            if (LOWORD(wp) == CHARACTER_LEFT+1) { CharacterPropertiesApply(hwnd, state, 1); }
            if (LOWORD(wp) == CHARACTER_HAT+1) { CharacterPropertiesApplyHat(hwnd, state); }
            if (LOWORD(wp) == CHARACTER_BEHAVIOR+1) { CharacterPropertiesApplyBehavior(hwnd, state); }
            if (LOWORD(wp) == CHARACTER_PATROL+1) { CharacterPropertiesApplyPatrol(hwnd, state); }
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
    if(state && state->selected && IsWindowVisible(panel) && message->message==WM_KEYDOWN
        && (message->hwnd==state->controls[CHARACTER_HEALTH] || message->hwnd==state->controls[CHARACTER_ARMOR]))
    {
        if(message->wParam==VK_RETURN) { CharacterPropertiesApplyHealth(panel,state,FALSE);return TRUE; }
        if(message->wParam==VK_ESCAPE) { CharacterPropertiesResetHealth(state);return TRUE; }
    }
    return state && state->selected && IsWindowVisible(panel) && IsChild(panel, message->hwnd)
        && IsDialogMessage(panel, message);
}
