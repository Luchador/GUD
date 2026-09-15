#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
typedef uintptr_t HWND, WPARAM;
typedef intptr_t LPARAM, LRESULT;
typedef unsigned int UINT;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
enum { WM_KEYDOWN=256, WM_COMMAND=273, VK_CONTROL=17, VK_MENU=18, VK_SHIFT=16,
       ID_EDIT_COPY_FACES=30, ID_EDIT_PASTE_FACES=31 };
static HWND g_Viewport = 2;
static BOOL flying;
static int control=0x8000,alt,shift,sent,last;
static const char *class = "Viewport";
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL IsChild(HWND frame,HWND child) { return frame == 1 && (child == 2 || child == 3); }
static int GetKeyState(int key) { return key == VK_CONTROL ? control : key == VK_MENU ? alt : shift; }
static void GetClassName(HWND hwnd,char *out,int size) { snprintf(out,(size_t)size,"%s",class); }
static LRESULT SendMessage(HWND hwnd,UINT msg,WPARAM w,LPARAM l)
{ assert(hwnd == 1 && msg == WM_COMMAND && !l); sent++; last = w; return 0; }
#include "input.inc"
int main(void)
{
    MSG msg = {2,WM_KEYDOWN,'C',0};
    assert(GEditorHandleFaceClipboardHotkey(1,&msg) && sent == 1 && last == ID_EDIT_COPY_FACES);
    msg.wParam = 'V'; assert(GEditorHandleFaceClipboardHotkey(1,&msg) && sent == 2 && last == ID_EDIT_PASTE_FACES);
    msg.lParam = (LPARAM)1<<30; assert(GEditorHandleFaceClipboardHotkey(1,&msg) && sent == 2); msg.lParam = 0;
    const char *inputs[] = {"Edit","ComboBox","ComboLBox"};
    for (unsigned i = 0; i < 3; i++) { class = inputs[i]; assert(!GEditorHandleFaceClipboardHotkey(1,&msg)); }
    class = "Viewport"; control = 0; assert(!GEditorHandleFaceClipboardHotkey(1,&msg)); control = 0x8000;
    shift = 0x8000; assert(!GEditorHandleFaceClipboardHotkey(1,&msg)); shift = 0;
    alt = 0x8000; assert(!GEditorHandleFaceClipboardHotkey(1,&msg)); alt = 0;
    flying = TRUE; assert(!GEditorHandleFaceClipboardHotkey(1,&msg)); flying = FALSE;
    msg.hwnd = 20; assert(!GEditorHandleFaceClipboardHotkey(1,&msg)); /* Floating editor. */
    msg.hwnd = 3; assert(GEditorHandleFaceClipboardHotkey(1,&msg) && sent == 3);
    puts("PASS: Ctrl+C/V, one paste per physical press, native text clipboard, unmodified V, flight and floating-window isolation.");
    return 0;
}
