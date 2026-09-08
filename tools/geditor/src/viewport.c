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
#include "gltf.h"
#include "resource.h"

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
#define VIEWPORT_VERTEX_MARKER_SIZE 5.0f /* screen pixels */
#define VIEWPORT_STAN_FILL_ALPHA 112
#define VIEWPORT_STAN_EDGE_ALPHA 224
#define VIEWPORT_PORTAL_FILL_ALPHA 64
#define VIEWPORT_PORTAL_EDGE_ALPHA 255
#define VIEWPORT_PICK_EPSILON 1.0e-10
#define VIEWPORT_PICK_BARY_EPSILON 1.0e-8
#define VIEWPORT_PICK_COPLANAR_EPSILON 1.0e-3
#define VIEWPORT_PICK_COPLANAR_RELATIVE_EPSILON 1.0e-6
#define VIEWPORT_OBJECT_NONE 0xffffffffu

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

typedef struct ViewportComponent {
    BgDocumentVertexRef refs[2];
    int corners[2];
} ViewportComponent;

/* Per-viewport state, allocated at WM_CREATE, freed at WM_DESTROY,
   reachable from the window via GWLP_USERDATA. */
typedef struct ViewportState {
    HDC hdc;      /* private DC - stable for the window's lifetime (CS_OWNDC) */
    HGLRC hglrc;  /* the GL context rendering into it */
    EditorTool tool;

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
    BgFaceRef *scenefacerefs;    /* stable document identity in the same order */
    DWORD *sceneobjectindices;   /* setup object identity in the same order */
    BgDocumentVertexRef *scenevertexrefs;
    ViewportComponent *components; /* insertion order preserves the vertex anchor */
    int componentcount, componentcapacity;
    int width, height;
    BgVertex *arrow;
    DWORD arrowtris;
    BOOL gizmovisible;
    double gizmoposition[3];
    int hoveraxis, dragaxis;
    double dragorigin[3], dragplane[3], dragparameter, dragdelta, dragscale;
    BOOL dragvertical;
    float (*dragvertices)[3];
    unsigned char *dragmask;
    int selectedtricount;
    DWORD selectedobject;
    Vertex objectselectionbox[VIEWPORT_BOX_VERTICES];
    GLsizei objectselectionboxcount;
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
    BOOL showobjects;
    BOOL cullbackfaces;  /* master toggle for authored backface culling */
    BOOL keyw, keya, keys, keyd, keyq, keye;
    POINT lastmouse;
    LONGLONG lastqpc;   /* QueryPerformanceCounter at the previous frame */
} ViewportState;


static void ViewportDrawTransformTools(const ViewportState *state);
static void ViewportUpdateGizmo(ViewportState *state);
static void ViewportRestoreComponents(ViewportState *state);

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


/* Draw over the shaded scene without changing document/selection colors.
   Polygon point mode gives camera-facing, fixed-pixel-size square markers
   while retaining the same triangle clipping and backface culling as BG. */
static void ViewportDrawBgToolOverlay(const ViewportState *state)
{
    BOOL vertices = state->tool == EDITOR_TOOL_VERTEX_SELECT;
    int batchindex, pass;

    if (state->scene == NULL || state->batchcount <= 0
        || (!vertices && state->tool != EDITOR_TOOL_EDGE_SELECT))
    {
        return;
    }

    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT | GL_POLYGON_BIT
                 | GL_POINT_BIT | GL_LINE_BIT | GL_DEPTH_BUFFER_BIT);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_LINE_SMOOTH);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_VERTEX_ARRAY);
    glColor4ub(255, 255, 255, 255);
    glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &state->scene[0].x);

    /* Keep hidden geometry occluded, with a small bias to lift the overlay
       off its own face. Points need clearance across their square footprint. */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glPointSize(VIEWPORT_VERTEX_MARKER_SIZE);
    glLineWidth(1.0f);

    /* Vertex mode adds its squares over the same non-selectable edge overlay. */
    for (pass = 0; pass < (vertices ? 2 : 1); pass++)
    {
        BOOL points = pass == 1;
        glPolygonMode(GL_FRONT_AND_BACK, points ? GL_POINT : GL_LINE);
        glDisable(GL_POLYGON_OFFSET_POINT);
        glDisable(GL_POLYGON_OFFSET_LINE);
        glEnable(points ? GL_POLYGON_OFFSET_POINT : GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(points ? -VIEWPORT_VERTEX_MARKER_SIZE : -1.0f, -1.0f);

        for (batchindex = 0; batchindex < state->batchcount; batchindex++)
        {
            const SceneBatch *batch = &state->batches[batchindex];

            if (batch->object
                || (batch->secondary ? !state->showbgsecondary : !state->showbgprimary))
            {
                continue;
            }
            if (state->cullbackfaces && batch->cullbackfaces)
            {
                glEnable(GL_CULL_FACE);
            }
            else
            {
                glDisable(GL_CULL_FACE);
            }
            glDrawArrays(GL_TRIANGLES, batch->first, batch->count);
        }
    }

    glPopClientAttrib();
    glPopAttrib();
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

                if (batch->object ? !state->showobjects
                    : (batch->secondary ? !state->showbgsecondary : !state->showbgprimary))
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

    if (state->showobjects && state->objectselectionboxcount > 0)
    {
        /* A selected object keeps its normal shading. The white bounds are
           a separate depth-tested overlay, so selection never mutates model
           colors or leaks into exported geometry. */
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);

        glVertexPointer(3, GL_FLOAT, sizeof(Vertex),
                        &state->objectselectionbox[0].x);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex),
                       &state->objectselectionbox[0].r);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, state->objectselectionboxcount);
        glLineWidth(1.0f);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    ViewportDrawBgToolOverlay(state);
    ViewportDrawTransformTools(state);
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

    state->hoveraxis = -1;
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
        || (state->dragaxis < 0 && (mousex < 0 || mousex >= width
        || mousey < 0 || mousey >= height)))
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


static int ViewportFindPickedTriangle(const ViewportState *state,
                                      const ViewportPickRay *ray,
                                      BOOL addtoselection, BOOL deselect,
                                      double *distanceout)
{
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

    *distanceout = DBL_MAX;

    if (state->scene == NULL || state->selectedtris == NULL)
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

            if (ViewportRayTriangleDistance(ray, &state->scene[vertex],
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

    *distanceout = nearestdistance;
    coplanartolerance = ViewportCoplanarPickTolerance(nearestdistance);

    /* Primary geometry writes the depth buffer before the secondary pass.
       With GL_LESS, an exactly coplanar secondary face is therefore hidden
       by its primary counterpart. Several levels (notably Depot) contain
       many such deliberate duplicates. Keep the visible primary face first
       in the coplanar hit stack; repeated clicks can still cycle through to
       the secondary face. A secondary face which is closer by more than the
       coplanar tolerance remains first because the primary is excluded. */
    for (secondary = 0; secondary <= 1; secondary++)
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

                if (!ViewportRayTriangleDistance(ray, &state->scene[vertex],
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


static DWORD ViewportFindPickedObject(const ViewportState *state,
                                      const ViewportPickRay *ray,
                                      double *distanceout)
{
    double nearestdistance = DBL_MAX;
    DWORD nearestobject = VIEWPORT_OBJECT_NONE;
    int batchindex;

    *distanceout = DBL_MAX;
    if (!state->showobjects || state->scene == NULL || state->sceneobjectindices == NULL)
    {
        return VIEWPORT_OBJECT_NONE;
    }

    for (batchindex = 0; batchindex < state->batchcount; batchindex++)
    {
        const SceneBatch *batch = &state->batches[batchindex];
        BOOL cullbackfaces;
        int vertex;
        int end;

        if (!batch->object)
        {
            continue;
        }

        cullbackfaces = state->cullbackfaces && batch->cullbackfaces;
        end = batch->first + batch->count;

        for (vertex = batch->first; vertex + 2 < end; vertex += 3)
        {
            double distance;
            int triangle = vertex / 3;

            if (state->sceneobjectindices[triangle] != VIEWPORT_OBJECT_NONE
                && ViewportRayTriangleDistance(ray, &state->scene[vertex],
                                               cullbackfaces, &distance)
                && distance < nearestdistance)
            {
                nearestdistance = distance;
                nearestobject = state->sceneobjectindices[triangle];
            }
        }
    }

    *distanceout = nearestdistance;
    return nearestobject;
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


void ViewportRefreshBgVertexColor(HWND viewport, const BgDocument *document,
                                   const ViewportBgVertexHit *hit)
{
    ViewportState *state = ViewportGetState(viewport);
    const BgDocumentRoom *room;
    const BgDocumentFace *paintedface;
    const BgDocumentVertex *source;
    DWORD vertexindex;
    int triangle;

    if (state == NULL || state->scene == NULL || state->scenefacerefs == NULL
        || state->scenecolors == NULL || hit == NULL || hit->corner >= 3)
    {
        return;
    }
    paintedface = BgDocumentFindFace(document, &hit->face, &room);
    if (paintedface == NULL || paintedface->vertexindices[hit->corner] >= room->vertexcount)
    {
        return;
    }
    vertexindex = paintedface->vertexindices[hit->corner];
    source = &room->vertices[vertexindex];
    for (triangle = 0; triangle < state->scenecount / 3; triangle++)
    {
        const BgFaceRef *ref = &state->scenefacerefs[triangle];
        const BgDocumentFace *face;
        unsigned int corner;

        if (ref->faceid == BG_FACE_ID_NONE || ref->room != hit->face.room)
        {
            continue;
        }
        face = BgDocumentFindFace(document, ref, NULL);
        if (face == NULL) { continue; }
        for (corner = 0; corner < 3; corner++)
        {
            int vertex = triangle * 3 + corner;
            if (face->vertexindices[corner] != vertexindex) { continue; }
            state->scenecolors[vertex].r = source->r;
            state->scenecolors[vertex].g = source->g;
            state->scenecolors[vertex].b = source->b;
            state->scene[vertex].a = source->a;
        }
        ViewportSetTriangleColor(state, triangle,
            state->selectedtris != NULL && state->selectedtris[triangle]);
    }
    ViewportRedraw(viewport);
}


static void ViewportBuildObjectSelectionBox(ViewportState *state)
{
    static const unsigned char edges[VIEWPORT_BOX_VERTICES] = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };
    float corners[8][3];
    float min[3] = { 0.0f, 0.0f, 0.0f };
    float max[3] = { 0.0f, 0.0f, 0.0f };
    float padding;
    BOOL found = FALSE;
    int trianglecount = state->scenecount / 3;
    int triangle;
    int axis;
    int vertex;

    state->objectselectionboxcount = 0;
    if (!state->showobjects || state->selectedobject == VIEWPORT_OBJECT_NONE
        || state->scene == NULL || state->sceneobjectindices == NULL)
    {
        return;
    }

    for (triangle = 0; triangle < trianglecount; triangle++)
    {
        if (state->sceneobjectindices[triangle] != state->selectedobject)
        {
            continue;
        }

        for (vertex = triangle * 3; vertex < triangle * 3 + 3; vertex++)
        {
            const Vertex *source = &state->scene[vertex];

            for (axis = 0; axis < 3; axis++)
            {
                float value = (&source->x)[axis];

                if (!found)
                {
                    min[axis] = max[axis] = value;
                }
                else
                {
                    if (value < min[axis]) min[axis] = value;
                    if (value > max[axis]) max[axis] = value;
                }
            }
            found = TRUE;
        }
    }

    if (!found)
    {
        state->selectedobject = VIEWPORT_OBJECT_NONE;
        return;
    }

    padding = max[0] - min[0];
    if (max[1] - min[1] > padding) padding = max[1] - min[1];
    if (max[2] - min[2] > padding) padding = max[2] - min[2];
    padding *= 0.01f;
    if (padding < 0.5f) padding = 0.5f;

    for (axis = 0; axis < 3; axis++)
    {
        min[axis] -= padding;
        max[axis] += padding;
    }

    for (vertex = 0; vertex < 8; vertex++)
    {
        corners[vertex][0] = (vertex == 1 || vertex == 2
                           || vertex == 5 || vertex == 6)
            ? max[0] : min[0];
        corners[vertex][1] = (vertex == 2 || vertex == 3
                           || vertex == 6 || vertex == 7)
            ? max[1] : min[1];
        corners[vertex][2] = vertex >= 4 ? max[2] : min[2];
    }

    for (vertex = 0; vertex < VIEWPORT_BOX_VERTICES; vertex++)
    {
        Vertex *destination = &state->objectselectionbox[vertex];
        const float *source = corners[edges[vertex]];

        destination->x = source[0];
        destination->y = source[1];
        destination->z = source[2];
        destination->r = destination->g = destination->b = 255;
        destination->a = 255;
        destination->s = destination->t = 0.0f;
    }
    state->objectselectionboxcount = VIEWPORT_BOX_VERTICES;
}


static void ViewportSelectObject(ViewportState *state, DWORD objectindex)
{
    state->selectedobject = objectindex;
    ViewportBuildObjectSelectionBox(state);
}


static void ViewportClearObjectSelection(ViewportState *state)
{
    state->selectedobject = VIEWPORT_OBJECT_NONE;
    state->objectselectionboxcount = 0;
}


static void ViewportClearBgSelection(ViewportState *state)
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


static void ViewportClearAllSelection(ViewportState *state)
{
    ViewportClearBgSelection(state);
    ViewportClearObjectSelection(state);
    state->componentcount = 0;
    state->gizmovisible = FALSE;
    state->hoveraxis = -1;
}


/* Painting never cycles through the face-selection hit stack. Resolve the
   closest visible BG face, then the corner nearest its world-space hit point.
   Primary wins exact depth ties, matching the viewport's GL_LESS draw order. */
static BOOL ViewportFindPaintTarget(const ViewportState *state,
                                     const ViewportPickRay *ray,
                                     ViewportBgVertexHit *hit)
{
    double nearestdistance = DBL_MAX;
    double objectdistance;
    double cornerdistance = DBL_MAX;
    double position[3];
    int triangle = -1;
    int secondary;
    int batchindex;
    unsigned int corner;

    if (state->scene == NULL || state->scenefacerefs == NULL)
    {
        return FALSE;
    }
    for (secondary = 0; secondary <= 1; secondary++)
    {
        for (batchindex = 0; batchindex < state->batchcount; batchindex++)
        {
            const SceneBatch *batch = &state->batches[batchindex];
            int vertex;

            if (!ViewportBatchIsPickable(state, batch) || batch->secondary != secondary)
            {
                continue;
            }
            for (vertex = batch->first; vertex + 2 < batch->first + batch->count; vertex += 3)
            {
                double distance;

                if (ViewportRayTriangleDistance(ray, &state->scene[vertex],
                        state->cullbackfaces && batch->cullbackfaces, &distance)
                    && distance < nearestdistance)
                {
                    nearestdistance = distance;
                    triangle = vertex / 3;
                }
            }
        }
    }
    if (triangle < 0 || state->scenefacerefs[triangle].faceid == BG_FACE_ID_NONE)
    {
        return FALSE;
    }
    /* A visible object in front of the wall blocks painting through it. */
    if (ViewportFindPickedObject(state, ray, &objectdistance) != VIEWPORT_OBJECT_NONE
        && objectdistance < nearestdistance)
    {
        return FALSE;
    }
    position[0] = ray->origin[0] + ray->direction[0] * nearestdistance;
    position[1] = ray->origin[1] + ray->direction[1] * nearestdistance;
    position[2] = ray->origin[2] + ray->direction[2] * nearestdistance;
    hit->face = state->scenefacerefs[triangle];
    hit->corner = 0;
    for (corner = 0; corner < 3; corner++)
    {
        const Vertex *vertex = &state->scene[triangle * 3 + corner];
        double x = vertex->x - position[0];
        double y = vertex->y - position[1];
        double z = vertex->z - position[2];
        double distance = x * x + y * y + z * z;

        if (distance < cornerdistance)
        {
            cornerdistance = distance;
            hit->corner = corner;
        }
    }
    return TRUE;
}


static void ViewportPaintAt(HWND hwnd, ViewportState *state, int mousex, int mousey)
{
    ViewportPickRay ray;
    ViewportBgVertexHit hit;

    if (state == NULL || state->flying || state->tool != EDITOR_TOOL_VERTEX_PAINT)
    {
        return;
    }
    if (ViewportBuildPickRay(hwnd, state, mousex, mousey, &ray)
        && ViewportFindPaintTarget(state, &ray, &hit))
    {
        /* The handler can replace all scene arrays; retain no pointers to them. */
        SendMessage(GetParent(hwnd), VIEWPORT_WM_PAINT_VERTEX, 0, (LPARAM)&hit);
    }
}


static void ViewportPickAt(HWND hwnd, ViewportState *state, int mousex,
                           int mousey, BOOL addtoselection, BOOL deselect)
{
    ViewportPickRay ray;
    double bgdistance;
    double objectdistance;
    int triangle;
    DWORD objectindex;

    if (state == NULL || state->selectedtris == NULL
        || state->scenecolors == NULL || state->flying
        || state->tool != EDITOR_TOOL_FACE_SELECT)
    {
        return;
    }

    if (deselect)
    {
        addtoselection = FALSE;
    }

    if (!ViewportBuildPickRay(hwnd, state, mousex, mousey, &ray))
    {
        return;
    }

    triangle = ViewportFindPickedTriangle(state, &ray, addtoselection,
                                          deselect, &bgdistance);
    objectindex = ViewportFindPickedObject(state, &ray, &objectdistance);

    /* Both geometry types share the exact same ray and distance metric.
       This prevents draw order and texture batching from affecting which
       item wins when an object is in front of a background face. */
    if (objectindex != VIEWPORT_OBJECT_NONE
        && (bgdistance == DBL_MAX || objectdistance < bgdistance))
    {
        ViewportClearBgSelection(state);
        if (deselect && state->selectedobject == objectindex)
        {
            ViewportClearObjectSelection(state);
        }
        else
        {
            ViewportSelectObject(state, objectindex);
        }
    }
    else if (bgdistance != DBL_MAX)
    {
        ViewportClearObjectSelection(state);

        if (deselect)
        {
            if (triangle >= 0)
            {
                state->selectedtris[triangle] = 0;
                state->selectedtricount--;
                ViewportSetTriangleColor(state, triangle, FALSE);
            }
        }
        else
        {
            if (!addtoselection)
            {
                ViewportClearBgSelection(state);
            }

            if (triangle >= 0 && !state->selectedtris[triangle])
            {
                state->selectedtris[triangle] = 1;
                state->selectedtricount++;
                ViewportSetTriangleColor(state, triangle, TRUE);
            }
        }
    }
    else
    {
        /* Clicking empty space clears either kind of selection, including
           while Shift or Control is held. */
        ViewportClearAllSelection(state);
    }

    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
}


static int ViewportCompareVertexRefs(const void *left, const void *right)
{
    const BgDocumentVertexRef *a = left, *b = right;
    if (a->room != b->room) { return a->room < b->room ? -1 : 1; }
    return a->index < b->index ? -1 : a->index > b->index;
}

static BOOL ViewportCornerVisible(const ViewportState *state, int corner)
{
    int i;
    if (corner < 0 || corner >= state->scenecount) { return FALSE; }
    for (i = 0; i < state->batchcount; i++)
    {
        const SceneBatch *batch = &state->batches[i];
        if (corner >= batch->first && corner < batch->first + batch->count)
        {
            return !batch->object && (batch->secondary ? state->showbgsecondary : state->showbgprimary);
        }
    }
    return FALSE;
}

static int ViewportFindVertexCorner(const ViewportState *state, const BgDocumentVertexRef *ref)
{
    int i;
    if (ref->room == 0 || state->scenevertexrefs == NULL) { return -1; }
    for (i = 0; i < state->scenecount; i++)
    {
        if (ViewportCompareVertexRefs(ref, &state->scenevertexrefs[i]) == 0) { return i; }
    }
    return -1;
}

/* Returns unique identities, never welded by position. Two corners at a UV
 * seam move together only if they share the document's actual vertex. */
BgDocumentVertexRef *ViewportGetMoveVertices(HWND hwnd, DWORD *countout)
{
    const ViewportState *state = ViewportGetState(hwnd);
    BgDocumentVertexRef *refs;
    DWORD count = 0, unique = 0, i;
    *countout = 0;
    if (state == NULL || state->scenevertexrefs == NULL || state->scenecount == 0
        || state->selectedobject != VIEWPORT_OBJECT_NONE) { return NULL; }
    refs = malloc(((size_t)state->scenecount + (size_t)state->componentcount * 2) * sizeof(*refs));
    if (refs == NULL) { return NULL; }
    if (state->tool == EDITOR_TOOL_FACE_SELECT)
    {
        for (i = 0; i < (DWORD)state->scenecount; i++)
        {
            if (state->selectedtris[i / 3] && ViewportCornerVisible(state, i)
                && state->scenevertexrefs[i].room != 0) { refs[count++] = state->scenevertexrefs[i]; }
        }
    }
    else if (state->tool == EDITOR_TOOL_VERTEX_SELECT || state->tool == EDITOR_TOOL_EDGE_SELECT)
    {
        for (i = 0; i < (DWORD)state->componentcount; i++)
        {
            const ViewportComponent *component = &state->components[i];
            int end, ends = state->tool == EDITOR_TOOL_EDGE_SELECT ? 2 : 1;
            for (end = 0; end < ends; end++)
            {
                if (ViewportCornerVisible(state, component->corners[end]))
                {
                    refs[count++] = component->refs[end];
                }
            }
        }
    }
    qsort(refs, count, sizeof(*refs), ViewportCompareVertexRefs);
    for (i = 0; i < count; i++)
    {
        if (unique == 0 || ViewportCompareVertexRefs(&refs[i], &refs[unique - 1]) != 0)
        {
            refs[unique++] = refs[i];
        }
    }
    if (unique == 0) { free(refs); return NULL; }
    *countout = unique;
    return refs;
}

int ViewportGetSelectedComponentCount(HWND hwnd)
{
    const ViewportState *state = ViewportGetState(hwnd);
    return state != NULL ? state->componentcount : 0;
}

/* Panel coordinates average the selected items equally. This is deliberately
 * separate from the gizmo's first-vertex anchor and length/area-weighted pivot. */
BOOL ViewportGetSelectionPosition(HWND hwnd, double position[3], DWORD *countout)
{
    const ViewportState *state = ViewportGetState(hwnd);
    double sum[3] = {0, 0, 0}, weight = 0;
    DWORD count = 0;
    BOOL object;
    int i, axis;

    *countout = 0;
    if (state == NULL || state->scene == NULL || state->tool == EDITOR_TOOL_VERTEX_PAINT)
    {
        return FALSE;
    }
    object = state->selectedobject != VIEWPORT_OBJECT_NONE;
    if (object && (!state->showobjects || state->sceneobjectindices == NULL)) { return FALSE; }
    if (!object && (state->tool == EDITOR_TOOL_VERTEX_SELECT || state->tool == EDITOR_TOOL_EDGE_SELECT))
    {
        for (i = 0; i < state->componentcount; i++)
        {
            const ViewportComponent *component = &state->components[i];
            int ends = state->tool == EDITOR_TOOL_EDGE_SELECT ? 2 : 1;
            int end;
            if (!ViewportCornerVisible(state, component->corners[0])
                || (ends == 2 && !ViewportCornerVisible(state, component->corners[1]))) { continue; }
            for (end = 0; end < ends; end++)
            {
                const Vertex *v = &state->scene[component->corners[end]];
                sum[0] += (double)v->x / ends;
                sum[1] += (double)v->y / ends;
                sum[2] += (double)v->z / ends;
            }
            count++;
        }
        weight = count;
    }
    else
    {
        for (i = 0; i < state->scenecount; i += 3)
        {
            const Vertex *v = &state->scene[i];
            double triangleweight = 1;
            if (object)
            {
                double a[3], b[3], cross[3];
                if (state->sceneobjectindices[i / 3] != state->selectedobject) { continue; }
                /* A model's geometric center matches its surface-centroid gizmo. */
                a[0]=(double)v[1].x-v[0].x; a[1]=(double)v[1].y-v[0].y; a[2]=(double)v[1].z-v[0].z;
                b[0]=(double)v[2].x-v[0].x; b[1]=(double)v[2].y-v[0].y; b[2]=(double)v[2].z-v[0].z;
                cross[0]=a[1]*b[2]-a[2]*b[1]; cross[1]=a[2]*b[0]-a[0]*b[2]; cross[2]=a[0]*b[1]-a[1]*b[0];
                triangleweight = sqrt(cross[0]*cross[0]+cross[1]*cross[1]+cross[2]*cross[2]);
            }
            else if (state->selectedtris == NULL || !state->selectedtris[i / 3]
                || !ViewportCornerVisible(state, i)) { continue; }
            sum[0] += ((double)v[0].x + v[1].x + v[2].x) * triangleweight / 3;
            sum[1] += ((double)v[0].y + v[1].y + v[2].y) * triangleweight / 3;
            sum[2] += ((double)v[0].z + v[1].z + v[2].z) * triangleweight / 3;
            weight += triangleweight;
            count++;
        }
    }
    if (!(weight > 0)) { return FALSE; }
    for (axis = 0; axis < 3; axis++) { position[axis] = sum[axis] / weight; }
    *countout = object ? 1 : count;
    return TRUE;
}

static void ViewportUpdateGizmo(ViewportState *state)
{
    double sum[3] = {0,0,0}, weight = 0;
    int i, axis;
    state->gizmovisible = FALSE;
    state->hoveraxis = -1;
    if (state->scene == NULL || state->tool == EDITOR_TOOL_VERTEX_PAINT) { return; }
    if (state->selectedobject != VIEWPORT_OBJECT_NONE
        && (!state->showobjects || (state->selectedobject & SETUP_CHARACTER_SELECTION_BIT))) { return; }
    if (state->tool == EDITOR_TOOL_VERTEX_SELECT && state->componentcount > 0)
    {
        int corner = state->components[0].corners[0];
        if (!ViewportCornerVisible(state, corner)) { return; }
        state->gizmoposition[0] = state->scene[corner].x;
        state->gizmoposition[1] = state->scene[corner].y;
        state->gizmoposition[2] = state->scene[corner].z;
        state->gizmovisible = TRUE;
        return;
    }
    if (state->tool == EDITOR_TOOL_EDGE_SELECT)
    {
        for (i = 0; i < state->componentcount; i++)
        {
            const ViewportComponent *c = &state->components[i];
            const Vertex *a, *b;
            double length;
            if (!ViewportCornerVisible(state, c->corners[0])
                || !ViewportCornerVisible(state, c->corners[1])) { continue; }
            a = &state->scene[c->corners[0]]; b = &state->scene[c->corners[1]];
            length = sqrt((a->x-b->x)*(double)(a->x-b->x)
                + (a->y-b->y)*(double)(a->y-b->y) + (a->z-b->z)*(double)(a->z-b->z));
            sum[0] += (a->x+b->x)*0.5*length;
            sum[1] += (a->y+b->y)*0.5*length;
            sum[2] += (a->z+b->z)*0.5*length;
            weight += length;
        }
    }
    else
    {
        /* Surface centroid: independent of triangle density or UV seams. */
        for (i = 0; i < state->scenecount; i += 3)
        {
            const Vertex *v = &state->scene[i];
            double a[3], b[3], cross[3], area;
            if (state->selectedobject != VIEWPORT_OBJECT_NONE)
            {
                if (state->sceneobjectindices[i/3] != state->selectedobject) { continue; }
            }
            else if (!state->selectedtris[i/3] || !ViewportCornerVisible(state, i)) { continue; }
            a[0]=v[1].x-v[0].x; a[1]=v[1].y-v[0].y; a[2]=v[1].z-v[0].z;
            b[0]=v[2].x-v[0].x; b[1]=v[2].y-v[0].y; b[2]=v[2].z-v[0].z;
            cross[0]=a[1]*b[2]-a[2]*b[1]; cross[1]=a[2]*b[0]-a[0]*b[2]; cross[2]=a[0]*b[1]-a[1]*b[0];
            area = sqrt(cross[0]*cross[0]+cross[1]*cross[1]+cross[2]*cross[2]);
            sum[0] += ((double)v[0].x+v[1].x+v[2].x)*area/3;
            sum[1] += ((double)v[0].y+v[1].y+v[2].y)*area/3;
            sum[2] += ((double)v[0].z+v[1].z+v[2].z)*area/3;
            weight += area;
        }
    }
    if (weight > 0)
    {
        for (axis = 0; axis < 3; axis++) { state->gizmoposition[axis] = sum[axis] / weight; }
        state->gizmovisible = TRUE;
    }
}

static void ViewportRestoreComponents(ViewportState *state)
{
    int i, kept = 0;
    for (i = 0; i < state->componentcount; i++)
    {
        ViewportComponent c = state->components[i];
        c.corners[0] = ViewportFindVertexCorner(state, &c.refs[0]);
        c.corners[1] = state->tool == EDITOR_TOOL_EDGE_SELECT
            ? ViewportFindVertexCorner(state, &c.refs[1]) : c.corners[0];
        if (c.corners[0] >= 0 && c.corners[1] >= 0) { state->components[kept++] = c; }
    }
    state->componentcount = kept;
}

static BOOL ViewportProject(const ViewportState *state, const Vertex *point, double screen[2])
{
    float f[3], r[3];
    double p[3] = {point->x-state->posx, point->y-state->posy, point->z-state->posz};
    double up[3], depth, focal;
    if (state->height <= 0 || state->width <= 0) { return FALSE; }
    ViewportGetBasis(state, f, r);
    up[0]=r[1]*f[2]-r[2]*f[1]; up[1]=r[2]*f[0]-r[0]*f[2]; up[2]=r[0]*f[1]-r[1]*f[0];
    depth=p[0]*f[0]+p[1]*f[1]+p[2]*f[2];
    if (depth < VIEWPORT_NEAR_Z || depth > VIEWPORT_FAR_Z) { return FALSE; }
    focal=state->height/(2.0*tan(VIEWPORT_FOV_Y*0.5*VIEWPORT_DEG_TO_RAD));
    screen[0]=state->width*0.5+focal*(p[0]*r[0]+p[1]*r[1]+p[2]*r[2])/depth;
    screen[1]=state->height*0.5-focal*(p[0]*up[0]+p[1]*up[1]+p[2]*up[2])/depth;
    return TRUE;
}

static BOOL ViewportComponentVisible(const ViewportState *state, int triangle,
                                      const Vertex *point, BOOL cull)
{
    ViewportPickRay ray;
    double length, distance, tolerance;
    int batchindex;
    ray.origin[0]=state->posx; ray.origin[1]=state->posy; ray.origin[2]=state->posz;
    ray.direction[0]=point->x-state->posx;
    ray.direction[1]=point->y-state->posy;
    ray.direction[2]=point->z-state->posz;
    length=sqrt(ray.direction[0]*ray.direction[0]+ray.direction[1]*ray.direction[1]+ray.direction[2]*ray.direction[2]);
    if (!(length>0)) { return FALSE; }
    ray.direction[0]/=length; ray.direction[1]/=length; ray.direction[2]/=length;
    ray.mindistance=0; ray.maxdistance=DBL_MAX;
    if (!ViewportRayTriangleDistance(&ray,&state->scene[triangle*3],cull,&distance)) { return FALSE; }
    tolerance=ViewportCoplanarPickTolerance(length);
    for (batchindex=0; batchindex<state->batchcount; batchindex++)
    {
        const SceneBatch *batch=&state->batches[batchindex];
        int corner;
        if (!ViewportBatchIsPickable(state,batch) && !(batch->object && state->showobjects)) { continue; }
        for (corner=batch->first; corner<batch->first+batch->count; corner+=3)
        {
            if (ViewportRayTriangleDistance(&ray,&state->scene[corner],
                    state->cullbackfaces && batch->cullbackfaces,&distance)
                && distance < length-tolerance) { return FALSE; }
        }
    }
    return TRUE;
}

static void ViewportPickComponent(HWND hwnd, ViewportState *state,
                                    int x, int y, BOOL add, BOOL remove)
{
    ViewportPickRay ray;
    double nearest = DBL_MAX, objectdistance, best = 100.0;
    int i, triangle = -1, chosen = -1, endcount, found = -1;
    ViewportComponent component;
    if (!ViewportBuildPickRay(hwnd,state,x,y,&ray)) { return; }
    /* Only offer components of the nearest visible face. The 10px target
       radius also allows choosing the vertex square or an edge itself. */
    for (i = 0; i < state->batchcount; i++)
    {
        const SceneBatch *batch = &state->batches[i];
        int corner;
        if (!ViewportBatchIsPickable(state,batch)) { continue; }
        for (corner=batch->first; corner<batch->first+batch->count; corner+=3)
        {
            double distance;
            if (ViewportRayTriangleDistance(&ray,&state->scene[corner],
                    state->cullbackfaces && batch->cullbackfaces,&distance) && distance < nearest)
            {
                nearest=distance; triangle=corner/3;
            }
        }
    }
    if (ViewportFindPickedObject(state,&ray,&objectdistance) != VIEWPORT_OBJECT_NONE
        && objectdistance < nearest) { triangle = -1; }
    /* Search the full marker footprint, including pixels just outside the
       triangle silhouette. Visibility is checked at the candidate component,
       so a nearby hidden/back-facing vertex cannot steal the click. */
    for (i=0; i<state->batchcount; i++)
    {
        const SceneBatch *batch=&state->batches[i];
        int first;
        if (!ViewportBatchIsPickable(state,batch)) { continue; }
        for (first=batch->first; first<batch->first+batch->count; first+=3)
        {
            int corner;
            for (corner=0; corner<3; corner++)
            {
                const Vertex *v=&state->scene[first+corner];
                Vertex point=*v;
                double a[2], b[2], dx, dy, distance;
                if (!ViewportProject(state,v,a)) { continue; }
                if (state->tool==EDITOR_TOOL_EDGE_SELECT)
                {
                    const Vertex *w=&state->scene[first+(corner+1)%3];
                    float forward[3], right[3];
                    double length, t, deptha, depthb, worldt;
                    if (!ViewportProject(state,w,b)) { continue; }
                    dx=b[0]-a[0]; dy=b[1]-a[1]; length=dx*dx+dy*dy;
                    t=length>0 ? ((x-a[0])*dx+(y-a[1])*dy)/length : 0;
                    if (t<0) t=0;
                    if (t>1) t=1;
                    a[0]+=t*dx; a[1]+=t*dy;
                    ViewportGetBasis(state,forward,right);
                    deptha=(v->x-state->posx)*forward[0]+(v->y-state->posy)*forward[1]+(v->z-state->posz)*forward[2];
                    depthb=(w->x-state->posx)*forward[0]+(w->y-state->posy)*forward[1]+(w->z-state->posz)*forward[2];
                    worldt=t*deptha/(t*deptha+(1-t)*depthb);
                    point.x=v->x+(w->x-v->x)*worldt;
                    point.y=v->y+(w->y-v->y)*worldt;
                    point.z=v->z+(w->z-v->z)*worldt;
                }
                dx=x-a[0]; dy=y-a[1]; distance=dx*dx+dy*dy;
                if (distance<best && state->scenevertexrefs[first+corner].room!=0
                    && ViewportComponentVisible(state,first/3,&point,state->cullbackfaces && batch->cullbackfaces))
                {
                    best=distance; chosen=corner; triangle=first/3;
                }
            }
        }
    }
    if (chosen < 0)
    {
        if (!add && !remove) { ViewportClearAllSelection(state); }
        /* A true void click always clears selection. */
        if (triangle < 0) { ViewportClearAllSelection(state); }
    }
    else
    {
        endcount=state->tool == EDITOR_TOOL_EDGE_SELECT ? 2 : 1;
        ZeroMemory(&component,sizeof(component));
        for (i=0; i<endcount; i++)
        {
            component.corners[i]=triangle*3+(chosen+i)%3;
            component.refs[i]=state->scenevertexrefs[component.corners[i]];
        }
        if (endcount == 2 && ViewportCompareVertexRefs(&component.refs[0],&component.refs[1])>0)
        {
            BgDocumentVertexRef ref=component.refs[0]; int corner=component.corners[0];
            component.refs[0]=component.refs[1]; component.refs[1]=ref;
            component.corners[0]=component.corners[1]; component.corners[1]=corner;
        }
        for (i=0; i<state->componentcount; i++)
        {
            if (ViewportCompareVertexRefs(&component.refs[0],&state->components[i].refs[0])==0
                && (endcount==1 || ViewportCompareVertexRefs(&component.refs[1],&state->components[i].refs[1])==0)) { found=i; break; }
        }
        if (remove)
        {
            if (found >= 0)
            {
                memmove(state->components+found,state->components+found+1,
                    (size_t)(state->componentcount-found-1)*sizeof(component));
                state->componentcount--;
            }
        }
        else
        {
            if (!add) { ViewportClearAllSelection(state); found=-1; }
            if (found < 0)
            {
                if (state->componentcount == state->componentcapacity)
                {
                    int capacity=state->componentcapacity ? state->componentcapacity*2 : 32;
                    ViewportComponent *grown=realloc(state->components,(size_t)capacity*sizeof(component));
                    if (grown == NULL) { return; }
                    state->components=grown; state->componentcapacity=capacity;
                }
                state->components[state->componentcount++]=component;
            }
        }
    }
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd,NULL,FALSE);
    SendMessage(GetParent(hwnd),VIEWPORT_WM_SELECTION_CHANGED,0,0);
}

static void ViewportLoadGizmo(ViewportState *state)
{
    HINSTANCE instance = GetModuleHandle(NULL);
    HRSRC resource = FindResource(instance,MAKEINTRESOURCE(IDR_GIZMO_ARROW),RT_RCDATA);
    HGLOBAL loaded = resource != NULL ? LoadResource(instance,resource) : NULL;
    const char *reason;
    DWORD i;
    float length=0;
    if (loaded == NULL) { return; }
    state->arrow=GltfLoadGlbMesh(LockResource(loaded),SizeofResource(instance,resource),
                                &state->arrowtris,&reason);
    if (state->arrow == NULL) { return; }
    for (i=0; i<state->arrowtris*3; i++)
    {
        if (state->arrow[i].x>length) { length=state->arrow[i].x; }
    }
    if (!(length > 0)) { free(state->arrow); state->arrow=NULL; state->arrowtris=0; return; }
    /* Normalize length while preserving the artist's gap at the base. */
    for (i=0; i<state->arrowtris*3; i++)
    {
        state->arrow[i].x/=length; state->arrow[i].y/=length; state->arrow[i].z/=length;
    }
}

static double ViewportGizmoScale(const ViewportState *state)
{
    float forward[3], right[3];
    double depth;
    /* Keep the selection's handles visible and sized to the current camera,
       including while flying. Only picking is disabled during navigation. */
    if (!state->gizmovisible || state->arrow == NULL || state->height <= 0) { return 0; }
    ViewportGetBasis(state,forward,right);
    depth=(state->gizmoposition[0]-state->posx)*forward[0]
        +(state->gizmoposition[1]-state->posy)*forward[1]
        +(state->gizmoposition[2]-state->posz)*forward[2];
    if (depth < VIEWPORT_NEAR_Z*2 || depth > VIEWPORT_FAR_Z*0.95) { return 0; }
    return 90.0*depth*2.0*tan(VIEWPORT_FOV_Y*0.5*VIEWPORT_DEG_TO_RAD)/state->height;
}

static void ViewportArrowVertex(const ViewportState *state, int axis,
                                DWORD index, double scale, Vertex *out)
{
    const BgVertex *source=&state->arrow[index];
    double p[3] = {source->x,source->y,source->z};
    if (axis==1) { p[0]=-source->y; p[1]=source->x; }
    if (axis==2) { p[0]=-source->z; p[2]=source->x; }
    out->x=(float)(state->gizmoposition[0]+p[0]*scale);
    out->y=(float)(state->gizmoposition[1]+p[1]*scale);
    out->z=(float)(state->gizmoposition[2]+p[2]*scale);
}

static void ViewportDrawTransformTools(const ViewportState *state)
{
    double scale=ViewportGizmoScale(state);
    int i, axis;
    glPushAttrib(GL_CURRENT_BIT|GL_ENABLE_BIT|GL_DEPTH_BUFFER_BIT|GL_LINE_BIT|GL_POINT_BIT|GL_POLYGON_BIT);
    glDisable(GL_TEXTURE_2D); glDisable(GL_ALPHA_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glDepthMask(GL_FALSE); glDepthFunc(GL_LEQUAL);
    glColor3ub(255,210,0); glPointSize(8); glLineWidth(3);
    glDepthRange(0.0, 0.99999);
    if (state->tool == EDITOR_TOOL_VERTEX_SELECT || state->tool == EDITOR_TOOL_EDGE_SELECT)
    {
        glBegin(state->tool == EDITOR_TOOL_VERTEX_SELECT ? GL_POINTS : GL_LINES);
        for (i=0; i<state->componentcount; i++)
        {
            const ViewportComponent *component=&state->components[i];
            int end, count=state->tool == EDITOR_TOOL_VERTEX_SELECT ? 1 : 2;
            for (end=0; end<count; end++)
            {
                if (ViewportCornerVisible(state,component->corners[end]))
                {
                    const Vertex *v=&state->scene[component->corners[end]];
                    glVertex3f(v->x,v->y,v->z);
                }
            }
        }
        glEnd();
    }
    /* Handles remain visible and clickable over the selection. Depth is
       local to the three arrows; scene depth must not hide a handle. */
    glDepthRange(0.0, 1.0);
    if (scale > 0)
    {
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
        for (axis=0; axis<3; axis++)
        {
            DWORD vertex;
            if (axis==state->dragaxis || axis==state->hoveraxis) { glColor3ub(255,205,0); }
            else if (axis==0) { glColor3ub(240,40,40); }
            else if (axis==1) { glColor3ub(40,220,60); }
            else { glColor3ub(40,100,255); }
            glBegin(GL_TRIANGLES);
            for (vertex=0; vertex<state->arrowtris*3; vertex++)
            {
                Vertex v;
                ViewportArrowVertex(state,axis,vertex,scale,&v);
                glVertex3f(v.x,v.y,v.z);
            }
            glEnd();
        }
    }
    glPopAttrib();
}

static int ViewportPickGizmo(HWND hwnd, const ViewportState *state, int x, int y)
{
    ViewportPickRay ray;
    double scale=ViewportGizmoScale(state), nearest=DBL_MAX;
    int axis, picked=-1;
    if (state->flying || !(scale>0) || !ViewportBuildPickRay(hwnd,state,x,y,&ray)) { return -1; }
    for (axis=0; axis<3; axis++)
    {
        DWORD tri;
        for (tri=0; tri<state->arrowtris; tri++)
        {
            Vertex v[3]; double distance; int corner;
            for (corner=0; corner<3; corner++) { ViewportArrowVertex(state,axis,tri*3+corner,scale,&v[corner]); }
            if (ViewportRayTriangleDistance(&ray,v,FALSE,&distance) && distance<nearest)
            {
                nearest=distance; picked=axis;
            }
        }
    }
    return picked;
}

static double ViewportDragParameter(const ViewportState *state, const ViewportPickRay *ray, int mousey)
{
    double denominator=0, numerator=0;
    int i;
    if (state->dragvertical) { return -mousey*state->dragscale/90.0; }
    for (i=0; i<3; i++)
    {
        denominator+=ray->direction[i]*state->dragplane[i];
        numerator+=(state->dragorigin[i]-ray->origin[i])*state->dragplane[i];
    }
    if (fabs(denominator)<1e-6 || numerator/denominator<0) { return state->dragparameter; }
    return ray->origin[state->dragaxis]+ray->direction[state->dragaxis]*numerator/denominator;
}

static BOOL ViewportBeginTransform(HWND hwnd, ViewportState *state, int x, int y)
{
    ViewportPickRay ray;
    BgDocumentVertexRef *refs=NULL;
    DWORD refcount=0;
    int axis=ViewportPickGizmo(hwnd,state,x,y), i;
    double length=0;
    if (axis<0) { return FALSE; }
    state->dragvertices=malloc((size_t)state->scenecount*sizeof(*state->dragvertices));
    state->dragmask=calloc((size_t)state->scenecount,1);
    if (state->dragvertices==NULL || state->dragmask==NULL)
    {
        free(state->dragvertices); free(state->dragmask);
        state->dragvertices=NULL; state->dragmask=NULL;
        return TRUE;
    }
    if (state->selectedobject==VIEWPORT_OBJECT_NONE)
    {
        refs=ViewportGetMoveVertices(hwnd,&refcount);
        if (refs==NULL) { free(state->dragvertices); free(state->dragmask); state->dragvertices=NULL; state->dragmask=NULL; return TRUE; }
    }
    for (i=0; i<state->scenecount; i++)
    {
        state->dragvertices[i][0]=state->scene[i].x;
        state->dragvertices[i][1]=state->scene[i].y;
        state->dragvertices[i][2]=state->scene[i].z;
        state->dragmask[i]=state->selectedobject!=VIEWPORT_OBJECT_NONE
            ? state->sceneobjectindices[i/3]==state->selectedobject
            : bsearch(&state->scenevertexrefs[i],refs,refcount,sizeof(*refs),ViewportCompareVertexRefs)!=NULL;
    }
    free(refs);
    ViewportBuildPickRay(hwnd,state,x,y,&ray);
    state->dragaxis=axis; state->hoveraxis=axis; state->dragdelta=0;
    state->dragscale=ViewportGizmoScale(state);
    for (i=0; i<3; i++)
    {
        state->dragorigin[i]=state->gizmoposition[i];
        state->dragplane[i]=i==axis ? 0 : ray.direction[i];
        length+=state->dragplane[i]*state->dragplane[i];
    }
    state->dragvertical=length<0.0025;
    state->dragparameter=ViewportDragParameter(state,&ray,y);
    SetCapture(hwnd);
    InvalidateRect(hwnd,NULL,FALSE);
    return TRUE;
}

static void ViewportDragTransform(HWND hwnd, ViewportState *state, int x, int y)
{
    ViewportPickRay ray;
    double delta;
    int i;
    if (!ViewportBuildPickRay(hwnd,state,x,y,&ray)) { return; }
    /* Snap the displacement, preserving the selection's relative shape and
       any authored fractional origin. Calculate from press, never last tick. */
    delta=round(ViewportDragParameter(state,&ray,y)-state->dragparameter);
    if (!isfinite(delta) || fabs(delta)>1000000 || delta==state->dragdelta) { return; }
    state->dragdelta=delta;
    for (i=0; i<state->scenecount; i++)
    {
        if (!state->dragmask[i]) { continue; }
        state->scene[i].x=state->dragvertices[i][0]+(state->dragaxis==0 ? delta : 0);
        state->scene[i].y=state->dragvertices[i][1]+(state->dragaxis==1 ? delta : 0);
        state->scene[i].z=state->dragvertices[i][2]+(state->dragaxis==2 ? delta : 0);
    }
    state->gizmoposition[state->dragaxis]=state->dragorigin[state->dragaxis]+delta;
    ViewportBuildObjectSelectionBox(state);
    InvalidateRect(hwnd,NULL,FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_TRANSFORM_PREVIEW, 0, 0);
}

void ViewportCancelTransform(HWND hwnd)
{
    ViewportState *state=ViewportGetState(hwnd);
    int i;
    if (state==NULL || state->dragaxis<0) { return; }
    for (i=0; i<state->scenecount; i++)
    {
        if (!state->dragmask[i]) { continue; }
        state->scene[i].x=state->dragvertices[i][0];
        state->scene[i].y=state->dragvertices[i][1];
        state->scene[i].z=state->dragvertices[i][2];
    }
    state->dragaxis=-1;
    free(state->dragvertices); free(state->dragmask);
    state->dragvertices=NULL; state->dragmask=NULL;
    if (GetCapture()==hwnd) { ReleaseCapture(); }
    ViewportBuildObjectSelectionBox(state);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd,NULL,FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_TRANSFORM_PREVIEW, 0, 0);
}

static void ViewportEndTransform(HWND hwnd, ViewportState *state)
{
    ViewportTranslation request;
    if (state==NULL || state->dragaxis<0) { return; }
    ZeroMemory(&request,sizeof(request));
    request.offset[state->dragaxis]=state->dragdelta;
    ViewportCancelTransform(hwnd);
    if (request.offset[0]!=0 || request.offset[1]!=0 || request.offset[2]!=0)
    {
        SendMessage(GetParent(hwnd),VIEWPORT_WM_TRANSLATE_SELECTION,0,(LPARAM)&request);
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
        state->showbgprimary = TRUE;
        state->showbgsecondary = TRUE;
        state->showstan = FALSE;
        state->showportals = FALSE;
        state->showobjects = TRUE;
        state->cullbackfaces = TRUE;
        state->selectedobject = VIEWPORT_OBJECT_NONE;
        state->hoveraxis = state->dragaxis = -1;
        ViewportLoadGizmo(state);
        state->tool = EDITOR_TOOL_FACE_SELECT;

        if (!ViewportInitGL(hwnd, state))
        {
            MessageBox(hwnd, "Could not create an OpenGL context.", "GEditor", MB_ICONERROR);
            return -1;
        }
        return 0;

    case WM_SIZE:
        if (state != NULL)
        {
            ViewportCancelTransform(hwnd);
            state->width = LOWORD(lparam); state->height = HIWORD(lparam);
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

    case WM_RBUTTONDOWN: ViewportCancelTransform(hwnd); ViewportBeginFly(hwnd, state);
        return 0;

    case WM_RBUTTONUP: ViewportEndFly(hwnd, state);
        return 0;

    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        if (state != NULL && !state->flying
            && ViewportBeginTransform(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)))
        {
            return 0;
        }
        if (state != NULL && !state->flying
            && (state->tool == EDITOR_TOOL_VERTEX_SELECT || state->tool == EDITOR_TOOL_EDGE_SELECT))
        {
            ViewportPickComponent(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam),
                (wparam & MK_SHIFT)!=0,(wparam & MK_CONTROL)!=0);
        }
        else if (state != NULL && state->tool == EDITOR_TOOL_VERTEX_PAINT)
        {
            ViewportPaintAt(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        }
        else
        {
            ViewportPickAt(hwnd, state, GET_X_LPARAM(lparam),
                           GET_Y_LPARAM(lparam), (wparam & MK_SHIFT) != 0,
                           (wparam & MK_CONTROL) != 0);
        }
        return 0;

    case WM_LBUTTONUP:
        if (state != NULL && state->dragaxis >= 0)
        {
            ViewportDragTransform(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
            ViewportEndTransform(hwnd,state);
            state->hoveraxis=ViewportPickGizmo(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
            InvalidateRect(hwnd,NULL,FALSE);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (state != NULL && state->dragaxis >= 0)
        {
            ViewportDragTransform(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
        }
        else if (state != NULL && !state->flying)
        {
            int axis=ViewportPickGizmo(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
            TRACKMOUSEEVENT tracking={sizeof(tracking),TME_LEAVE,hwnd,0};
            TrackMouseEvent(&tracking);
            if (axis != state->hoveraxis) { state->hoveraxis=axis; InvalidateRect(hwnd,NULL,FALSE); }
        }
        else { ViewportFlyLook(hwnd, state); }
        return 0;

    case WM_MOUSELEAVE:
        if (state != NULL && state->dragaxis < 0) { state->hoveraxis=-1; InvalidateRect(hwnd,NULL,FALSE); }
        return 0;

    case WM_KEYDOWN:
        if (state != NULL && state->dragaxis >= 0)
        {
            if (wparam == VK_ESCAPE) { ViewportCancelTransform(hwnd); }
            return 0;
        }
        if (wparam == VK_DELETE && state != NULL
            && state->tool == EDITOR_TOOL_FACE_SELECT)
        {
            SendMessage(GetParent(hwnd), VIEWPORT_WM_DELETE_SELECTION, 0, 0);
        }
        else if (wparam == VK_ESCAPE && state != NULL)
        {
            ViewportClearAllSelection(state);
            InvalidateRect(hwnd, NULL, FALSE);
            SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
        }
        else
        {
            ViewportSetKey(state, wparam, lparam, 1);
        }
        return 0;

    case WM_KEYUP: ViewportSetKey(state, wparam, lparam, 0);
        return 0;

    case WM_CAPTURECHANGED:
        ViewportCancelTransform(hwnd);
        /**
         * If capture is taken away, act as if right mouse was released so the camera doesn't keep flying with a hidden cursor.
         */
        ViewportEndFly(hwnd, state);
        return 0;

    case WM_KILLFOCUS:
        ViewportCancelTransform(hwnd);
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
        ViewportCancelTransform(hwnd);
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
            free(state->arrow);
            free(state->components);
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


EditorTool ViewportGetTool(HWND viewport)
{
    ViewportState *state = ViewportGetState(viewport);

    return state != NULL ? state->tool : EDITOR_TOOL_FACE_SELECT;
}


void ViewportSetTool(HWND viewport, EditorTool tool)
{
    ViewportState *state = ViewportGetState(viewport);

    if (state == NULL || tool < 0 || tool >= EDITOR_TOOL_COUNT || tool == state->tool)
    {
        return;
    }
    state->tool = tool;
    /* Vertex/edge/paint tools must not inherit a face or object
       selection that Delete or Transform could inadvertently edit. */
    ViewportCancelTransform(viewport);
    ViewportClearAllSelection(state);
    ViewportRedraw(viewport);
    SendMessage(GetParent(viewport), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
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
    free(state->scenefacerefs);
    free(state->sceneobjectindices);
    free(state->scenevertexrefs);
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
    state->scenefacerefs = NULL;
    state->sceneobjectindices = NULL;
    state->scenevertexrefs = NULL;
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
    state->selectedobject = VIEWPORT_OBJECT_NONE;
    state->objectselectionboxcount = 0;
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
                                   BOOL portals, BOOL objects)
{
    ViewportState *state = ViewportGetState(hwnd);

    if (state == NULL)
    {
        return;
    }

    ViewportCancelTransform(hwnd);
    state->showbgprimary = bgprimary;
    state->showbgsecondary = bgsecondary;
    state->showstan = stan;
    state->showportals = portals;
    state->showobjects = objects;
    if (!objects) { state->selectedobject = VIEWPORT_OBJECT_NONE; }
    ViewportBuildObjectSelectionBox(state);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
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

/* Stable IDs let a geometry rebuild keep surviving selected faces even
   when their texture-sorted display order changes. */
static int ViewportCompareFaceRefs(const void *left, const void *right)
{
    const BgFaceRef *a = (const BgFaceRef *)left;
    const BgFaceRef *b = (const BgFaceRef *)right;

    if (a->room != b->room) { return a->room < b->room ? -1 : 1; }
    if (a->layer != b->layer) { return a->layer < b->layer ? -1 : 1; }
    if (a->faceid != b->faceid) { return a->faceid < b->faceid ? -1 : 1; }
    return 0;
}

BOOL ViewportSetScene(HWND hwnd, const BgVertex *tris,
                      const unsigned short *tritags,
                      const BgFaceRef *facerefs,
                      const BgDocumentVertexRef *vertexrefs,
                      const DWORD *objectindices, int objectfirsttriangle,
                      int tricount,
                      const char *projectdir, BOOL framecamera)
{
    ViewportState *state = (ViewportState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    Vertex *scene = NULL;
    VertexColor *scenecolors = NULL;
    SceneBatch *batches = NULL;
    GLuint *textures = NULL;
    unsigned char *selectedtris = NULL;
    BgDocumentVertexRef *scenevertexrefs = NULL;
    DWORD savedobject = VIEWPORT_OBJECT_NONE;
    BgFaceRef *scenefacerefs = NULL;
    BgFaceRef *selectedrefs = NULL;
    int savedselectioncount = 0;
    int selectedcount = 0;
    DWORD *sceneobjectindices = NULL;
    TriKey *order = NULL;
    TexPixel *decode = NULL;
    int batchcount = 0;
    int texturecount = 0;
    float minx = 0, miny = 0, minz = 0, maxx = 0, maxy = 0, maxz = 0;
    int i;

    ViewportCancelTransform(hwnd);
    if (state != NULL && !framecamera) { savedobject = state->selectedobject; }
    if (state == NULL
        || (objectindices != NULL
            && (objectfirsttriangle < 0
                || objectfirsttriangle > tricount)))
    {
        return FALSE;
    }

    if (tris != NULL && tricount > 0)
    {
        if (!framecamera)
        {
            savedselectioncount = ViewportGetSelectedBgFaceCount(hwnd);
            if (savedselectioncount > 0)
            {
                selectedrefs = (BgFaceRef *)malloc(
                    (size_t)savedselectioncount * sizeof(*selectedrefs));
                if (selectedrefs == NULL
                    || !ViewportGetSelectedBgFaces(hwnd, selectedrefs, savedselectioncount))
                {
                    free(selectedrefs);
                    return FALSE;
                }
                qsort(selectedrefs, (size_t)savedselectioncount,
                      sizeof(*selectedrefs), ViewportCompareFaceRefs);
            }
        }
        scene = (Vertex *)malloc((size_t)tricount * 3 * sizeof(Vertex));
        scenecolors = (VertexColor *)malloc((size_t)tricount * 3
                                            * sizeof(*scenecolors));
        order = (TriKey *)malloc((size_t)tricount * sizeof(TriKey));
        batches = (SceneBatch *)malloc((size_t)tricount * sizeof(SceneBatch));
        textures = (GLuint *)malloc((size_t)tricount * sizeof(GLuint));
        selectedtris = (unsigned char *)calloc((size_t)tricount,
                                               sizeof(*selectedtris));
        scenefacerefs = (BgFaceRef *)calloc((size_t)tricount,
                                            sizeof(*scenefacerefs));
        sceneobjectindices = (DWORD *)malloc((size_t)tricount
                                             * sizeof(*sceneobjectindices));
        scenevertexrefs = (BgDocumentVertexRef *)calloc((size_t)tricount * 3, sizeof(*scenevertexrefs));
        decode = (TexPixel *)malloc(256 * 256 * sizeof(TexPixel));

        if (scene == NULL || scenecolors == NULL || order == NULL || batches == NULL
            || textures == NULL || selectedtris == NULL
            || scenefacerefs == NULL || sceneobjectindices == NULL
            || decode == NULL || scenevertexrefs == NULL)
        {
            free(scene); free(scenecolors); free(order); free(batches);
            free(textures); free(selectedtris); free(scenefacerefs);
            free(sceneobjectindices);
            free(scenevertexrefs);
            free(decode);
            free(selectedrefs);
            return FALSE; /* keep whatever we had */
        }

        /* Keep primary geometry before secondary geometry, then group
           by texture and culling state to minimize GL state changes. */
        for (i = 0; i < tricount; i++)
        {
            order[i].tag = tritags != NULL ? tritags[i] : BG_TEX_NONE;
            order[i].tri = i;
            sceneobjectindices[i] = VIEWPORT_OBJECT_NONE;
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

            if (vertexrefs != NULL)
            {
                memcpy(scenevertexrefs + i * 3, vertexrefs + order[i].tri * 3,
                       3 * sizeof(*scenevertexrefs));
            }
            if (facerefs != NULL)
            {
                scenefacerefs[i] = facerefs[order[i].tri];
                if (savedselectioncount > 0
                    && scenefacerefs[i].faceid != BG_FACE_ID_NONE
                    && bsearch(&scenefacerefs[i], selectedrefs,
                               (size_t)savedselectioncount, sizeof(*selectedrefs),
                               ViewportCompareFaceRefs) != NULL)
                {
                    selectedtris[i] = 1;
                    selectedcount++;
                }
            }
            if (objectindices != NULL
                && order[i].tri >= objectfirsttriangle)
            {
                sceneobjectindices[i] =
                    objectindices[order[i].tri - objectfirsttriangle];
            }

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
        free(selectedrefs);
    }

    ViewportFreeScene(state);
    state->scene = scene;
    state->scenecolors = scenecolors;
    state->scenecount = scene != NULL ? (GLsizei)(tricount * 3) : 0;
    state->batches = batches;
    state->batchcount = scene != NULL ? batchcount : 0;
    state->selectedtris = selectedtris;
    state->selectedtricount = selectedcount;
    state->scenefacerefs = scenefacerefs;
    state->sceneobjectindices = sceneobjectindices;
    state->scenevertexrefs = scenevertexrefs;
    state->textures = textures;
    state->texturecount = scene != NULL ? texturecount : 0;

    if (scene == NULL)
    {
        free(batches);
        free(textures);
        free(selectedtris);
        free(scenefacerefs);
        free(sceneobjectindices);
        free(scenevertexrefs);
        free(scenecolors);
        state->batches = NULL;
        state->textures = NULL;
        state->selectedtris = NULL;
        state->scenefacerefs = NULL;
        state->sceneobjectindices = NULL;
        state->scenevertexrefs = NULL;
        state->scenecolors = NULL;
        state->batchcount = 0;
        state->texturecount = 0;
    }

    for (i = 0; scene != NULL && i < tricount; i++)
    {
        if (selectedtris[i])
        {
            ViewportSetTriangleColor(state, i, TRUE);
        }
    }

    if (scene != NULL && framecamera)
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

    if (framecamera || scene == NULL) { state->componentcount = 0; }
    ViewportRestoreComponents(state);
    if (scene != NULL && savedobject != VIEWPORT_OBJECT_NONE) { ViewportSelectObject(state, savedobject); }
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    return TRUE;
}


int ViewportGetSelectedBgFaceCount(HWND hwnd)
{
    const ViewportState *state = ViewportGetState(hwnd);
    int count = 0;
    int triangle;
    int trianglecount;

    if (state == NULL || state->selectedtris == NULL
        || state->scenefacerefs == NULL)
    {
        return 0;
    }

    trianglecount = state->scenecount / 3;
    for (triangle = 0; triangle < trianglecount; triangle++)
    {
        if (state->selectedtris[triangle]
            && state->scenefacerefs[triangle].faceid != BG_FACE_ID_NONE)
        {
            count++;
        }
    }

    return count;
}


BOOL ViewportGetSelectedBgFaces(HWND hwnd, BgFaceRef *out, int count)
{
    const ViewportState *state = ViewportGetState(hwnd);
    int output = 0;
    int triangle;
    int trianglecount;

    if (state == NULL || state->selectedtris == NULL
        || state->scenefacerefs == NULL || out == NULL || count < 0)
    {
        return FALSE;
    }

    trianglecount = state->scenecount / 3;
    for (triangle = 0; triangle < trianglecount; triangle++)
    {
        if (!state->selectedtris[triangle]
            || state->scenefacerefs[triangle].faceid == BG_FACE_ID_NONE)
        {
            continue;
        }
        if (output >= count)
        {
            return FALSE;
        }
        out[output++] = state->scenefacerefs[triangle];
    }

    return output == count;
}


BOOL ViewportGetSingleSelectedBgFace(HWND hwnd, BgFaceRef *out)
{
    const ViewportState *state = ViewportGetState(hwnd);
    const BgFaceRef *found = NULL;
    int triangle;
    int trianglecount;

    if (out == NULL)
    {
        return FALSE;
    }
    ZeroMemory(out, sizeof(*out));

    if (state == NULL || state->selectedtris == NULL
        || state->scenefacerefs == NULL)
    {
        return FALSE;
    }

    trianglecount = state->scenecount / 3;
    for (triangle = 0; triangle < trianglecount; triangle++)
    {
        if (!state->selectedtris[triangle]
            || state->scenefacerefs[triangle].faceid == BG_FACE_ID_NONE)
        {
            continue;
        }

        if (found != NULL)
        {
            return FALSE;
        }
        found = &state->scenefacerefs[triangle];
    }

    if (found == NULL)
    {
        return FALSE;
    }

    *out = *found;
    return TRUE;
}


BOOL ViewportGetSelectedObject(HWND hwnd, DWORD *setupobjectindex)
{
    const ViewportState *state = ViewportGetState(hwnd);

    if (state == NULL || !state->showobjects || setupobjectindex == NULL
        || state->selectedobject == VIEWPORT_OBJECT_NONE)
    {
        return FALSE;
    }

    *setupobjectindex = state->selectedobject;
    return TRUE;
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
