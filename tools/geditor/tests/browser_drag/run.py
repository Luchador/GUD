#!/usr/bin/env python3
"""Run the browser's production drag lifecycle with synchronous Win32 message stubs.

SetCapture notifies the old owner even when the new owner is the same HWND.
That reentrancy must not cancel a palette drag before it reaches mouse-up.
No Windows desktop is needed; actual rendering remains a manual check.
An optional browser.c path lets this regression be checked against an older revision.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    editor = Path(__file__).resolve().parents[2]
    source = Path(sys.argv[1]).read_text() if len(sys.argv) > 1 else (editor / 'src/browser.c').read_text()
    end_drag = function(source, 'static void BrowserEndAssetDrag(')
    start_drag = function(source, 'static BOOL BrowserStartAssetDrag(')
    events = source[source.index('    case WM_LBUTTONUP:'):source.index('    case WM_CONTEXTMENU:')]
    prelude = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef intptr_t HWND, HIMAGELIST, HBITMAP, LPARAM, WPARAM, LRESULT;
typedef unsigned int DWORD, UINT;
typedef int BOOL;
typedef struct { long x, y; } POINT;
typedef struct { int unused; } RECT;
typedef int BrowserObjectType;
typedef struct { DWORD textureid; POINT screen; } BrowserImageDrop;
typedef struct { char name[64]; POINT screen; } BrowserModelDrop;
typedef struct { BrowserObjectType type; POINT screen; } BrowserObjectDrop;
typedef struct {
    HIMAGELIST dragimage;
    char dragmodel[64];
    BOOL dragobject;
    int pressedobject, dragsection;
    DWORD dragtextureid;
    struct { RECT bodyrc; } sections[1];
} BrowserState;
#define TRUE 1
#define FALSE 0
#define NULL_HANDLE 0
#undef NULL
#define NULL 0
#define ILC_COLOR32 0
#define IDC_ARROW 0
#define BROWSER_SECTION_OBJECTS 0
#define BROWSER_OBJECT_SPAWN 2
#define BROWSER_OBJECT_INTRO_CAMERA 4
#define BROWSER_OBJECT_OUTRO_CAMERA 5
#define BROWSER_OBJECT_DOOR 6
#define WM_LBUTTONUP 1
#define WM_CAPTURECHANGED 2
#define WM_CANCELMODE 3
#define BROWSER_WM_OBJECT_DROP 4
#define BROWSER_WM_IMAGE_DROP 5
#define BROWSER_WM_MODEL_DROP 6
#define GET_X_LPARAM(p) ((int16_t)((p) & 0xffff))
#define GET_Y_LPARAM(p) ((int16_t)(((p) >> 16) & 0xffff))
static void lstrcpyn(char *dst, const char *src, size_t size)
{
    size_t i = 0;
    if (!size) { return; }
    while (i + 1 < size && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}
static const HWND browser = 1, frame = 2, other = 3;
static BrowserState g_state;
static HWND capture;
static int captures, destroyed, objectdrops, imagedrops, modeldrops;
static BrowserObjectDrop placed;
static LRESULT Dispatch(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static HWND GetParent(HWND hwnd) { return frame; }
static HWND GetCapture(void) { return capture; }
static void SetCapture(HWND hwnd)
{
    HWND previous = capture; capture = hwnd; captures++;
    if (previous == browser) { Dispatch(previous, WM_CAPTURECHANGED, 0, hwnd); }
}
static void ReleaseCapture(void)
{
    HWND previous = capture; capture = 0;
    if (previous == browser) { Dispatch(previous, WM_CAPTURECHANGED, 0, 0); }
}
static BOOL ImageList_DragLeave(HWND hwnd) { return TRUE; }
static void ImageList_EndDrag(void) {}
static BOOL ImageList_Destroy(HIMAGELIST images) { assert(images == 4); destroyed++; return TRUE; }
static HIMAGELIST ImageList_Create(int w, int h, UINT flags, int count, int grow) { return 4; }
static int ImageList_Add(HIMAGELIST images, HBITMAP bitmap, HBITMAP mask) { return 0; }
static BOOL ImageList_BeginDrag(HIMAGELIST images, int image, int x, int y) { return TRUE; }
static BOOL ImageList_DragEnter(HWND hwnd, int x, int y) { return TRUE; }
static BOOL BrowserImageDragPoint(HWND hwnd, POINT *p) { return TRUE; }
static void DeleteObject(HBITMAP bitmap) {}
static void SetFocus(HWND hwnd) {}
static void SetCursor(int cursor) {}
static int LoadCursor(HWND instance, int id) { return 1; }
static void InvalidateRect(HWND hwnd, const RECT *rect, BOOL erase) {}
static void ClientToScreen(HWND hwnd, POINT *p) { p->x += 100; p->y += 200; }
static LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    assert(hwnd == frame && capture == 0 && g_state.dragimage == 0);
    if (msg == BROWSER_WM_OBJECT_DROP) { placed = *(BrowserObjectDrop *)lparam; objectdrops++; }
    else if (msg == BROWSER_WM_IMAGE_DROP) { imagedrops++; }
    else if (msg == BROWSER_WM_MODEL_DROP) { modeldrops++; }
    else { assert(0); }
    return TRUE;
}
'''
    tests = r'''
static void Reset(BOOL palette)
{
    memset(&g_state, 0, sizeof(g_state));
    g_state.dragsection = -1; g_state.pressedobject = palette ? BROWSER_OBJECT_SPAWN : -1;
    capture = palette ? browser : 0;
    captures = destroyed = objectdrops = imagedrops = modeldrops = 0;
}
static void Start(BOOL palette)
{
    POINT point = {10, 10};
    assert(BrowserStartAssetDrag(browser, &g_state, 5, 100, 38, point));
    /* The object palette does this after the shared helper returns. */
    g_state.dragobject = palette;
    assert(g_state.dragimage == 4 && capture == browser && destroyed == 0);
    if (palette) { assert(g_state.pressedobject == BROWSER_OBJECT_SPAWN && captures == 0); }
    else { assert(captures == 1); }
}
int main(void)
{
    Reset(TRUE); Start(TRUE);
    Dispatch(browser, WM_LBUTTONUP, 0, (100 << 16) | 320);
    assert(objectdrops == 1 && placed.type == BROWSER_OBJECT_SPAWN);
    assert(placed.screen.x == 420 && placed.screen.y == 300);
    assert(destroyed == 1 && g_state.pressedobject == -1 && capture == 0);
    Dispatch(browser, WM_LBUTTONUP, 0, 0); assert(objectdrops == 1);
    for (int kind = BROWSER_OBJECT_INTRO_CAMERA; kind <= BROWSER_OBJECT_DOOR; kind++)
    {
        Reset(TRUE); Start(TRUE); g_state.pressedobject = kind;
        Dispatch(browser, WM_LBUTTONUP, 0, (100 << 16) | 320);
        assert(objectdrops == 1 && placed.type == kind && placed.screen.x == 420 && placed.screen.y == 300);
        assert(destroyed == 1 && capture == 0);
    }
    Reset(TRUE); Start(TRUE); SetCapture(other);
    assert(capture == other && !g_state.dragimage && g_state.pressedobject == -1 && destroyed == 1);
    Dispatch(browser, WM_LBUTTONUP, 0, 0); assert(objectdrops == 0);
    Reset(TRUE); Start(TRUE); Dispatch(browser, WM_CANCELMODE, 0, 0);
    assert(!capture && !g_state.dragimage && !objectdrops && destroyed == 1);
    Reset(TRUE); Start(TRUE); BrowserEndAssetDrag(browser, &g_state); /* Escape */
    Dispatch(browser, WM_LBUTTONUP, 0, 0); assert(!objectdrops && destroyed == 1);
    Reset(FALSE); Start(FALSE); g_state.dragtextureid = 12;
    Dispatch(browser, WM_LBUTTONUP, 0, 0); assert(imagedrops == 1 && destroyed == 1);
    Reset(FALSE); Start(FALSE); strcpy(g_state.dragmodel, "PcrateZ");
    Dispatch(browser, WM_LBUTTONUP, 0, 0); assert(modeldrops == 1 && destroyed == 1);
    puts("PASS: palette capture, spawn/intro/outro/door drop type/position, capture loss, cancellation, image/model drags.");
    return 0;
}
'''
    dispatch = '\nstatic LRESULT Dispatch(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)\n{\n'
    dispatch += 'BrowserState *state = &g_state;\nswitch (msg) {\n'
    dispatch += events + '\ndefault: return 0;\n}\n}\n'
    with tempfile.TemporaryDirectory(prefix='geditor-browser-drag-') as temp:
        temp = Path(temp)
        unit = temp / 'check.c'
        unit.write_text(prelude + end_drag + '\n' + start_drag + dispatch + tests)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-Wall', '-Wextra',
                        '-Werror', '-Wno-unused-parameter', '-fsanitize=address,undefined',
                        str(unit), '-o', str(temp / 'check')], check=True)
        subprocess.run([str(temp / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))


if __name__ == '__main__':
    main()
