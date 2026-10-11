/* Minimal Win32 plumbing for the actual UVCanvasWndProc. Rendering and window
 * management are inert; production picking, selection and drag logic run. */
typedef unsigned int UINT;
typedef intptr_t LONG_PTR;
#define CALLBACK
#define GWLP_USERDATA (-21)
#define GET_X_LPARAM(p) ((int)(int16_t)(p))
#define GET_Y_LPARAM(p) ((int)(int16_t)((p)>>16))
#define LOWORD(p) ((unsigned short)(p))
enum { WM_CREATE=1,WM_DESTROY=2,WM_SIZE=5,WM_KILLFOCUS=8,WM_PAINT=15,
    WM_ERASEBKGND=20,WM_CANCELMODE=31,WM_SETCURSOR=32,WM_NCDESTROY=130,
    WM_KEYDOWN=256,WM_KEYUP=257,WM_MOUSEMOVE=512,WM_LBUTTONDOWN=513,WM_LBUTTONUP=514,
    WM_MBUTTONDOWN=519,WM_MBUTTONUP=520,WM_MOUSEWHEEL=522,WM_CAPTURECHANGED=533,
    MK_LBUTTON=1,MK_SHIFT=4,MK_CONTROL=8,MK_MBUTTON=16,HTCLIENT=1,IDC_SIZEALL=32646 };
static POINT livecursor;
/* Keep these usable against the old code to demonstrate the regression. */
#define GetCursorPos(p) ((*(p)=livecursor),TRUE)
#define ScreenToClient(h,p) TRUE
static void UVCanvasResize(HWND h,UVCanvasState*s) {}
static void UVCanvasPaint(HWND h,UVCanvasState*s) {}
static void UVCanvasPan(HWND h,UVCanvasState*s,int x,int y) {}
static void UVCanvasZoom(HWND h,UVCanvasState*s,WPARAM w,LPARAM l) {}
static void SetFocus(HWND h) {}
static void SetCapture(HWND h) { capture=h; }
static void SetWindowLongPtr(HWND h,int key,LONG_PTR value) {}
static void *LoadCursor(HWND h,int id) { return NULL; }
static void SetCursor(void *cursor) {}
static LRESULT DefWindowProc(HWND h,UINT message,WPARAM w,LPARAM l) { return 0; }
#include "events.inc"
