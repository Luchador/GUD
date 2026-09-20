/* Exercise the production combo commit/key handling without a Windows GUI. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <windows.h>
typedef uintptr_t HWND, WPARAM;
typedef intptr_t LPARAM, LRESULT;
typedef unsigned int UINT;
typedef void *HACCEL;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
typedef struct { unsigned char fVirt; WORD key, cmd; } ACCEL;
enum { WM_KEYDOWN = 256, VK_RETURN = 13, VK_ESCAPE = 27,
    CB_GETCURSEL = 0x147, CB_ERR = -1, CB_GETDROPPEDSTATE = 0x157,
    FACEPROPERTIES_WM_ROOM_CHANGED = 0x803b, MB_ICONERROR = 16,
    FVIRTKEY = 1, FCONTROL = 8, FSHIFT = 4,
    ID_EDIT_UNDO = 20, ID_EDIT_REDO, ID_FILE_SAVE_PROJECT, ID_TOOLS_UV_EDITOR, ID_TOOLS_CREATE_ROM,
    FACE_ROOM = 2, FACE_CONTROL_COUNT = 3, FACE_DETAIL_IMAGE = 3 };
typedef struct { HWND controls[3]; DWORD roomcount; char roomtext[32], detailimagetext[32], detaillodtext[32]; } FacePropertiesState;
static FacePropertiesState state = {.controls = {0,0,20}, .roomcount = 90, .roomtext = "22"};
static char text[32] = "22";
static HWND focus = 21;
static int choice = 21, dropped, requests, errors;
static DWORD target;
static FacePropertiesState *FacePropertiesGetState(HWND panel) { return panel == 10 ? &state : NULL; }
static HWND GetFocus(void) { return focus; }
static BOOL IsChild(HWND control, HWND child) { return control == 20 && child == 21; }
static HWND GetParent(HWND hwnd) { assert(hwnd == 10); return 1; }
static LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (msg == CB_GETCURSEL) { assert(hwnd == 20); return choice; }
    if (msg == CB_GETDROPPEDSTATE) { assert(hwnd == 20); return dropped; }
    assert(hwnd == 1 && msg == FACEPROPERTIES_WM_ROOM_CHANGED && !l);
    requests++; target = (DWORD)w; return TRUE;
}
static void GetWindowText(HWND hwnd,char *out,int size) { assert(hwnd == 20); snprintf(out,(size_t)size,"%s",text); }
static void SetWindowText(HWND hwnd,const char *value) { assert(hwnd == 20); snprintf(text,sizeof(text),"%s",value); }
static int lstrcmp(const char *a,const char *b) { return strcmp(a,b); }
static void MessageBox(HWND hwnd,const char *body,const char *title,unsigned flags) { assert(hwnd == 10 && body[0]); errors++; }
static HACCEL CreateAcceleratorTable(const ACCEL *entries,int count)
{
    assert(count == 6);
    assert(entries[0].cmd == ID_EDIT_UNDO && entries[0].key == 'Z');
    assert(entries[1].cmd == ID_EDIT_REDO && entries[1].key == 'Y');
    assert(entries[2].cmd == ID_EDIT_REDO && entries[2].fVirt == (FVIRTKEY | FCONTROL | FSHIFT));
    assert(entries[3].cmd == ID_FILE_SAVE_PROJECT && entries[3].key == 'S' && entries[3].fVirt == (FVIRTKEY | FCONTROL));
    assert(entries[4].cmd == ID_TOOLS_UV_EDITOR && entries[4].key == 'T' && entries[4].fVirt == (FVIRTKEY | FCONTROL));
    assert(entries[5].cmd == ID_TOOLS_CREATE_ROM && entries[5].key == 'R' && entries[5].fVirt == (FVIRTKEY | FCONTROL));
    return (void *)1;
}
static BOOL FacePropertiesIsEdit(int id) { return FALSE; }
static void FacePropertiesApplyDetailNumber(HWND hwnd, FacePropertiesState *state, int id) { assert(0); }
#include "input.inc"
int main(void)
{
    MSG msg = {21,WM_KEYDOWN,VK_RETURN,0};
    /* Typing is only committed by Enter. List acceptance uses its selection,
       even when the edit child's text has not updated to match it yet. */
    strcpy(text,"23"); assert(FacePropertiesHandleMessage(10,&msg) && target == 23 && requests == 1);
    strcpy(text,"22"); choice = 23; FacePropertiesApplyRoom(10,&state,TRUE); assert(target == 24 && requests == 2);
    choice = CB_ERR; FacePropertiesApplyRoom(10,&state,TRUE); assert(requests == 2);
    const char *invalid[] = {"", "0", "91", "-1", "2x", "1.5", "999999999999999999999999"};
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(*invalid); i++)
    { strcpy(text,invalid[i]); assert(FacePropertiesHandleMessage(10,&msg)); assert(requests == 2 && !strcmp(text,"22")); }
    assert(errors == 7);
    strcpy(text,"22"); assert(FacePropertiesHandleMessage(10,&msg) && requests == 2);
    strcpy(text,"90"); assert(FacePropertiesHandleMessage(10,&msg) && requests == 3 && target == 90);
    strcpy(text,"23"); msg.wParam = VK_ESCAPE; assert(FacePropertiesHandleMessage(10,&msg) && !strcmp(text,"22"));
    msg.wParam = VK_RETURN; dropped = TRUE; assert(!FacePropertiesHandleMessage(10,&msg)); dropped = FALSE;
    focus = 30; assert(!FacePropertiesHandleMessage(10,&msg)); focus = 21;
    msg.wParam = 'S'; assert(!FacePropertiesHandleMessage(10,&msg));
    assert(GEditorCreateAccelerators());
    puts("PASS: typed/list room commits, invalid-room handling, Escape, open-list routing and Ctrl+S/Ctrl+T/Ctrl+R mappings.");
    return 0;
}
