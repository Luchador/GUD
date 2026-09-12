#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "modelload.h"
#include "orbitcamera.h"
typedef unsigned int GLenum, GLuint, UINT;
typedef int GLsizei;
typedef void *HWND;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef struct {int x, y;} POINT;
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define MK_LBUTTON 1
#define MK_RBUTTON 2
#define MK_SHIFT 4
#define MK_CONTROL 8
#define WHEEL_DELTA 120
#define GET_X_LPARAM(l) ((short)(l))
#define GET_Y_LPARAM(l) ((short)((l) >> 16))
#define GET_WHEEL_DELTA_WPARAM(w) ((short)((w) >> 16))
enum {WM_LBUTTONDOWN=0x201, WM_LBUTTONUP, WM_RBUTTONDOWN=0x204, WM_RBUTTONUP,
    WM_MOUSEMOVE=0x200, WM_MOUSEWHEEL=0x20a, WM_CANCELMODE=0x1f, WM_CAPTURECHANGED=0x215,
    WM_KILLFOCUS=8, WM_KEYDOWN=0x100, WM_KEYUP};
#include "types.inc"
typedef struct {
    OrbitCamera orbitcamera;
    unsigned int orbitbuttons;
    POINT lastmouse, orbitstart;
    BOOL orbitdragged;
} ViewportState;
static HWND captured;
static int picks, updates;
static BOOL add, deselected;
static void SetFocus(HWND hwnd) {(void)hwnd;}
static void SetCapture(HWND hwnd) {captured=hwnd;}
static HWND GetCapture(void) {return captured;}
static void ReleaseCapture(void) {captured=NULL;}
static void InvalidateRect(HWND hwnd, void *rect, BOOL erase) {}
static void ViewportUpdateOrbit(ViewportState *state) {updates++;}
static void ViewportPickAt(HWND hwnd, ViewportState *state, int x, int y, BOOL a, BOOL r)
{picks++; add=a; deselected=r;}
#include "logic.inc"
static LPARAM Position(int x, int y) {return (unsigned short)x | (LPARAM)(unsigned short)y << 16;}
int main(void)
{
    HWND hwnd=(HWND)(uintptr_t)1;
    ViewportState state={0};
    SceneBatch batch={0};
    ModelSourceFace face={0};
    const double low[3]={-10,-10,-10}, high[3]={10,10,10};
    OrbitCameraFrame(&state.orbitcamera, low, high, 1, 60);
    assert(ViewportBatchCullMode(&batch, TRUE)==0);
    batch.cullbackfaces=TRUE;
    assert(ViewportBatchCullMode(&batch, TRUE)==GL_BACK); /* legacy BG */
    batch.renderflags=BG_RENDER_CULL_EXPLICIT;
    assert(ViewportBatchCullMode(&batch, TRUE)==0);
    batch.renderflags|=BG_RENDER_CULL_FRONT;
    assert(ViewportBatchCullMode(&batch, TRUE)==GL_FRONT);
    batch.renderflags|=BG_RENDER_CULL_BACK;
    assert(ViewportBatchCullMode(&batch, TRUE)==GL_FRONT_AND_BACK);
    assert(ViewportBatchCullMode(&batch, FALSE)==0);
    assert(ModelEditorCulling(&face)==4); /* inherited is distinct from disabled */
    face.state.geometryknown=0x3000;
    assert(ModelEditorCulling(&face)==0);
    face.state.geometrymode=0x2000;
    assert(ModelEditorCulling(&face)==2);
    assert(ModelEditorSurface(BG_RENDER_DEPTH_WRITE)==0);
    assert(ModelEditorSurface(BG_RENDER_ALPHA_TEST)==1);
    assert(ModelEditorSurface(BG_RENDER_BLEND)==2);
    assert(ModelEditorSurface(BG_RENDER_ALPHA_TEST|BG_RENDER_BLEND)==4);
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,MK_SHIFT,Position(10,20));
    assert(picks==1 && add && !deselected && !captured);
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_MOUSEMOVE,0,Position(12,21));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,MK_CONTROL,Position(12,21));
    assert(picks==2 && !add && deselected && updates==0); /* small hand jitter */
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_MOUSEMOVE,0,Position(40,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(40,20));
    assert(picks==2 && updates==1);
    ViewportOrbitInput(hwnd,&state,WM_RBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_RBUTTONUP,0,Position(10,20));
    assert(picks==2 && !captured);
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONDOWN,0,Position(10,20));
    ViewportOrbitInput(hwnd,&state,WM_CANCELMODE,0,0);
    ViewportOrbitInput(hwnd,&state,WM_LBUTTONUP,0,Position(10,20));
    assert(picks==2 && !captured);
    puts("PASS model viewer click/add/remove, orbit/cancel, culling and inspector labels (ASan + UBSan)");
    return 0;
}
