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
#include <windowsx.h>
#include <GL/gl.h>
#include <stdlib.h>
#include <math.h>

#include "viewport.h"

#define VIEWPORT_CLASS "GEditorViewport"

#define VIEWPORT_FLY_TIMER 1
#define VIEWPORT_FLY_TICK_MS 16 // ~60 ticks/s

/**
 * Default to 400.0, but can be changed with the mouse scroll wheel.
 */
#define VIEWPORT_FLY_SPEED 400.0f

#define VIEWPORT_FOV_Y 60.0f
#define VIEWPORT_NEAR_Z 10.0f
#define VIEWPORT_FAR_Z 100000.0f

/**
 * Degrees per pixel of mouse movement in the viewport.
 */
#define VIEWPORT_LOOK_SENSITIVITY 0.15f

#define VIEWPORT_PITCH_LIMIT 89.0f

#define VIEWPORT_DEG_TO_RAD (3.14159265358979323846f / 180.0f)

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
    BOOL keyw, keya, keys, keyd, keyq, keye;
    POINT lastmouse;
    DWORD lasttick;
} ViewportState;

typedef struct TestVertex {
    GLfloat x, y, z;
    GLubyte r, g, b, a;
} TestVertex;

static const TestVertex g_TestTriangle[3] = {
    {    0.0f,  160.0f, 0.0f,   255,  40,  40, 255 },
    { -160.0f, -120.0f, 0.0f,    40, 255,  40, 255 },
    {  160.0f, -120.0f, 0.0f,    40,  40, 255, 255 },
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

    /* N64 style fixed-function state: smooth vertex colors, one
       modulated texture. Texturing stays off until
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
    GLdouble halfheight;
    GLdouble halfwidth;

    if (width < 1)  width = 1;
    if (height < 1) height = 1;

    wglMakeCurrent(state->hdc, state->hglrc);
    glViewport(0, 0, width, height);

    aspect = (GLdouble)width / (GLdouble)height;

    halfheight = tan(VIEWPORT_FOV_Y * 0.5 * VIEWPORT_DEG_TO_RAD) * VIEWPORT_NEAR_Z;
    halfwidth = halfheight * aspect;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-halfwidth, halfwidth, -halfheight, halfheight, VIEWPORT_NEAR_Z, VIEWPORT_FAR_Z);
    glMatrixMode(GL_MODELVIEW);
}


static void ViewportPaintGL(ViewportState *state)
{
    wglMakeCurrent(state->hdc, state->hglrc);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /**
     * Build the view transform. -z = forward. Eye stays at the origin.
     */
    glLoadIdentity();
    glRotatef(-state->pitch, 1.0f, 0.0f, 0.0f);
    glRotatef(-state->yaw,   0.0f, 1.0f, 0.0f);
    glTranslatef(-state->posx, -state->posy, -state->posz);

    glVertexPointer(3, GL_FLOAT, sizeof(TestVertex), &g_TestTriangle[0].x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(TestVertex), &g_TestTriangle[0].r);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    SwapBuffers(state->hdc);
}


/**
 * Enter fly mode on right mouse down. Use SetFocus to make keyboard inputs go to the viewport.
 */
static void ViewportBeginFly(HWND hwnd, ViewportState *state)
{
    if (state == NULL || state->flying)
    {
        return;
    }

    SetFocus(hwnd); // Give keyboard inputs to the viewport.
    SetCapture(hwnd); // All mouse inputs go to the viewport.

    GetCursorPos(&state->lastmouse);
    ShowCursor(FALSE);

    state->flying = TRUE;
    state->lasttick = GetTickCount();

    SetTimer(hwnd, VIEWPORT_FLY_TIMER, VIEWPORT_FLY_TICK_MS, NULL);
}


static void ViewportEndFly(HWND hwnd, ViewportState *state)
{
    if (state == NULL || !state->flying)
    {
        return;
    }

    KillTimer(hwnd, VIEWPORT_FLY_TIMER);

    state->flying = FALSE;
    state->keyw = state->keya = state->keys = state->keyd = state->keyq = state->keye = FALSE;

    ShowCursor(TRUE);
    SetCursorPos(state->lastmouse.x, state->lastmouse.y);
    ReleaseCapture();
}


/**
 * Mouse look. Consumes the movement since the last event as a delta then puts
 * the cursor back on its anchor.
 */
static void ViewportFlyLook(HWND hwnd, ViewportState *state)
{
    POINT p;
    
    if (state == NULL || !state->flying)
    {
        return;
    }

    GetCursorPos(&p);

    if (p.x == state->lastmouse.x && p.y == state->lastmouse.y)
    {
        return;
    }

    state->yaw   -= (float)(p.x - state->lastmouse.x) * VIEWPORT_LOOK_SENSITIVITY;
    state->pitch -= (float)(p.y - state->lastmouse.y) * VIEWPORT_LOOK_SENSITIVITY;

    SetCursorPos(state->lastmouse.x, state->lastmouse.y);

    ViewportRedraw(hwnd);
}


static void ViewportGetBasis(const ViewportState *state, float fwd[3], float right[3])
{
    float yaw = state->yaw * VIEWPORT_DEG_TO_RAD;
    float pitch = state->pitch * VIEWPORT_DEG_TO_RAD;
    float cospitch = cosf(pitch);

    fwd[0] = -sinf(yaw) * cospitch;
    fwd[1] =  sinf(pitch);
    fwd[2] = -cosf(yaw) * cospitch;

    right[0] =  cosf(yaw);
    right[1] =  0.0f;
    right[2] = -sinf(yaw);
}


static void ViewportFlyTick(HWND hwnd, ViewportState *state)
{
    float fwd[3];
    float right[3];
    float move[3];
    DWORD now;
    float dt;
    float dist;
    int moving;

    if (state == NULL || !state->flying)
    {
        return;
    }

    now = GetTickCount();
    dt = (float)(now - state->lasttick) * 0.001f;
    state->lasttick = now;

    moving = state->keyw || state->keya || state->keys || state->keyd || state->keyq || state->keye;

    if (!moving)
    {
        return; /* Nothing to redraw. */
    } 

    ViewportGetBasis(state, fwd, right);

    move[0] = move[1] = move[2] = 0.0f;

    if (state->keyw) { move[0] += fwd[0];   move[1] += fwd[1];   move[2] += fwd[2];   }
    if (state->keys) { move[0] -= fwd[0];   move[1] -= fwd[1];   move[2] -= fwd[2];   }
    if (state->keyd) { move[0] += right[0]; move[1] += right[1]; move[2] += right[2]; }
    if (state->keya) { move[0] -= right[0]; move[1] -= right[1]; move[2] -= right[2]; }

    /**
     * Q and E move on the world axis.
     */
    if (state->keye) { move[1] += 1.0f; } 
    if (state->keyq) { move[1] -= 1.0f; }

    /**
     * Normalize so moving diagonally isn't faster (unlike GE).
     */
    dist = sqrtf(move[0] * move[0] + move[1] * move[1] + move[2] * move[2]);

    if (dist < 0.0001f)
    {
        return;
    }

    dist = (state->speed * dt) / dist;

    state->posx += move[0] * dist;
    state->posy += move[1] * dist;
    state->posz += move[2] * dist;

    ViewportRedraw(hwnd);
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
        case 'Q': state->keyq = down; break;
        case 'E': state->keye = down; break;
    }
}


static LRESULT CALLBACK ViewportWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ViewportState *state = ViewportGetState(hwnd);

    switch (msg)
    {
    case WM_CREATE:
        state = (ViewportState *)calloc(1, sizeof(*state));
        if (state == NULL)
        {
            return -1; /* abort window creation */
        }

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        state->speed = VIEWPORT_FLY_SPEED;
        state->posz = 600.0f;

        if (!ViewportInitGL(hwnd, state))
        {
            MessageBox(hwnd, "Could not create an OpenGL context.", "GEditor", MB_ICONERROR);
            return -1;
        }
        return 0;

    case WM_SIZE:
        if (state != NULL)
        {
            ViewportResizeGL(state, LOWORD(lparam), HIWORD(lparam));
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;

        BeginPaint(hwnd, &ps);
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

    case WM_CAPTURECHANGED:
        /**
         * If capture is taken away, act as if right mouse was released so the camera doesn't keep flying with a hidden cursor.
         */
        ViewportEndFly(hwnd, state);
        return 0;

    case WM_KILLFOCUS:
        if(state != NULL)   
        {
            state->keyw = state->keya = state->keys = state->keyd = state->keyq = state->keye = FALSE;
        }
        return 0;

    case WM_TIMER:
        if(wparam == VIEWPORT_FLY_TIMER)
        {
            ViewportFlyTick(hwnd, state);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if(state != NULL)
        {
            int clicks = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;

            while(clicks > 0)
            {
                state->speed *= 1.25f;
                clicks--;
            }

            while(clicks < 0)
            {
                state->speed /= 1.25f;
                clicks++;
            }

            if(state->speed < 1.0f)
            {
                state->speed = 1.0f;
            }

            if(state->speed > 50000.0f)
            {
                state->speed = 50000.0f;
            }
        }
        return 0;

    case WM_ERASEBKGND:
        /* GL repaints every pixel; skipping the GDI erase kills the
           flicker you would otherwise see on every resize. */
        return 1;

    case WM_DESTROY:
        ViewportEndFly(hwnd, state); // Never leave the cursor hidden.
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
