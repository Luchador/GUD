#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stanload.h"
typedef void *HWND;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM, LRESULT;
#define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))
enum { CB_RESETCONTENT, CB_ADDSTRING, CB_SETITEMDATA, CB_SETCURSEL,
    CB_GETCURSEL, CB_GETITEMDATA, RIGHTPANEL_WM_STAN_TYPE_CHANGED, CB_ERR = -1 };
typedef struct RightPanelState { HWND stantype; BOOL updatingstantype, showingstantype; } RightPanelState;
static struct { char name[32]; LRESULT data; } items[4];
static unsigned count, changes;
static LRESULT current, sent;
static HWND GetParent(HWND hwnd) { return hwnd; }
char *lstrcpyn(char *out, const char *in, int n) { snprintf(out, n, "%s", in); return out; }
static LRESULT SendMessage(HWND hwnd, unsigned msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case CB_RESETCONTENT: count = 0; current = -1; return 0;
    case CB_ADDSTRING:
        assert(count < 4); snprintf(items[count].name, sizeof(items[count].name), "%s", (char *)lparam);
        return count++;
    case CB_SETITEMDATA: assert(wparam < count); items[wparam].data = lparam; return 0;
    case CB_SETCURSEL: assert(wparam < count); current = wparam; return current;
    case CB_GETCURSEL: return current;
    case CB_GETITEMDATA: assert(wparam < count); return items[wparam].data;
    case RIGHTPANEL_WM_STAN_TYPE_CHANGED: changes++; sent = wparam; return TRUE;
    default: assert(0); return 0;
    }
}
#include "properties.inc"
int main(void)
{
    StanTile tiles[2] = {{.special=STAN_TYPE_NORMAL}, {.special=STAN_TYPE_LADDER}};
    StanFile stan = {.tiles=tiles, .tilecount=2}; DWORD selected[] = {0, 1};
    RightPanelState state = {0};
    RightPanelUpdateStanType(&state, &stan, selected, 1);
    assert(count == 3 && current == 0 && !changes && state.showingstantype && !state.updatingstantype);
    assert(!strcmp(items[0].name, "Normal") && items[0].data == 0);
    assert(!strcmp(items[1].name, "Ladder") && items[1].data == 3);
    assert(!strcmp(items[2].name, "Forced Crouch") && items[2].data == 1);
    for (unsigned i = 0; i < 3; i++)
    {
        current = i; RightPanelApplyStanType(NULL, &state);
        assert(changes == i + 1 && sent == items[i].data);
        tiles[0].special = sent;
        RightPanelUpdateStanType(&state, &stan, selected, 1);
        assert(current == i && !stan.dirty);
    }
    RightPanelUpdateStanType(&state, &stan, selected, 2);
    assert(count == 4 && current == 0 && !strcmp(items[0].name, "Mixed"));
    RightPanelApplyStanType(NULL, &state); assert(changes == 3);
    current = 2; RightPanelApplyStanType(NULL, &state); assert(changes == 4 && sent == STAN_TYPE_LADDER);
    tiles[0].special = 2;
    RightPanelUpdateStanType(&state, &stan, selected, 1);
    assert(count == 4 && !strcmp(items[0].name, "Unknown (0x2)"));
    RightPanelApplyStanType(NULL, &state); assert(changes == 4 && tiles[0].special == 2);
    current = -1; RightPanelApplyStanType(NULL, &state); assert(changes == 4);
    current = 1; state.updatingstantype = TRUE; RightPanelApplyStanType(NULL, &state); assert(changes == 4);
    state.updatingstantype = FALSE; state.showingstantype = FALSE;
    RightPanelApplyStanType(NULL, &state); assert(changes == 4);
    puts("PASS: properties labels/native values, current type, mixed/unknown placeholders, guarded editing and refresh without mutation.");
}
