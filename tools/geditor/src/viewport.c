/*
 * GEditor 3D viewport.
 *
 * A child window with a private DC (CS_OWNDC) and a legacy OpenGL
 * context. Fixed-function GL was chosen deliberately: the N64's
 * pipeline - positions, per-vertex colours, one modulated texture -
 * maps onto it directly, so level geometry can be handed to
 * glVertexPointer/glColorPointer with no shader work.
 *
 * Rendering is on demand: everything happens in WM_PAINT, and the
 * window repaints only when invalidated (resize, or ViewportRedraw).
 * Idle, the editor draws nothing and uses no CPU.
 */

#include <windows.h>
#include <GL/gl.h>
#include <stdlib.h>

#include "viewport.h"

#define VIEWPORT_CLASS "GEditorViewport"

/* Per-viewport state, allocated at WM_CREATE, freed at WM_DESTROY,
   reachable from the window via GWLP_USERDATA. */
typedef struct ViewportState {
    HDC hdc;      /* private DC - stable for the window's lifetime (CS_OWNDC) */
    HGLRC hglrc;  /* the GL context rendering into it */

    /* Fly Camera */
    float posx, posy, posz;
    float yaw, pitch;
    float speed;

    BOOL flying;
    BOOL keyw, keya, keys, keyd;
    POINT lastmouse;
    DWORD lasttick;
} ViewportState;

/*
 * Placeholder scene: one triangle with red, green and blue corners,
 * drawn through the same vertex-array path real level geometry will
 * use. Interleaving positions and colours in one struct keeps each
 * vertex's data together, exactly like the game's Vtx records; the
 * stride argument tells GL how far apart consecutive vertices sit.
 */
typedef struct TestVertex {
    GLfloat x, y, z;
    GLubyte r, g, b, a;
} TestVertex;

static const TestVertex g_TestTriangle[3] = {
    {  0.0f,  0.8f, 0.0f,   255,  40,  40, 255 },
    { -0.8f, -0.6f, 0.0f,    40, 255,  40, 255 },
    {  0.8f, -0.6f, 0.0f,    40,  40, 255, 255 },
};

static ViewportState *ViewportGetState(HWND hwnd)
{
    return (ViewportState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

/*
 * Chooses a pixel format and creates the GL context. Returns FALSE if
 * the machine's driver refuses - vanishingly rare on a desktop, but a
 * clear message beats a mystery black rectangle.
 */
static BOOL ViewportInitGL(HWND hwnd, ViewportState *state)
{
    PIXELFORMATDESCRIPTOR pfd;
    int format;

    state->hdc = GetDC(hwnd); /* with CS_OWNDC this DC is ours for good */

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    format = ChoosePixelFormat(state->hdc, &pfd);
    if (format == 0 || !SetPixelFormat(state->hdc, format, &pfd))
    {
        return FALSE;
    }

    state->hglrc = wglCreateContext(state->hdc);
    if (state->hglrc == NULL)
    {
        return FALSE;
    }

    wglMakeCurrent(state->hdc, state->hglrc);

    /* N64-flavoured fixed-function state: smooth vertex colours, one
       modulated texture, nearest filtering. Texturing stays off until
       we have textures to bind. */
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    return TRUE;
}

/* Keeps the GL viewport and projection matched to the window size. */
static void ViewportResizeGL(ViewportState *state, int width, int height)
{
    GLdouble aspect;

    if (width < 1)  width = 1;
    if (height < 1) height = 1;

    wglMakeCurrent(state->hdc, state->hglrc);
    glViewport(0, 0, width, height);

    aspect = (GLdouble)width / (GLdouble)height;

    /* Perspective via glFrustum (no glu dependency): 90-degree-ish
       FOV, near 0.1, far 100 - placeholder values until the camera
       is driven by level data. */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-0.1 * aspect, 0.1 * aspect, -0.1, 0.1, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

static void ViewportPaintGL(ViewportState *state)
{
    wglMakeCurrent(state->hdc, state->hglrc);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -2.0f);

    glVertexPointer(3, GL_FLOAT, sizeof(TestVertex), &g_TestTriangle[0].x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(TestVertex), &g_TestTriangle[0].r);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    SwapBuffers(state->hdc);
}

static void ViewportBeginFly(HWND hwnd, ViewportState *state)
{

}

static void ViewportEndFly(HWND hwnd, ViewportState *state)
{

}

static void ViewportFlyLook(HWND hwnd, ViewportState *state)
{

}

static void ViewportSetKey(ViewportState *state, WPARAM wparam, LPARAM lparam, int down)
{
    if(down && (lparam & (1 << 30)))
    {
        return; /* auto-repeat, the flag is already set */
    }

    switch(wparam)
    {
        case 'W': state->keyw = down; break;
        case 'A': state->keya = down; break;
        case 'S': state->keys = down; break;
        case 'D': state->keyd = down; break;
    }
}

static LRESULT CALLBACK ViewportWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ViewportState *state;

    switch (msg)
    {
    case WM_CREATE:
        state = (ViewportState *)calloc(1, sizeof(*state));
        if (state == NULL)
        {
            return -1; /* abort window creation */
        }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        if (!ViewportInitGL(hwnd, state))
        {
            MessageBox(hwnd, "Could not create an OpenGL context.",
                       "GEditor", MB_ICONERROR);
            return -1;
        }
        return 0;

    case WM_SIZE:
        state = ViewportGetState(hwnd);
        if (state != NULL)
        {
            ViewportResizeGL(state, LOWORD(lparam), HIWORD(lparam));
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;

        BeginPaint(hwnd, &ps);
        state = ViewportGetState(hwnd);
        if (state != NULL)
        {
            ViewportPaintGL(state);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_RBUTTONDOWN: ViewportBeginFly(hwnd, state);
        return 0;

    case WM_RBUTTONUP: ViewportEndFly(hwnd, state);
        return 0;

    case WM_MOUSEMOVE: ViewportFlyLook(hwnd, state);
        return 0;

    case WM_KEYDOWN: ViewportSetKey(state, wparam, lparam, 1);
        return 0;

    case WM_KEYUP: ViewportSetKey(state, wparam, lparam, 0);
        return 0;

    case WM_ERASEBKGND:
        /* GL repaints every pixel; skipping the GDI erase kills the
           flicker you would otherwise see on every resize. */
        return 1;

    case WM_DESTROY:
        state = ViewportGetState(hwnd);
        if (state != NULL)
        {
            wglMakeCurrent(NULL, NULL);
            if (state->hglrc != NULL)
            {
                wglDeleteContext(state->hglrc);
            }
            free(state);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

BOOL ViewportRegisterClass(HINSTANCE hinstance)
{
    WNDCLASS wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_OWNDC; /* one private DC per window - the GL convention */
    wc.lpfnWndProc   = ViewportWndProc;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;     /* no GDI background; see WM_ERASEBKGND */
    wc.lpszClassName = VIEWPORT_CLASS;

    return RegisterClass(&wc) != 0;
}

HWND ViewportCreate(HWND parent, HINSTANCE hinstance)
{
    /* Size is a placeholder; the parent's WM_SIZE lays it out. */
    return CreateWindowEx(
        0,
        VIEWPORT_CLASS,
        NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 16, 16,
        parent, NULL, hinstance, NULL);
}

void ViewportRedraw(HWND viewport)
{
    /* FALSE: no GDI erase - WM_ERASEBKGND is suppressed anyway. */
    InvalidateRect(viewport, NULL, FALSE);
}