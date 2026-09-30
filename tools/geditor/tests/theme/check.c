/* Exercise real menu preparation/restoration and mnemonic handling with a
 * small in-memory Win32 menu model. Painting is verified by the Windows build. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
typedef int BOOL;
typedef unsigned int UINT;
typedef uintptr_t ULONG_PTR, WPARAM;
typedef intptr_t LRESULT;
typedef wchar_t WCHAR;
typedef void *HWND, *HBRUSH;
#define TRUE 1
#define FALSE 0
#define LOWORD(n) ((unsigned int)(n)&65535)
#define MAKELRESULT(a,b) ((LRESULT)((a)|((unsigned int)(b)<<16)))
#define MNC_IGNORE 0
#define MNC_EXECUTE 2
#define MNC_SELECT 3
#define MIIM_FTYPE 1
#define MIIM_DATA 2
#define MIIM_STRING 4
#define MIIM_STATE 8
#define MFT_OWNERDRAW 0x100
#define MFT_BITMAP 4
#define MFT_SEPARATOR 0x800
#define MFS_DISABLED 3
#define MFS_HILITE 0x80
#define MIM_BACKGROUND 2
#define COLOR_MENU 4
#define THEME_MENU 11
#define EDITOR_THEME_LIGHT 0
#define EDITOR_THEME_DARK 1
static int g_Theme = EDITOR_THEME_DARK;
struct Row { UINT type, state; ULONG_PTR data; WCHAR text[256]; };
struct Menu { int count, valid; HBRUSH background; struct Row rows[20]; };
typedef struct Menu *HMENU;
typedef struct { UINT cbSize, fMask, fType, fState, cch; ULONG_PTR dwItemData; WCHAR *dwTypeData; } MENUITEMINFOW;
typedef struct { UINT cbSize, fMask; HBRUSH hbrBack; } MENUINFO;
static HBRUSH ThemeBrush(int role) { assert(role==THEME_MENU); return (HBRUSH)1; }
static HBRUSH GetSysColorBrush(int role) { assert(role==COLOR_MENU); return (HBRUSH)2; }
static BOOL IsMenu(HMENU menu) { return menu && menu->valid; }
static int GetMenuItemCount(HMENU menu) { return menu->count; }
static BOOL GetMenuItemInfoW(HMENU menu, UINT index, BOOL position, MENUITEMINFOW *info)
{
    assert(position); if (!IsMenu(menu) || index >= (UINT)menu->count) return FALSE;
    struct Row *r=&menu->rows[index];
    if (info->fMask & MIIM_FTYPE) info->fType=r->type;
    if (info->fMask & MIIM_DATA) info->dwItemData=r->data;
    if (info->fMask & MIIM_STATE) info->fState=r->state;
    if (info->fMask & MIIM_STRING) {
        size_t length=wcslen(r->text);
        if (info->dwTypeData && info->cch) {
            size_t copy=length<info->cch-1 ? length : info->cch-1;
            wmemcpy(info->dwTypeData,r->text,copy); info->dwTypeData[copy]=0;
        }
        info->cch=(UINT)length;
    }
    return TRUE;
}
static BOOL SetMenuItemInfoW(HMENU menu, UINT index, BOOL position, const MENUITEMINFOW *info)
{
    assert(position); if (!IsMenu(menu) || index >= (UINT)menu->count) return FALSE;
    struct Row *r=&menu->rows[index];
    if (info->fMask & MIIM_FTYPE) r->type=info->fType;
    if (info->fMask & MIIM_DATA) r->data=info->dwItemData;
    return TRUE;
}
static BOOL SetMenuInfo(HMENU menu, const MENUINFO *info)
{ assert(info->fMask==MIM_BACKGROUND); menu->background=info->hbrBack; return TRUE; }
static int allocations;
static void *tracked_calloc(size_t count, size_t size)
{ void *p=calloc(count,size); if(p) allocations++; return p; }
static void tracked_free(void *p) { if(p) allocations--; free(p); }
#define calloc tracked_calloc
#define free tracked_free
#include "menus.inc"
static void row(HMENU m, int i, const WCHAR *text, UINT type, UINT state, ULONG_PTR data)
{ m->rows[i]=(struct Row){.type=type,.state=state,.data=data}; wcscpy(m->rows[i].text,text); }
int main(void)
{
    struct Menu a={.count=7,.valid=TRUE}, b={.count=1,.valid=TRUE};
    row(&a,0,L"&File",0,0,100);
    row(&a,1,L"&Find\tCtrl+F",0,0,200);
    row(&a,2,L"&&Export",0,0,300);
    row(&a,3,L"&Disabled",0,MFS_DISABLED,400);
    row(&a,4,L"",MFT_SEPARATOR,0,500);
    row(&a,5,L"Existing custom",MFT_OWNERDRAW,0,600);
    row(&a,6,L"&Édition",0,0,700);
    row(&b,0,L"&Other",0,0,800);
    for (int trial=0; trial<1000; trial++) {
        ThemePrepareMenu(NULL,&a,FALSE); ThemePrepareMenu(NULL,&b,TRUE);
        assert(a.background==(HBRUSH)1);
        assert(a.rows[5].data==600 && a.rows[5].type==MFT_OWNERDRAW);
        ThemeMenuItem *item=ThemeFindMenuItem(a.rows[1].data);
        assert(item && !wcscmp(item->text,L"&Find\tCtrl+F") && item->originaldata==200);
        assert(ThemeMenuChar('f',&a)==MAKELRESULT(0,MNC_SELECT));
        a.rows[0].state=MFS_HILITE;
        assert(ThemeMenuChar('F',&a)==MAKELRESULT(1,MNC_SELECT));
        a.rows[0].state=0;
        assert(ThemeMenuChar('D',&a)==MAKELRESULT(0,MNC_IGNORE));
        assert(ThemeMenuChar('E',&a)==MAKELRESULT(0,MNC_IGNORE));
        assert(ThemeMenuChar('O',&b)==MAKELRESULT(0,MNC_EXECUTE));
        ThemeRestoreMenu(&a);
        assert(ThemeFindMenuItem(b.rows[0].data));
        for (int i=0;i<7;i++) assert(a.rows[i].data==(ULONG_PTR)(100*(i+1)));
        assert(a.rows[4].type==MFT_SEPARATOR && a.rows[3].state==MFS_DISABLED);
        ThemeRestoreMenu(NULL); assert(!g_MenuItems && b.rows[0].data==800);
    }
    ThemePrepareMenu(NULL,&a,FALSE);
    /* Dynamic Open Recent menus can remove/rebuild rows before reopening. */
    a.count=1; row(&a,0,L"&Recent",0,0,900);
    ThemePrepareMenu(NULL,&a,FALSE);
    assert(ThemeMenuChar('R',&a)==MAKELRESULT(0,MNC_EXECUTE));
    g_Theme=EDITOR_THEME_LIGHT; ThemePrepareMenu(NULL,&a,FALSE);
    assert(!g_MenuItems && a.rows[0].data==900 && !a.rows[0].type && a.background==(HBRUSH)2);
    g_Theme=EDITOR_THEME_DARK; ThemePrepareMenu(NULL,&a,FALSE);
    a.valid=FALSE; ThemeRestoreMenu(NULL); assert(!g_MenuItems && allocations==0);
    puts("PASS: menu data/types/state/labels preserved; duplicate mnemonics cycle, escaped/disabled items excluded; dynamic rebuilds and 1000 theme/cleanup cycles.");
}
