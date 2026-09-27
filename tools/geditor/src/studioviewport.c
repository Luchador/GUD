/* The studio's empty-scene preview. Scene rendering can grow here independently
 * of the game's display-list renderer and its native asset formats. */
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>
#include <math.h>
#include <stdlib.h>
#include "studioviewport.h"
#include "orbitcamera.h"

#define STUDIO_VIEWPORT_CLASS "GEditorStudioViewport"
#define STUDIO_FOV 45.0

typedef struct StudioViewport {
    HDC dc;
    HGLRC context;
    OrbitCamera camera;
    POINT mouse;
    unsigned buttons;
} StudioViewport;

void StudioViewportReset(HWND viewport)
{
    StudioViewport *state = (StudioViewport *)GetWindowLongPtr(viewport, GWLP_USERDATA);
    const double lower[3] = {-10, 0, -10}, upper[3] = {10, 0, 10};
    if (!state) { return; }
    state->buttons = 0;
    if (GetCapture() == viewport) { ReleaseCapture(); }
    OrbitCameraFrame(&state->camera, lower, upper, 1, STUDIO_FOV);
    state->camera.pitch = -25;
    InvalidateRect(viewport, NULL, FALSE);
}

static BOOL StudioViewportInit(HWND hwnd, StudioViewport *state)
{
    PIXELFORMATDESCRIPTOR format = {0};
    HDC previousdc = wglGetCurrentDC();
    HGLRC previous = wglGetCurrentContext();
    int index;
    state->dc = GetDC(hwnd);
    if (!state->dc) { return FALSE; }
    format.nSize = sizeof(format); format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA; format.cColorBits = 32;
    format.cDepthBits = 24; format.iLayerType = PFD_MAIN_PLANE;
    index = ChoosePixelFormat(state->dc, &format);
    if (!index || !SetPixelFormat(state->dc, index, &format)) { return FALSE; }
    state->context = wglCreateContext(state->dc);
    if (!state->context || !wglMakeCurrent(state->dc, state->context)) { return FALSE; }
    wglMakeCurrent(previousdc, previous);
    return TRUE;
}

static void StudioViewportDraw(const StudioViewport *state, int width, int height)
{
    double nearz, farz, eye[3], halfheight;
    if (width < 1 || height < 1) { return; }
    OrbitCameraClip(&state->camera, &nearz, &farz);
    halfheight = tan(STUDIO_FOV * 3.14159265358979323846 / 360.0) * nearz;
    glViewport(0, 0, width, height);
    glClearColor(0.13f, 0.14f, 0.16f, 1);
    glDepthMask(GL_TRUE); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glFrustum(-halfheight * width / height, halfheight * width / height,
        -halfheight, halfheight, nearz, farz);
    OrbitCameraPosition(&state->camera, eye);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glRotated(-state->camera.pitch, 1, 0, 0);
    glRotated(-state->camera.yaw, 0, 1, 0);
    glTranslated(-eye[0], -eye[1], -eye[2]);

    glLineWidth(1); glBegin(GL_LINES);
    for (int i = -10; i <= 10; i++)
    {
        if (!i) { continue; }
        if (i % 5) { glColor3ub(65, 69, 77); }
        else { glColor3ub(89, 94, 103); }
        glVertex3i(i, 0, -10); glVertex3i(i, 0, 10);
        glVertex3i(-10, 0, i); glVertex3i(10, 0, i);
    }
    glEnd();
    glLineWidth(2); glBegin(GL_LINES);
    glColor3ub(190, 83, 83); glVertex3i(-10, 0, 0); glVertex3i(10, 0, 0);
    glColor3ub(90, 175, 111); glVertex3i(0, 0, 0); glVertex3i(0, 3, 0);
    glColor3ub(88, 130, 205); glVertex3i(0, 0, -10); glVertex3i(0, 0, 10);
    glEnd(); glLineWidth(1);
}

static LRESULT CALLBACK StudioViewportProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    StudioViewport *state = (StudioViewport *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (message)
    {
    case WM_CREATE:
        state = calloc(1, sizeof(*state));
        if (!state) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        if (!StudioViewportInit(hwnd, state)) { return -1; }
        StudioViewportReset(hwnd);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: InvalidateRect(hwnd, NULL, FALSE); return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT paint; RECT client;
        HDC previousdc = wglGetCurrentDC();
        HGLRC previous = wglGetCurrentContext();
        BeginPaint(hwnd, &paint); GetClientRect(hwnd, &client);
        if (state && state->context && wglMakeCurrent(state->dc, state->context))
        {
            StudioViewportDraw(state, client.right, client.bottom);
            SwapBuffers(state->dc);
            wglMakeCurrent(previousdc, previous);
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        if (!state) { break; }
        SetFocus(hwnd); SetCapture(hwnd);
        state->buttons |= message == WM_LBUTTONDOWN ? MK_LBUTTON : message == WM_MBUTTONDOWN ? MK_MBUTTON : MK_RBUTTON;
        state->mouse = (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        if (!state) { break; }
        state->buttons &= ~(message == WM_LBUTTONUP ? MK_LBUTTON : message == WM_MBUTTONUP ? MK_MBUTTON : MK_RBUTTON);
        if (!state->buttons && GetCapture() == hwnd) { ReleaseCapture(); }
        return 0;
    case WM_MOUSEMOVE:
        if (state && state->buttons)
        {
            RECT client;
            POINT mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            GetClientRect(hwnd, &client);
            if (state->buttons & MK_MBUTTON)
                OrbitCameraPan(&state->camera, mouse.x - state->mouse.x, mouse.y - state->mouse.y, client.bottom, STUDIO_FOV);
            else
                OrbitCameraRotate(&state->camera, mouse.x - state->mouse.x, mouse.y - state->mouse.y);
            state->mouse = mouse; InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (state)
        {
            OrbitCameraDolly(&state->camera, (double)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_CANCELMODE:
    case WM_KILLFOCUS:
    case WM_CAPTURECHANGED:
        if (state) { state->buttons = 0; }
        if (GetCapture() == hwnd) { ReleaseCapture(); }
        return 0;
    case WM_NCDESTROY:
        if (state)
        {
            if (GetCapture() == hwnd) { ReleaseCapture(); }
            if (state->context)
            {
                if (wglGetCurrentContext() == state->context) { wglMakeCurrent(NULL, NULL); }
                wglDeleteContext(state->context);
            }
            if (state->dc) { ReleaseDC(hwnd, state->dc); }
            free(state); SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        break;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

HWND StudioViewportCreate(HWND parent, HINSTANCE instance)
{
    WNDCLASS window = {0};
    window.style = CS_OWNDC;
    window.lpfnWndProc = StudioViewportProc;
    window.hInstance = instance;
    window.hCursor = LoadCursor(NULL, IDC_ARROW);
    window.lpszClassName = STUDIO_VIEWPORT_CLASS;
    if (!RegisterClass(&window) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { return NULL; }
    return CreateWindowEx(WS_EX_CLIENTEDGE, STUDIO_VIEWPORT_CLASS, "Studio viewport",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
        0, 0, 16, 16, parent, NULL, instance, NULL);
}
