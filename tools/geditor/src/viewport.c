/**
  * GEditor 3D viewport.
  *
  * A child window with a private DC (CS_OWNDC) and a legacy OpenGL
  * context. We're using fixed-function OpenGL because it closely mimics
  * the N64's pipeline.
  */

#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "browser.h"
#include "viewport.h"

#define VIEWPORT_CLASS "GEditorViewport"

/**
 * Default to 4000.0, but can be changed with the mouse scroll wheel.
 */
#define VIEWPORT_FLY_SPEED 4000.0f

#define VIEWPORT_FOV_Y 60.0f
#define VIEWPORT_NEAR_Z 10.0f
#define VIEWPORT_FAR_Z 100000.0f

/**
 * Degrees per pixel of mouse movement in the viewport.
 */
#define VIEWPORT_LOOK_SENSITIVITY 0.15f

#define VIEWPORT_PITCH_LIMIT 89.0f

#define VIEWPORT_DEG_TO_RAD (3.14159265358979323846f / 180.0f)
#define VIEWPORT_PAD_HALF_SIZE 5.0f
#define VIEWPORT_BOX_VERTICES  24
#define VIEWPORT_STAN_FILL_ALPHA 112
#define VIEWPORT_STAN_EDGE_ALPHA 224
#define VIEWPORT_PORTAL_FILL_ALPHA 64
#define VIEWPORT_PORTAL_EDGE_ALPHA 255
#define VIEWPORT_PICK_EPSILON 1.0e-10
#define VIEWPORT_PICK_BARY_EPSILON 1.0e-8
#define VIEWPORT_PICK_COPLANAR_EPSILON 1.0e-3
#define VIEWPORT_PICK_COPLANAR_RELATIVE_EPSILON 1.0e-6

/* A contiguous run of scene vertices sharing one texture. */
typedef struct SceneBatch {
    GLuint  gltex;      /* 0 = untextured, vertex colors only */
    GLsizei first;
    GLsizei count;
    BOOL    secondary;  /* transparent layer: blended, no depth write */
    BOOL    cullbackfaces;
    BOOL    object;     /* setup model, independent of BG visibility */
} SceneBatch;

struct ViewportState;
static void ViewportFreeScene(struct ViewportState *state);

typedef struct Vertex {
    GLfloat x, y, z;
    GLubyte r, g, b, a;
    GLfloat s, t;
} Vertex;

typedef struct VertexColor {
    GLubyte r, g, b;
} VertexColor;

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
    Vertex *scene;       /* malloc'd level geometry, or NULL for the test scene */
    VertexColor *scenecolors; /* original RGB restored when faces are deselected */
    GLsizei scenecount;  /* vertices in scene */
    struct SceneBatch *batches;  /* texture-sorted draw ranges */
    int batchcount;
    unsigned char *selectedtris; /* one byte per texture-sorted triangle */
    int selectedtricount;
    GLuint *textures;    /* GL texture names owned by the scene */
    int texturecount;
    Vertex *stanfill;    /* translucent GL_TRIANGLES tile overlay */
    GLsizei stanfillcount;
    Vertex *stanedges;   /* colored GL_LINES around each tile */
    GLsizei stanedgecount;
    Vertex *portalfill;  /* half-transparent cyan portal polygons */
    GLsizei portalfillcount;
    Vertex *portaledges; /* opaque cyan GL_LINES around portals */
    GLsizei portaledgecount;
    Vertex *padmarkers;  /* GL_LINES: 24 vertices per wireframe box */
    GLsizei padmarkercount;
    BOOL showbgprimary;
    BOOL showbgsecondary;
    BOOL showstan;
    BOOL showportals;
    BOOL cullbackfaces;  /* master toggle for authored backface culling */
    BOOL keyw, keya, keys, keyd, keyq, keye;
    POINT lastmouse;
    LONGLONG lastqpc;   /* QueryPerformanceCounter at the previous frame */
} ViewportState;


static const Vertex g_TestScene[6] = {
    {    0.0f,  160.0f, 0.0f,   255,  40,  40, 255 , 1.0f, 0.0f},
    { -160.0f, -120.0f, 0.0f,    40, 255,  40, 255 , 0.0f, 1.0f},
    {  160.0f, -120.0f, 0.0f,    40,  40, 255, 255 , 0.0f, 0.0f},
    {    -80.0f,  160.0f, -200.0f,   255,  255,  0, 255, 2.0f, 0.0f},
    { -240.0f, -120.0f, -200.0f,    0, 255,  255, 255, 2.0f, 2.0f},
    {  80.0f, -120.0f, -200.0f,    255,  0, 0, 255 , 0.0f, 0.0f},
};

#define TESTSCENE_VERTS ((GLsizei)(sizeof(g_TestScene) / sizeof(g_TestScene[0])))

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

    /* Ask the driver to pace SwapBuffers to the display refresh
       (vsync). While flying, the render loop then runs at exactly the
       monitor rate and SwapBuffers blocking IS the frame limiter. The
       function is an extension a driver may not provide; flying still
       works without it, just unpaced. */
    {
        typedef BOOL (WINAPI *SwapIntervalFn)(int);
        SwapIntervalFn setinterval =
            (SwapIntervalFn)(void *)wglGetProcAddress("wglSwapIntervalEXT");
        if (setinterval != NULL)
        {
            setinterval(1);
        }
    }

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

    /* Fast3D and OpenGL both treat counter-clockwise faces as front. */
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);

    /* Loaded BG scenes select culling per batch below. The built-in
       test scene continues to use the menu option as a global switch. */
    if (state->scene == NULL && state->cullbackfaces)
    {
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }

    /**
     * Build the view transform. -z = forward. Eye stays at the origin.
     */
    glLoadIdentity();
    glRotatef(-state->pitch, 1.0f, 0.0f, 0.0f);
    glRotatef(-state->yaw,   0.0f, 1.0f, 0.0f);
    glTranslatef(-state->posx, -state->posy, -state->posz);

    {
        const Vertex *verts = state->scene != NULL ? state->scene : g_TestScene;
        GLsizei count = state->scene != NULL ? state->scenecount : TESTSCENE_VERTS;

        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &verts[0].x);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &verts[0].r);
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &verts[0].s);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        if (state->scene != NULL && state->batchcount > 0)
        {
            int i;
            BOOL insecondary = FALSE;
            BOOL incullback = FALSE;

            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

            for (i = 0; i < state->batchcount; i++)
            {
                const SceneBatch *batch = &state->batches[i];
                BOOL wantcullback;

                if (!batch->object && ((!batch->secondary && !state->showbgprimary) || (batch->secondary && !state->showbgsecondary)))
                {
                    continue;
                }

                wantcullback = state->cullbackfaces && batch->cullbackfaces;

                if (batch->secondary && !insecondary)
                {
                    /*
                     * Secondary pass: blended glass and decals. Depth
                     * WRITES stop - transparent surfaces must not
                     * occlude each other or later batches - but depth
                     * TESTING continues, so walls still hide windows
                     * behind them. No per-triangle sorting yet;
                     * overlapping transparencies may pick the wrong
                     * winner, which matches the console's own habits.
                     */
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                    insecondary = TRUE;
                }

                if (wantcullback != incullback)
                {
                    if (wantcullback)
                    {
                        glEnable(GL_CULL_FACE);
                    }
                    else
                    {
                        glDisable(GL_CULL_FACE);
                    }

                    incullback = wantcullback;
                }

                if (batch->gltex != 0)
                {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, batch->gltex);
                }
                else
                {
                    glDisable(GL_TEXTURE_2D);
                }

                glDrawArrays(GL_TRIANGLES, batch->first, batch->count);
            }

            glDisable(GL_TEXTURE_2D);
            glDisable(GL_ALPHA_TEST);
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
        }
        else
        {
            glDrawArrays(GL_TRIANGLES, 0, count);
        }

        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    if (state->showstan && state->stanfill != NULL && state->stanfillcount > 0)
    {
        /* Stan polygons commonly lie directly on their matching BG
           floors. Pull the overlay infinitesimally toward the camera
           to prevent z-fighting while retaining normal depth tests. */
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);

        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &state->stanfill[0].x);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &state->stanfill[0].r);
        glDrawArrays(GL_TRIANGLES, 0, state->stanfillcount);

        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    if (state->showstan && state->stanedges != NULL && state->stanedgecount > 0)
    {
        /* Tile outlines make adjacent polygons readable even when they
           share the same authored RGB value. */
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);

        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &state->stanedges[0].x);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &state->stanedges[0].r);
        glLineWidth(1.0f);
        glDrawArrays(GL_LINES, 0, state->stanedgecount);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    if (state->showportals && state->portalfill != NULL && state->portalfillcount > 0)
    {
        /* Portals are editor-only, double-sided translucent surfaces.
           Keep depth testing so a portal remains hidden by walls, but
           do not let its fill occlude its own outline. */
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);

        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &state->portalfill[0].x);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &state->portalfill[0].r);
        glDrawArrays(GL_TRIANGLES, 0, state->portalfillcount);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    if (state->showportals && state->portaledges != NULL && state->portaledgecount > 0)
    {
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);

        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &state->portaledges[0].x);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), &state->portaledges[0].r);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, state->portaledgecount);
        glLineWidth(1.0f);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    if (state->padmarkers != NULL && state->padmarkercount > 0)
    {
        /* Editor overlays remain depth-tested, so pads hidden behind a
           wall do not turn the whole level into an unreadable lattice. */
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);

        glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &state->padmarkers[0].x);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex),
                       &state->padmarkers[0].r);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, state->padmarkercount);
        glLineWidth(1.0f);
    }

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
    QueryPerformanceCounter((LARGE_INTEGER *)&state->lastqpc);
}


static void ViewportEndFly(HWND hwnd, ViewportState *state)
{
    if (state == NULL || !state->flying)
    {
        return;
    }

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

    /* Clamp: at exactly +/-90 the forward and up axes align and the
       view matrix degenerates. */
    if (state->pitch >  VIEWPORT_PITCH_LIMIT) state->pitch =  VIEWPORT_PITCH_LIMIT;
    if (state->pitch < -VIEWPORT_PITCH_LIMIT) state->pitch = -VIEWPORT_PITCH_LIMIT;

    SetCursorPos(state->lastmouse.x, state->lastmouse.y);

    /* No redraw here: while flying, ViewportFlyFrame renders once per
       pass of the main loop. Painting per mouse event at a 1000 Hz
       polling rate is exactly what caused the stutter. */
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


/*
 * One flight frame: advance the camera by the measured elapsed time,
 * then render immediately.
 *
 * Called by the main message loop once per pass while flying - not by
 * a timer, and not through WM_PAINT. Both are the lowest-priority
 * messages Windows has, and a fast mouse floods the queue faster than
 * they can surface: that starvation was the skipping camera. Rendering
 * here, synchronously, makes the frame rate independent of queue
 * pressure; with vsync on, SwapBuffers blocking paces the loop to the
 * monitor.
 */
void ViewportFlyFrame(HWND hwnd)
{
    ViewportState *state = ViewportGetState(hwnd);
    float fwd[3];
    float right[3];
    float move[3];
    LONGLONG now;
    LONGLONG freq;
    float dt;
    float dist;
    int moving;

    if (state == NULL || !state->flying)
    {
        return;
    }

    QueryPerformanceCounter((LARGE_INTEGER *)&now);
    QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
    dt = (float)(now - state->lastqpc) / (float)freq;
    state->lastqpc = now;

    /* A stall (menu open, window drag) makes one interval huge; clamp
       so the camera does not teleport when flight resumes. */
    if (dt > 0.1f)
    {
        dt = 0.1f;
    }

    moving = state->keyw || state->keya || state->keys
          || state->keyd || state->keyq || state->keye;

    if (moving)
    {
        ViewportGetBasis(state, fwd, right);

        move[0] = move[1] = move[2] = 0.0f;

        if (state->keyw) { move[0] += fwd[0];   move[1] += fwd[1];   move[2] += fwd[2];   }
        if (state->keys) { move[0] -= fwd[0];   move[1] -= fwd[1];   move[2] -= fwd[2];   }
        if (state->keyd) { move[0] += right[0]; move[1] += right[1]; move[2] += right[2]; }
        if (state->keya) { move[0] -= right[0]; move[1] -= right[1]; move[2] -= right[2]; }
        if (state->keye) { move[1] += 1.0f; }
        if (state->keyq) { move[1] -= 1.0f; }

        dist = sqrtf(move[0] * move[0] + move[1] * move[1] + move[2] * move[2]);
        if (dist > 0.0001f)
        {
            dist = (state->speed * dt) / dist;
            state->posx += move[0] * dist;
            state->posy += move[1] * dist;
            state->posz += move[2] * dist;
        }
    }

    /* Render now, and tell Windows the window is clean so no stale
       WM_PAINT arrives behind our back. */
    ViewportPaintGL(state);
    ValidateRect(hwnd, NULL);
}

BOOL ViewportIsFlying(HWND hwnd)
{
    ViewportState *state = ViewportGetState(hwnd);

    return (state != NULL) && state->flying;
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


typedef struct ViewportPickRay {
    double origin[3];
    double direction[3];
    double mindistance;
    double maxdistance;
} ViewportPickRay;


static BOOL ViewportBuildPickRay(HWND hwnd, const ViewportState *state,
                                 int mousex, int mousey,
                                 ViewportPickRay *ray)
{
    RECT client;
    float fwdf[3];
    float rightf[3];
    double fwd[3];
    double right[3];
    double up[3];
    double ndcx;
    double ndcy;
    double halfheight;
    double aspect;
    double length;
    double forwardcomponent;
    int width;
    int height;
    int axis;

    if (!GetClientRect(hwnd, &client))
    {
        return FALSE;
    }

    width = client.right - client.left;
    height = client.bottom - client.top;

    if (width < 1 || height < 1
        || mousex < 0 || mousex >= width
        || mousey < 0 || mousey >= height)
    {
        return FALSE;
    }

    ViewportGetBasis(state, fwdf, rightf);

    for (axis = 0; axis < 3; axis++)
    {
        fwd[axis] = fwdf[axis];
        right[axis] = rightf[axis];
    }

    /* right x forward is the camera's screen-up vector. */
    up[0] = right[1] * fwd[2] - right[2] * fwd[1];
    up[1] = right[2] * fwd[0] - right[0] * fwd[2];
    up[2] = right[0] * fwd[1] - right[1] * fwd[0];

    ndcx = ((mousex + 0.5) * 2.0 / width) - 1.0;
    ndcy = 1.0 - ((mousey + 0.5) * 2.0 / height);
    halfheight = tan(VIEWPORT_FOV_Y * 0.5 * VIEWPORT_DEG_TO_RAD);
    aspect = (double)width / height;

    for (axis = 0; axis < 3; axis++)
    {
        ray->direction[axis] = fwd[axis]
            + right[axis] * ndcx * aspect * halfheight
            + up[axis] * ndcy * halfheight;
    }

    length = sqrt(ray->direction[0] * ray->direction[0] + ray->direction[1] * ray->direction[1] + ray->direction[2] * ray->direction[2]);

    if (!(length > 0.0))
    {
        return FALSE;
    }

    for (axis = 0; axis < 3; axis++)
    {
        ray->direction[axis] /= length;
    }

    ray->origin[0] = state->posx;
    ray->origin[1] = state->posy;
    ray->origin[2] = state->posz;

    forwardcomponent = ray->direction[0] * fwd[0] + ray->direction[1] * fwd[1] + ray->direction[2] * fwd[2];

    if (!(forwardcomponent > 0.0))
    {
        return FALSE;
    }

    /* The OpenGL near/far planes are perpendicular to camera forward,
       not spherical distances from the eye. Convert them to distances
       along this particular off-axis ray. */
    ray->mindistance = VIEWPORT_NEAR_Z / forwardcomponent;
    ray->maxdistance = VIEWPORT_FAR_Z / forwardcomponent;

    return TRUE;
}


static BOOL ViewportRayTriangleDistance(const ViewportPickRay *ray, const Vertex *vertices, BOOL cullbackfaces, double *distanceout)
{
    double edge1[3];
    double edge2[3];
    double fromvertex[3];
    double cross1[3];
    double cross2[3];
    double determinant;
    double inverse;
    double u;
    double v;
    double distance;

    edge1[0] = vertices[1].x - vertices[0].x;
    edge1[1] = vertices[1].y - vertices[0].y;
    edge1[2] = vertices[1].z - vertices[0].z;
    edge2[0] = vertices[2].x - vertices[0].x;
    edge2[1] = vertices[2].y - vertices[0].y;
    edge2[2] = vertices[2].z - vertices[0].z;
    fromvertex[0] = ray->origin[0] - vertices[0].x;
    fromvertex[1] = ray->origin[1] - vertices[0].y;
    fromvertex[2] = ray->origin[2] - vertices[0].z;

    cross1[0] = ray->direction[1] * edge2[2] - ray->direction[2] * edge2[1];
    cross1[1] = ray->direction[2] * edge2[0] - ray->direction[0] * edge2[2];
    cross1[2] = ray->direction[0] * edge2[1] - ray->direction[1] * edge2[0];
    determinant = edge1[0] * cross1[0] + edge1[1] * cross1[1] + edge1[2] * cross1[2];

    if (cullbackfaces)
    {
        if (determinant <= VIEWPORT_PICK_EPSILON)
        {
            return FALSE;
        }
    }
    else if (fabs(determinant) <= VIEWPORT_PICK_EPSILON)
    {
        return FALSE;
    }

    inverse = 1.0 / determinant;
    u = (fromvertex[0] * cross1[0]
       + fromvertex[1] * cross1[1]
       + fromvertex[2] * cross1[2]) * inverse;
       
    if (u < -VIEWPORT_PICK_BARY_EPSILON || u > 1.0 + VIEWPORT_PICK_BARY_EPSILON)
    {
        return FALSE;
    }

    cross2[0] = fromvertex[1] * edge1[2] - fromvertex[2] * edge1[1];
    cross2[1] = fromvertex[2] * edge1[0] - fromvertex[0] * edge1[2];
    cross2[2] = fromvertex[0] * edge1[1] - fromvertex[1] * edge1[0];

    v = (ray->direction[0] * cross2[0] + ray->direction[1] * cross2[1] + ray->direction[2] * cross2[2]) * inverse;

    if (v < -VIEWPORT_PICK_BARY_EPSILON || u + v > 1.0 + VIEWPORT_PICK_BARY_EPSILON)
    {
        return FALSE;
    }

    distance = (edge2[0] * cross2[0] + edge2[1] * cross2[1] + edge2[2] * cross2[2]) * inverse;

    if (distance < ray->mindistance || distance > ray->maxdistance)
    {
        return FALSE;
    }

    *distanceout = distance;
    return TRUE;
}


static BOOL ViewportBatchIsPickable(const ViewportState *state,
                                    const SceneBatch *batch)
{
    return !batch->object
        && (batch->secondary ? state->showbgsecondary
                             : state->showbgprimary);
}


static double ViewportCoplanarPickTolerance(double distance)
{
    double relative = distance * VIEWPORT_PICK_COPLANAR_RELATIVE_EPSILON;

    return relative > VIEWPORT_PICK_COPLANAR_EPSILON
        ? relative : VIEWPORT_PICK_COPLANAR_EPSILON;
}


static int ViewportFindPickedTriangle(HWND hwnd, const ViewportState *state,
                                      int mousex, int mousey,
                                      BOOL addtoselection, BOOL deselect,
                                      BOOL *hitanything)
{
    ViewportPickRay ray;
    double nearestdistance = DBL_MAX;
    double coplanartolerance;
    int firsttriangle = -1;
    int firstselected = -1;
    int firstunselected = -1;
    int nextafterselected = -1;
    int selectedhits = 0;
    BOOL passedselected = FALSE;
    int batchindex;
    int secondary;

    *hitanything = FALSE;

    if (state->scene == NULL || state->selectedtris == NULL || !ViewportBuildPickRay(hwnd, state, mousex, mousey, &ray))
    {
        return -1;
    }

    for (batchindex = 0; batchindex < state->batchcount; batchindex++)
    {
        const SceneBatch *batch = &state->batches[batchindex];
        BOOL cullbackfaces;
        int vertex;
        int end;

        if (!ViewportBatchIsPickable(state, batch))
        {
            continue;
        }

        cullbackfaces = state->cullbackfaces && batch->cullbackfaces;
        end = batch->first + batch->count;

        for (vertex = batch->first; vertex + 2 < end; vertex += 3)
        {
            double distance;

            if (ViewportRayTriangleDistance(&ray, &state->scene[vertex],
                                            cullbackfaces, &distance)
                && distance < nearestdistance)
            {
                nearestdistance = distance;
            }
        }
    }

    if (nearestdistance == DBL_MAX)
    {
        return -1;
    }

    *hitanything = TRUE;
    coplanartolerance = ViewportCoplanarPickTolerance(nearestdistance);

    /* Secondary geometry is drawn over the primary layer and commonly
       contains signs, decals, and other coplanar details. Put it first
       in the hit stack, then allow repeated clicks to cycle down to the
       primary surface beneath it. */
    for (secondary = 1; secondary >= 0; secondary--)
    {
        for (batchindex = 0; batchindex < state->batchcount; batchindex++)
        {
            const SceneBatch *batch = &state->batches[batchindex];
            BOOL cullbackfaces;
            int vertex;
            int end;

            if (!ViewportBatchIsPickable(state, batch)
                || batch->secondary != secondary)
            {
                continue;
            }

            cullbackfaces = state->cullbackfaces && batch->cullbackfaces;
            end = batch->first + batch->count;

            for (vertex = batch->first; vertex + 2 < end; vertex += 3)
            {
                double distance;
                int triangle = vertex / 3;

                if (!ViewportRayTriangleDistance(&ray, &state->scene[vertex],
                                                 cullbackfaces, &distance)
                    || fabs(distance - nearestdistance) > coplanartolerance)
                {
                    continue;
                }

                if (firsttriangle < 0)
                {
                    firsttriangle = triangle;
                }

                if (!state->selectedtris[triangle] && firstunselected < 0)
                {
                    firstunselected = triangle;
                }

                if (state->selectedtris[triangle] && firstselected < 0)
                {
                    firstselected = triangle;
                }

                if (passedselected && nextafterselected < 0)
                {
                    nextafterselected = triangle;
                }

                if (state->selectedtris[triangle])
                {
                    selectedhits++;
                    passedselected = TRUE;
                }
            }
        }
    }

    if (deselect)
    {
        return firstselected;
    }

    if (addtoselection && firstunselected >= 0)
    {
        return firstunselected;
    }

    if (!addtoselection && selectedhits == 1)
    {
        return nextafterselected >= 0 ? nextafterselected : firsttriangle;
    }

    return firsttriangle;
}


static void ViewportSetTriangleColor(ViewportState *state, int triangle,
                                     BOOL selected)
{
    int vertex;
    int end = triangle * 3 + 3;

    for (vertex = triangle * 3; vertex < end; vertex++)
    {
        if (selected)
        {
            state->scene[vertex].r = 0;
            state->scene[vertex].g = 255;
            state->scene[vertex].b = 255;
        }
        else
        {
            state->scene[vertex].r = state->scenecolors[vertex].r;
            state->scene[vertex].g = state->scenecolors[vertex].g;
            state->scene[vertex].b = state->scenecolors[vertex].b;
        }
    }
}


static void ViewportClearSelection(ViewportState *state)
{
    int triangle;
    int trianglecount = state->scenecount / 3;

    if (state->selectedtricount == 0)
    {
        return;
    }

    for (triangle = 0; triangle < trianglecount; triangle++)
    {
        if (state->selectedtris[triangle])
        {
            ViewportSetTriangleColor(state, triangle, FALSE);
        }
    }

    memset(state->selectedtris, 0, (size_t)trianglecount);
    state->selectedtricount = 0;
}


static void ViewportPickAt(HWND hwnd, ViewportState *state, int mousex,
                           int mousey, BOOL addtoselection, BOOL deselect)
{
    BOOL hitanything;
    int triangle;

    if (state == NULL || state->selectedtris == NULL
        || state->scenecolors == NULL || state->flying)
    {
        return;
    }

    if (deselect)
    {
        addtoselection = FALSE;
    }

    triangle = ViewportFindPickedTriangle(hwnd, state, mousex, mousey,
                                          addtoselection, deselect,
                                          &hitanything);

    if (deselect)
    {
        if (triangle >= 0)
        {
            state->selectedtris[triangle] = 0;
            state->selectedtricount--;
            ViewportSetTriangleColor(state, triangle, FALSE);
        }
        else if (!hitanything)
        {
            ViewportClearSelection(state);
        }

        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    /* A miss always clears, even with the additive-selection modifier.
       A normal hit also replaces the old selection. */
    if (triangle < 0 || !addtoselection)
    {
        ViewportClearSelection(state);
    }

    if (triangle >= 0 && !state->selectedtris[triangle])
    {
        state->selectedtris[triangle] = 1;
        state->selectedtricount++;
        ViewportSetTriangleColor(state, triangle, TRUE);
    }

    InvalidateRect(hwnd, NULL, FALSE);
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
        state->showbgprimary = TRUE;
        state->showbgsecondary = TRUE;
        state->showstan = FALSE;
        state->showportals = FALSE;
        state->cullbackfaces = TRUE;

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

    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        ViewportPickAt(hwnd, state, GET_X_LPARAM(lparam),
                       GET_Y_LPARAM(lparam), (wparam & MK_SHIFT) != 0,
                       (wparam & MK_CONTROL) != 0);
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


    case WM_MOUSEWHEEL:
        /* The wheel is fly-speed only while flying. Otherwise let
           DefWindowProc bubble it to the frame, which routes it to
           whichever panel the cursor is over. */
        if (state != NULL && state->flying)
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

            return 0; /* consumed: wheel steers fly speed */
        }
        break; /* not flying: DefWindowProc forwards the wheel to the frame */

    case WM_ERASEBKGND:
        /* GL repaints every pixel; skipping the GDI erase kills the
           flicker you would otherwise see on every resize. */
        return 1;

    case WM_DESTROY:
        if (state != NULL)
        {
            ViewportFreeScene(state);
        }
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


/* Frees the scene's GL textures, geometry, and editor overlays. Needs the
   GL context current for glDeleteTextures. */
static void ViewportFreeScene(struct ViewportState *state_)
{
    ViewportState *state = (ViewportState *)state_;

    if (state->texturecount > 0)
    {
        wglMakeCurrent(state->hdc, state->hglrc);
        glDeleteTextures(state->texturecount, state->textures);
    }

    free(state->textures);
    free(state->batches);
    free(state->selectedtris);
    free(state->scenecolors);
    free(state->stanedges);
    free(state->stanfill);
    free(state->portaledges);
    free(state->portalfill);
    free(state->padmarkers);
    free(state->scene);
    state->textures = NULL;
    state->batches = NULL;
    state->selectedtris = NULL;
    state->scenecolors = NULL;
    state->stanedges = NULL;
    state->stanfill = NULL;
    state->portaledges = NULL;
    state->portalfill = NULL;
    state->padmarkers = NULL;
    state->scene = NULL;
    state->texturecount = 0;
    state->batchcount = 0;
    state->selectedtricount = 0;
    state->stanedgecount = 0;
    state->stanfillcount = 0;
    state->portaledgecount = 0;
    state->portalfillcount = 0;
    state->padmarkercount = 0;
    state->scenecount = 0;
}


static void ViewportSetStanVertex(Vertex *vertex, const StanPoint *point,
                                  const StanTile *tile,
                                  unsigned char alpha)
{
    vertex->x = point->x;
    vertex->y = point->y;
    vertex->z = point->z;
    vertex->r = tile->red;
    vertex->g = tile->green;
    vertex->b = tile->blue;
    vertex->a = alpha;
    vertex->s = 0.0f;
    vertex->t = 0.0f;
}


void ViewportSetStanTiles(HWND hwnd, const StanFile *stan)
{
    ViewportState *state = ViewportGetState(hwnd);
    Vertex *fill = NULL;
    Vertex *edges = NULL;
    size_t fillcount = 0;
    size_t edgecount = 0;
    size_t fillat = 0;
    size_t edgeat = 0;
    DWORD i;

    if (state == NULL)
    {
        return;
    }

    free(state->stanfill);
    free(state->stanedges);
    state->stanfill = NULL;
    state->stanedges = NULL;
    state->stanfillcount = 0;
    state->stanedgecount = 0;

    if (stan == NULL || stan->tiles == NULL || stan->tilecount == 0)
    {
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    for (i = 0; i < stan->tilecount; i++)
    {
        fillcount += (stan->tiles[i].pointcount - 2) * 3;
        edgecount += stan->tiles[i].pointcount * 2;
    }

    fill = (Vertex *)malloc(fillcount * sizeof(*fill));
    edges = (Vertex *)malloc(edgecount * sizeof(*edges));
    if (fill == NULL || edges == NULL)
    {
        free(fill);
        free(edges);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    for (i = 0; i < stan->tilecount; i++)
    {
        const StanTile *tile = &stan->tiles[i];
        unsigned int point;

        /* Stan polygons are authored in perimeter order and are convex
           for the game's point-in-tile tests, so a fan preserves the
           original 3-10 sided face without inventing new positions. */
        for (point = 1; point + 1 < tile->pointcount; point++)
        {
            ViewportSetStanVertex(&fill[fillat++], &tile->points[0],
                                  tile, VIEWPORT_STAN_FILL_ALPHA);
            ViewportSetStanVertex(&fill[fillat++], &tile->points[point],
                                  tile, VIEWPORT_STAN_FILL_ALPHA);
            ViewportSetStanVertex(&fill[fillat++], &tile->points[point + 1],
                                  tile, VIEWPORT_STAN_FILL_ALPHA);
        }

        for (point = 0; point < tile->pointcount; point++)
        {
            ViewportSetStanVertex(&edges[edgeat++], &tile->points[point],
                                  tile, VIEWPORT_STAN_EDGE_ALPHA);
            ViewportSetStanVertex(&edges[edgeat++],
                                  &tile->points[(point + 1) % tile->pointcount],
                                  tile, VIEWPORT_STAN_EDGE_ALPHA);
        }
    }

    state->stanfill = fill;
    state->stanedges = edges;
    state->stanfillcount = (GLsizei)fillat;
    state->stanedgecount = (GLsizei)edgeat;
    InvalidateRect(hwnd, NULL, FALSE);
}


static void ViewportSetPortalVertex(Vertex *vertex,
                                    const BgPortalPoint *point,
                                    unsigned char alpha)
{
    vertex->x = point->x;
    vertex->y = point->y;
    vertex->z = point->z;
    vertex->r = 0;
    vertex->g = 255;
    vertex->b = 255;
    vertex->a = alpha;
    vertex->s = 0.0f;
    vertex->t = 0.0f;
}


static BOOL ViewportPortalGeometryIsFirst(const BgPortalFile *file,
                                          DWORD index)
{
    DWORD earlier;

    for (earlier = 0; earlier < index; earlier++)
    {
        if (file->portals[earlier].geometryoffset
            == file->portals[index].geometryoffset)
        {
            return FALSE;
        }
    }

    return TRUE;
}


void ViewportSetPortals(HWND hwnd, const BgPortalFile *portals)
{
    ViewportState *state = ViewportGetState(hwnd);
    Vertex *fill = NULL;
    Vertex *edges = NULL;
    size_t fillcount = 0;
    size_t edgecount = 0;
    size_t fillat = 0;
    size_t edgeat = 0;
    DWORD i;

    if (state == NULL)
    {
        return;
    }

    free(state->portalfill);
    free(state->portaledges);
    state->portalfill = NULL;
    state->portaledges = NULL;
    state->portalfillcount = 0;
    state->portaledgecount = 0;

    if (portals == NULL || portals->portals == NULL
        || portals->portalcount == 0)
    {
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    for (i = 0; i < portals->portalcount; i++)
    {
        if (ViewportPortalGeometryIsFirst(portals, i))
        {
            fillcount += (portals->portals[i].pointcount - 2) * 3;
            edgecount += portals->portals[i].pointcount * 2;
        }
    }

    fill = (Vertex *)malloc(fillcount * sizeof(*fill));
    edges = (Vertex *)malloc(edgecount * sizeof(*edges));
    if (fill == NULL || edges == NULL)
    {
        free(fill);
        free(edges);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    for (i = 0; i < portals->portalcount; i++)
    {
        const BgPortal *portal = &portals->portals[i];
        unsigned int point;

        if (!ViewportPortalGeometryIsFirst(portals, i))
        {
            continue;
        }

        /* Authored portal polygons are convex and perimeter ordered.
           Most have four points, while the few 3-7 point records use
           the same fan triangulation as the game's visibility tests. */
        for (point = 1; point + 1 < portal->pointcount; point++)
        {
            ViewportSetPortalVertex(&fill[fillat++], &portal->points[0],
                                    VIEWPORT_PORTAL_FILL_ALPHA);
            ViewportSetPortalVertex(&fill[fillat++], &portal->points[point],
                                    VIEWPORT_PORTAL_FILL_ALPHA);
            ViewportSetPortalVertex(&fill[fillat++],
                                    &portal->points[point + 1],
                                    VIEWPORT_PORTAL_FILL_ALPHA);
        }

        for (point = 0; point < portal->pointcount; point++)
        {
            ViewportSetPortalVertex(&edges[edgeat++], &portal->points[point],
                                    VIEWPORT_PORTAL_EDGE_ALPHA);
            ViewportSetPortalVertex(&edges[edgeat++],
                &portal->points[(point + 1) % portal->pointcount],
                VIEWPORT_PORTAL_EDGE_ALPHA);
        }
    }

    state->portalfill = fill;
    state->portaledges = edges;
    state->portalfillcount = (GLsizei)fillat;
    state->portaledgecount = (GLsizei)edgeat;
    InvalidateRect(hwnd, NULL, FALSE);
}


void ViewportSetGeometryVisibility(HWND hwnd, BOOL bgprimary,
                                   BOOL bgsecondary, BOOL stan,
                                   BOOL portals)
{
    ViewportState *state = ViewportGetState(hwnd);

    if (state == NULL)
    {
        return;
    }

    state->showbgprimary = bgprimary;
    state->showbgsecondary = bgsecondary;
    state->showstan = stan;
    state->showportals = portals;
    InvalidateRect(hwnd, NULL, FALSE);
}

/* qsort helper: keep the transparent pass last, then group triangles
   by texture and authored culling state. */
typedef struct TriKey { unsigned short tag; int tri; } TriKey;

static int ViewportTriKeyCompare(const void *a, const void *b)
{
    const TriKey *ka = (const TriKey *)a;
    const TriKey *kb = (const TriKey *)b;
    int d = (int)BG_TRI_IS_SECONDARY(ka->tag)
          - (int)BG_TRI_IS_SECONDARY(kb->tag);

    if (d == 0)
    {
        d = (int)BG_TRI_IS_OBJECT(ka->tag)
          - (int)BG_TRI_IS_OBJECT(kb->tag);
    }

    if (d == 0)
    {
        d = (int)BG_TEX_ID(ka->tag) - (int)BG_TEX_ID(kb->tag);
    }

    if (d == 0)
    {
        d = (int)BG_TRI_CULLS_BACK(ka->tag)
          - (int)BG_TRI_CULLS_BACK(kb->tag);
    }

    return d != 0 ? d : ka->tri - kb->tri;
}

void ViewportSetScene(HWND hwnd, const BgVertex *tris,
                      const unsigned short *tritags, int tricount,
                      const char *projectdir)
{
    ViewportState *state = (ViewportState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    Vertex *scene = NULL;
    VertexColor *scenecolors = NULL;
    SceneBatch *batches = NULL;
    GLuint *textures = NULL;
    unsigned char *selectedtris = NULL;
    TriKey *order = NULL;
    TexPixel *decode = NULL;
    int batchcount = 0;
    int texturecount = 0;
    float minx = 0, miny = 0, minz = 0, maxx = 0, maxy = 0, maxz = 0;
    int i;

    if (state == NULL)
    {
        return;
    }

    if (tris != NULL && tricount > 0)
    {
        scene = (Vertex *)malloc((size_t)tricount * 3 * sizeof(Vertex));
        scenecolors = (VertexColor *)malloc((size_t)tricount * 3
                                            * sizeof(*scenecolors));
        order = (TriKey *)malloc((size_t)tricount * sizeof(TriKey));
        batches = (SceneBatch *)malloc((size_t)tricount * sizeof(SceneBatch));
        textures = (GLuint *)malloc((size_t)tricount * sizeof(GLuint));
        selectedtris = (unsigned char *)calloc((size_t)tricount,
                                               sizeof(*selectedtris));
        decode = (TexPixel *)malloc(256 * 256 * sizeof(TexPixel));

        if (scene == NULL || scenecolors == NULL || order == NULL || batches == NULL
            || textures == NULL || selectedtris == NULL || decode == NULL)
        {
            free(scene); free(scenecolors); free(order); free(batches);
            free(textures); free(selectedtris); free(decode);
            return; /* keep whatever we had */
        }

        /* Keep primary geometry before secondary geometry, then group
           by texture and culling state to minimize GL state changes. */
        for (i = 0; i < tricount; i++)
        {
            order[i].tag = tritags != NULL ? tritags[i] : BG_TEX_NONE;
            order[i].tri = i;
        }

        qsort(order, (size_t)tricount, sizeof(TriKey), ViewportTriKeyCompare);

        wglMakeCurrent(state->hdc, state->hglrc);

        for (i = 0; i < tricount; i++)
        {
            const BgVertex *src = &tris[order[i].tri * 3];
            Vertex *dst = &scene[i * 3];
            float invw = 0.0f;
            float invh = 0.0f;
            int k;

            if (i == 0 || order[i].tag != order[i - 1].tag)
            {
                SceneBatch *batch = &batches[batchcount++];

                batch->gltex = 0;
                batch->first = i * 3;
                batch->count = 0;
                batch->secondary = BG_TRI_IS_SECONDARY(order[i].tag);
                batch->cullbackfaces = BG_TRI_CULLS_BACK(order[i].tag);
                batch->object = BG_TRI_IS_OBJECT(order[i].tag);

                /* Culling can split one texture into two adjacent
                   batches. Share the existing GL texture instead of
                   decoding and uploading it again. */
                if (i > 0
                    && BG_TEX_ID(order[i].tag) == BG_TEX_ID(order[i - 1].tag))
                {
                    batch->gltex = batches[batchcount - 2].gltex;
                }
                else if (BG_TEX_ID(order[i].tag) != BG_TEX_NONE
                         && projectdir != NULL)
                {
                    int tw = 0;
                    int th = 0;

                    if (TexLoadProjectImage(projectdir,
                                            BG_TEX_ID(order[i].tag),
                                            decode, &tw, &th))
                    {
                        GLuint name = 0;

                        glGenTextures(1, &name);
                        glBindTexture(GL_TEXTURE_2D, name);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0,
                                     GL_RGBA, GL_UNSIGNED_BYTE, decode);

                        batch->gltex = name;
                        textures[texturecount++] = name;
                    }
                }
            }

            {
                SceneBatch *batch = &batches[batchcount - 1];

                batch->count += 3;

                if (batch->gltex != 0)
                {
                    GLint tw = 0;
                    GLint th = 0;

                    glBindTexture(GL_TEXTURE_2D, batch->gltex);
                    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw);
                    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th);

                    if (tw > 0) { invw = 1.0f / (float)tw; }
                    if (th > 0) { invh = 1.0f / (float)th; }
                }
            }

            for (k = 0; k < 3; k++)
            {
                dst[k].x = src[k].x;
                dst[k].y = src[k].y;
                dst[k].z = src[k].z;
                dst[k].s = src[k].s * invw;   /* texels -> normalized */
                dst[k].t = src[k].t * invh;
                dst[k].r = src[k].r;
                dst[k].g = src[k].g;
                dst[k].b = src[k].b;
                dst[k].a = src[k].a;
                scenecolors[i * 3 + k].r = src[k].r;
                scenecolors[i * 3 + k].g = src[k].g;
                scenecolors[i * 3 + k].b = src[k].b;

                if (i == 0 && k == 0)
                {
                    minx = maxx = dst[k].x;
                    miny = maxy = dst[k].y;
                    minz = maxz = dst[k].z;
                }
                else
                {
                    if (dst[k].x < minx) { minx = dst[k].x; }
                    if (dst[k].x > maxx) { maxx = dst[k].x; }
                    if (dst[k].y < miny) { miny = dst[k].y; }
                    if (dst[k].y > maxy) { maxy = dst[k].y; }
                    if (dst[k].z < minz) { minz = dst[k].z; }
                    if (dst[k].z > maxz) { maxz = dst[k].z; }
                }
            }
        }

        free(order);
        free(decode);
    }

    ViewportFreeScene(state);
    state->scene = scene;
    state->scenecolors = scenecolors;
    state->scenecount = scene != NULL ? (GLsizei)(tricount * 3) : 0;
    state->batches = batches;
    state->batchcount = scene != NULL ? batchcount : 0;
    state->selectedtris = selectedtris;
    state->textures = textures;
    state->texturecount = scene != NULL ? texturecount : 0;

    if (scene == NULL)
    {
        free(batches);
        free(textures);
        free(selectedtris);
        free(scenecolors);
        state->batches = NULL;
        state->textures = NULL;
        state->selectedtris = NULL;
        state->scenecolors = NULL;
        state->batchcount = 0;
        state->texturecount = 0;
    }

    if (scene != NULL)
    {
        /* Frame the level: eye at the bbox centre, pulled back along
           +Z by most of the larger horizontal extent. Free-fly from
           there. */
        float dx = maxx - minx;
        float dz = maxz - minz;
        float dim = dx > dz ? dx : dz;

        state->posx = (minx + maxx) * 0.5f;
        state->posy = (miny + maxy) * 0.5f;
        state->posz = (minz + maxz) * 0.5f + dim * 0.75f;
        state->yaw = 0.0f;
        state->pitch = 0.0f;
    }

    InvalidateRect(hwnd, NULL, FALSE);
}


static void ViewportSetMarkerVertex(Vertex *vertex, const float point[3],
                                    unsigned char r, unsigned char g,
                                    unsigned char b)
{
    vertex->x = point[0];
    vertex->y = point[1];
    vertex->z = point[2];
    vertex->r = r;
    vertex->g = g;
    vertex->b = b;
    vertex->a = 255;
    vertex->s = 0.0f;
    vertex->t = 0.0f;
}


/* Appends the twelve edges of an oriented pad box. */
static void ViewportAppendPadBox(Vertex *vertices, int *vertexcount,
                                 const SetupPad *pad,
                                 float xmin, float xmax,
                                 float ymin, float ymax,
                                 float zmin, float zmax,
                                 float worldscale,
                                 unsigned char r, unsigned char g,
                                 unsigned char b)
{
    static const unsigned char edges[12][2] = {
        {0, 1}, {2, 3}, {4, 5}, {6, 7},
        {0, 2}, {1, 3}, {4, 6}, {5, 7},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    float corners[8][3];
    int edge;

    SetupPadGetBoxCorners(pad, xmin, xmax, ymin, ymax, zmin, zmax,
                          worldscale, corners);

    for (edge = 0; edge < 12; edge++)
    {
        ViewportSetMarkerVertex(&vertices[*vertexcount],
            corners[edges[edge][0]], r, g, b);
        (*vertexcount)++;
        ViewportSetMarkerVertex(&vertices[*vertexcount],
            corners[edges[edge][1]], r, g, b);
        (*vertexcount)++;
    }
}


void ViewportSetSetupPads(HWND hwnd, const SetupFile *setup, float levelscale, const unsigned char *occupiedpads, const unsigned char *occupiedboundpads)
{
    ViewportState *state = ViewportGetState(hwnd);
    Vertex *markers = NULL;
    DWORD boxcount = 0;
    int vertexcount = 0;
    float worldscale;
    DWORD i;

    if (state == NULL)
    {
        return;
    }

    free(state->padmarkers);
    state->padmarkers = NULL;
    state->padmarkercount = 0;

    if (setup == NULL || !(levelscale > 0.0f))
    {
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    boxcount = setup->padcount + setup->boundpadcount;
    if (boxcount == 0)
    {
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    markers = (Vertex *)malloc((size_t)boxcount * VIEWPORT_BOX_VERTICES
                              * sizeof(*markers));
    if (markers == NULL)
    {
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    worldscale = 1.0f / levelscale;

    for (i = 0; i < setup->padcount; i++)
    {
        if (occupiedpads != NULL && occupiedpads[i])
        {
            continue;
        }

        ViewportAppendPadBox(markers, &vertexcount, &setup->pads[i],
            -VIEWPORT_PAD_HALF_SIZE, VIEWPORT_PAD_HALF_SIZE,
            -VIEWPORT_PAD_HALF_SIZE, VIEWPORT_PAD_HALF_SIZE,
            -VIEWPORT_PAD_HALF_SIZE, VIEWPORT_PAD_HALF_SIZE,
            worldscale, 32, 255, 64);
    }

    for (i = 0; i < setup->boundpadcount; i++)
    {
        const SetupBoundPad *pad = &setup->boundpads[i];

        if (occupiedboundpads != NULL && occupiedboundpads[i])
        {
            continue;
        }

        ViewportAppendPadBox(markers, &vertexcount, &pad->pad,
            pad->xmin, pad->xmax, pad->ymin, pad->ymax,
            pad->zmin, pad->zmax, worldscale, 255, 48, 48);
    }

    state->padmarkers = markers;
    state->padmarkercount = vertexcount;
    InvalidateRect(hwnd, NULL, FALSE);
}


BOOL ViewportGetBackfaceCulling(HWND hwnd)
{
    ViewportState *state = (ViewportState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    return state != NULL && state->cullbackfaces;
}

void ViewportSetBackfaceCulling(HWND hwnd, BOOL enabled)
{
    ViewportState *state = (ViewportState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    if (state == NULL || state->cullbackfaces == enabled)
    {
        return;
    }

    state->cullbackfaces = enabled;
    InvalidateRect(hwnd, NULL, FALSE);
}
