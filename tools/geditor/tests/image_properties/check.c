#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "texload.h"
typedef unsigned int UINT;
typedef uintptr_t UINT_PTR;
typedef void *HWND;
enum { MF_STRING=0, MF_POPUP=16, MF_GRAYED=1, MF_CHECKED=8, MF_ENABLED=0, MF_SEPARATOR=0x800,
    BROWSER_WM_IMAGE_HIT_SOUND=132, BROWSER_WM_IMAGE_BULLET_HOLE=133 };
typedef struct Menu *HMENU;
typedef struct Item { UINT flags;UINT_PTR id;char label[64]; } Item;
struct Menu { Item items[32];unsigned count; };
static int live,failafter=-1;
static BOOL Failing(void) { if(failafter<0)return FALSE;if(!failafter)return TRUE;failafter--;return FALSE; }
static HMENU CreatePopupMenu(void)
{ if(Failing())return NULL;HMENU m=calloc(1,sizeof(*m));assert(m);live++;return m; }
static BOOL AppendMenu(HMENU menu,UINT flags,UINT_PTR id,const char *label)
{
    if(Failing()) { return FALSE; }
    assert(menu && menu->count<32);
    Item *item=&menu->items[menu->count++];item->flags=flags;item->id=id;
    if(label) { snprintf(item->label,sizeof(item->label),"%s",label); }
    return TRUE;
}
static void DestroyMenu(HMENU menu)
{
    for(unsigned i=0;i<menu->count;i++)if(menu->items[i].flags&MF_POPUP)DestroyMenu((HMENU)menu->items[i].id);
    free(menu);live--;
}
static HWND GetParent(HWND hwnd) { assert(hwnd==(HWND)1);return (HWND)2; }
static UINT sent;static DWORD imageid,type;static int sends;
static intptr_t SendMessage(HWND hwnd,UINT message,UINT_PTR wparam,intptr_t lparam)
{ assert(hwnd==(HWND)2);sends++;sent=message;imageid=wparam;type=lparam;return 0; }
#include "menus.inc"
int main(void)
{
    for(unsigned value=0;value<=12;value++) {
        TexImageInfo info={.surfacevalid=TRUE,.hitsound=value,.hittexture=12-value};
        HMENU menu=CreatePopupMenu();assert(BrowserAppendImageActions(menu,&info));assert(menu->count==10);
        assert(!strcmp(menu->items[5].label,"Hit sound") && !strcmp(menu->items[6].label,"Bullet hole"));
        assert(!strcmp(menu->items[8].label,"Flip vertical") && !strcmp(menu->items[9].label,"Flip horizontal"));
        for(unsigned field=0;field<2;field++) {
            Item *parent=&menu->items[5+field];assert((parent->flags&MF_POPUP) && !(parent->flags&MF_GRAYED));
            HMENU choices=(HMENU)parent->id;assert(choices->count==13);
            for(unsigned i=0;i<13;i++) {
                Item *item=&choices->items[i];assert(!strcmp(item->label,TexInfoSurfaceName(i)));
                assert(item->id==(field?200:100)+i);
                assert(!!(item->flags&MF_CHECKED)==(i==(field?12-value:value)));
                assert(BrowserDispatchSurfaceCommand((HWND)1,item->id,0xAA4));
                assert(imageid==0xAA4 && type==i && sent==(field?BROWSER_WM_IMAGE_BULLET_HOLE:BROWSER_WM_IMAGE_HIT_SOUND));
            }
        }
        DestroyMenu(menu);assert(!live);
    }
    int before=sends;
    for(UINT i=0;i<8;i++)assert(!BrowserDispatchSurfaceCommand((HWND)1,i,1));
    assert(!BrowserDispatchSurfaceCommand((HWND)1,113,1) && !BrowserDispatchSurfaceCommand((HWND)1,213,1) && sends==before);
    HMENU menu=CreatePopupMenu();TexImageInfo missing={0};assert(BrowserAppendImageActions(menu,&missing));
    assert((menu->items[5].flags&MF_GRAYED) && (menu->items[6].flags&MF_GRAYED));DestroyMenu(menu);
    /* Any native-menu allocation/append failure must release every submenu. */
    int failures=0;
    for(int f=0;f<50;f++) {
        menu=CreatePopupMenu();failafter=f;BOOL ok=BrowserAppendImageActions(menu,&missing);failafter=-1;
        DestroyMenu(menu);assert(!live);if(ok)break;failures++;
    }
    assert(failures==38);
    puts("PASS: both image property submenus, all 13 labels/checked values, preserved flip placement, exact image/type dispatch, unavailable metadata and menu cleanup.");
    return 0;
}
