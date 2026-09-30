#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int BOOL;
typedef uintptr_t HWND, WPARAM, DWORD;
typedef intptr_t LPARAM, LRESULT;
typedef struct { int left, top, right, bottom; } RECT;
typedef struct { unsigned int cbSize; RECT rcWork; } MONITORINFO;
#define TRUE 1
#define FALSE 0
#define MENU 1
#define ANCHOR 2
#define FRAME 3
#define CHECK 4
#define TOOLBAR 5
#define GEOMETRY 6
#define VISIBILITY_CHECK_COUNT 5
#define VISIBILITY_WIDTH 280
#define VISIBILITY_HEIGHT 184
#define EDITOR_TOOL_COUNT 5
#define WM_COMMAND 0x111
#define WM_APP 0x8000
#define TOOLTOOLBAR_WM_MENU (WM_APP + 53)
#define EDITTOOL_WM_SELECT (WM_APP + 1)
#define BN_CLICKED 0
#define LOWORD(x) ((unsigned int)(x) & 0xffff)
#define HIWORD(x) (((unsigned int)(x) >> 16) & 0xffff)
#define BM_SETSTATE 1
#define GWLP_USERDATA 2
#define GW_OWNER 3
#define SW_HIDE 0
#define HWND_TOP 0
#define MONITOR_DEFAULTTONEAREST 0
#define SWP_SHOWWINDOW 0x40
#define SWP_NOACTIVATE 0x10
#define SWP_NOOWNERZORDER 0x200
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#include "types.inc"
typedef struct { HWND menus[TOOLTOOLBAR_MENU_COUNT]; } ToolToolbarState;
static ToolToolbarState toolbar;
static VisibilityMenuState state;
static BOOL visible, pressed[7], failshow, cancelactivate, cancelfocus, clickpending;
static HWND focus, active;
static int posted, geometrycalls;
static WPARAM queuedkind;
static LPARAM queuedanchor;
static RECT placement;
void VisibilityMenuClose(HWND hwnd, BOOL restorefocus);

static intptr_t GetWindowLongPtr(HWND hwnd, int index) { return hwnd == MENU ? (intptr_t)&state : 0; }
static BOOL IsWindow(HWND hwnd) { return hwnd >= MENU && hwnd <= GEOMETRY; }
static BOOL IsWindowVisible(HWND hwnd) { return hwnd == MENU && visible; }
static HWND GetWindow(HWND hwnd, int which) { return FRAME; }
static HWND GetParent(HWND hwnd) { return FRAME; }
static void ShowWindow(HWND hwnd, int mode) { assert(hwnd == MENU && mode == SW_HIDE); visible = FALSE; }
static HWND SetActiveWindow(HWND hwnd)
{
    HWND old = active;
    active = hwnd;
    if (hwnd == MENU && cancelactivate) { VisibilityMenuClose(MENU, FALSE); active = FRAME; }
    return old;
}
static HWND SetFocus(HWND hwnd)
{
    HWND old = focus;
    focus = hwnd;
    if (hwnd == CHECK && cancelfocus) { VisibilityMenuClose(MENU, FALSE); focus = FRAME; }
    return old;
}
static BOOL GetWindowRect(HWND hwnd, RECT *rect)
{ *rect = (RECT){1600, 40, 1700, 72}; return IsWindow(hwnd); }
static int MonitorFromRect(const RECT *rect, int flags) { return 1; }
static BOOL GetMonitorInfo(int monitor, MONITORINFO *info)
{ info->rcWork = (RECT){0, 0, 1920, 1080}; return TRUE; }
static BOOL SetWindowPos(HWND hwnd, HWND after, int x, int y, int w, int h, unsigned flags)
{
    assert(hwnd == MENU && (flags & SWP_SHOWWINDOW));
    if (failshow) { return FALSE; }
    placement = (RECT){x, y, x+w, y+h};
    visible = TRUE;
    if (!(flags & SWP_NOACTIVATE)) { SetActiveWindow(hwnd); }
    return TRUE;
}
static LRESULT SendMessage(HWND hwnd, unsigned msg, WPARAM wp, LPARAM lp)
{
    if (msg == BM_SETSTATE) { assert(IsWindow(hwnd)); pressed[hwnd] = wp != 0; return 0; }
    assert(msg == TOOLTOOLBAR_WM_MENU && wp != TOOLTOOLBAR_MENU_VISIBILITY);
    assert(pressed[GEOMETRY]); geometrycalls++;
    return 0;
}
BOOL PostMessage(HWND hwnd, unsigned msg, WPARAM wp, LPARAM lp)
{
    assert(hwnd == FRAME && msg == TOOLTOOLBAR_WM_MENU && clickpending);
    posted++; queuedkind = wp; queuedanchor = lp;
    return TRUE;
}
#include "popup.inc"
#include "toolbar.inc"

static void Reset(void)
{
    memset(&state, 0, sizeof(state)); memset(&toolbar, 0, sizeof(toolbar)); memset(pressed, 0, sizeof(pressed));
    state.anchor = ANCHOR; state.checks[0] = CHECK;
    toolbar.menus[TOOLTOOLBAR_MENU_VISIBILITY] = ANCHOR;
    toolbar.menus[TOOLTOOLBAR_MENU_VERTEX] = GEOMETRY;
    visible = failshow = cancelactivate = cancelfocus = clickpending = FALSE;
    posted = geometrycalls = 0; focus = ANCHOR; active = FRAME;
}
int main(void)
{
    Reset(); failshow = TRUE; pressed[ANCHOR] = TRUE;
    VisibilityMenuOpen(MENU, ANCHOR);
    assert(!visible && !pressed[ANCHOR] && focus == ANCHOR);

    Reset(); cancelactivate = TRUE; VisibilityMenuOpen(MENU, ANCHOR);
    assert(!visible && !pressed[ANCHOR]);
    Reset(); cancelfocus = TRUE; VisibilityMenuOpen(MENU, ANCHOR);
    assert(!visible && !pressed[ANCHOR]);

    Reset(); pressed[ANCHOR] = TRUE; focus = FRAME;
    VisibilityMenuClose(MENU, TRUE); /* Already hidden: clear stale highlight without stealing focus. */
    assert(!pressed[ANCHOR] && focus == FRAME);

    Reset(); clickpending = TRUE; ToolbarClick(TOOLTOOLBAR_MENU_VISIBILITY);
    assert(posted == 1 && !visible && !pressed[ANCHOR]);
    clickpending = FALSE;
    assert(queuedkind == TOOLTOOLBAR_MENU_VISIBILITY && queuedanchor == ANCHOR);
    VisibilityMenuOpen(MENU, (HWND)queuedanchor);
    assert(visible && pressed[ANCHOR] && active == MENU && focus == CHECK);
    assert(placement.right == 1700 && placement.top == 72);
    VisibilityMenuOpen(MENU, ANCHOR); /* Toggle closed. */
    assert(!visible && !pressed[ANCHOR] && focus == ANCHOR && active == FRAME);
    for (int i = 0; i < 100; i++) {
        VisibilityMenuOpen(MENU, ANCHOR); assert(visible && pressed[ANCHOR]);
        VisibilityMenuClose(MENU, TRUE); assert(!visible && !pressed[ANCHOR]);
    }
    Reset(); ToolbarClick(TOOLTOOLBAR_MENU_VERTEX);
    assert(geometrycalls == 1 && posted == 0 && !pressed[GEOMETRY]);
    puts("PASS: queued Visibility opening; show failure and reentrant focus dismissal; hidden-button cleanup; repeated open/close; synchronous geometry menus.");
    return 0;
}
