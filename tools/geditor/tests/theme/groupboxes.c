/* Exercise the actual group-box painter over controls already painted by
 * sibling windows, including the Model Editor's composite color picker. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
typedef int BOOL, LONG;
typedef unsigned UINT, DWORD;
typedef intptr_t LRESULT;
typedef wchar_t WCHAR;
typedef void *HFONT, *HGDIOBJ, *HPEN;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG x, y; } POINT;
typedef struct { LONG cx, cy; } SIZE;
typedef struct Window {
    struct Window *parent, *child, *next;
    RECT rect;
    DWORD style;
    BOOL visible;
} Window, *HWND;
typedef struct { unsigned char pixels[160][220], clip[160][220]; } Canvas, *HDC;
typedef struct { BOOL hot; } ThemeWindow;
enum { GWL_STYLE, GW_CHILD, GW_HWNDNEXT, BM_GETSTATE, WM_QUERYUISTATE };
enum { BS_PUSHBUTTON, BS_DEFPUSHBUTTON, BS_CHECKBOX, BS_AUTOCHECKBOX,
    BS_RADIOBUTTON, BS_3STATE, BS_AUTO3STATE, BS_GROUPBOX, BS_AUTORADIOBUTTON };
enum { THEME_BACKGROUND=1, THEME_BORDER, THEME_TEXT, THEME_MUTED,
    THEME_PRESSED, THEME_HOVER, THEME_INPUT, THEME_SELECTION, THEME_BUTTON };
#define TRUE 1
#define FALSE 0
#define BS_TYPEMASK 15
#define BS_LEFTTEXT 32
#define BS_MULTILINE 64
#define BST_PUSHED 1
#define BST_CHECKED 2
#define BST_INDETERMINATE 4
#define UISF_HIDEFOCUS 1
#define PS_SOLID 0
#define TRANSPARENT 0
#define DT_SINGLELINE 1
#define DT_VCENTER 2
#define DT_WORDBREAK 4
#define DT_CALCRECT 8
#define DT_CENTER 16
#define DT_END_ELLIPSIS 32
#define max(a,b) ((a) > (b) ? (a) : (b))
static HWND GetParent(HWND hwnd) { return hwnd->parent; }
static HWND GetWindow(HWND hwnd, int relation) { return relation == GW_CHILD ? hwnd->child : hwnd->next; }
static BOOL IsWindowVisible(HWND hwnd) { return hwnd->visible; }
static BOOL GetWindowRect(HWND hwnd, RECT *rect) { *rect = hwnd->rect; return TRUE; }
static void MapWindowPoints(HWND from, HWND to, POINT *points, UINT count)
{
    assert(!from && count == 2);
    for (UINT i=0; i<count; i++) { points[i].x -= to->rect.left; points[i].y -= to->rect.top; }
}
static void ExcludeClipRect(HDC dc, int left, int top, int right, int bottom)
{
    for (int y=0; y<160; y++) for (int x=0; x<220; x++)
        if (x>=left && x<right && y>=top && y<bottom) { dc->clip[y][x]=0; }
}
static intptr_t GetWindowLongPtr(HWND hwnd, int index) { return hwnd->style; }
static BOOL IsWindowEnabled(HWND hwnd) { return TRUE; }
static HWND GetFocus(void) { return NULL; }
static LRESULT SendMessage(HWND hwnd, UINT msg, uintptr_t wp, intptr_t lp) { return 0; }
static HFONT ThemeSelectFont(HWND hwnd, HDC dc) { return NULL; }
static void GetClientRect(HWND hwnd, RECT *rect)
{ *rect=(RECT){0,0,hwnd->rect.right-hwnd->rect.left,hwnd->rect.bottom-hwnd->rect.top}; }
static void GetWindowTextW(HWND hwnd, WCHAR *text, int count) { wcscpy(text,L"Materials"); }
static void SetBkMode(HDC dc, int mode) {}
static void SetTextColor(HDC dc, int color) {}
static int ThemeColor(int role) { return role; }
static void ThemeFill(HDC dc, const RECT *r, int role)
{
    for (int y=0; y<160; y++) for (int x=0; x<220; x++)
        if (dc->clip[y][x] && x>=r->left && x<r->right && y>=r->top && y<r->bottom)
        { dc->pixels[y][x]=(unsigned char)role; }
}
static void ThemeFrame(HDC dc, const RECT *r, int role)
{
    RECT edge=*r; edge.bottom=edge.top+1; ThemeFill(dc,&edge,role);
    edge=*r; edge.top=edge.bottom-1; ThemeFill(dc,&edge,role);
    edge=*r; edge.right=edge.left+1; ThemeFill(dc,&edge,role);
    edge=*r; edge.left=edge.right-1; ThemeFill(dc,&edge,role);
}
static int lstrlenW(const WCHAR *text) { return (int)wcslen(text); }
static void GetTextExtentPoint32W(HDC dc, const WCHAR *text, int count, SIZE *size)
{ *size=(SIZE){count*7,14}; }
static int ThemePixels(HWND hwnd, int value) { return value; }
static int DrawTextW(HDC dc, const WCHAR *text, int count, RECT *rect, UINT flags)
{ ThemeFill(dc,rect,THEME_TEXT); return 14; }
static HGDIOBJ SelectObject(HDC dc, HGDIOBJ object) { return NULL; }
static void OffsetRect(RECT *r, int x, int y) { r->left+=x;r->right+=x;r->top+=y;r->bottom+=y; }
static void InflateRect(RECT *r, int x, int y) { r->left-=x;r->right+=x;r->top-=y;r->bottom+=y; }
static HPEN CreatePen(int style, int width, int color) { return NULL; }
static HGDIOBJ ThemeBrush(int role) { return NULL; }
static void Ellipse(HDC dc, int left, int top, int right, int bottom) {}
static void DeleteObject(HGDIOBJ object) {}
static void ThemeLine(HDC dc, int x1, int y1, int x2, int y2, int role, int width) {}
static void DrawFocusRect(HDC dc, const RECT *rect) {}
#include "groupboxes.inc"

int main(void)
{
    Canvas canvas;
    ThemeWindow theme={0};
    Window parent={0}, group={.parent=&parent,.style=BS_GROUPBOX,.visible=TRUE};
    Window material={.parent=&parent,.visible=TRUE}, picker={.parent=&parent,.visible=TRUE};
    Window hidden={.parent=&parent}, unrelated={.parent=&parent,.visible=TRUE};
    /* Include controls before and after the frame in Z order. */
    parent.child=&material; material.next=&group; group.next=&picker; picker.next=&hidden; hidden.next=&unrelated;
    for (int pass=0; pass<4; pass++) {
        int x=pass & 1 ? -1400 : 500, y=pass & 2 ? -300 : 200;
        group.rect=(RECT){x,y,x+220,y+160};
        material.rect=(RECT){x+10,y+24,x+210,y+65};
        picker.rect=(RECT){x+10,y+80,x+210,y+120};
        hidden.rect=(RECT){x+10,y+130,x+100,y+145};
        unrelated.rect=(RECT){x-200,y,x-10,y+160};
        memset(canvas.pixels,99,sizeof(canvas.pixels)); memset(canvas.clip,1,sizeof(canvas.clip));
        ThemeDrawButton(&group,&theme,&canvas);
        for (int py=24;py<65;py++) for (int px=10;px<210;px++) { assert(canvas.pixels[py][px]==99); }
        for (int py=80;py<120;py++) for (int px=10;px<210;px++) { assert(canvas.pixels[py][px]==99); }
        assert(canvas.pixels[70][100]==THEME_BACKGROUND); /* Empty interior still repaints. */
        assert(canvas.pixels[135][50]==THEME_BACKGROUND); /* Hidden controls don't leave holes. */
        assert(canvas.pixels[7][0]==THEME_BORDER && canvas.pixels[159][100]==THEME_BORDER);
        assert(canvas.pixels[5][12]==THEME_TEXT); /* Caption still draws. */
        /* An existing update-region clip must never be expanded. */
        memset(canvas.pixels,99,sizeof(canvas.pixels)); memset(canvas.clip,0,sizeof(canvas.clip));
        ThemeDrawButton(&group,&theme,&canvas);
        assert(canvas.pixels[70][100]==99 && canvas.pixels[5][12]==99);
    }
    puts("PASS: group backgrounds preserve material/color controls in either Z order, including negative monitor coordinates, while repainting gaps and captions.");
    return 0;
}
