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
#include <math.h>
#include "browser.h"
#include "viewport.h"

#define VIEWPORT_CLASS "GEditorViewport"

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

/* A contiguous run of scene vertices sharing one texture. */
typedef struct SceneBatch {
    GLuint  gltex;      /* 0 = untextured, vertex colors only */
    GLsizei first;
    GLsizei count;
    BOOL    secondary;  /* transparent layer: blended, no depth write */
} SceneBatch;

struct ViewportState;
static void ViewportFreeScene(struct ViewportState *state);

typedef struct Vertex {
    GLfloat x, y, z;
    GLubyte r, g, b, a;
    GLfloat s, t;
} Vertex;

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
    GLsizei scenecount;  /* vertices in scene */
    struct SceneBatch *batches;  /* texture-sorted draw ranges */
    int batchcount;
    GLuint *textures;    /* GL texture names owned by the scene */
    int texturecount;
    BOOL cullbackfaces;  /* global backface culling toggle */
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

    /*
     * Global backface culling. Front faces are counter-clockwise, the
     * shared convention of Fast3D and OpenGL, so the level's authored
     * winding carries through unchanged. The game's per-material cull
     * state is deliberately ignored for now - this is one big switch.
     */
    if (state->cullbackfaces)
    {
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);
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

            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

            /*
             * Primary pass: opaque, with alpha TEST so cutout textures
             * (fences, grates, foliage) punch real holes - fully
             * opaque texels are unaffected. The sort key put every
             * secondary batch after every primary one, so the state
             * flip below happens exactly once per frame.
             */
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.5f);

            for (i = 0; i < state->batchcount; i++)
            {
                const SceneBatch *batch = &state->batches[i];

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
                    glDisable(GL_ALPHA_TEST);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthMask(GL_FALSE);
                    insecondary = TRUE;
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


/* Frees the scene's GL textures and CPU arrays. Needs the GL context
   current for glDeleteTextures. */
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
    free(state->scene);
    state->textures = NULL;
    state->batches = NULL;
    state->scene = NULL;
    state->texturecount = 0;
    state->batchcount = 0;
    state->scenecount = 0;
}

/* qsort helper: order triangle indices by texture id. */
typedef struct TriKey { unsigned short texid; int tri; } TriKey;

static int ViewportTriKeyCompare(const void *a, const void *b)
{
    int d = (int)((const TriKey *)a)->texid - (int)((const TriKey *)b)->texid;

    return d != 0 ? d : ((const TriKey *)a)->tri - ((const TriKey *)b)->tri;
}

void ViewportSetScene(HWND hwnd, const BgVertex *tris,
                      const unsigned short *texids, int tricount,
                      const RomFile *rom)
{
    ViewportState *state = (ViewportState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    Vertex *scene = NULL;
    SceneBatch *batches = NULL;
    GLuint *textures = NULL;
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
        order = (TriKey *)malloc((size_t)tricount * sizeof(TriKey));
        batches = (SceneBatch *)malloc((size_t)tricount * sizeof(SceneBatch));
        textures = (GLuint *)malloc((size_t)tricount * sizeof(GLuint));
        decode = (TexPixel *)malloc(256 * 256 * sizeof(TexPixel));

        if (scene == NULL || order == NULL || batches == NULL
            || textures == NULL || decode == NULL)
        {
            free(scene); free(order); free(batches);
            free(textures); free(decode);
            return; /* keep whatever we had */
        }

        /*
         * Sort triangles by texture so each texture binds exactly once
         * per frame - the same batching that fixes Cradle in the game
         * fixes it here.
         */
        for (i = 0; i < tricount; i++)
        {
            order[i].texid = texids != NULL ? texids[i] : BG_TEX_NONE;
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

            if (i == 0 || order[i].texid != order[i - 1].texid)
            {
                SceneBatch *batch = &batches[batchcount++];

                batch->gltex = 0;
                batch->first = i * 3;
                batch->count = 0;
                batch->secondary = BG_TRI_IS_SECONDARY(order[i].texid);

                if (BG_TEX_ID(order[i].texid) != BG_TEX_NONE && rom != NULL)
                {
                    int tw = 0;
                    int th = 0;

                    if (TexDecodeById(rom, BG_TEX_ID(order[i].texid), decode, &tw, &th))
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
    state->scenecount = scene != NULL ? (GLsizei)(tricount * 3) : 0;
    state->batches = batches;
    state->batchcount = scene != NULL ? batchcount : 0;
    state->textures = textures;
    state->texturecount = scene != NULL ? texturecount : 0;

    if (scene == NULL)
    {
        free(batches);
        free(textures);
        state->batches = NULL;
        state->textures = NULL;
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