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
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "browser.h"
#include "viewport.h"
#include "gltf.h"
#include "fog.h"
#include "orbitcamera.h"
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
#define VIEWPORT_PAD_HALF_SIZE SETUP_PAD_HALF_SIZE
#define VIEWPORT_BOX_VERTICES  24
#define VIEWPORT_VERTEX_MARKER_SIZE 5.0f /* screen pixels */
#define VIEWPORT_PORTAL_FILL_ALPHA 64
#define VIEWPORT_PORTAL_EDGE_ALPHA 255
#define VIEWPORT_PICK_EPSILON 1.0e-10
#define VIEWPORT_PICK_BARY_EPSILON 1.0e-8
#define VIEWPORT_PICK_COPLANAR_EPSILON 1.0e-3
#define VIEWPORT_PICK_COPLANAR_RELATIVE_EPSILON 1.0e-6
#define VIEWPORT_OBJECT_NONE 0xffffffffu
#define VIEWPORT_BLEND_ALPHA_THRESHOLD 0.01f
#define VIEWPORT_CUTOUT_ALPHA_THRESHOLD 0.5f
/* Windows' OpenGL 1.1 header omits these core 1.2 / 1.4 sampler tokens. */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_MIRRORED_REPEAT
#define GL_MIRRORED_REPEAT 0x8370
#endif
#define VIEWPORT_TEXTURE_VARIANT_COUNT ((BG_TEX_NONE + 1) * 2)
#define VIEWPORT_STATS_FONT_GLYPHS 128

/* Core since GL 1.4, also exposed by GL_EXT_fog_coord on older drivers. */
#ifndef GL_FOG_COORDINATE_SOURCE
#define GL_FOG_COORDINATE_SOURCE 0x8450
#define GL_FOG_COORDINATE        0x8451
#define GL_FOG_COORDINATE_ARRAY  0x8457
#endif
typedef void (APIENTRY *FogCoordPointerFn)(GLenum, GLsizei, const GLvoid *);

/* Cached by texture ID and alpha use: authored order can revisit a texture many times.
 * Keep alpha for CPU picking through transparent areas of decals. */
typedef struct ViewportTexture {
    GLuint name;
    int width, height;
    BOOL attempted;
    unsigned char *alpha;
    BgRenderFlags wrapflags; /* current GL sampler state for this texture */
} ViewportTexture;

/* A contiguous run sharing texture, depth/blend state and culling. */
typedef struct SceneBatch {
    GLuint  gltex;      /* 0 = untextured, vertex colors only */
    GLsizei first;
    GLsizei count;
    BOOL    secondary;  /* authored room layer / visibility toggle */
    unsigned short textureid;
    BgRenderFlags renderflags;
    BOOL    cullbackfaces;
    BOOL    object;     /* setup model, independent of BG visibility */
} SceneBatch;

struct ViewportState;
static void ViewportFreeScene(struct ViewportState *state);

typedef struct Vertex {
    GLfloat x, y, z;
    GLubyte r, g, b, a;
    GLfloat s, t;
    BgEnvironmentVertex environment; /* world normal and normalized generation ranges */
    GLfloat fogamount;
} Vertex;

typedef struct VertexColor {
    GLubyte r, g, b;
} VertexColor;

typedef struct ViewportComponent {
    BgDocumentVertexRef refs[2];
    int corners[2];
} ViewportComponent;

typedef struct ViewportStanComponent {
    StanPointRef refs[2];
} ViewportStanComponent;

typedef struct ViewportPad {
    SetupPadRef ref;
    float position[3]; /* authored origin in world units */
    BOOL occupied;
} ViewportPad;

/* Per-viewport state, allocated at WM_CREATE, freed at WM_DESTROY,
   reachable from the window via GWLP_USERDATA. */
typedef struct ViewportState {
    HDC hdc;      /* private DC - stable for the window's lifetime (CS_OWNDC) */
    HGLRC hglrc;  /* the GL context rendering into it */
    EditorTool tool;
    BOOL vertexsnap;
    BOOL showbgstatistics;
    GLuint statisticsfont; /* ASCII bitmap display lists, owned by the GL context */
    DWORD bgprimarytris, bgsecondarytris, bgtexturecount; /* cached on scene rebuild */

    float backgroundcolor[3];
    BOOL showfog, levelfog;
    FogCurve fog;
    FogCoordPointerFn fogcoordpointer; /* owned by this viewport's GL context */
    
    /* Fly Camera */
    float posx, posy, posz;
    float yaw, pitch;
    float speed;

    BOOL orbit;
    OrbitCamera orbitcamera;
    unsigned int orbitbuttons;

    BOOL flying;
    Vertex *scene;       /* malloc'd level geometry, or NULL for the test scene */
    VertexColor *scenecolors; /* original RGB restored when faces are deselected */
    GLsizei scenecount;  /* vertices in scene */
    struct SceneBatch *batches;  /* draw-ordered draw ranges */
    int batchcount;
    unsigned char *selectedtris; /* one byte per draw-ordered triangle */
    unsigned char *hiddentris;   /* derived from hiddenrefs in the current draw order */
    BgFaceRef *hiddenrefs;       /* sorted level-local visibility; never document data */
    int hiddenrefcount;
    DWORD bghiddentris;          /* count of hidden faces currently in the scene */
    BgFaceRef *scenefacerefs;    /* stable document identity in the same order */
    DWORD *sceneobjectindices;   /* setup object identity in the same order */
    BgDocumentVertexRef *scenevertexrefs;
    ViewportComponent *components; /* insertion order preserves the vertex anchor */
    int componentcount, componentcapacity;
    int width, height;
    BOOL boxpending, boxdragging, boxadd, boxremove;
    POINT boxstart, boxend; /* viewport client pixels, independent of monitor origin */
    BgVertex *arrow;
    DWORD arrowtris;
    BgVertex *markermodels[SETUP_MARKER_KIND_COUNT];
    DWORD markermodeltris[SETUP_MARKER_KIND_COUNT];
    SetupMarker *setupmarkers;
    DWORD setupmarkercount;
    SetupSwirlPath swirlpath;
    BgVertex *cylinder;
    DWORD cylindertris;
    BOOL scalemode, dragscaling, scalevalid;
    Rotation scaleaxes;
    double dragdirection[3];
    BgVertex *scalehandle;
    DWORD scalehandletris;
    BOOL rotationmode, dragrotation;
    Rotation rotationframe;
    unsigned int rotationaxes;
    double rotationlast, rotationtotal, rotationmouse[2], rotationtangent[2];
    double gizmohit[3];
    BOOL gizmovisible;
    double gizmoposition[3];
    int hoveraxis, dragaxis;
    double dragorigin[3], dragplane[3], dragparameter, dragdelta, dragscale;
    BOOL dragvertical;
    BOOL dragstan;
    BOOL dragpad;
    float (*dragvertices)[3];
    unsigned char *dragmask;
    int selectedtricount;
    DWORD selectedobject;
    Vertex objectselectionbox[VIEWPORT_BOX_VERTICES];
    GLsizei objectselectionboxcount;
    ViewportTexture *texturecache; /* VIEWPORT_TEXTURE_VARIANT_COUNT entries */
    GLuint *textures;    /* GL texture names owned by the scene */
    int texturecount;
    StanFile stan; /* owned preview; edits are committed to the frame's document */
    DWORD *stanpointmap;
    unsigned char *stanselected;
    ViewportStanComponent *stancomponents;
    int stancomponentcount, stancomponentcapacity;
    int stanopacity; /* percent, independent of the tile's stored RGB */
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
    ViewportPad *pads; /* parallels the 24-vertex marker boxes */
    DWORD padcount;
    SetupPadRef selectedpad;
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


static void ViewportRefreshStanOverlay(ViewportState *state);
static void ViewportGetBasis(const ViewportState *state, float fwd[3], float right[3]);
static BOOL ViewportStanVisible(const ViewportState *state);
static void ViewportClearStanSelection(ViewportState *state);
static BOOL ViewportStanSelectionPosition(const ViewportState *state, BOOL gizmo,
                                           double position[3], DWORD *countout);
static Vertex ViewportStanPointVertex(const StanPoint *point);
static int ViewportCompareStanRefs(const void *left, const void *right);
static StanPointRef ViewportStanPointRef(const ViewportState *state, DWORD tile, DWORD point);
static void ViewportDrawTransformTools(const ViewportState *state);
static void ViewportUpdateGizmo(ViewportState *state);
static void ViewportRestoreComponents(ViewportState *state);
static void ViewportDrawBoxSelection(const ViewportState *state);

static int ViewportSelectedPadIndex(const ViewportState *state)
{
    DWORD i;
    if (state->selectedpad.index == SETUP_PAD_INDEX_NONE) { return -1; }
    for (i = 0; i < state->padcount; i++)
    {
        if (state->pads[i].ref.index == state->selectedpad.index
            && state->pads[i].ref.bound == state->selectedpad.bound) { return (int)i; }
    }
    return -1;
}

static BOOL ViewportPadVisible(const ViewportState *state, DWORD index)
{
    const ViewportPad *pad = &state->pads[index];
    return !pad->occupied || !state->showobjects
        || (pad->ref.index == state->selectedpad.index && pad->ref.bound == state->selectedpad.bound);
}

static void ViewportRefreshPadColors(ViewportState *state)
{
    int selected = ViewportSelectedPadIndex(state);
    DWORD i;
    for (i = 0; i < state->padcount; i++)
    {
        int vertex;
        BOOL white = (int)i == selected, bound = state->pads[i].ref.bound;
        for (vertex = 0; vertex < VIEWPORT_BOX_VERTICES; vertex++)
        {
            Vertex *v = &state->padmarkers[i * VIEWPORT_BOX_VERTICES + vertex];
            v->r = white || bound ? 255 : 32;
            v->g = white || !bound ? 255 : 48;
            v->b = white ? 255 : bound ? 48 : 64;
        }
    }
}

static void ViewportClearPadSelection(ViewportState *state)
{
    if (state->selectedpad.index == SETUP_PAD_INDEX_NONE) { return; }
    state->selectedpad.index = SETUP_PAD_INDEX_NONE;
    ViewportRefreshPadColors(state);
}

static BOOL ViewportPadSelectionPosition(const ViewportState *state, double position[3])
{
    int index = ViewportSelectedPadIndex(state), axis;
    if (index < 0 || state->tool != EDITOR_TOOL_FACE_SELECT) { return FALSE; }
    for (axis = 0; axis < 3; axis++)
    {
        position[axis] = state->pads[index].position[axis];
        if (state->dragpad && !state->dragrotation && !state->dragscaling && state->dragaxis == axis) { position[axis] += state->dragdelta; }
    }
    return TRUE;
}

static const Vertex g_TestScene[6] = {
    {    0.0f,  160.0f, 0.0f,   255,  40,  40, 255 , 1.0f, 0.0f, {{0, 0, 0}, {0, 0}}, 0},
    { -160.0f, -120.0f, 0.0f,    40, 255,  40, 255 , 0.0f, 1.0f, {{0, 0, 0}, {0, 0}}, 0},
    {  160.0f, -120.0f, 0.0f,    40,  40, 255, 255 , 0.0f, 0.0f, {{0, 0, 0}, {0, 0}}, 0},
    {    -80.0f,  160.0f, -200.0f,   255,  255,  0, 255, 2.0f, 0.0f, {{0, 0, 0}, {0, 0}}, 0},
    { -240.0f, -120.0f, -200.0f,    0, 255,  255, 255, 2.0f, 2.0f, {{0, 0, 0}, {0, 0}}, 0},
    {  80.0f, -120.0f, -200.0f,    255,  0, 0, 255 , 0.0f, 0.0f, {{0, 0, 0}, {0, 0}}, 0},
};

#define TESTSCENE_VERTS ((GLsizei)(sizeof(g_TestScene) / sizeof(g_TestScene[0])))

static ViewportState *ViewportGetState(HWND hwnd)
{
    return (ViewportState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

/* Cache counts for the level's BG or the complete orbit model preview.
 * Texture IDs are shared across rooms, layers and GL variants; missing images
 * still count as used, but untextured faces do not add a texture. */
static void ViewportUpdateStatistics(ViewportState *state)
{
    unsigned char used[BG_TEX_NONE] = {0};
    int i;

    state->bgprimarytris = state->bgsecondarytris = state->bgtexturecount = 0;
    for (i = 0; i < state->batchcount; i++)
    {
        const SceneBatch *batch = &state->batches[i];
        if ((!state->orbit && batch->object) || batch->count <= 0) { continue; }
        if (batch->secondary) { state->bgsecondarytris += batch->count / 3; }
        else { state->bgprimarytris += batch->count / 3; }
        if (batch->textureid < BG_TEX_NONE && !used[batch->textureid])
        {
            used[batch->textureid] = 1;
            state->bgtexturecount++;
        }
    }
}

static BOOL ViewportCreateStatisticsFont(ViewportState *state)
{
    HFONT font = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HGDIOBJ previous;
    GLuint lists;
    BOOL ok = FALSE;

    if (font == NULL) { return FALSE; }
    lists = glGenLists(VIEWPORT_STATS_FONT_GLYPHS);
    if (lists != 0)
    {
        previous = SelectObject(state->hdc, font);
        if (previous != NULL && previous != HGDI_ERROR)
        {
            ok = wglUseFontBitmapsA(state->hdc, 0, VIEWPORT_STATS_FONT_GLYPHS, lists);
            SelectObject(state->hdc, previous);
        }
        if (!ok) { glDeleteLists(lists, VIEWPORT_STATS_FONT_GLYPHS); }
    }
    DeleteObject(font);
    state->statisticsfont = ok ? lists : 0;
    return ok;
}

static void ViewportDrawStatistics(const ViewportState *state)
{
    char lines[5][64];
    int line, linecount;

    if ((!state->orbit && !state->showbgstatistics) || !state->statisticsfont
        || state->width <= 0 || state->height <= 0) { return; }
    if (state->orbit)
    {
        linecount = 2;
        snprintf(lines[0], sizeof(lines[0]), "Total tris: %lu",
                 (unsigned long)(state->bgprimarytris + state->bgsecondarytris));
        snprintf(lines[1], sizeof(lines[1]), "Unique textures: %lu", (unsigned long)state->bgtexturecount);
    }
    else
    {
        linecount = 5;
        snprintf(lines[0], sizeof(lines[0]), "Primary tris: %lu", (unsigned long)state->bgprimarytris);
        snprintf(lines[1], sizeof(lines[1]), "Secondary tris: %lu", (unsigned long)state->bgsecondarytris);
        snprintf(lines[2], sizeof(lines[2]), "Total tris: %lu",
                 (unsigned long)(state->bgprimarytris + state->bgsecondarytris));
        snprintf(lines[3], sizeof(lines[3]), "Unique textures: %lu", (unsigned long)state->bgtexturecount);
        snprintf(lines[4], sizeof(lines[4]), "Hidden faces: %lu", (unsigned long)state->bghiddentris);
    }

    /* Draw into the back buffer so text stays steady during camera flight.
       A one-pixel shadow keeps it legible over bright geometry. */
    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
                 | GL_TRANSFORM_BIT | GL_LIST_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDepthMask(GL_FALSE);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, state->width, state->height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glListBase(state->statisticsfont);
    for (line = 0; line < linecount; line++)
    {
        GLsizei length = (GLsizei)strlen(lines[line]);
        int baseline = 24 + line * 20;
        glColor4ub(0, 0, 0, 255);
        glRasterPos2i(13, baseline + 1);
        glCallLists(length, GL_UNSIGNED_BYTE, lines[line]);
        glColor4ub(255, 255, 255, 255);
        glRasterPos2i(12, baseline);
        glCallLists(length, GL_UNSIGNED_BYTE, lines[line]);
    }
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
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

    {
        const char *version = (const char *)glGetString(GL_VERSION);
        const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
        const char *match = extensions ? strstr(extensions, "GL_EXT_fog_coord") : NULL;
        int major = 0, minor = 0;
        BOOL extension = match && (match == extensions || match[-1] == ' ')
            && (match[16] == ' ' || match[16] == '\0');
        PROC pointer = NULL;
        if (version != NULL) { sscanf(version, "%d.%d", &major, &minor); }
        if (major > 1 || (major == 1 && minor >= 4))
        { pointer = wglGetProcAddress("glFogCoordPointer"); }
        /* Some WGL drivers return sentinel values for unsupported functions. */
        if ((INT_PTR)pointer == -1 || (UINT_PTR)pointer <= 3) { pointer = NULL; }
        if (pointer == NULL && extension)
        { pointer = wglGetProcAddress("glFogCoordPointerEXT"); }
        if ((INT_PTR)pointer != -1 && (UINT_PTR)pointer > 3)
        { state->fogcoordpointer = (FogCoordPointerFn)(void *)pointer; }
    }

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

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    return ViewportCreateStatisticsFont(state);
}


/* Keeps the GL viewport and projection matched to the window size. */
static void ViewportResizeGL(ViewportState *state, int width, int height)
{
    GLdouble aspect;
    GLdouble halfheight;
    GLdouble halfwidth;
    GLdouble nearz = VIEWPORT_NEAR_Z, farz = VIEWPORT_FAR_Z;

    if (width < 1)  width = 1;
    if (height < 1) height = 1;

    wglMakeCurrent(state->hdc, state->hglrc);
    glViewport(0, 0, width, height);

    aspect = (GLdouble)width / (GLdouble)height;

    if (state->orbit) { OrbitCameraClip(&state->orbitcamera, &nearz, &farz); }
    halfheight = tan(VIEWPORT_FOV_Y * 0.5 * VIEWPORT_DEG_TO_RAD) * nearz;
    halfwidth = halfheight * aspect;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-halfwidth, halfwidth, -halfheight, halfheight, nearz, farz);
    glMatrixMode(GL_MODELVIEW);
}


static BOOL ViewportTriangleHidden(const ViewportState *state, int triangle)
{
    return state->hiddentris != NULL && state->hiddentris[triangle] != 0;
}

/* Keep adjacent visible triangles batched, including the point/edge passes.
   With no hidden faces the original single draw call is retained. */
static void ViewportDrawVisibleBatch(const ViewportState *state, const SceneBatch *batch)
{
    int first = batch->first, end = first + batch->count, corner;
    if (batch->object || state->bghiddentris == 0)
    { glDrawArrays(GL_TRIANGLES, first, batch->count); return; }
    for (corner = first; corner < end; corner += 3)
    {
        if (!ViewportTriangleHidden(state, corner / 3)) { continue; }
        if (corner > first) { glDrawArrays(GL_TRIANGLES, first, corner - first); }
        first = corner + 3;
    }
    if (first < end) { glDrawArrays(GL_TRIANGLES, first, end - first); }
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
            ViewportDrawVisibleBatch(state, batch);
        }
    }

    glPopClientAttrib();
    glPopAttrib();
}


static unsigned int ViewportTextureKey(unsigned short textureid, BgRenderFlags flags)
{
    return textureid + ((flags & BG_RENDER_IGNORE_TEXTURE_ALPHA) ? BG_TEX_NONE + 1 : 0);
}


/* Wrapping belongs to the draw's material, not the image ID. Reuse texture
   storage and change its sampler only when an adjacent use needs new modes. */
static void ViewportApplyTextureWrap(ViewportState *state, const SceneBatch *batch)
{
    ViewportTexture *texture =
        &state->texturecache[ViewportTextureKey(batch->textureid, batch->renderflags)];
    BgRenderFlags wrap = batch->renderflags & BG_RENDER_WRAP_MASK;
    BgRenderFlags changed = wrap ^ texture->wrapflags;
    if (changed & (BG_RENDER_CLAMP_S | BG_RENDER_MIRROR_S))
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        (wrap & BG_RENDER_CLAMP_S)    ? GL_CLAMP_TO_EDGE
                        : (wrap & BG_RENDER_MIRROR_S) ? GL_MIRRORED_REPEAT
                                                      : GL_REPEAT);
    }
    if (changed & (BG_RENDER_CLAMP_T | BG_RENDER_MIRROR_T))
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        (wrap & BG_RENDER_CLAMP_T)    ? GL_CLAMP_TO_EDGE
                        : (wrap & BG_RENDER_MIRROR_T) ? GL_MIRRORED_REPEAT
                                                      : GL_REPEAT);
    }
    texture->wrapflags = wrap;
}

static void ViewportApplyRenderFlags(BgRenderFlags flags)
{
    /* GL_ALWAYS allows independent depth writes when N64 Z_CMP is disabled. */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(!(flags & BG_RENDER_DEPTH_TEST) ? GL_ALWAYS
                : (flags & BG_RENDER_DECAL)     ? GL_LEQUAL
                                                : GL_LESS);
    glDepthMask((flags & BG_RENDER_DEPTH_WRITE) != 0);
    if (flags & BG_RENDER_DECAL)
    {
        /* All decals share the same bias and leave the supporting depth
         * intact, so later decals cover earlier ones in authored order. */
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
    }
    else
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    if (flags & BG_RENDER_BLEND)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    if (flags & (BG_RENDER_BLEND | BG_RENDER_ALPHA_TEST))
    {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, (flags & BG_RENDER_ALPHA_TEST) ? VIEWPORT_CUTOUT_ALPHA_THRESHOLD
                                                               : VIEWPORT_BLEND_ALPHA_THRESHOLD);
    }
    else
    {
        glDisable(GL_ALPHA_TEST);
    }
}

/* Match guLookAtReflect: the world-space right and up axes rotate with the
 * camera. CPU-generated vertex UVs feed both OpenGL and alpha-aware picking. */
static void ViewportEnvironmentAxes(const ViewportState *state, float right[3], float up[3])
{
    float forward[3];
    ViewportGetBasis(state, forward, right);
    up[0] = right[1] * forward[2] - right[2] * forward[1];
    up[1] = right[2] * forward[0] - right[0] * forward[2];
    up[2] = right[0] * forward[1] - right[1] * forward[0];
}

static void ViewportEnvironmentCoordinates(const ViewportState *state, int index,
                                           BgRenderFlags flags, const float right[3],
                                           const float up[3], float uv[2])
{
    BgEnvironmentVertex environment = state->scene[index].environment;
    if (state->dragrotation && !state->dragstan && !state->dragpad && state->dragmask &&
        state->dragmask[index])
    {
        Rotation rotation;
        double normal[3] = {environment.normal[0], environment.normal[1], environment.normal[2]};
        double rotated[3];
        int axis;
        RotationAxis(&rotation, state->dragaxis, state->dragdelta);
        RotationVector(&rotation, normal, rotated);
        for (axis = 0; axis < 3; axis++)
        {
            environment.normal[axis] = (float)rotated[axis];
        }
    }
    if (state->dragscaling && state->selectedobject != VIEWPORT_OBJECT_NONE
        && !state->dragstan && !state->dragpad && state->dragaxis >= 0
        && state->dragmask && state->dragmask[index])
    {
        Scaling inverse = {0};
        double normal[3] = {environment.normal[0], environment.normal[1], environment.normal[2]};
        int axis;
        inverse.axes = state->scaleaxes;
        for (axis = 0; axis < 3; axis++)
        {
            inverse.factor[axis] = axis == state->dragaxis ? 1.0 / (1 + state->dragdelta) : 1;
        }
        ScalingPoint(&inverse, normal, normal);
        for (axis = 0; axis < 3; axis++) { environment.normal[axis] = (float)normal[axis]; }
    }
    BgRenderEnvironmentCoordinates(&environment, flags, right, up, uv);
}

static void ViewportUpdateEnvironmentMapping(ViewportState *state)
{
    float right[3], up[3];
    int batchindex;
    ViewportEnvironmentAxes(state, right, up);
    for (batchindex = 0; batchindex < state->batchcount; batchindex++)
    {
        const SceneBatch *batch = &state->batches[batchindex];
        int index;
        if (!(batch->renderflags & BG_RENDER_ENVIRONMENT))
        {
            continue;
        }
        for (index = batch->first; index < batch->first + batch->count; index++)
        {
            float uv[2];
            ViewportEnvironmentCoordinates(state, index, batch->renderflags, right, up, uv);
            state->scene[index].s = uv[0];
            state->scene[index].t = uv[1];
        }
    }
}

static void ViewportBeginFog(ViewportState *state)
{
    GLfloat color[4] = {state->backgroundcolor[0], state->backgroundcolor[1],
                        state->backgroundcolor[2], 1.0f};
    glDisable(GL_FOG);
    if (state->fogcoordpointer != NULL) { glDisableClientState(GL_FOG_COORDINATE_ARRAY); }
    if (!state->showfog || !state->levelfog || state->orbit
        || state->scene == NULL || state->scenecount == 0) { return; }

    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, color);
    if (state->fogcoordpointer != NULL)
    {
        float forward[3], right[3];
        GLsizei i;
        ViewportGetBasis(state, forward, right);
        for (i = 0; i < state->scenecount; i++)
        {
            Vertex *v = &state->scene[i];
            double depth = ((double)v->x - state->posx) * forward[0]
                         + ((double)v->y - state->posy) * forward[1]
                         + ((double)v->z - state->posz) * forward[2];
            v->fogamount = FogAmount(&state->fog, depth);
        }
        glFogi(GL_FOG_COORDINATE_SOURCE, GL_FOG_COORDINATE);
        glFogf(GL_FOG_START, 0.0f);
        glFogf(GL_FOG_END, 1.0f);
        state->fogcoordpointer(GL_FLOAT, sizeof(Vertex), &state->scene[0].fogamount);
        glEnableClientState(GL_FOG_COORDINATE_ARRAY);
    }
    else
    {
        glFogf(GL_FOG_START, state->fog.linearstart);
        glFogf(GL_FOG_END, state->fog.linearend);
    }
    glEnable(GL_FOG);
}

/* The supplied GLBs use +X for the arrow/lens and +Y for up. Convert
 * metres to GoldenEye's centimetre world units without any level-scale factor. */
#define VIEWPORT_MARKER_MODEL_SCALE 100.0f
static void ViewportDrawSetupMarkers(const ViewportState *state)
{
    static const GLfloat ambient[4] = {0.4f, 0.4f, 0.4f, 1};
    static const GLfloat diffuse[4] = {0.6f, 0.6f, 0.6f, 1};
    static const GLfloat black[4] = {0, 0, 0, 1};
    static const GLfloat lightdirection[4] = {-0.4f, 0.8f, 0.3f, 0};
    DWORD i;
    if (!state->showobjects || state->setupmarkercount == 0) { return; }
    glPushAttrib(GL_ENABLE_BIT | GL_LIGHTING_BIT | GL_CURRENT_BIT
        | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT);
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    if (state->swirlpath.curvecount > 0 && state->markermodels[SETUP_MARKER_SWIRL])
    {
        const SetupSwirlPath *path = &state->swirlpath;
        const BgVertex *diamond = state->markermodels[SETUP_MARKER_SWIRL];
        glDisable(GL_LIGHTING);
        glDisable(GL_LINE_STIPPLE);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
        /* Use the resource's orange vertex color for the entire curve. */
        glColor3ub(diamond->r, diamond->g, diamond->b);
        glLineWidth(2.0f);
        glVertexPointer(3, GL_FLOAT, sizeof(*path->curve), path->curve);
        glDrawArrays(GL_LINE_STRIP, 0, path->curvecount);
        /* Connect the two tangent-only end controls as dashed guides. These
         * influence the spline but are not part of the camera's travel. */
        glLineWidth(1.0f);
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0x00ff);
        glBegin(GL_LINES);
        glVertex3fv(path->points[0].position);
        glVertex3fv(path->points[1].position);
        glVertex3fv(path->points[path->pointcount - 2].position);
        glVertex3fv(path->points[path->pointcount - 1].position);
        glEnd();
        glDisable(GL_LINE_STIPPLE);
    }
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glLightfv(GL_LIGHT0, GL_AMBIENT, black);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, black);
    /* Set after the view transform so the light stays fixed in world space. */
    glLightfv(GL_LIGHT0, GL_POSITION, lightdirection);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    for (i = 0; i < state->setupmarkercount + state->swirlpath.pointcount; i++)
    {
        SetupMarker control = {0};
        const SetupMarker *marker;
        if (i < state->setupmarkercount) { marker = &state->setupmarkers[i]; }
        else
        {
            control.kind = SETUP_MARKER_SWIRL;
            memcpy(control.position, state->swirlpath.points[i - state->setupmarkercount].position,
                sizeof(control.position));
            control.look[0] = 1;
            control.up[1] = 1;
            marker = &control;
        }
        const BgVertex *model = state->markermodels[marker->kind];
        GLfloat matrix[16] = {0};
        const float *look = marker->look, *up = marker->up;
        int axis;
        if (model == NULL) { continue; }
        for (axis = 0; axis < 3; axis++)
        {
            matrix[axis] = look[axis];
            matrix[4 + axis] = up[axis];
            matrix[12 + axis] = marker->position[axis];
        }
        matrix[8] = look[1] * up[2] - look[2] * up[1];
        matrix[9] = look[2] * up[0] - look[0] * up[2];
        matrix[10] = look[0] * up[1] - look[1] * up[0];
        matrix[15] = 1;
        glPushMatrix();
        glMultMatrixf(matrix);
        glScalef(VIEWPORT_MARKER_MODEL_SCALE, VIEWPORT_MARKER_MODEL_SCALE, VIEWPORT_MARKER_MODEL_SCALE);
        glVertexPointer(3, GL_FLOAT, sizeof(*model), &model->x);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(*model), &model->r);
        glNormalPointer(GL_FLOAT, sizeof(*model), model->environment.normal);
        glDrawArrays(GL_TRIANGLES, 0, state->markermodeltris[marker->kind] * 3);
        glPopMatrix();
    }
    glPopClientAttrib();
    glPopAttrib();
}

static void ViewportPaintGL(ViewportState *state)
{
    wglMakeCurrent(state->hdc, state->hglrc);

    glClearColor(state->backgroundcolor[0], state->backgroundcolor[1], state->backgroundcolor[2], 1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (state->orbit && state->scene == NULL)
    {
        ViewportDrawStatistics(state);
        SwapBuffers(state->hdc);
        return;
    }

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

    ViewportBeginFog(state);

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
            BOOL incullback = FALSE;
            int activerenderflags = -1;

            ViewportUpdateEnvironmentMapping(state);

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

                if (activerenderflags != batch->renderflags)
                {
                    ViewportApplyRenderFlags(batch->renderflags);
                    activerenderflags = batch->renderflags;
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
                    ViewportApplyTextureWrap(state, batch);
                }
                else
                {
                    glDisable(GL_TEXTURE_2D);
                }

                ViewportDrawVisibleBatch(state, batch);
            }

            glDisable(GL_TEXTURE_2D);
            glDisable(GL_ALPHA_TEST);
            glDisable(GL_BLEND);
            glDisable(GL_POLYGON_OFFSET_FILL);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
        }
        else
        {
            glDrawArrays(GL_TRIANGLES, 0, count);
        }

        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    /* Fog belongs to level surfaces/models, not editing aids or statistics.
       Disable the array too: overlay vertex buffers have different lengths. */
    glDisable(GL_FOG);
    if (state->fogcoordpointer != NULL) { glDisableClientState(GL_FOG_COORDINATE_ARRAY); }

    ViewportDrawSetupMarkers(state);

    if (ViewportStanVisible(state) && state->stanfill != NULL && state->stanfillcount > 0)
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

    if (ViewportStanVisible(state) && state->stanedges != NULL && state->stanedgecount > 0)
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
        DWORD first = 0;
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
        /* Batch adjacent visible boxes while exposing occupied pads when
         * their models are hidden. Selected pads remain available. */
        while (first < state->padcount)
        {
            DWORD end;
            if (!ViewportPadVisible(state, first)) { first++; continue; }
            end = first + 1;
            while (end < state->padcount && ViewportPadVisible(state, end)) { end++; }
            glDrawArrays(GL_LINES, first * VIEWPORT_BOX_VERTICES,
                         (end - first) * VIEWPORT_BOX_VERTICES);
            first = end;
        }
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

    if (!state->orbit)
    {
        ViewportDrawBgToolOverlay(state);
        ViewportDrawTransformTools(state);
        ViewportDrawBoxSelection(state);
    }
    ViewportDrawStatistics(state);
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
    /* W/E may already be held after selecting a transform mode. Seed the
       movement state here because repeated key-down messages are ignored. */
    state->keyw = (GetKeyState('W') & 0x8000) != 0;
    state->keya = (GetKeyState('A') & 0x8000) != 0;
    state->keys = (GetKeyState('S') & 0x8000) != 0;
    state->keyd = (GetKeyState('D') & 0x8000) != 0;
    state->keyq = (GetKeyState('Q') & 0x8000) != 0;
    state->keye = (GetKeyState('E') & 0x8000) != 0;
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
static DWORD ViewportFindPickedStan(const ViewportState *state, const ViewportPickRay *ray,
                                     double *distanceout);
static BOOL ViewportTryPickStan(HWND hwnd, ViewportState *state, int x, int y, BOOL add, BOOL remove);



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

/* CPU equivalent of GL_LINEAR and the material's wrap modes at a triangle hit.
 * Vertex colors remain affine on the triangle; ray intersection supplies
 * the same perspective-correct surface point used by rasterization. */
static BOOL ViewportRayBatchTriangleDistance(const ViewportState *state, const SceneBatch *batch,
                                             const ViewportPickRay *ray, int corner,
                                             double *distance)
{
    const Vertex *v = &state->scene[corner];
    const ViewportTexture *texture =
        state->texturecache ? &state->texturecache[ViewportTextureKey(batch->textureid, batch->renderflags)] : NULL;
    double edge[2][3], delta[3], aa = 0, ab = 0, bb = 0, ap = 0, bp = 0;
    double u, w, denominator, alpha, threshold;
    int axis;
    if (ViewportTriangleHidden(state, corner / 3)) { return FALSE; }
    if (!ViewportRayTriangleDistance(ray, v, state->cullbackfaces && batch->cullbackfaces,
                                     distance))
    {
        return FALSE;
    }
    if (!(batch->renderflags & (BG_RENDER_BLEND | BG_RENDER_ALPHA_TEST)))
    {
        return TRUE;
    }
    for (axis = 0; axis < 3; axis++)
    {
        edge[0][axis] = (&v[1].x)[axis] - (&v[0].x)[axis];
        edge[1][axis] = (&v[2].x)[axis] - (&v[0].x)[axis];
        delta[axis] = ray->origin[axis] + ray->direction[axis] * *distance - (&v[0].x)[axis];
        aa += edge[0][axis] * edge[0][axis];
        ab += edge[0][axis] * edge[1][axis];
        bb += edge[1][axis] * edge[1][axis];
        ap += edge[0][axis] * delta[axis];
        bp += edge[1][axis] * delta[axis];
    }
    denominator = aa * bb - ab * ab;
    if (!(denominator > 0))
    {
        return FALSE;
    }
    u = (bb * ap - ab * bp) / denominator;
    w = (aa * bp - ab * ap) / denominator;
    alpha = ((1 - u - w) * v[0].a + u * v[1].a + w * v[2].a) / 255.0;
    if (texture && texture->name && texture->alpha)
    {
        float uv[3][2] = {{v[0].s, v[0].t}, {v[1].s, v[1].t}, {v[2].s, v[2].t}};
        if (batch->renderflags & BG_RENDER_ENVIRONMENT)
        {
            float right[3], up[3];
            int vertex;
            ViewportEnvironmentAxes(state, right, up);
            for (vertex = 0; vertex < 3; vertex++)
            {
                ViewportEnvironmentCoordinates(state, corner + vertex, batch->renderflags,
                                               right, up, uv[vertex]);
            }
        }
        double tx = (1 - u - w) * uv[0][0] + u * uv[1][0] + w * uv[2][0];
        double ty = (1 - u - w) * uv[0][1] + u * uv[1][1] + w * uv[2][1];
        double x = BgRenderWrapCoordinate(tx, batch->renderflags, FALSE) * texture->width - 0.5;
        double y = BgRenderWrapCoordinate(ty, batch->renderflags, TRUE) * texture->height - 0.5;
        double fx = x - floor(x), fy = y - floor(y), sample = 0;
        int ix, iy;
        for (iy = 0; iy < 2; iy++)
        {
            for (ix = 0; ix < 2; ix++)
            {
                int px = BgRenderWrapTexel((int)floor(x) + ix, texture->width, batch->renderflags, FALSE);
                int py = BgRenderWrapTexel((int)floor(y) + iy, texture->height, batch->renderflags, TRUE);
                sample += texture->alpha[py * texture->width + px] * (ix ? fx : 1 - fx) *
                          (iy ? fy : 1 - fy);
            }
        }
        alpha *= sample / 255.0;
    }
    threshold = (batch->renderflags & BG_RENDER_ALPHA_TEST) ? VIEWPORT_CUTOUT_ALPHA_THRESHOLD
                                                                 : VIEWPORT_BLEND_ALPHA_THRESHOLD;
    return alpha > threshold;
}

/* Replay depth tests in actual draw order at one surface point. Decals use
 * a small coplanar tolerance to model the GL bias; they never replace the
 * supporting depth. Ordinary depth-writing ties keep the first surface. */
static int ViewportFindVisibleSceneTriangle(const ViewportState *state, const ViewportPickRay *ray,
                                            double *distanceout)
{
    double depth = DBL_MAX;
    int winner = -1, i;
    *distanceout = DBL_MAX;
    if (!state->scene)
    {
        return -1;
    }
    for (i = 0; i < state->batchcount; i++)
    {
        const SceneBatch *batch = &state->batches[i];
        int corner;
        if (!ViewportBatchIsPickable(state, batch) && !(batch->object && state->showobjects))
        {
            continue;
        }
        for (corner = batch->first; corner < batch->first + batch->count; corner += 3)
        {
            double distance, tolerance;
            if (!ViewportRayBatchTriangleDistance(state, batch, ray, corner, &distance))
            {
                continue;
            }
            tolerance = ViewportCoplanarPickTolerance(distance);
            if ((batch->renderflags & BG_RENDER_DEPTH_TEST) && depth != DBL_MAX &&
                ((batch->renderflags & BG_RENDER_DECAL) ? distance > depth + tolerance
                                                        : distance >= depth - tolerance))
            {
                continue;
            }
            winner = corner / 3;
            *distanceout = distance;
            if (batch->renderflags & BG_RENDER_DEPTH_WRITE)
            {
                depth = distance;
            }
        }
    }
    return winner;
}

static int ViewportFindPickedTriangle(const ViewportState *state, const ViewportPickRay *ray,
                                      BOOL addtoselection, BOOL deselect, double *distanceout)
{
    double distance;
    int visible = ViewportFindVisibleSceneTriangle(state, ray, distanceout);
    int firsttriangle = -1, firstselected = -1, firstunselected = -1;
    int nextafterselected = -1, selectedhits = 0;
    BOOL passedselected = FALSE;
    int pass, batchindex;
    if (visible < 0 || !state->selectedtris || !state->scenefacerefs ||
        state->scenefacerefs[visible].faceid == BG_FACE_ID_NONE)
    {
        *distanceout = DBL_MAX;
        return -1;
    }
    /* The rendered winner comes first. Cycle down through the remaining
     * coplanar faces in reverse authored order, without painting hidden ones. */
    for (pass = 0; pass < 2; pass++)
    {
        for (batchindex = state->batchcount - 1; batchindex >= 0; batchindex--)
        {
            const SceneBatch *batch = &state->batches[batchindex];
            int vertex;
            if (!ViewportBatchIsPickable(state, batch))
            {
                continue;
            }
            for (vertex = batch->first + batch->count - 3; vertex >= batch->first; vertex -= 3)
            {
                int triangle = vertex / 3;
                if ((pass == 0) != (triangle == visible) ||
                    !ViewportRayBatchTriangleDistance(state, batch, ray, vertex, &distance) ||
                    fabs(distance - *distanceout) > ViewportCoplanarPickTolerance(*distanceout))
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


static DWORD ViewportFindPickedObject(const ViewportState *state, const ViewportPickRay *ray,
                                      double *distanceout)
{
    int triangle = ViewportFindVisibleSceneTriangle(state, ray, distanceout);
    if (triangle >= 0 && state->sceneobjectindices &&
        state->sceneobjectindices[triangle] != VIEWPORT_OBJECT_NONE)
    {
        return state->sceneobjectindices[triangle];
    }
    *distanceout = DBL_MAX;
    return VIEWPORT_OBJECT_NONE;
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
    int triangle, batchindex = 0;

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
        unsigned char previewalpha;
        unsigned int corner;

        if (ref->faceid == BG_FACE_ID_NONE || ref->room != hit->face.room)
        {
            continue;
        }
        face = BgDocumentFindFace(document, ref, NULL);
        if (face == NULL || (face->vertexindices[0] != vertexindex
            && face->vertexindices[1] != vertexindex && face->vertexindices[2] != vertexindex)) { continue; }
        previewalpha = BgDocumentPreviewVertexAlpha(room, face, source->a);
        while (batchindex + 1 < state->batchcount
               && triangle * 3 >= state->batches[batchindex].first + state->batches[batchindex].count)
        {
            batchindex++;
        }
        BgRenderFlags flags = state->batches[batchindex].renderflags;
        BgVertex preview = {.r = source->r, .g = source->g, .b = source->b};
        BgRenderPrepareEnvironment(&preview, flags, &face->material);
        for (corner = 0; corner < 3; corner++)
        {
            int vertex = triangle * 3 + corner;
            if (face->vertexindices[corner] != vertexindex) { continue; }
            state->scenecolors[vertex].r = preview.r;
            state->scenecolors[vertex].g = preview.g;
            state->scenecolors[vertex].b = preview.b;
            memcpy(state->scene[vertex].environment.normal, preview.environment.normal,
                   sizeof(preview.environment.normal));
            state->scene[vertex].a = previewalpha;
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
    ViewportClearPadSelection(state);
    ViewportClearBgSelection(state);
    ViewportClearObjectSelection(state);
    ViewportClearStanSelection(state);
    state->componentcount = 0;
    state->gizmovisible = FALSE;
    state->hoveraxis = -1;
}


/* Painting and texture drops always target the rendered winner, without cycling. */
static int ViewportFindNearestBgTriangle(const ViewportState *state, const ViewportPickRay *ray,
                                         double *distanceout)
{
    int triangle = ViewportFindVisibleSceneTriangle(state, ray, distanceout);
    if (triangle < 0 || !state->scenefacerefs ||
        state->scenefacerefs[triangle].faceid == BG_FACE_ID_NONE)
    {
        *distanceout = DBL_MAX;
        return -1;
    }
    return triangle;
}


BOOL ViewportGetTextureDropFace(HWND hwnd, POINT screen,
                                BgFaceRef *out, BOOL *selectedout)
{
    ViewportState *state = (ViewportState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    ViewportPickRay ray;
    RECT client;
    double distance, standistance;
    int triangle;

    if (state == NULL || state->flying || state->dragaxis >= 0
        || state->tool != EDITOR_TOOL_FACE_SELECT || out == NULL || selectedout == NULL)
    {
        return FALSE;
    }
    ScreenToClient(hwnd, &screen);
    GetClientRect(hwnd, &client);
    if (!PtInRect(&client, screen)
        || !ViewportBuildPickRay(hwnd, state, screen.x, screen.y, &ray))
    {
        return FALSE;
    }
    triangle = ViewportFindNearestBgTriangle(state, &ray, &distance);
    if (triangle < 0) { return FALSE; }
    if (ViewportFindPickedStan(state, &ray, &standistance) != STAN_TILE_NONE
        && standistance <= distance + ViewportCoplanarPickTolerance(distance))
    {
        return FALSE;
    }
    *out = state->scenefacerefs[triangle];
    *selectedout = state->selectedtris != NULL && state->selectedtris[triangle];
    return TRUE;
}


BOOL ViewportGetModelDropPosition(HWND hwnd, POINT screen, double position[3])
{
    ViewportState *state = (ViewportState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    ViewportPickRay ray;
    RECT client;
    double distance, standistance;
    int axis;

    if (state == NULL || state->flying || state->dragaxis >= 0 || position == NULL)
    {
        return FALSE;
    }
    ScreenToClient(hwnd, &screen);
    GetClientRect(hwnd, &client);
    if (!PtInRect(&client, screen) || !ViewportBuildPickRay(hwnd, state, screen.x, screen.y, &ray))
    {
        return FALSE;
    }
    ViewportFindVisibleSceneTriangle(state, &ray, &distance);
    if (ViewportFindPickedStan(state, &ray, &standistance) != STAN_TILE_NONE &&
        standistance < distance)
    {
        distance = standistance;
    }
    if (distance == DBL_MAX)
    {
        return FALSE;
    }
    for (axis = 0; axis < 3; axis++)
    {
        position[axis] = ray.origin[axis] + ray.direction[axis] * distance;
    }
    return TRUE;
}


/* Resolve the corner nearest the world-space hit on the closest BG face. */
static BOOL ViewportFindPaintTarget(const ViewportState *state,
                                     const ViewportPickRay *ray,
                                     ViewportBgVertexHit *hit)
{
    double nearestdistance;
    double cornerdistance = DBL_MAX;
    double position[3];
    unsigned int corner;
    int triangle = ViewportFindNearestBgTriangle(state, ray, &nearestdistance);

    if (triangle < 0) { return FALSE; }
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

    if (state == NULL || state->flying
        || state->tool != EDITOR_TOOL_FACE_SELECT)
    {
        return;
    }

    if (state->selectedtris == NULL || state->scenecolors == NULL)
    {
        ViewportClearAllSelection(state);
        ViewportUpdateGizmo(state);
        InvalidateRect(hwnd, NULL, FALSE);
        SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
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
    ViewportClearPadSelection(state);
    if (objectindex != VIEWPORT_OBJECT_NONE || bgdistance != DBL_MAX) { ViewportClearStanSelection(state); }

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
    if (ViewportTriangleHidden(state, corner / 3)) { return FALSE; }
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
        if (ViewportCompareVertexRefs(ref, &state->scenevertexrefs[i]) == 0
            && ViewportCornerVisible(state, i)) { return i; }
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
    if (state != NULL && ViewportPadSelectionPosition(state, position)) { *countout = 1; return TRUE; }
    if (state != NULL && ViewportStanSelectionPosition(state, FALSE, position, countout)) { return TRUE; }
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
    if (state->vertexsnap) { return; }
    if (state->scalemode)
    {
        int padindex = ViewportSelectedPadIndex(state);
        if (state->selectedobject != VIEWPORT_OBJECT_NONE
            && (state->selectedobject & SETUP_CHARACTER_SELECTION_BIT)) { return; }
        if (padindex >= 0)
        {
            /* The volume's center can differ from its authored pad origin. */
            for (axis = 0; axis < 3; axis++) { state->gizmoposition[axis] = 0; }
            for (i = 0; i < VIEWPORT_BOX_VERTICES; i++)
            {
                const Vertex *v = &state->padmarkers[padindex * VIEWPORT_BOX_VERTICES + i];
                state->gizmoposition[0] += v->x / (double)VIEWPORT_BOX_VERTICES;
                state->gizmoposition[1] += v->y / (double)VIEWPORT_BOX_VERTICES;
                state->gizmoposition[2] += v->z / (double)VIEWPORT_BOX_VERTICES;
            }
            state->gizmovisible = TRUE;
            return;
        }
        if (state->tool == EDITOR_TOOL_VERTEX_SELECT && state->selectedobject == VIEWPORT_OBJECT_NONE)
        {
            DWORD count;
            if (ViewportStanSelectionPosition(state, FALSE, state->gizmoposition, &count))
            {
                state->gizmovisible = count >= 2;
                return;
            }
            for (i = 0; i < state->componentcount; i++)
            {
                int corner = state->components[i].corners[0];
                if (!ViewportCornerVisible(state, corner)) { continue; }
                sum[0] += state->scene[corner].x;
                sum[1] += state->scene[corner].y;
                sum[2] += state->scene[corner].z;
                weight++;
            }
            if (weight >= 2)
            {
                for (axis = 0; axis < 3; axis++) { state->gizmoposition[axis] = sum[axis] / weight; }
                state->gizmovisible = TRUE;
            }
            return;
        }
    }

    if (ViewportPadSelectionPosition(state, state->gizmoposition))
    {
        state->gizmovisible = TRUE;
        return;
    }
    {
        DWORD count;
        if (ViewportStanSelectionPosition(state, TRUE, state->gizmoposition, &count))
        {
            state->gizmovisible = TRUE;
            return;
        }
    }
    if (state->scene == NULL || state->tool == EDITOR_TOOL_VERTEX_PAINT) { return; }
    if (state->selectedobject != VIEWPORT_OBJECT_NONE
        && (!state->showobjects || state->sceneobjectindices == NULL)) { return; }
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
    if (state->tool == EDITOR_TOOL_EDGE_SELECT && state->selectedobject == VIEWPORT_OBJECT_NONE)
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
    if (ViewportTriangleHidden(state, triangle)) { return FALSE; }
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
    if (ViewportFindPickedStan(state, &ray, &distance) != STAN_TILE_NONE
        && distance <= length+tolerance) { return FALSE; }
    for (batchindex=0; batchindex<state->batchcount; batchindex++)
    {
        const SceneBatch *batch=&state->batches[batchindex];
        int corner;
        if (!ViewportBatchIsPickable(state,batch) && !(batch->object && state->showobjects)) { continue; }
        for (corner=batch->first; corner<batch->first+batch->count; corner+=3)
        {
            if (ViewportRayBatchTriangleDistance(state, batch, &ray, corner, &distance)
                && distance < length-tolerance) { return FALSE; }
        }
    }
    return TRUE;
}

/* Vertex and edge modes still pick placed models as a whole. Use the same
 * visible-surface test as face mode, so hidden models cannot steal clicks. */
static BOOL ViewportTryPickObject(HWND hwnd, ViewportState *state, int x, int y, BOOL remove)
{
    ViewportPickRay ray;
    double distance, standistance;
    DWORD object;
    BOOL deselect;
    if (state == NULL || state->flying || state->vertexsnap
        || !ViewportBuildPickRay(hwnd, state, x, y, &ray)) { return FALSE; }
    object = ViewportFindPickedObject(state, &ray, &distance);
    if (object == VIEWPORT_OBJECT_NONE) { return FALSE; }
    if (ViewportFindPickedStan(state, &ray, &standistance) != STAN_TILE_NONE
        && standistance <= distance + ViewportCoplanarPickTolerance(distance)) { return FALSE; }
    deselect = remove && state->selectedobject == object;
    ViewportClearAllSelection(state);
    if (!deselect) { ViewportSelectObject(state, object); }
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
    return TRUE;
}

static void ViewportPickComponent(HWND hwnd, ViewportState *state,
                                    int x, int y, BOOL add, BOOL remove)
{
    ViewportPickRay ray;
    double nearest = DBL_MAX, objectdistance, best = 100.0;
    int i, triangle = -1, chosen = -1, endcount, found = -1;
    ViewportComponent component;
    if (ViewportTryPickObject(hwnd, state, x, y, remove)) { return; }
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
            if (ViewportRayBatchTriangleDistance(state, batch, &ray, corner, &distance) && distance < nearest)
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
            if (ViewportTriangleHidden(state, first / 3)) { continue; }
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
        ViewportClearStanSelection(state);
        ViewportClearObjectSelection(state);
        ViewportClearPadSelection(state);
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

static BOOL ViewportStanVisible(const ViewportState *state)
{
    return state->showstan && state->stanopacity > 0 && state->stan.tiles != NULL;
}

static Vertex ViewportStanPointVertex(const StanPoint *point)
{
    Vertex vertex = {0};
    vertex.x = point->x; vertex.y = point->y; vertex.z = point->z;
    return vertex;
}

static int ViewportCompareStanRefs(const void *left, const void *right)
{
    const StanPointRef *a = left, *b = right;
    if (a->tile != b->tile) { return a->tile < b->tile ? -1 : 1; }
    return a->point < b->point ? -1 : a->point > b->point;
}

static StanPointRef ViewportStanPointRef(const ViewportState *state, DWORD tile, DWORD point)
{
    DWORD root = state->stanpointmap[tile * STAN_TILE_MAX_POINTS + point];
    StanPointRef ref = {root / STAN_TILE_MAX_POINTS, root % STAN_TILE_MAX_POINTS};
    return ref;
}

static void ViewportClearStanSelection(ViewportState *state)
{
    if (state->stanselected != NULL) { memset(state->stanselected, 0, state->stan.tilecount); }
    state->stancomponentcount = 0;
    ViewportRefreshStanOverlay(state);
}

DWORD ViewportGetStanSelectionCount(HWND hwnd, DWORD *singletile)
{
    const ViewportState *state = ViewportGetState(hwnd);
    DWORD count = 0, tile;
    if (singletile != NULL) { *singletile = STAN_TILE_NONE; }
    if (state == NULL || !ViewportStanVisible(state)) { return 0; }
    if (state->tool == EDITOR_TOOL_VERTEX_SELECT || state->tool == EDITOR_TOOL_EDGE_SELECT)
    {
        return state->stancomponentcount;
    }
    if (state->tool != EDITOR_TOOL_FACE_SELECT) { return 0; }
    for (tile = 0; tile < state->stan.tilecount; tile++)
    {
        if (state->stanselected[tile])
        {
            count++;
            if (singletile != NULL) { *singletile = tile; }
        }
    }
    if (count != 1 && singletile != NULL) { *singletile = STAN_TILE_NONE; }
    return count;
}

StanPointRef *ViewportGetMoveStanPoints(HWND hwnd, DWORD *countout)
{
    const ViewportState *state = ViewportGetState(hwnd);
    StanPointRef *refs;
    DWORD count = 0, unique = 0, tile, i;
    *countout = 0;
    if (ViewportGetStanSelectionCount(hwnd, NULL) == 0) { return NULL; }
    refs = malloc(((size_t)state->stan.tilecount * STAN_TILE_MAX_POINTS
        + (size_t)state->stancomponentcount * 2) * sizeof(*refs));
    if (refs == NULL) { return NULL; }
    if (state->tool == EDITOR_TOOL_FACE_SELECT)
    {
        for (tile = 0; tile < state->stan.tilecount; tile++)
        {
            if (!state->stanselected[tile]) { continue; }
            for (i = 0; i < state->stan.tiles[tile].pointcount; i++)
            {
                refs[count++] = ViewportStanPointRef(state, tile, i);
            }
        }
    }
    else
    {
        for (i = 0; i < (DWORD)state->stancomponentcount; i++)
        {
            refs[count++] = state->stancomponents[i].refs[0];
            if (state->tool == EDITOR_TOOL_EDGE_SELECT) { refs[count++] = state->stancomponents[i].refs[1]; }
        }
    }
    qsort(refs, count, sizeof(*refs), ViewportCompareStanRefs);
    for (i = 0; i < count; i++)
    {
        if (unique == 0 || ViewportCompareStanRefs(&refs[i], &refs[unique - 1]) != 0)
        {
            refs[unique++] = refs[i];
        }
    }
    *countout = unique;
    return refs;
}

static double ViewportStanTileCenter(const StanTile *tile, double center[3])
{
    double total = 0;
    unsigned int point;
    center[0] = center[1] = center[2] = 0;
    for (point = 1; point + 1 < tile->pointcount; point++)
    {
        const StanPoint *a = &tile->points[0], *b = &tile->points[point], *c = &tile->points[point + 1];
        double abx = (double)b->x-a->x, aby = (double)b->y-a->y, abz = (double)b->z-a->z;
        double acx = (double)c->x-a->x, acy = (double)c->y-a->y, acz = (double)c->z-a->z;
        double nx = aby*acz-abz*acy, ny = abz*acx-abx*acz, nz = abx*acy-aby*acx;
        double area = sqrt(nx*nx+ny*ny+nz*nz);
        center[0] += ((double)a->x+b->x+c->x)*area/3;
        center[1] += ((double)a->y+b->y+c->y)*area/3;
        center[2] += ((double)a->z+b->z+c->z)*area/3;
        total += area;
    }
    if (total > 0)
    {
        center[0] /= total; center[1] /= total; center[2] /= total;
    }
    else
    {
        for (point = 0; point < tile->pointcount; point++)
        {
            center[0] += (double)tile->points[point].x/tile->pointcount;
            center[1] += (double)tile->points[point].y/tile->pointcount;
            center[2] += (double)tile->points[point].z/tile->pointcount;
        }
    }
    return total;
}

static BOOL ViewportStanSelectionPosition(const ViewportState *state, BOOL gizmo,
                                           double position[3], DWORD *countout)
{
    double total = 0;
    DWORD count = 0, tile;
    int i, axis;
    position[0] = position[1] = position[2] = 0;
    if (!ViewportStanVisible(state)) { return FALSE; }
    if (state->tool == EDITOR_TOOL_VERTEX_SELECT || state->tool == EDITOR_TOOL_EDGE_SELECT)
    {
        for (i = 0; i < state->stancomponentcount; i++)
        {
            const StanPointRef *refs = state->stancomponents[i].refs;
            const StanPoint *a = &state->stan.tiles[refs[0].tile].points[refs[0].point];
            const StanPoint *b = state->tool == EDITOR_TOOL_EDGE_SELECT
                ? &state->stan.tiles[refs[1].tile].points[refs[1].point] : a;
            double weight = 1;
            if (gizmo && state->tool == EDITOR_TOOL_EDGE_SELECT)
            {
                double x = (double)a->x-b->x, y = (double)a->y-b->y, z = (double)a->z-b->z;
                weight = sqrt(x*x+y*y+z*z);
            }
            position[0] += ((double)a->x+b->x)*0.5*weight;
            position[1] += ((double)a->y+b->y)*0.5*weight;
            position[2] += ((double)a->z+b->z)*0.5*weight;
            total += weight; count++;
            if (gizmo && state->tool == EDITOR_TOOL_VERTEX_SELECT) { break; }
        }
    }
    else if (state->tool == EDITOR_TOOL_FACE_SELECT)
    {
        for (tile = 0; tile < state->stan.tilecount; tile++)
        {
            double center[3], area, weight;
            if (!state->stanselected[tile]) { continue; }
            area = ViewportStanTileCenter(&state->stan.tiles[tile], center);
            weight = gizmo ? area : 1;
            for (axis = 0; axis < 3; axis++) { position[axis] += center[axis]*weight; }
            total += weight; count++;
        }
    }
    if (!(total > 0)) { return FALSE; }
    for (axis = 0; axis < 3; axis++) { position[axis] /= total; }
    *countout = count;
    return TRUE;
}

static DWORD ViewportFindPickedStan(const ViewportState *state, const ViewportPickRay *ray,
                                     double *distanceout)
{
    DWORD tile, nearest = STAN_TILE_NONE;
    *distanceout = DBL_MAX;
    if (!ViewportStanVisible(state)) { return nearest; }
    for (tile = 0; tile < state->stan.tilecount; tile++)
    {
        const StanTile *polygon = &state->stan.tiles[tile];
        unsigned int point;
        Vertex triangle[3];
        triangle[0] = ViewportStanPointVertex(&polygon->points[0]);
        for (point = 1; point + 1 < polygon->pointcount; point++)
        {
            double distance;
            triangle[1] = ViewportStanPointVertex(&polygon->points[point]);
            triangle[2] = ViewportStanPointVertex(&polygon->points[point + 1]);
            if (ViewportRayTriangleDistance(ray, triangle, FALSE, &distance) && distance < *distanceout)
            {
                *distanceout = distance;
                nearest = tile;
            }
        }
    }
    return nearest;
}

static double ViewportSceneHitDistance(const ViewportState *state, const ViewportPickRay *ray)
{
    double distance;
    ViewportFindVisibleSceneTriangle(state, ray, &distance);
    return distance;
}

static BOOL ViewportStanComponentVisible(const ViewportState *state, const Vertex *point)
{
    ViewportPickRay ray;
    double length, stan, scene;
    ray.origin[0] = state->posx; ray.origin[1] = state->posy; ray.origin[2] = state->posz;
    ray.direction[0] = point->x-state->posx;
    ray.direction[1] = point->y-state->posy;
    ray.direction[2] = point->z-state->posz;
    length = sqrt(ray.direction[0]*ray.direction[0]+ray.direction[1]*ray.direction[1]+ray.direction[2]*ray.direction[2]);
    if (!(length > 0)) { return FALSE; }
    ray.direction[0] /= length; ray.direction[1] /= length; ray.direction[2] /= length;
    ray.mindistance = 0; ray.maxdistance = DBL_MAX;
    scene = ViewportSceneHitDistance(state, &ray);
    ViewportFindPickedStan(state, &ray, &stan);
    return scene >= length-ViewportCoplanarPickTolerance(length)
        && stan >= length-ViewportCoplanarPickTolerance(length);
}

/* Pick visible wire edges, not the empty interiors of large bound pads.
 * Visibility is checked at the perspective-correct point on the edge, using
 * the same scene/stan occlusion test as the other editor overlays. */
static BOOL ViewportTryPickPad(HWND hwnd, ViewportState *state, int x, int y, BOOL remove)
{
    double nearest = DBL_MAX;
    int hit = -1;
    DWORD i;
    float forward[3], right[3];

    if (state->flying || state->tool != EDITOR_TOOL_FACE_SELECT) { return FALSE; }
    ViewportGetBasis(state, forward, right);
    for (i = 0; i < state->padcount; i++)
    {
        int edge;
        if (!ViewportPadVisible(state, i)) { continue; }
        for (edge = 0; edge < VIEWPORT_BOX_VERTICES; edge += 2)
        {
            const Vertex *a = &state->padmarkers[i * VIEWPORT_BOX_VERTICES + edge], *b = a + 1;
            double screen[2], other[2], dx, dy, length, t, deptha, depthb, worldt, distance;
            Vertex point = *a;
            if (!ViewportProject(state, a, screen) || !ViewportProject(state, b, other)) { continue; }
            dx = other[0] - screen[0]; dy = other[1] - screen[1]; length = dx * dx + dy * dy;
            t = length > 0 ? ((x - screen[0]) * dx + (y - screen[1]) * dy) / length : 0;
            if (t < 0) { t = 0; }
            if (t > 1) { t = 1; }
            dx = x - screen[0] - t * dx; dy = y - screen[1] - t * dy;
            if (dx * dx + dy * dy > 36) { continue; }
            deptha = (a->x-state->posx)*forward[0] + (a->y-state->posy)*forward[1] + (a->z-state->posz)*forward[2];
            depthb = (b->x-state->posx)*forward[0] + (b->y-state->posy)*forward[1] + (b->z-state->posz)*forward[2];
            worldt = t * deptha / (t * deptha + (1 - t) * depthb);
            point.x = a->x + (b->x-a->x)*worldt;
            point.y = a->y + (b->y-a->y)*worldt;
            point.z = a->z + (b->z-a->z)*worldt;
            distance = (point.x-state->posx)*(double)(point.x-state->posx)
                     + (point.y-state->posy)*(double)(point.y-state->posy)
                     + (point.z-state->posz)*(double)(point.z-state->posz);
            if (distance < nearest && ViewportStanComponentVisible(state, &point))
            {
                nearest = distance; hit = (int)i;
            }
        }
    }
    if (hit < 0) { return FALSE; }
    {
        BOOL deselect = remove && ViewportSelectedPadIndex(state) == hit;
        ViewportClearAllSelection(state);
        if (!deselect) { state->selectedpad = state->pads[hit].ref; }
    }
    ViewportRefreshPadColors(state);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
    return TRUE;
}

/* Returns TRUE when the visible stan layer owns this click. Polygon edges
 * come from the perimeter, so triangulation diagonals are never selectable. */
static BOOL ViewportTryPickStan(HWND hwnd, ViewportState *state, int x, int y, BOOL add, BOOL remove)
{
    ViewportPickRay ray;
    DWORD tile, hit;
    double distance, nearestscene, best = 100;
    ViewportStanComponent component;
    BOOL found = FALSE, hitstan;
    int index = -1, i, ends = state->tool == EDITOR_TOOL_EDGE_SELECT ? 2 : 1;
    if (state->flying || !ViewportStanVisible(state) || !ViewportBuildPickRay(hwnd, state, x, y, &ray)) { return FALSE; }
    hit = ViewportFindPickedStan(state, &ray, &distance);
    nearestscene = ViewportSceneHitDistance(state, &ray);
    hitstan = hit != STAN_TILE_NONE && distance <= nearestscene+ViewportCoplanarPickTolerance(distance);
    if (state->tool == EDITOR_TOOL_VERTEX_PAINT)
    {
        if (!hitstan) { return FALSE; }
        SendMessage(GetParent(hwnd), VIEWPORT_WM_PAINT_STAN, hit, 0);
        return TRUE;
    }
    if (state->tool == EDITOR_TOOL_FACE_SELECT)
    {
        if (!hitstan) { return FALSE; }
        ViewportClearPadSelection(state);
        ViewportClearBgSelection(state); ViewportClearObjectSelection(state); state->componentcount = 0;
        if (remove) { state->stanselected[hit] = 0; }
        else
        {
            if (!add) { ViewportClearStanSelection(state); }
            state->stanselected[hit] = 1;
        }
    }
    else
    {
        for (tile = 0; tile < state->stan.tilecount; tile++)
        {
            const StanTile *polygon = &state->stan.tiles[tile];
            unsigned int point;
            for (point = 0; point < polygon->pointcount; point++)
            {
                Vertex a = ViewportStanPointVertex(&polygon->points[point]);
                Vertex candidate = a;
                double screen[2], dx, dy, squared;
                if (!ViewportProject(state, &a, screen)) { continue; }
                if (ends == 2)
                {
                    Vertex b = ViewportStanPointVertex(&polygon->points[(point+1)%polygon->pointcount]);
                    double other[2], length, t, da, db, worldt;
                    float forward[3], right[3];
                    if (!ViewportProject(state, &b, other)) { continue; }
                    dx = other[0]-screen[0]; dy = other[1]-screen[1]; length = dx*dx+dy*dy;
                    t = length > 0 ? ((x-screen[0])*dx+(y-screen[1])*dy)/length : 0;
                    if (t < 0) t = 0;
                    if (t > 1) t = 1;
                    screen[0] += t*dx; screen[1] += t*dy;
                    ViewportGetBasis(state, forward, right);
                    da = (a.x-state->posx)*forward[0]+(a.y-state->posy)*forward[1]+(a.z-state->posz)*forward[2];
                    db = (b.x-state->posx)*forward[0]+(b.y-state->posy)*forward[1]+(b.z-state->posz)*forward[2];
                    worldt = t*da/(t*da+(1-t)*db);
                    candidate.x += (b.x-a.x)*worldt; candidate.y += (b.y-a.y)*worldt; candidate.z += (b.z-a.z)*worldt;
                }
                dx = x-screen[0]; dy = y-screen[1]; squared = dx*dx+dy*dy;
                if (squared < best && ViewportStanComponentVisible(state, &candidate))
                {
                    best = squared; found = TRUE;
                    component.refs[0] = ViewportStanPointRef(state, tile, point);
                    component.refs[1] = ViewportStanPointRef(state, tile, (point+1)%polygon->pointcount);
                }
            }
        }
        if (!found)
        {
            if (!hitstan) { return FALSE; }
            if (!add && !remove) { ViewportClearAllSelection(state); }
        }
        else
        {
            if (ends == 2 && ViewportCompareStanRefs(&component.refs[0], &component.refs[1]) > 0)
            {
                StanPointRef swap = component.refs[0]; component.refs[0] = component.refs[1]; component.refs[1] = swap;
            }
            for (i = 0; i < state->stancomponentcount; i++)
            {
                if (!ViewportCompareStanRefs(&component.refs[0], &state->stancomponents[i].refs[0])
                    && (ends == 1 || !ViewportCompareStanRefs(&component.refs[1], &state->stancomponents[i].refs[1]))) { index = i; break; }
            }
            if (remove)
            {
                if (index >= 0)
                {
                    memmove(state->stancomponents+index, state->stancomponents+index+1,
                        (size_t)(state->stancomponentcount-index-1)*sizeof(component));
                    state->stancomponentcount--;
                }
            }
            else
            {
                /* Switching asset types clears the other selection, even with Shift. */
                if (!add) { ViewportClearStanSelection(state); index = -1; }
                if (index < 0)
                {
                    if (state->stancomponentcount == state->stancomponentcapacity)
                    {
                        int capacity = state->stancomponentcapacity ? state->stancomponentcapacity*2 : 32;
                        ViewportStanComponent *grown = realloc(state->stancomponents, (size_t)capacity*sizeof(*grown));
                        if (grown == NULL) { return TRUE; }
                        state->stancomponents = grown; state->stancomponentcapacity = capacity;
                    }
                    state->stancomponents[state->stancomponentcount++] = component;
                }
                ViewportClearBgSelection(state); ViewportClearObjectSelection(state); state->componentcount = 0;
            }
        }
    }
    ViewportRefreshStanOverlay(state);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
    return TRUE;
}

/* Reuse normal vertex picking, including visibility and shared-point identity.
 * Snapshot the source before picking the destination: picking replaces the
 * selection, and committing the edit may rebuild all viewport geometry. */
static void ViewportSnapVertexAt(HWND hwnd, ViewportState *state, int x, int y)
{
    ViewportComponent sourcebg = {0};
    ViewportStanComponent sourcestan = {0};
    double source[3], target[3];
    DWORD count;
    BOOL stan = state->stancomponentcount == 1;
    BOOL pending = state->componentcount + state->stancomponentcount == 1
        && ViewportGetSelectionPosition(hwnd, source, &count);

    if (pending)
    {
        if (stan) { sourcestan = state->stancomponents[0]; }
        else { sourcebg = state->components[0]; }
    }
    ViewportClearAllSelection(state);
    /* Shift/Ctrl cannot extend or subtract from this one-vertex selection. */
    if (!ViewportTryPickStan(hwnd, state, x, y, FALSE, FALSE))
    {
        ViewportPickComponent(hwnd, state, x, y, FALSE, FALSE);
    }
    if (pending && ViewportGetSelectionPosition(hwnd, target, &count))
    {
        ViewportTranslation request;
        int axis;

        ViewportClearAllSelection(state);
        /* The source's allocation still exists, even after picking a vertex
           in the other asset type. Only the source is passed to history. */
        if (stan)
        {
            state->stancomponents[0] = sourcestan;
            state->stancomponentcount = 1;
        }
        else
        {
            state->components[0] = sourcebg;
            state->componentcount = 1;
        }
        for (axis = 0; axis < 3; axis++) { request.offset[axis] = target[axis] - source[axis]; }
        if (SendMessage(GetParent(hwnd), VIEWPORT_WM_SNAP_VERTEX, 0, (LPARAM)&request))
        {
            ViewportClearAllSelection(state);
        }
        /* Cancel or a rejected edit retains the source for another target. */
    }
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
}

/* A marquee holds the click until mouse-up, so a drag never changes the
 * existing selection before it commits (or is cancelled). */
static void ViewportCancelBoxSelection(HWND hwnd, ViewportState *state)
{
    if (state == NULL || !state->boxpending)
    {
        return;
    }
    state->boxpending = state->boxdragging = FALSE;
    if (GetCapture() == hwnd)
    {
        ReleaseCapture();
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static void ViewportBeginBoxSelection(HWND hwnd, ViewportState *state, int x, int y, BOOL add,
                                      BOOL remove)
{
    state->boxstart.x = state->boxend.x = x;
    state->boxstart.y = state->boxend.y = y;
    state->boxadd = add;
    state->boxremove = remove;
    state->boxpending = TRUE;
    state->boxdragging = FALSE;
    state->hoveraxis = -1;
    SetCapture(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void ViewportUpdateBoxSelection(HWND hwnd, ViewportState *state, int x, int y)
{
    int thresholdx = GetSystemMetrics(SM_CXDRAG);
    int thresholdy = GetSystemMetrics(SM_CYDRAG);
    if (thresholdx < 1)
    {
        thresholdx = 1;
    }
    if (thresholdy < 1)
    {
        thresholdy = 1;
    }
    if (abs(x - state->boxstart.x) >= thresholdx || abs(y - state->boxstart.y) >= thresholdy)
    {
        state->boxdragging = TRUE;
    }
    state->boxend.x = x;
    state->boxend.y = y;
    InvalidateRect(hwnd, NULL, FALSE);
}

static RECT ViewportBoxRectangle(const ViewportState *state)
{
    RECT box;
    box.left = min(state->boxstart.x, state->boxend.x);
    box.right = max(state->boxstart.x, state->boxend.x);
    box.top = min(state->boxstart.y, state->boxend.y);
    box.bottom = max(state->boxstart.y, state->boxend.y);
    /* Capture can deliver negative coordinates and releases outside the window. */
    box.left = max(0, box.left);
    box.top = max(0, box.top);
    box.right = min(state->width - 1, box.right);
    box.bottom = min(state->height - 1, box.bottom);
    return box;
}

static BOOL ViewportVertexInBox(const ViewportState *state, const Vertex *vertex, const RECT *box)
{
    double screen[2];
    /* Project performs near/far clipping. Deliberately do not raycast or test
     * face winding: even completely occluded/back-facing vertices qualify. */
    return ViewportProject(state, vertex, screen) && screen[0] >= box->left &&
           screen[0] <= box->right && screen[1] >= box->top && screen[1] <= box->bottom;
}

typedef struct ViewportBoxVertex
{
    DWORD owner, index; /* BG room/vertex, or canonical stan tile/point */
    int corner;         /* visible-layer BG corner used by transform tools */
} ViewportBoxVertex;

static int ViewportCompareBoxVertices(const void *left, const void *right)
{
    const ViewportBoxVertex *a = left, *b = right;
    if (a->owner != b->owner)
    {
        return a->owner < b->owner ? -1 : 1;
    }
    return a->index < b->index ? -1 : a->index > b->index;
}

static BOOL ViewportCollectBoxVertices(const ViewportState *state, const RECT *box, BOOL stan,
                                       ViewportBoxVertex **out, int *countout)
{
    size_t capacity =
        stan ? (size_t)state->stan.tilecount * STAN_TILE_MAX_POINTS : (size_t)state->scenecount;
    ViewportBoxVertex *vertices;
    int count = 0, unique = 0, i;
    *out = NULL;
    *countout = 0;
    if (capacity == 0 || (stan ? !ViewportStanVisible(state) || !state->stanpointmap
                               : !state->scene || !state->scenevertexrefs))
    {
        return TRUE;
    }
    if (capacity > INT_MAX || capacity > SIZE_MAX / sizeof(*vertices))
    {
        return FALSE;
    }
    vertices = malloc(capacity * sizeof(*vertices));
    if (vertices == NULL)
    {
        return FALSE;
    }
    if (stan)
    {
        DWORD tile;
        for (tile = 0; tile < state->stan.tilecount; tile++)
        {
            const StanTile *polygon = &state->stan.tiles[tile];
            unsigned int point;
            for (point = 0; point < polygon->pointcount; point++)
            {
                Vertex vertex = ViewportStanPointVertex(&polygon->points[point]);
                if (ViewportVertexInBox(state, &vertex, box))
                {
                    StanPointRef ref = ViewportStanPointRef(state, tile, point);
                    vertices[count++] = (ViewportBoxVertex){ref.tile, ref.point, 0};
                }
            }
        }
    }
    else
    {
        for (i = 0; i < state->batchcount; i++)
        {
            const SceneBatch *batch = &state->batches[i];
            int corner;
            if (!ViewportBatchIsPickable(state, batch))
            {
                continue;
            }
            for (corner = batch->first; corner < batch->first + batch->count; corner++)
            {
                const BgDocumentVertexRef *ref = &state->scenevertexrefs[corner];
                if (ref->room && !ViewportTriangleHidden(state, corner / 3)
                    && ViewportVertexInBox(state, &state->scene[corner], box))
                {
                    vertices[count++] = (ViewportBoxVertex){ref->room, ref->index, corner};
                }
            }
        }
    }
    qsort(vertices, count, sizeof(*vertices), ViewportCompareBoxVertices);
    for (i = 0; i < count; i++)
    {
        if (unique == 0 || ViewportCompareBoxVertices(&vertices[i], &vertices[unique - 1]))
        {
            vertices[unique++] = vertices[i];
        }
        else if (vertices[i].corner < vertices[unique - 1].corner)
        {
            vertices[unique - 1].corner = vertices[i].corner;
        }
    }
    *out = vertices;
    *countout = unique;
    return TRUE;
}

/* Linear filtering with sorted membership lookups avoids scanning the entire
 * selection for every triangle corner. Shared identities occur only once;
 * distinct vertices at the same position remain distinct. */
static BOOL ViewportApplyBoxVertices(ViewportState *state, const ViewportBoxVertex *hits,
                                     int hitcount, BOOL stan, BOOL add, BOOL remove)
{
    int previous = stan ? state->stancomponentcount : state->componentcount;
    int keep = add || remove ? previous : 0;
    int capacity, count = 0, i;
    ViewportBoxVertex *selected = NULL;
    ViewportComponent *bg = NULL;
    ViewportStanComponent *tiles = NULL;
    if (hitcount == 0 && (add || remove))
    {
        return TRUE;
    }
    if (hitcount > INT_MAX - keep)
    {
        return FALSE;
    }
    capacity = keep + (remove ? 0 : hitcount);
    if (capacity)
    {
        if (stan)
        {
            tiles = calloc((size_t)capacity, sizeof(*tiles));
        }
        else
        {
            bg = calloc((size_t)capacity, sizeof(*bg));
        }
        if (stan ? tiles == NULL : bg == NULL)
        {
            return FALSE;
        }
    }
    if (add && !remove && keep)
    {
        selected = malloc((size_t)keep * sizeof(*selected));
        if (selected == NULL)
        {
            free(bg);
            free(tiles);
            return FALSE;
        }
    }
    for (i = 0; i < keep; i++)
    {
        ViewportBoxVertex key;
        if (stan)
        {
            StanPointRef ref = state->stancomponents[i].refs[0];
            key = (ViewportBoxVertex){ref.tile, ref.point, 0};
        }
        else
        {
            const ViewportComponent *component = &state->components[i];
            key = (ViewportBoxVertex){component->refs[0].room, component->refs[0].index,
                                      component->corners[0]};
        }
        if (selected)
        {
            selected[i] = key;
        }
        if (remove && bsearch(&key, hits, hitcount, sizeof(*hits), ViewportCompareBoxVertices))
        {
            continue;
        }
        if (stan)
        {
            tiles[count] = state->stancomponents[i];
        }
        else
        {
            bg[count] = state->components[i];
        }
        count++;
    }
    if (selected)
    {
        qsort(selected, keep, sizeof(*selected), ViewportCompareBoxVertices);
    }
    if (!remove)
    {
        for (i = 0; i < hitcount; i++)
        {
            if (selected &&
                bsearch(&hits[i], selected, keep, sizeof(*selected), ViewportCompareBoxVertices))
            {
                continue;
            }
            if (stan)
            {
                tiles[count].refs[0] = (StanPointRef){hits[i].owner, hits[i].index};
                tiles[count].refs[1] = tiles[count].refs[0];
            }
            else
            {
                bg[count].refs[0] = state->scenevertexrefs[hits[i].corner];
                bg[count].refs[1] = bg[count].refs[0];
                bg[count].corners[0] = bg[count].corners[1] = hits[i].corner;
            }
            count++;
        }
    }
    free(selected);
    /* Allocate first: cancellation or allocation failure cannot destroy the
     * previous selection. Its insertion order also keeps the original anchor. */
    ViewportClearAllSelection(state);
    if (stan)
    {
        free(state->stancomponents);
        state->stancomponents = tiles;
        state->stancomponentcount = count;
        state->stancomponentcapacity = capacity;
        ViewportRefreshStanOverlay(state);
    }
    else
    {
        free(state->components);
        state->components = bg;
        state->componentcount = count;
        state->componentcapacity = capacity;
    }
    return TRUE;
}

static void ViewportEndBoxSelection(HWND hwnd, ViewportState *state, int x, int y)
{
    BOOL add = state->boxadd, remove = state->boxremove;
    BOOL dragged;
    POINT start = state->boxstart;
    RECT box;
    ViewportUpdateBoxSelection(hwnd, state, x, y);
    box = ViewportBoxRectangle(state);
    dragged = state->boxdragging;
    ViewportCancelBoxSelection(hwnd, state);
    if (!dragged)
    {
        if (!ViewportTryPickStan(hwnd, state, start.x, start.y, add, remove))
        {
            ViewportPickComponent(hwnd, state, start.x, start.y, add, remove);
        }
        return;
    }
    {
        ViewportBoxVertex *hits = NULL;
        int count = 0;
        BOOL stan = ViewportStanVisible(state) && state->stancomponentcount > 0;
        BOOL ok = ViewportCollectBoxVertices(state, &box, stan, &hits, &count);
        /* Component transforms edit one asset type at a time. Keep the current
         * type; with no selection prefer BG, falling back to stan-only hits. */
        if (ok && count == 0 && !stan && state->componentcount == 0 && ViewportStanVisible(state))
        {
            free(hits);
            stan = TRUE;
            ok = ViewportCollectBoxVertices(state, &box, stan, &hits, &count);
        }
        if (ok)
        {
            ok = ViewportApplyBoxVertices(state, hits, count, stan, add, remove);
        }
        free(hits);
        if (!ok)
        {
            MessageBox(hwnd, "Not enough memory to select these vertices.", "GEditor",
                       MB_ICONERROR);
            return;
        }
    }
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
}

static void ViewportDrawBoxSelection(const ViewportState *state)
{
    RECT box;
    if (!state->boxpending || !state->boxdragging || state->width <= 0 || state->height <= 0)
    {
        return;
    }
    box = ViewportBoxRectangle(state);
    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                 GL_LINE_BIT | GL_POLYGON_BIT | GL_TRANSFORM_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LINE_SMOOTH);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, state->width, state->height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(255, 255, 255, 24);
    glBegin(GL_QUADS);
    glVertex2f(box.left, box.top);
    glVertex2f(box.right, box.top);
    glVertex2f(box.right, box.bottom);
    glVertex2f(box.left, box.bottom);
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, 0xAAAA);
    glLineWidth(1.0f);
    glColor3ub(255, 255, 255);
    glBegin(GL_LINE_LOOP);
    glVertex2f(box.left + 0.5f, box.top + 0.5f);
    glVertex2f(box.right + 0.5f, box.top + 0.5f);
    glVertex2f(box.right + 0.5f, box.bottom + 0.5f);
    glVertex2f(box.left + 0.5f, box.bottom + 0.5f);
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

static BgVertex *ViewportLoadHandle(int id, DWORD *count, BOOL radial)
{
    HINSTANCE instance = GetModuleHandle(NULL);
    HRSRC resource = FindResource(instance, MAKEINTRESOURCE(id), RT_RCDATA);
    HGLOBAL loaded = resource ? LoadResource(instance, resource) : NULL;
    BgVertex *vertices;
    const char *reason;
    DWORD i;
    double size = 0;
    if (!loaded)
    {
        return NULL;
    }
    vertices =
        GltfLoadGlbMesh(LockResource(loaded), SizeofResource(instance, resource), count, &reason);
    if (!vertices)
    {
        return NULL;
    }
    for (i = 0; i < *count * 3; i++)
    {
        double extent = radial ? hypot(vertices[i].y, vertices[i].z) : vertices[i].x;
        if (extent > size)
        {
            size = extent;
        }
    }
    if (!(size > 0))
    {
        free(vertices);
        *count = 0;
        return NULL;
    }
    for (i = 0; i < *count * 3; i++)
    {
        vertices[i].x /= size;
        vertices[i].y /= size;
        vertices[i].z /= size;
    }
    return vertices;
}
static void ViewportLoadGizmo(ViewportState *state)
{
    state->arrow = ViewportLoadHandle(IDR_GIZMO_ARROW, &state->arrowtris, FALSE);
    state->cylinder = ViewportLoadHandle(IDR_GIZMO_CYLINDER, &state->cylindertris, TRUE);
    state->scalehandle = ViewportLoadHandle(IDR_GIZMO_SCALE, &state->scalehandletris, FALSE);
}

static double ViewportGizmoScale(const ViewportState *state)
{
    float forward[3], right[3];
    double depth;
    /* Keep the selection's handles visible and sized to the current camera,
       including while flying. Only picking is disabled during navigation. */
    if (!state->gizmovisible || state->height <= 0
        || (state->scalemode && (!state->scalehandle || !state->scalevalid))
        || (state->rotationmode ? state->cylinder == NULL || !state->rotationaxes
            || state->tool == EDITOR_TOOL_VERTEX_PAINT : !state->scalemode && state->arrow == NULL)) { return 0; }
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
    const BgVertex *source = state->rotationmode ? &state->cylinder[index]
        : state->scalemode ? &state->scalehandle[index] : &state->arrow[index];
    double p[3] = {source->x,source->y,source->z};
    if (axis==1) { p[0]=-source->y; p[1]=source->x; }
    if (axis==2) { p[0]=-source->z; p[2]=source->x; }
    if (state->scalemode) { RotationVector(&state->scaleaxes, p, p); }
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
    glDisable(GL_POINT_SMOOTH);
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
    if (ViewportStanVisible(state))
    {
        if (state->tool == EDITOR_TOOL_VERTEX_SELECT)
        {
            DWORD tile;
            glColor4ub(255,255,255,(GLubyte)(state->stanopacity*255/100));
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glPointSize(VIEWPORT_VERTEX_MARKER_SIZE);
            glBegin(GL_POINTS);
            for (tile=0; tile<state->stan.tilecount; tile++)
            {
                unsigned int point;
                for (point=0; point<state->stan.tiles[tile].pointcount; point++)
                {
                    const StanPoint *v=&state->stan.tiles[tile].points[point];
                    glVertex3f(v->x,v->y,v->z);
                }
            }
            glEnd(); glDisable(GL_BLEND);
        }
        if (state->tool == EDITOR_TOOL_VERTEX_SELECT || state->tool == EDITOR_TOOL_EDGE_SELECT)
        {
            glColor3ub(255,210,0); glPointSize(8); glLineWidth(3);
            glBegin(state->tool == EDITOR_TOOL_VERTEX_SELECT ? GL_POINTS : GL_LINES);
            for (i=0; i<state->stancomponentcount; i++)
            {
                int end, ends=state->tool == EDITOR_TOOL_EDGE_SELECT ? 2 : 1;
                for (end=0; end<ends; end++)
                {
                    const StanPointRef *ref=&state->stancomponents[i].refs[end];
                    const StanPoint *v=&state->stan.tiles[ref->tile].points[ref->point];
                    glVertex3f(v->x,v->y,v->z);
                }
            }
            glEnd();
        }
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
            if(state->rotationmode && !(state->rotationaxes & (1u<<axis)))continue;
            if (axis==state->dragaxis || axis==state->hoveraxis) { glColor3ub(255,205,0); }
            else if (axis==0) { glColor3ub(240,40,40); }
            else if (axis==1) { glColor3ub(40,220,60); }
            else { glColor3ub(40,100,255); }
            glBegin(GL_TRIANGLES);
            for (vertex=0; vertex<(state->rotationmode ? state->cylindertris : state->scalemode ? state->scalehandletris : state->arrowtris)*3; vertex++)
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

static int ViewportPickGizmo(HWND hwnd, ViewportState *state, int x, int y)
{
    ViewportPickRay ray;
    double scale=ViewportGizmoScale(state), nearest=DBL_MAX;
    int axis, picked=-1;
    if (state->flying || !(scale>0) || !ViewportBuildPickRay(hwnd,state,x,y,&ray)) { return -1; }
    for (axis=0; axis<3; axis++)
    {
        DWORD tri;
        if(state->rotationmode && !(state->rotationaxes & (1u<<axis)))continue;
        for (tri=0; tri<(state->rotationmode ? state->cylindertris : state->scalemode ? state->scalehandletris : state->arrowtris); tri++)
        {
            Vertex v[3]; double distance; int corner;
            for (corner=0; corner<3; corner++) { ViewportArrowVertex(state,axis,tri*3+corner,scale,&v[corner]); }
            if (ViewportRayTriangleDistance(&ray,v,FALSE,&distance) && distance<nearest)
            {
                int i;nearest=distance; picked=axis;
                for(i=0;i<3;i++)state->gizmohit[i]=ray.origin[i]+ray.direction[i]*distance;
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
    {
        double parameter = 0;
        for (i = 0; i < 3; i++)
        {
            parameter += (ray->origin[i] + ray->direction[i] * numerator / denominator)
                * state->dragdirection[i];
        }
        return parameter;
    }
}

static double ViewportRotationParameter(const ViewportState *state, const ViewportPickRay *ray,
                                        int x, int y)
{
    int axis = state->dragaxis, a = (axis + 1) % 3, b = (axis + 2) % 3;
    double distance;
    if (state->dragvertical)
    {
        return ((x - state->rotationmouse[0]) * state->rotationtangent[0] +
                (y - state->rotationmouse[1]) * state->rotationtangent[1]) *
               180.0 / (90.0 * 3.14159265358979323846);
    }
    if (fabs(ray->direction[axis]) < 1e-5)
    {
        return state->rotationlast;
    }
    distance = (state->dragorigin[axis] - ray->origin[axis]) / ray->direction[axis];
    if (distance <= 0)
    {
        return state->rotationlast;
    }
    return atan2(ray->origin[b] + distance * ray->direction[b] - state->dragorigin[b],
                 ray->origin[a] + distance * ray->direction[a] - state->dragorigin[a]) *
           180.0 / 3.14159265358979323846;
}

static BOOL ViewportBeginTransform(HWND hwnd, ViewportState *state, int x, int y)
{
    ViewportPickRay ray;
    BgDocumentVertexRef *refs = NULL;
    DWORD refcount = 0;
    int axis = ViewportPickGizmo(hwnd, state, x, y), i, vertexcount;
    double length = 0;
    if (axis < 0)
    {
        return FALSE;
    }
    state->dragrotation = state->rotationmode;
    state->dragscaling = state->scalemode;
    state->dragpad = ViewportSelectedPadIndex(state) >= 0;
    state->dragstan = ViewportGetStanSelectionCount(hwnd, NULL) > 0;
    vertexcount = state->dragpad    ? VIEWPORT_BOX_VERTICES
                  : state->dragstan ? (int)(state->stan.tilecount * STAN_TILE_MAX_POINTS)
                                    : state->scenecount;
    state->dragvertices = malloc((size_t)vertexcount * sizeof(*state->dragvertices));
    state->dragmask = calloc((size_t)vertexcount, 1);
    if (state->dragvertices == NULL || state->dragmask == NULL)
    {
        free(state->dragvertices);
        free(state->dragmask);
        state->dragvertices = NULL;
        state->dragmask = NULL;
        return TRUE;
    }
    if (state->dragpad)
    {
        int first = ViewportSelectedPadIndex(state) * VIEWPORT_BOX_VERTICES;
        for (i = 0; i < vertexcount; i++)
        {
            const Vertex *v = &state->padmarkers[first + i];
            state->dragvertices[i][0] = v->x;
            state->dragvertices[i][1] = v->y;
            state->dragvertices[i][2] = v->z;
            state->dragmask[i] = 1;
        }
    }
    else if (state->dragstan)
    {
        StanPointRef *stanrefs = ViewportGetMoveStanPoints(hwnd, &refcount);
        if (stanrefs == NULL)
        {
            free(state->dragvertices);
            free(state->dragmask);
            state->dragvertices = NULL;
            state->dragmask = NULL;
            return TRUE;
        }
        for (i = 0; i < vertexcount; i++)
        {
            DWORD tile = (DWORD)i / STAN_TILE_MAX_POINTS, point = (DWORD)i % STAN_TILE_MAX_POINTS;
            const StanPoint *v = &state->stan.tiles[tile].points[point];
            StanPointRef ref = ViewportStanPointRef(state, tile, point);
            state->dragvertices[i][0] = v->x;
            state->dragvertices[i][1] = v->y;
            state->dragvertices[i][2] = v->z;
            state->dragmask[i] = point < state->stan.tiles[tile].pointcount &&
                                 bsearch(&ref, stanrefs, refcount, sizeof(*stanrefs),
                                         ViewportCompareStanRefs) != NULL;
        }
        free(stanrefs);
    }
    else
    {
        if (state->selectedobject == VIEWPORT_OBJECT_NONE)
        {
            refs = ViewportGetMoveVertices(hwnd, &refcount);
            if (refs == NULL)
            {
                free(state->dragvertices);
                free(state->dragmask);
                state->dragvertices = NULL;
                state->dragmask = NULL;
                return TRUE;
            }
        }
        for (i = 0; i < state->scenecount; i++)
        {
            state->dragvertices[i][0] = state->scene[i].x;
            state->dragvertices[i][1] = state->scene[i].y;
            state->dragvertices[i][2] = state->scene[i].z;
            state->dragmask[i] = state->selectedobject != VIEWPORT_OBJECT_NONE
                                     ? state->sceneobjectindices[i / 3] == state->selectedobject
                                     : bsearch(&state->scenevertexrefs[i], refs, refcount,
                                               sizeof(*refs), ViewportCompareVertexRefs) != NULL;
        }
        free(refs);
    }
    ViewportBuildPickRay(hwnd, state, x, y, &ray);
    state->dragaxis = axis;
    state->hoveraxis = axis;
    state->dragdelta = 0;
    state->dragscale = ViewportGizmoScale(state);
    {
        double dot = 0;
        for (i = 0; i < 3; i++)
        {
            state->dragdirection[i] = state->dragscaling ? state->scaleaxes.m[i][axis] : i == axis;
            dot += ray.direction[i] * state->dragdirection[i];
        }
        for (i = 0; i < 3; i++)
        {
            state->dragorigin[i] = state->gizmoposition[i];
            state->dragplane[i] = ray.direction[i] - dot * state->dragdirection[i];
            length += state->dragplane[i] * state->dragplane[i];
        }
    }
    state->dragvertical = length < 0.0025;
    state->dragparameter = ViewportDragParameter(state, &ray, y);
    if (state->dragrotation)
    {
        int a = (axis + 1) % 3, b = (axis + 2) % 3;
        double tangent[3] = {0}, screen[2], other[2], norm;
        Vertex point = {0}, end = {0};
        state->dragvertical =
            fabs(ray.direction[axis]) < 0.15 ||
            hypot(state->gizmohit[a] - state->dragorigin[a],
                  state->gizmohit[b] - state->dragorigin[b]) < state->dragscale * .05;
        state->rotationmouse[0] = x;
        state->rotationmouse[1] = y;
        tangent[a] = -(state->gizmohit[b] - state->dragorigin[b]);
        tangent[b] = state->gizmohit[a] - state->dragorigin[a];
        point.x = state->gizmohit[0];
        point.y = state->gizmohit[1];
        point.z = state->gizmohit[2];
        end.x = point.x + tangent[0];
        end.y = point.y + tangent[1];
        end.z = point.z + tangent[2];
        state->rotationtangent[0] = 1;
        state->rotationtangent[1] = 0;
        if (ViewportProject(state, &point, screen) && ViewportProject(state, &end, other) &&
            (norm = hypot(other[0] - screen[0], other[1] - screen[1])) > 1e-5)
        {
            state->rotationtangent[0] = (other[0] - screen[0]) / norm;
            state->rotationtangent[1] = (other[1] - screen[1]) / norm;
        }
        state->rotationtotal = 0;
        state->rotationlast = ViewportRotationParameter(state, &ray, x, y);
    }
    SetCapture(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
    return TRUE;
}

static void ViewportDragTransform(HWND hwnd, ViewportState *state, int x, int y)
{
    ViewportPickRay ray;
    double delta;
    int i;
    Rotation rotation;
    Scaling scale;
    if (!ViewportBuildPickRay(hwnd, state, x, y, &ray))
    {
        return;
    }
    /* Snap the displacement, preserving the selection's relative shape and
       any authored fractional origin. Calculate from press, never last tick. */
    if (state->dragrotation)
    {
        double angle = ViewportRotationParameter(state, &ray, x, y);
        state->rotationtotal += state->dragvertical ? angle - state->rotationlast
                                                    : remainder(angle - state->rotationlast, 360.0);
        state->rotationlast = angle;
        delta = round(state->rotationtotal);
        RotationAxis(&rotation, state->dragaxis, delta);
    }
    else if (state->dragscaling)
    {
        double movement = ViewportDragParameter(state, &ray, y) - state->dragparameter;
        /* A handle-length drag doubles the selected dimension. One-percent
           drag steps never cross zero or reflect/collapse the selection. */
        delta = fmax(-.99, round(movement / state->dragscale * 100.0) / 100.0);
        scale.axes = state->scaleaxes;
        memcpy(scale.pivot, state->dragorigin, sizeof(scale.pivot));
        for (i = 0; i < 3; i++) { scale.factor[i] = i == state->dragaxis ? 1 + delta : 1; }
    }
    else
    {
        delta = round(ViewportDragParameter(state, &ray, y) - state->dragparameter);
    }
    if (!isfinite(delta) || fabs(delta) > 1000000 || delta == state->dragdelta)
    {
        return;
    }
    state->dragdelta = delta;
    for (i = 0; i < (state->dragpad    ? VIEWPORT_BOX_VERTICES
                     : state->dragstan ? (int)(state->stan.tilecount * STAN_TILE_MAX_POINTS)
                                       : state->scenecount);
         i++)
    {
        if (!state->dragmask[i])
        {
            continue;
        }
        if (state->dragrotation || state->dragscaling)
        {
            double source[3] = {state->dragvertices[i][0], state->dragvertices[i][1],
                                state->dragvertices[i][2]},
                   point[3];
            if (state->dragscaling) { ScalingPoint(&scale, source, point); }
            else { RotationPoint(&rotation, state->dragorigin, source, point); }
            if (state->dragpad)
            {
                Vertex *v =
                    &state->padmarkers[ViewportSelectedPadIndex(state) * VIEWPORT_BOX_VERTICES + i];
                v->x = point[0];
                v->y = point[1];
                v->z = point[2];
            }
            else if (state->dragstan)
            {
                StanPoint *v =
                    &state->stan.tiles[i / STAN_TILE_MAX_POINTS].points[i % STAN_TILE_MAX_POINTS];
                v->x = point[0];
                v->y = point[1];
                v->z = point[2];
            }
            else
            {
                state->scene[i].x = point[0];
                state->scene[i].y = point[1];
                state->scene[i].z = point[2];
            }
        }
        else if (state->dragpad)
        {
            Vertex *point =
                &state->padmarkers[ViewportSelectedPadIndex(state) * VIEWPORT_BOX_VERTICES + i];
            point->x = state->dragvertices[i][0] + (state->dragaxis == 0 ? delta : 0);
            point->y = state->dragvertices[i][1] + (state->dragaxis == 1 ? delta : 0);
            point->z = state->dragvertices[i][2] + (state->dragaxis == 2 ? delta : 0);
        }
        else if (state->dragstan)
        {
            StanPoint *point =
                &state->stan.tiles[i / STAN_TILE_MAX_POINTS].points[i % STAN_TILE_MAX_POINTS];
            point->x = state->dragvertices[i][0] + (state->dragaxis == 0 ? delta : 0);
            point->y = state->dragvertices[i][1] + (state->dragaxis == 1 ? delta : 0);
            point->z = state->dragvertices[i][2] + (state->dragaxis == 2 ? delta : 0);
        }
        else
        {
            state->scene[i].x = state->dragvertices[i][0] + (state->dragaxis == 0 ? delta : 0);
            state->scene[i].y = state->dragvertices[i][1] + (state->dragaxis == 1 ? delta : 0);
            state->scene[i].z = state->dragvertices[i][2] + (state->dragaxis == 2 ? delta : 0);
        }
    }
    if (!state->dragrotation && !state->dragscaling)
    {
        state->gizmoposition[state->dragaxis] = state->dragorigin[state->dragaxis] + delta;
    }
    if (state->dragstan)
    {
        ViewportRefreshStanOverlay(state);
    }
    ViewportBuildObjectSelectionBox(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_TRANSFORM_PREVIEW, 0, 0);
}

void ViewportCancelTransform(HWND hwnd)
{
    ViewportState *state = ViewportGetState(hwnd);
    int i;
    ViewportCancelBoxSelection(hwnd, state);
    if (state == NULL || state->dragaxis < 0)
    {
        return;
    }
    for (i = 0; i < (state->dragpad    ? VIEWPORT_BOX_VERTICES
                     : state->dragstan ? (int)(state->stan.tilecount * STAN_TILE_MAX_POINTS)
                                       : state->scenecount);
         i++)
    {
        if (!state->dragmask[i])
        {
            continue;
        }
        if (state->dragpad)
        {
            Vertex *point =
                &state->padmarkers[ViewportSelectedPadIndex(state) * VIEWPORT_BOX_VERTICES + i];
            point->x = state->dragvertices[i][0];
            point->y = state->dragvertices[i][1];
            point->z = state->dragvertices[i][2];
        }
        else if (state->dragstan)
        {
            StanPoint *point =
                &state->stan.tiles[i / STAN_TILE_MAX_POINTS].points[i % STAN_TILE_MAX_POINTS];
            point->x = state->dragvertices[i][0];
            point->y = state->dragvertices[i][1];
            point->z = state->dragvertices[i][2];
        }
        else
        {
            state->scene[i].x = state->dragvertices[i][0];
            state->scene[i].y = state->dragvertices[i][1];
            state->scene[i].z = state->dragvertices[i][2];
        }
    }
    state->dragaxis = -1;
    free(state->dragvertices);
    free(state->dragmask);
    state->dragvertices = NULL;
    state->dragmask = NULL;
    if (GetCapture() == hwnd)
    {
        ReleaseCapture();
    }
    if (state->dragstan)
    {
        ViewportRefreshStanOverlay(state);
    }
    ViewportBuildObjectSelectionBox(state);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_TRANSFORM_PREVIEW, 0, 0);
}

static void ViewportEndTransform(HWND hwnd, ViewportState *state)
{
    ViewportTranslation request;
    if (state == NULL || state->dragaxis < 0)
    {
        return;
    }
    if (state->dragscaling)
    {
        Scaling scale;
        int axis;
        double delta = state->dragdelta;
        scale.axes = state->scaleaxes;
        memcpy(scale.pivot, state->dragorigin, sizeof(scale.pivot));
        for (axis = 0; axis < 3; axis++) { scale.factor[axis] = axis == state->dragaxis ? 1 + delta : 1; }
        ViewportCancelTransform(hwnd);
        if (delta != 0)
        {
            SendMessage(GetParent(hwnd), VIEWPORT_WM_SCALE_SELECTION, 0, (LPARAM)&scale);
        }
        return;
    }
    if (state->dragrotation)
    {
        ViewportRotation rotation;
        double angle = state->dragdelta;
        RotationAxis(&rotation.rotation, state->dragaxis, angle);
        memcpy(rotation.pivot, state->dragorigin, sizeof(rotation.pivot));
        ViewportCancelTransform(hwnd);
        if (angle != 0)
        {
            SendMessage(GetParent(hwnd), VIEWPORT_WM_ROTATE_SELECTION, 0, (LPARAM)&rotation);
        }
        return;
    }
    ZeroMemory(&request, sizeof(request));
    request.offset[state->dragaxis] = state->dragdelta;
    ViewportCancelTransform(hwnd);
    if (request.offset[0] != 0 || request.offset[1] != 0 || request.offset[2] != 0)
    {
        SendMessage(GetParent(hwnd), VIEWPORT_WM_TRANSLATE_SELECTION, 0, (LPARAM)&request);
    }
}

static void ViewportUpdateOrbit(ViewportState *state)
{
    double position[3];
    OrbitCameraPosition(&state->orbitcamera, position);
    state->posx = (float)position[0];
    state->posy = (float)position[1];
    state->posz = (float)position[2];
    state->yaw = (float)state->orbitcamera.yaw;
    state->pitch = (float)state->orbitcamera.pitch;
    ViewportResizeGL(state, state->width, state->height);
}

/* Consume model-viewer input before level picking, transforms, or flight. */
static BOOL ViewportOrbitInput(HWND hwnd, ViewportState *state,
                               UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        SetFocus(hwnd);
        SetCapture(hwnd);
        state->orbitbuttons |= msg == WM_LBUTTONDOWN ? MK_LBUTTON : MK_RBUTTON;
        state->lastmouse.x = GET_X_LPARAM(lparam);
        state->lastmouse.y = GET_Y_LPARAM(lparam);
        return TRUE;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        state->orbitbuttons &= ~(msg == WM_LBUTTONUP ? MK_LBUTTON : MK_RBUTTON);
        if (state->orbitbuttons == 0 && GetCapture() == hwnd) { ReleaseCapture(); }
        return TRUE;
    case WM_MOUSEMOVE:
        if (state->orbitbuttons != 0)
        {
            int x = GET_X_LPARAM(lparam), y = GET_Y_LPARAM(lparam);
            OrbitCameraRotate(&state->orbitcamera, x - state->lastmouse.x, y - state->lastmouse.y);
            state->lastmouse.x = x; state->lastmouse.y = y;
            ViewportUpdateOrbit(state);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return TRUE;
    case WM_MOUSEWHEEL:
        OrbitCameraDolly(&state->orbitcamera, (double)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA);
        ViewportUpdateOrbit(state);
        InvalidateRect(hwnd, NULL, FALSE);
        return TRUE;
    case WM_CANCELMODE:
    case WM_CAPTURECHANGED:
    case WM_KILLFOCUS:
        state->orbitbuttons = 0;
        if (GetCapture() == hwnd) { ReleaseCapture(); }
        return TRUE;
    case WM_KEYDOWN:
    case WM_KEYUP:
        return TRUE;
    }
    return FALSE;
}

static LRESULT CALLBACK ViewportWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ViewportState *state = ViewportGetState(hwnd);

    if (state != NULL && state->orbit && ViewportOrbitInput(hwnd, state, msg, wparam, lparam))
    { return 0; }

    switch (msg)
    {
    case WM_CREATE:
        state = (ViewportState *)calloc(1, sizeof(*state));

        if (state == NULL)
        {
            return -1; /* abort window creation */
        }

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);

        ViewportSetBackgroundColor(hwnd, NULL);

        state->orbit = ((CREATESTRUCT *)lparam)->lpCreateParams != NULL;

        if (state->orbit)
        {
            const double min[3] = { -1, -1, -1 }, max[3] = { 1, 1, 1 };
            OrbitCameraFrame(&state->orbitcamera, min, max, 1, VIEWPORT_FOV_Y);
        }

        state->speed = VIEWPORT_FLY_SPEED;
        state->posz = 600.0f;
        state->showbgprimary = TRUE;
        state->showbgsecondary = TRUE;
        state->showstan = FALSE;
        state->stanopacity = 44;
        state->showportals = FALSE;
        state->showobjects = TRUE;
        state->cullbackfaces = TRUE;
        state->showbgstatistics = !state->orbit;
        state->showfog = !state->orbit;
        state->selectedobject = VIEWPORT_OBJECT_NONE;
        state->selectedpad.index = SETUP_PAD_INDEX_NONE;
        state->hoveraxis = state->dragaxis = -1;
        if (!state->orbit) { ViewportLoadGizmo(state); }
        state->tool = EDITOR_TOOL_FACE_SELECT;

        if (!ViewportInitGL(hwnd, state))
        {
            MessageBox(hwnd, "Could not initialize the OpenGL viewport.", "GEditor", MB_ICONERROR);
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
        if (state != NULL && !state->flying && state->vertexsnap)
        {
            ViewportSnapVertexAt(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        }
        if (state != NULL && !state->flying
            && ViewportBeginTransform(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)))
        {
            return 0;
        }
        if (state != NULL && !state->flying && state->tool == EDITOR_TOOL_VERTEX_SELECT)
        {
            ViewportBeginBoxSelection(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam),
                                      (wparam & MK_SHIFT) != 0, (wparam & MK_CONTROL) != 0);
            return 0;
        }
        if (state != NULL && ViewportTryPickPad(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam),
                (wparam & MK_CONTROL)!=0)) { return 0; }
        if (state != NULL && ViewportTryPickStan(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam),
                (wparam & MK_SHIFT)!=0,(wparam & MK_CONTROL)!=0)) { return 0; }
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
        if (state != NULL && state->boxpending)
        {
            ViewportEndBoxSelection(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        }
        else if (state != NULL && state->dragaxis >= 0)
        {
            ViewportDragTransform(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
            ViewportEndTransform(hwnd,state);
            state->hoveraxis=ViewportPickGizmo(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
            InvalidateRect(hwnd,NULL,FALSE);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (state != NULL && state->boxpending)
        {
            ViewportUpdateBoxSelection(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        }
        else if (state != NULL && state->dragaxis >= 0)
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
        if (state != NULL && (state->dragaxis >= 0 || state->boxpending))
        {
            if (wparam == VK_ESCAPE) { ViewportCancelTransform(hwnd); }
            return 0;
        }
        if (wparam == VK_DELETE && state != NULL
            && (state->tool == EDITOR_TOOL_FACE_SELECT
                || ((state->tool == EDITOR_TOOL_VERTEX_SELECT || state->tool == EDITOR_TOOL_EDGE_SELECT)
                    && state->selectedobject != VIEWPORT_OBJECT_NONE)))
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

    case WM_CANCELMODE:
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
            ViewportSetStanTiles(hwnd, NULL);
            free(state->stancomponents);
            ViewportFreeScene(state);
        }
        ViewportEndFly(hwnd, state); // Never leave the cursor hidden.
        if (state != NULL)
        {
            if (state->statisticsfont != 0)
            {
                wglMakeCurrent(state->hdc, state->hglrc);
                glDeleteLists(state->statisticsfont, VIEWPORT_STATS_FONT_GLYPHS);
            }
            wglMakeCurrent(NULL, NULL);
            if (state->hglrc != NULL)
            {
                wglDeleteContext(state->hglrc);
            }
            free(state->cylinder);
            free(state->scalehandle);
            {
                int kind;
                for (kind = 0; kind < SETUP_MARKER_KIND_COUNT; kind++) { free(state->markermodels[kind]); }
            }
            free(state->arrow);
            free(state->components);
            free(state->hiddenrefs);
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


HWND ViewportCreateOrbit(HWND parent, HINSTANCE hinstance)
{
    return CreateWindowEx(0, VIEWPORT_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                          0, 0, 16, 16, parent, NULL, hinstance, (void *)1);
}


void ViewportRedraw(HWND viewport)
{
    /* FALSE: no GDI erase - WM_ERASEBKGND is suppressed anyway. */
    InvalidateRect(viewport, NULL, FALSE);
}


void ViewportSetBackgroundColor(HWND viewport, const unsigned char rgb[3])
{
    static const float fallback[3] = { 0.15f, 0.15f, 0.18f };
    ViewportState *state = ViewportGetState(viewport);
    int axis;

    if (state == NULL) 
    { 
        return; 
    }

    for (axis = 0; axis < 3; axis++)
    {
        state->backgroundcolor[axis] = rgb != NULL ? rgb[axis] / 255.0f : fallback[axis];
    }

    ViewportRedraw(viewport);
}

void ViewportSetLevelFog(HWND viewport, const RomFog *fog, float renderscale)
{
    ViewportState *state = ViewportGetState(viewport);
    if (state == NULL) { return; }
    state->levelfog = FALSE;
    ZeroMemory(&state->fog, sizeof(state->fog));
    if (fog != NULL && fog->enabled && !state->orbit)
    {
        state->levelfog = FogConfigure(&state->fog, fog->nearclip, fog->farclip,
                                      renderscale, fog->start, fog->end);
    }
    ViewportRedraw(viewport);
}

BOOL ViewportGetFogVisible(HWND viewport)
{
    ViewportState *state = ViewportGetState(viewport);
    return state != NULL && state->showfog;
}

void ViewportSetFogVisible(HWND viewport, BOOL visible)
{
    ViewportState *state = ViewportGetState(viewport);
    if (state == NULL || state->orbit) { return; }
    state->showfog = visible != FALSE;
    ViewportRedraw(viewport);
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
    state->vertexsnap = FALSE;
    /* Clear the previous tool's selection before starting a new one. */
    ViewportCancelTransform(viewport);
    ViewportClearAllSelection(state);
    ViewportRedraw(viewport);
    SendMessage(GetParent(viewport), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
}


BOOL ViewportGetVertexSnap(HWND viewport)
{
    const ViewportState *state = ViewportGetState(viewport);
    return state != NULL && state->vertexsnap;
}

void ViewportSetVertexSnap(HWND viewport, BOOL enabled)
{
    ViewportState *state = ViewportGetState(viewport);

    if (state == NULL) { return; }
    enabled = enabled && state->tool == EDITOR_TOOL_VERTEX_SELECT;
    if (state->vertexsnap == enabled) { return; }
    ViewportCancelTransform(viewport);
    state->vertexsnap = enabled;
    /* Starting and ending a snap session both discard the pending source. */
    ViewportClearAllSelection(state);
    ViewportRedraw(viewport);
    SendMessage(GetParent(viewport), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
}

/* CPU alpha copies share the lifetime of the GL textures. */
static void ViewportFreeTextureCache(ViewportTexture *cache)
{
    int id;
    if (!cache)
    {
        return;
    }
    for (id = 0; id < VIEWPORT_TEXTURE_VARIANT_COUNT; id++)
    {
        free(cache[id].alpha);
    }
    free(cache);
}

/* Releases GL textures, geometry and editor overlays in the scene context. */
static void ViewportFreeScene(struct ViewportState *state_)
{
    ViewportState *state = (ViewportState *)state_;

    if (state->texturecount > 0)
    {
        wglMakeCurrent(state->hdc, state->hglrc);
        glDeleteTextures(state->texturecount, state->textures);
    }

    ViewportFreeTextureCache(state->texturecache);
    state->texturecache = NULL;
    free(state->textures);
    free(state->batches);
    free(state->selectedtris);
    free(state->hiddentris);
    free(state->scenefacerefs);
    free(state->sceneobjectindices);
    free(state->scenevertexrefs);
    free(state->scenecolors);
    free(state->portaledges);
    free(state->portalfill);
    free(state->setupmarkers);
    state->setupmarkers = NULL;
    state->setupmarkercount = 0;
    SetupSwirlPathFree(&state->swirlpath);
    free(state->padmarkers);
    free(state->pads);
    free(state->scene);
    state->textures = NULL;
    state->batches = NULL;
    state->selectedtris = NULL;
    state->hiddentris = NULL;
    state->bghiddentris = 0;
    state->scenefacerefs = NULL;
    state->sceneobjectindices = NULL;
    state->scenevertexrefs = NULL;
    state->scenecolors = NULL;
    state->portaledges = NULL;
    state->portalfill = NULL;
    state->padmarkers = NULL;
    state->pads = NULL;
    state->padcount = 0;
    state->selectedpad.index = SETUP_PAD_INDEX_NONE;
    state->scene = NULL;
    state->texturecount = 0;
    state->batchcount = 0;
    state->selectedtricount = 0;
    state->selectedobject = VIEWPORT_OBJECT_NONE;
    state->objectselectionboxcount = 0;
    state->portaledgecount = 0;
    state->portalfillcount = 0;
    state->padmarkercount = 0;
    state->scenecount = 0;
    state->bgprimarytris = state->bgsecondarytris = state->bgtexturecount = 0;
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


static void ViewportRefreshStanOverlay(ViewportState *state)
{
    DWORD tile;
    size_t fillat=0, edgeat=0;
    unsigned char alpha=(unsigned char)(state->stanopacity*255/100);
    if (state->stanfill == NULL || state->stanedges == NULL) { return; }
    for (tile=0; tile<state->stan.tilecount; tile++)
    {
        StanTile color=state->stan.tiles[tile];
        unsigned int point;
        if (state->stanselected[tile]) { color.red=0; color.green=255; color.blue=255; }
        for (point=1; point+1<color.pointcount; point++)
        {
            ViewportSetStanVertex(&state->stanfill[fillat++], &color.points[0], &color, alpha);
            ViewportSetStanVertex(&state->stanfill[fillat++], &color.points[point], &color, alpha);
            ViewportSetStanVertex(&state->stanfill[fillat++], &color.points[point+1], &color, alpha);
        }
        if (state->tool == EDITOR_TOOL_VERTEX_SELECT || state->tool == EDITOR_TOOL_EDGE_SELECT)
        {
            color.red=color.green=color.blue=255;
        }
        for (point=0; point<color.pointcount; point++)
        {
            ViewportSetStanVertex(&state->stanedges[edgeat++], &color.points[point], &color, alpha);
            ViewportSetStanVertex(&state->stanedges[edgeat++], &color.points[(point+1)%color.pointcount], &color, alpha);
        }
    }
}

BOOL ViewportSetStanTiles(HWND hwnd, const StanFile *stan)
{
    ViewportState *state=ViewportGetState(hwnd);
    StanFile copy={0};
    Vertex *fill=NULL, *edges=NULL;
    DWORD *map=NULL;
    unsigned char *selected=NULL;
    size_t fillcount=0, edgecount=0;
    DWORD tile;
    BOOL keep=FALSE;
    const char *reason;
    if (state == NULL) { return FALSE; }
    ViewportCancelTransform(hwnd);
    if (stan != NULL && stan->tiles != NULL && stan->tilecount > 0)
    {
        if (!StanFileClone(stan,&copy,&reason)) { return FALSE; }
        map=StanBuildPointMap(&copy,&reason);
        selected=calloc(copy.tilecount,1);
        for (tile=0; tile<copy.tilecount; tile++)
        {
            fillcount+=(copy.tiles[tile].pointcount-2)*3;
            edgecount+=copy.tiles[tile].pointcount*2;
        }
        fill=malloc(fillcount*sizeof(*fill)); edges=malloc(edgecount*sizeof(*edges));
        if (map==NULL || selected==NULL || fill==NULL || edges==NULL)
        {
            free(map); free(selected); free(fill); free(edges); StanFileFree(&copy);
            return FALSE;
        }
        keep=state->stan.tilecount==copy.tilecount && strcmp(state->stan.name,copy.name)==0;
        if (keep) { memcpy(selected,state->stanselected,copy.tilecount); }
    }
    StanFileFree(&state->stan); free(state->stanpointmap); free(state->stanselected);
    free(state->stanfill); free(state->stanedges);
    state->stan=copy; state->stanpointmap=map; state->stanselected=selected;
    state->stanfill=fill; state->stanedges=edges;
    state->stanfillcount=(GLsizei)fillcount; state->stanedgecount=(GLsizei)edgecount;
    if (!keep) { state->stancomponentcount=0; }
    else
    {
        int i, kept=0, ends=state->tool == EDITOR_TOOL_EDGE_SELECT ? 2 : 1;
        for (i=0; i<state->stancomponentcount; i++)
        {
            ViewportStanComponent component=state->stancomponents[i];
            int end, previous;
            for (end=0; end<ends; end++)
            {
                StanPointRef *ref=&component.refs[end];
                if (ref->tile>=copy.tilecount || ref->point>=copy.tiles[ref->tile].pointcount) { break; }
                *ref=ViewportStanPointRef(state,ref->tile,ref->point);
            }
            if (end<ends) { continue; }
            if (ends==2 && ViewportCompareStanRefs(&component.refs[0],&component.refs[1])>0)
            {
                StanPointRef swap=component.refs[0]; component.refs[0]=component.refs[1]; component.refs[1]=swap;
            }
            for (previous=0; previous<kept; previous++)
            {
                if (!ViewportCompareStanRefs(&component.refs[0],&state->stancomponents[previous].refs[0])
                    && (ends==1 || !ViewportCompareStanRefs(&component.refs[1],&state->stancomponents[previous].refs[1]))) { break; }
            }
            if (previous==kept) { state->stancomponents[kept++]=component; }
        }
        state->stancomponentcount=kept;
    }
    ViewportRefreshStanOverlay(state);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd,NULL,FALSE);
    return TRUE;
}

void ViewportSetStanOpacity(HWND hwnd, int percent)
{
    ViewportState *state=ViewportGetState(hwnd);
    if (state == NULL) { return; }
    if (percent < 0) percent=0;
    if (percent > 100) percent=100;
    ViewportCancelTransform(hwnd);
    state->stanopacity=percent;
    if (percent==0) { ViewportClearStanSelection(state); }
    ViewportRefreshStanOverlay(state);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd,NULL,FALSE);
    SendMessage(GetParent(hwnd),VIEWPORT_WM_SELECTION_CHANGED,0,0);
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
    if (!stan) { ViewportClearStanSelection(state); }
    state->showportals = portals;
    state->showobjects = objects;
    if (!objects) { state->selectedobject = VIEWPORT_OBJECT_NONE; }
    ViewportBuildObjectSelectionBox(state);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
}

/* Keep room-layer passes ordered; never texture-sort background decals. */
typedef struct TriKey { unsigned short tag; int tri; } TriKey;

static int ViewportTriKeyCompare(const void *a, const void *b)
{
    const TriKey *ka = (const TriKey *)a;
    const TriKey *kb = (const TriKey *)b;
    int d = (int)BG_TRI_IS_SECONDARY(ka->tag) - (int)BG_TRI_IS_SECONDARY(kb->tag);
    if (!d)
    {
        d = (int)BG_TRI_IS_OBJECT(ka->tag) - (int)BG_TRI_IS_OBJECT(kb->tag);
    }
    /* Both backgrounds and models contain ordered, coplanar decals.
     * Only batch adjacent matching materials; texture sorting changes which
     * surface is visible (and therefore which surface picking should hit). */
    return d ? d : ka->tri - kb->tri;
}

/* Stable IDs let a geometry rebuild keep surviving selected faces even
   when their draw-ordered display order changes. */
static int ViewportCompareFaceRefs(const void *left, const void *right)
{
    const BgFaceRef *a = (const BgFaceRef *)left;
    const BgFaceRef *b = (const BgFaceRef *)right;

    if (a->room != b->room) { return a->room < b->room ? -1 : 1; }
    if (a->layer != b->layer) { return a->layer < b->layer ? -1 : 1; }
    if (a->faceid != b->faceid) { return a->faceid < b->faceid ? -1 : 1; }
    return 0;
}

/* Rebuild only the display mask. Keep absent IDs until Unhide All or a level
   change, so undoing a geometry edit also restores its temporary visibility. */
static void ViewportRestoreHiddenFaces(ViewportState *state, BOOL reset)
{
    int triangle;
    if (reset)
    {
        free(state->hiddenrefs);
        state->hiddenrefs = NULL;
        state->hiddenrefcount = 0;
    }
    state->bghiddentris = 0;
    if (state->hiddentris == NULL) { return; }
    memset(state->hiddentris, 0, (size_t)state->scenecount / 3);
    if (state->scenefacerefs == NULL || state->hiddenrefcount == 0) { return; }
    for (triangle = 0; triangle < state->scenecount / 3; triangle++)
    {
        const BgFaceRef *ref = &state->scenefacerefs[triangle];
        if (ref->faceid == BG_FACE_ID_NONE
            || (state->sceneobjectindices != NULL && state->sceneobjectindices[triangle] != VIEWPORT_OBJECT_NONE))
        { continue; }
        if (bsearch(ref, state->hiddenrefs, (size_t)state->hiddenrefcount,
                    sizeof(*state->hiddenrefs), ViewportCompareFaceRefs) != NULL)
        {
            state->hiddentris[triangle] = 1;
            state->bghiddentris++;
        }
    }
}

BOOL ViewportHideSelectedBgFaces(HWND hwnd)
{
    ViewportState *state = ViewportGetState(hwnd);
    BgFaceRef *refs;
    int count, total, i, unique = 0;
    if (state == NULL || state->tool != EDITOR_TOOL_FACE_SELECT) { return TRUE; }
    ViewportCancelTransform(hwnd);
    count = ViewportGetSelectedBgFaceCount(hwnd);
    if (count == 0) { return TRUE; }
    if (state->hiddenrefcount > INT_MAX - count) { return FALSE; }
    total = state->hiddenrefcount + count;
    if ((size_t)total > SIZE_MAX / sizeof(*refs)) { return FALSE; }
    refs = malloc((size_t)total * sizeof(*refs));
    if (refs == NULL) { return FALSE; }
    if (state->hiddenrefcount > 0)
    { memcpy(refs, state->hiddenrefs, (size_t)state->hiddenrefcount * sizeof(*refs)); }
    if (!ViewportGetSelectedBgFaces(hwnd, refs + state->hiddenrefcount, count))
    { free(refs); return FALSE; }
    qsort(refs, (size_t)total, sizeof(*refs), ViewportCompareFaceRefs);
    for (i = 0; i < total; i++)
    {
        if (unique == 0 || ViewportCompareFaceRefs(&refs[i], &refs[unique - 1]))
        { refs[unique++] = refs[i]; }
    }
    free(state->hiddenrefs);
    state->hiddenrefs = refs;
    state->hiddenrefcount = unique;
    ViewportClearAllSelection(state);
    ViewportRestoreHiddenFaces(state, FALSE);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
    return TRUE;
}

void ViewportUnhideAllBgFaces(HWND hwnd)
{
    ViewportState *state = ViewportGetState(hwnd);
    if (state == NULL) { return; }
    ViewportCancelTransform(hwnd);
    ViewportRestoreHiddenFaces(state, TRUE);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
}

BOOL ViewportHasHiddenBgFaces(HWND hwnd)
{
    const ViewportState *state = ViewportGetState(hwnd);
    return state != NULL && state->hiddenrefcount > 0;
}

BOOL ViewportSetScene(HWND hwnd, const BgVertex *tris,
                      const unsigned short *tritags,
                      const BgRenderFlags *renderflags,
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
    ViewportTexture *texturecache = NULL;
    unsigned char *selectedtris = NULL;
    unsigned char *hiddentris = NULL;
    BgDocumentVertexRef *scenevertexrefs = NULL;
    DWORD savedobject = VIEWPORT_OBJECT_NONE;
    SetupPadRef savedpad = {SETUP_PAD_INDEX_NONE, FALSE};
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
    if (state != NULL && !framecamera) { savedpad = state->selectedpad; }
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
        hiddentris = (unsigned char *)calloc((size_t)tricount, sizeof(*hiddentris));
        scenefacerefs = (BgFaceRef *)calloc((size_t)tricount,
                                            sizeof(*scenefacerefs));
        sceneobjectindices = (DWORD *)malloc((size_t)tricount
                                             * sizeof(*sceneobjectindices));
        scenevertexrefs = (BgDocumentVertexRef *)calloc((size_t)tricount * 3, sizeof(*scenevertexrefs));
        decode = (TexPixel *)malloc(256 * 256 * sizeof(TexPixel));
        texturecache = (ViewportTexture *)calloc(VIEWPORT_TEXTURE_VARIANT_COUNT, sizeof(*texturecache));

        if (scene == NULL || scenecolors == NULL || order == NULL || batches == NULL
            || textures == NULL || selectedtris == NULL || hiddentris == NULL
            || scenefacerefs == NULL || sceneobjectindices == NULL
            || decode == NULL || texturecache == NULL || scenevertexrefs == NULL)
        {
            goto scene_failed;
        }

        /* Preserve authored BG order within each layer. Batch only adjacent
           compatible triangles; the texture cache avoids duplicate uploads. */
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
            unsigned short textureid = BG_TEX_ID(order[i].tag);
            BgRenderFlags flags = renderflags ? renderflags[order[i].tri]
                : BgRenderDefaultFlags(BG_TRI_IS_SECONDARY(order[i].tag));
            ViewportTexture *texture = &texturecache[ViewportTextureKey(textureid, flags)];

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

            if (!texture->attempted && textureid != BG_TEX_NONE && projectdir)
            {
                int tw = 0, th = 0;
                texture->attempted = TRUE;
                if (TexLoadProjectImage(projectdir, textureid, decode, &tw, &th)
                    && tw > 0 && tw <= 256 && th > 0 && th <= 256)
                {
                    int pixel;
                    texture->width = tw; texture->height = th;
                    if (!(flags & BG_RENDER_IGNORE_TEXTURE_ALPHA))
                    {
                        texture->alpha = (unsigned char *)malloc((size_t)tw * th);
                        if (!texture->alpha) { goto scene_failed; }
                        for (pixel = 0; pixel < tw * th; pixel++) { texture->alpha[pixel] = decode[pixel].a; }
                    }
                    glGenTextures(1, &texture->name);
                    glBindTexture(GL_TEXTURE_2D, texture->name);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    /* RGB storage supplies alpha=1 for ENVIRONMENT-only
                     * combiners; retain texture RGB and normal modulation. */
                    glTexImage2D(GL_TEXTURE_2D, 0,
                                 (flags & BG_RENDER_IGNORE_TEXTURE_ALPHA) ? GL_RGB : GL_RGBA, tw, th, 0,
                                 GL_RGBA, GL_UNSIGNED_BYTE, decode);
                    textures[texturecount++] = texture->name;
                }
            }
            if (i == 0 || order[i].tag != order[i - 1].tag
                || flags != batches[batchcount - 1].renderflags)
            {
                SceneBatch *batch = &batches[batchcount++];
                batch->gltex = texture->name;
                batch->textureid = textureid;
                batch->renderflags = flags;
                batch->first = i * 3;
                batch->count = 0;
                batch->secondary = BG_TRI_IS_SECONDARY(order[i].tag);
                batch->cullbackfaces = BG_TRI_CULLS_BACK(order[i].tag);
                batch->object = BG_TRI_IS_OBJECT(order[i].tag);
            }
            batches[batchcount - 1].count += 3;
            if (texture->name)
            {
                invw = 1.0f / texture->width;
                invh = 1.0f / texture->height;
            }

            for (k = 0; k < 3; k++)
            {
                dst[k].x = src[k].x;
                dst[k].y = src[k].y;
                dst[k].z = src[k].z;
                dst[k].s = src[k].s * invw;   /* texels -> normalized */
                dst[k].t = src[k].t * invh;
                dst[k].environment = src[k].environment;
                dst[k].environment.scale[0] *= invw;
                dst[k].environment.scale[1] *= invh;
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
    state->selectedpad = savedpad; /* resolved when the pad overlay is rebuilt */
    state->scene = scene;
    state->scenecolors = scenecolors;
    state->scenecount = scene != NULL ? (GLsizei)(tricount * 3) : 0;
    state->batches = batches;
    state->batchcount = scene != NULL ? batchcount : 0;
    state->selectedtris = selectedtris;
    state->hiddentris = hiddentris;
    state->selectedtricount = selectedcount;
    state->scenefacerefs = scenefacerefs;
    state->sceneobjectindices = sceneobjectindices;
    state->scenevertexrefs = scenevertexrefs;
    state->textures = textures;
    state->texturecache = texturecache;
    state->texturecount = scene != NULL ? texturecount : 0;
    ViewportRestoreHiddenFaces(state, framecamera || scene == NULL);

    if (scene == NULL)
    {
        free(batches);
        free(textures);
        free(selectedtris);
        free(hiddentris);
        free(scenefacerefs);
        free(sceneobjectindices);
        free(scenevertexrefs);
        free(scenecolors);
        state->batches = NULL;
        state->textures = NULL;
        state->selectedtris = NULL;
        state->hiddentris = NULL;
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

    if (scene != NULL && framecamera && state->orbit)
    {
        const double min[3] = { minx, miny, minz }, max[3] = { maxx, maxy, maxz };
        double aspect = (double)max(1, state->width) / max(1, state->height);
        OrbitCameraFrame(&state->orbitcamera, min, max, aspect, VIEWPORT_FOV_Y);
        ViewportUpdateOrbit(state);
    }
    else if (scene != NULL && framecamera)
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

    if (framecamera || scene == NULL)
    {
        state->componentcount = 0;
        ViewportSetStanTiles(hwnd, NULL);
    }
    ViewportRestoreComponents(state);
    if (scene != NULL && savedobject != VIEWPORT_OBJECT_NONE) { ViewportSelectObject(state, savedobject); }
    ViewportUpdateStatistics(state);
    ViewportUpdateGizmo(state);
    InvalidateRect(hwnd, NULL, FALSE);
    return TRUE;

scene_failed:
    if (texturecount) { glDeleteTextures(texturecount, textures); }
    ViewportFreeTextureCache(texturecache);
    free(scene); free(scenecolors); free(order); free(batches);
    free(textures); free(selectedtris); free(hiddentris); free(scenefacerefs);
    free(sceneobjectindices); free(scenevertexrefs); free(decode); free(selectedrefs);
    return FALSE;
}


BOOL ViewportGetTextureSize(HWND hwnd, unsigned short textureid, int *width, int *height)
{
    const ViewportState *state = ViewportGetState(hwnd);
    int variant;

    *width = *height = 1;
    if (state == NULL || state->texturecache == NULL || textureid >= BG_TEX_NONE)
    {
        return FALSE;
    }
    for (variant = 0; variant < 2; variant++)
    {
        const ViewportTexture *texture =
            &state->texturecache[textureid + variant * (BG_TEX_NONE + 1)];
        if (texture->width > 0 && texture->height > 0)
        {
            *width = texture->width;
            *height = texture->height;
            return TRUE;
        }
    }
    return FALSE;
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
            && !ViewportTriangleHidden(state, triangle)
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
            || ViewportTriangleHidden(state, triangle)
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
            || ViewportTriangleHidden(state, triangle)
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


void ViewportSelectSetupModel(HWND hwnd, DWORD selection)
{
    ViewportState *state = ViewportGetState(hwnd);
    if (state == NULL)
    {
        return;
    }
    ViewportCancelTransform(hwnd);
    ViewportClearAllSelection(state);
    ViewportSelectObject(state, selection);
    ViewportUpdateGizmo(state);
    ViewportRedraw(hwnd);
    SendMessage(GetParent(hwnd), VIEWPORT_WM_SELECTION_CHANGED, 0, 0);
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


BOOL ViewportGetSelectedPad(HWND hwnd, SetupPadRef *out)
{
    const ViewportState *state = ViewportGetState(hwnd);
    if (state == NULL || out == NULL || ViewportSelectedPadIndex(state) < 0) { return FALSE; }
    *out = state->selectedpad;
    return TRUE;
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


static void ViewportSetSetupMarkers(HWND hwnd, ViewportState *state,
                                     const SetupFile *setup, float levelscale)
{
    static const int resources[SETUP_MARKER_KIND_COUNT] = {
        IDR_MARKER_START, IDR_MARKER_INTRO_CAMERA, IDR_MARKER_OUTRO_CAMERA,
        IDR_MARKER_INTRO_SPLINE
    };
    const char *reason = "";
    const SetupMarker *spawn = NULL;
    BOOL used[SETUP_MARKER_KIND_COUNT] = {0};
    DWORD i;
    free(state->setupmarkers);
    state->setupmarkers = NULL;
    state->setupmarkercount = 0;
    SetupSwirlPathFree(&state->swirlpath);
    if (setup == NULL) { return; }
    if (!SetupFileBuildMarkers(setup, levelscale, &state->setupmarkers, &state->setupmarkercount, &reason))
    { MessageBox(hwnd, reason, "GEditor setup markers", MB_ICONWARNING); return; }
    for (i = 0; i < state->setupmarkercount; i++)
    {
        SetupMarker *marker = &state->setupmarkers[i];
        used[marker->kind] = TRUE;
        if (marker->kind == SETUP_MARKER_SPAWN && state->stan.tilecount > 0)
        {
            const SetupPad *pad = &setup->pads[marker->pad];
            DWORD tile = StanResolvePadTile(&state->stan, pad->stanname, marker->position);
            float height;
            /* Spawn origins sit on the collision floor, like the player's
             * feet. Without a usable stan, keep the authored pad position. */
            if (StanGetTileHeight(&state->stan, tile, marker->position[0], marker->position[2], &height))
            { marker->position[1] = height; }
        }
        if (marker->kind == SETUP_MARKER_SPAWN && spawn == NULL) { spawn = marker; }
    }
    if (!SetupFileBuildSwirlPath(setup, spawn, &state->swirlpath, &reason))
    { MessageBox(hwnd, reason, "GEditor intro swirl", MB_ICONWARNING); }
    used[SETUP_MARKER_SWIRL] = state->swirlpath.pointcount > 0;
    for (i = 0; i < SETUP_MARKER_KIND_COUNT; i++)
    {
        if (used[i] && state->markermodels[i] == NULL)
        {
            HINSTANCE instance = GetModuleHandle(NULL);
            HRSRC resource = FindResource(instance, MAKEINTRESOURCE(resources[i]), RT_RCDATA);
            HGLOBAL loaded = resource ? LoadResource(instance, resource) : NULL;
            if (loaded)
            {
                state->markermodels[i] = GltfLoadGlbLitMesh(LockResource(loaded),
                    SizeofResource(instance, resource), NULL, &state->markermodeltris[i], &reason);
            }
            if (state->markermodels[i] == NULL)
            {
                MessageBox(hwnd, loaded ? reason : "An embedded setup marker model is missing.",
                    "GEditor setup markers", MB_ICONWARNING);
                free(state->setupmarkers); state->setupmarkers = NULL; state->setupmarkercount = 0;
                SetupSwirlPathFree(&state->swirlpath);
                return;
            }
        }
    }
}

void ViewportSetSetupPads(HWND hwnd, const SetupFile *setup, float levelscale, const unsigned char *occupiedpads, const unsigned char *occupiedboundpads)
{
    ViewportState *state = ViewportGetState(hwnd);
    Vertex *markers = NULL;
    ViewportPad *pads = NULL;
    DWORD boxcount = 0;
    int vertexcount = 0;
    float worldscale;
    DWORD i;

    if (state == NULL)
    {
        return;
    }

    ViewportCancelTransform(hwnd);
    ViewportSetSetupMarkers(hwnd, state, setup, levelscale);
    free(state->padmarkers);
    free(state->pads);
    state->padmarkers = NULL;
    state->padmarkercount = 0;
    state->pads = NULL;
    state->padcount = 0;

    if (setup == NULL || !(levelscale > 0.0f))
    {
        state->selectedpad.index = SETUP_PAD_INDEX_NONE;
        ViewportUpdateGizmo(state);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    boxcount = setup->padcount + setup->boundpadcount;
    if (boxcount == 0)
    {
        state->selectedpad.index = SETUP_PAD_INDEX_NONE;
        ViewportUpdateGizmo(state);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    markers = (Vertex *)malloc((size_t)boxcount * VIEWPORT_BOX_VERTICES
                              * sizeof(*markers));
    pads = (ViewportPad *)malloc((size_t)boxcount * sizeof(*pads));
    if (markers == NULL || pads == NULL)
    {
        free(markers); free(pads);
        state->selectedpad.index = SETUP_PAD_INDEX_NONE;
        ViewportUpdateGizmo(state);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }

    worldscale = 1.0f / levelscale;

    for (i = 0; i < setup->padcount; i++)
    {
        int axis;
        pads[i].ref.index = i; pads[i].ref.bound = FALSE;
        pads[i].occupied = occupiedpads != NULL && occupiedpads[i];
        for (axis = 0; axis < 3; axis++) { pads[i].position[axis] = setup->pads[i].pos[axis] * worldscale; }

        ViewportAppendPadBox(markers, &vertexcount, &setup->pads[i],
            -VIEWPORT_PAD_HALF_SIZE, VIEWPORT_PAD_HALF_SIZE,
            -VIEWPORT_PAD_HALF_SIZE, VIEWPORT_PAD_HALF_SIZE,
            -VIEWPORT_PAD_HALF_SIZE, VIEWPORT_PAD_HALF_SIZE,
            worldscale, 32, 255, 64);
    }

    for (i = 0; i < setup->boundpadcount; i++)
    {
        const SetupBoundPad *pad = &setup->boundpads[i];
        ViewportPad *preview = &pads[setup->padcount + i];
        int axis;
        preview->ref.index = i; preview->ref.bound = TRUE;
        preview->occupied = occupiedboundpads != NULL && occupiedboundpads[i];
        for (axis = 0; axis < 3; axis++) { preview->position[axis] = pad->pad.pos[axis] * worldscale; }

        ViewportAppendPadBox(markers, &vertexcount, &pad->pad,
            pad->xmin, pad->xmax, pad->ymin, pad->ymax,
            pad->zmin, pad->zmax, worldscale, 255, 48, 48);
    }

    for (i = 0; i < state->setupmarkercount; i++)
    {
        const SetupMarker *marker = &state->setupmarkers[i];
        if (marker->kind == SETUP_MARKER_SPAWN && marker->pad < setup->padcount)
        { pads[marker->pad].occupied = TRUE; }
    }
    state->padmarkers = markers;
    state->padmarkercount = vertexcount;
    state->pads = pads;
    state->padcount = boxcount;
    if (ViewportSelectedPadIndex(state) < 0) { state->selectedpad.index = SETUP_PAD_INDEX_NONE; }
    ViewportRefreshPadColors(state);
    ViewportUpdateGizmo(state);
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

BOOL ViewportGetBgStatisticsVisible(HWND hwnd)
{
    const ViewportState *state = ViewportGetState(hwnd);
    return state != NULL && state->showbgstatistics;
}

void ViewportSetBgStatisticsVisible(HWND hwnd, BOOL enabled)
{
    ViewportState *state = ViewportGetState(hwnd);
    if (state == NULL || state->showbgstatistics == enabled) { return; }
    state->showbgstatistics = enabled;
    InvalidateRect(hwnd, NULL, FALSE);
}

static BOOL ViewportTriangleRotation(const Vertex triangle[3], Rotation *frame)
{
    double up[3], look[3], side[3];
    int axis;
    look[0] = triangle[1].x - triangle[0].x;
    look[1] = triangle[1].y - triangle[0].y;
    look[2] = triangle[1].z - triangle[0].z;
    side[0] = triangle[2].x - triangle[0].x;
    side[1] = triangle[2].y - triangle[0].y;
    side[2] = triangle[2].z - triangle[0].z;
    for (axis = 0; axis < 3; axis++)
    {
        up[axis] = look[(axis + 1) % 3] * side[(axis + 2) % 3] -
                   look[(axis + 2) % 3] * side[(axis + 1) % 3];
    }
    return RotationBasis(frame, up, look);
}
/* Components have no stored orientation. Derive a frame from selected
   positions in selection order, using only vertices that will actually rotate.
   A non-collinear third point supplies roll; a line uses a canonical up axis.
   Shared identities still come from GetMoveVertices/GetMoveStanPoints at commit. */
static BOOL ViewportGetComponentRotation(HWND hwnd, const ViewportState *state,
                                         Rotation *frame)
{
    BOOL stan = ViewportGetStanSelectionCount(hwnd, NULL) > 0;
    int count = stan ? state->stancomponentcount : state->componentcount;
    int ends = state->tool == EDITOR_TOOL_EDGE_SELECT ? 2 : 1;
    double origin[3] = {0}, look[3] = {0}, up[3];
    double looklength = 0;
    BOOL haveorigin = FALSE;
    int i, end, axis;

    for (i = 0; i < count; i++)
    for (end = 0; end < ends; end++)
    {
        double point[3], delta[3], length = 0, crosslength = 0;
        if (stan)
        {
            const StanPointRef *ref = &state->stancomponents[i].refs[end];
            const StanPoint *vertex;
            if (ref->tile >= state->stan.tilecount
                || ref->point >= state->stan.tiles[ref->tile].pointcount) { return FALSE; }
            vertex = &state->stan.tiles[ref->tile].points[ref->point];
            point[0] = vertex->x; point[1] = vertex->y; point[2] = vertex->z;
        }
        else
        {
            int corner = state->components[i].corners[end];
            const Vertex *vertex;
            if (!ViewportCornerVisible(state, corner)) { continue; }
            vertex = &state->scene[corner];
            point[0] = vertex->x; point[1] = vertex->y; point[2] = vertex->z;
        }
        if (!haveorigin)
        {
            memcpy(origin, point, sizeof(origin));
            haveorigin = TRUE;
            continue;
        }
        for (axis = 0; axis < 3; axis++)
        {
            delta[axis] = point[axis] - origin[axis];
            length += delta[axis] * delta[axis];
        }
        if (!(length > 1e-12)) { continue; }
        if (!(looklength > 0))
        {
            memcpy(look, delta, sizeof(look));
            looklength = length;
            continue;
        }
        for (axis = 0; axis < 3; axis++)
        {
            up[axis] = look[(axis+1)%3]*delta[(axis+2)%3]
                     - look[(axis+2)%3]*delta[(axis+1)%3];
            crosslength += up[axis] * up[axis];
        }
        if (crosslength > looklength * length * 1e-12)
        {
            return RotationBasis(frame, up, look);
        }
    }
    /* This also excludes single vertices, duplicate corners of one vertex,
       and zero-length selections for which rotation cannot move anything. */
    if (!(looklength > 0)) { return FALSE; }
    up[0] = up[1] = up[2] = 0;
    up[look[1]*look[1] < looklength * 0.99 ? 1 : 0] = 1;
    return RotationBasis(frame, up, look);
}

/* A stable reference face supplies the Euler frame. Its normal is local Y,
   and its first edge is local Z. Texture sorting must not change the frame. */
BOOL ViewportGetGeometryRotation(HWND hwnd, Rotation *frame)
{
    const ViewportState *state = ViewportGetState(hwnd);
    Vertex triangle[3];
    int i, best = -1;
    if (!state || state->tool == EDITOR_TOOL_VERTEX_PAINT)
    {
        return FALSE;
    }
    if (state->tool == EDITOR_TOOL_VERTEX_SELECT || state->tool == EDITOR_TOOL_EDGE_SELECT)
    {
        return ViewportGetComponentRotation(hwnd, state, frame);
    }
    if (ViewportGetStanSelectionCount(hwnd, NULL) > 0)
    {
        DWORD tile;
        for (tile = 0; tile < state->stan.tilecount; tile++)
        {
            if (state->stanselected[tile])
            {
                const StanTile *polygon = &state->stan.tiles[tile];
                unsigned int point;
                triangle[0] = ViewportStanPointVertex(&polygon->points[0]);
                /* Do not use the collision triple, which can change with XZ area
                   after rotation even when the polygon's shape is unchanged. */
                for (point = 1; point + 1 < polygon->pointcount; point++)
                {
                    triangle[1] = ViewportStanPointVertex(&polygon->points[point]);
                    triangle[2] = ViewportStanPointVertex(&polygon->points[point + 1]);
                    if (ViewportTriangleRotation(triangle, frame))
                    {
                        return TRUE;
                    }
                }
            }
        }
        return FALSE;
    }
    if (state->scenefacerefs && state->selectedtris)
    {
        for (i = 0; i < state->scenecount / 3; i++)
        {
            if (state->selectedtris[i] &&
                (best < 0 || state->scenefacerefs[i].faceid < state->scenefacerefs[best].faceid))
            {
                best = i;
            }
        }
    }
    if (best < 0)
    {
        return FALSE;
    }
    memcpy(triangle, &state->scene[best * 3], sizeof(triangle));
    return ViewportTriangleRotation(triangle, frame);
}
BOOL ViewportIsTransforming(HWND hwnd)
{
    ViewportState *s = ViewportGetState(hwnd);
    return s && s->dragaxis >= 0;
}
BOOL ViewportIsRotating(HWND hwnd)
{
    ViewportState *s = ViewportGetState(hwnd);
    return s && s->rotationmode;
}
void ViewportSetTransformMode(HWND hwnd, TransformMode mode)
{
    ViewportState *s = ViewportGetState(hwnd);
    if (!s)
    {
        return;
    }
    ViewportCancelTransform(hwnd);
    s->rotationmode = mode == TRANSFORM_ROTATE;
    s->scalemode = mode == TRANSFORM_SCALE;
    ViewportUpdateGizmo(s);
    s->hoveraxis = -1;
    InvalidateRect(hwnd, NULL, FALSE);
}
void ViewportSetRotationFrame(HWND hwnd, const Rotation *frame, unsigned int axes)
{
    ViewportState *s = ViewportGetState(hwnd);
    if (!s || s->dragaxis >= 0)
    {
        return;
    }
    s->rotationaxes = frame ? axes : 0;
    if (frame)
    {
        s->rotationframe = *frame;
    }
    InvalidateRect(hwnd, NULL, FALSE);
}
BOOL ViewportGetRotation(HWND hwnd, Rotation *frame, double degrees[3], double pivot[3])
{
    ViewportState *s = ViewportGetState(hwnd);
    Rotation delta;
    if (!s || !s->rotationaxes || !s->gizmovisible || s->tool == EDITOR_TOOL_VERTEX_PAINT)
    {
        return FALSE;
    }
    *frame = s->rotationframe;
    if (s->dragaxis >= 0 && s->dragrotation)
    {
        RotationAxis(&delta, s->dragaxis, s->dragdelta);
        RotationMultiply(frame, &delta, frame);
    }
    RotationDegrees(frame, degrees);
    if (s->rotationaxes == 2)
    {
        degrees[0] = degrees[2] = 0;
        degrees[1] = atan2(frame->m[0][2], frame->m[2][2]) * 180.0 / 3.14159265358979323846;
    }
    memcpy(pivot, s->gizmoposition, sizeof(s->gizmoposition));
    return TRUE;
}

TransformMode ViewportGetTransformMode(HWND hwnd)
{
    ViewportState *s = ViewportGetState(hwnd);
    return s && s->scalemode      ? TRANSFORM_SCALE
           : s && s->rotationmode ? TRANSFORM_ROTATE
                                  : TRANSFORM_MOVE;
}
void ViewportSetScaleAxes(HWND hwnd, const Rotation *axes)
{
    ViewportState *s = ViewportGetState(hwnd);
    if (!s || s->dragaxis >= 0)
    {
        return;
    }
    s->scalevalid = axes && RotationValid(axes);
    if (s->scalevalid)
    {
        s->scaleaxes = *axes;
    }
    InvalidateRect(hwnd, NULL, FALSE);
}
BOOL ViewportGetScaling(HWND hwnd, Scaling *scale)
{
    ViewportState *s = ViewportGetState(hwnd);
    int axis;
    if (!s || !s->scalemode || !s->scalevalid || !s->gizmovisible || s->vertexsnap)
    {
        return FALSE;
    }
    scale->axes = s->scaleaxes;
    memcpy(scale->pivot, s->gizmoposition, sizeof(scale->pivot));
    for (axis = 0; axis < 3; axis++)
    {
        scale->factor[axis] = s->dragscaling && s->dragaxis == axis ? 1 + s->dragdelta : 1;
    }
    return TRUE;
}
/* Pad promotion keeps the new bound volume selected after the scene rebuild. */
void ViewportSelectPad(HWND hwnd, const SetupPadRef *ref)
{
    ViewportState *s = ViewportGetState(hwnd);
    if (!s || !ref)
    {
        return;
    }
    s->selectedpad = *ref;
    if (ViewportSelectedPadIndex(s) < 0)
    {
        s->selectedpad.index = SETUP_PAD_INDEX_NONE;
    }
    ViewportRefreshPadColors(s);
    ViewportUpdateGizmo(s);
    InvalidateRect(hwnd, NULL, FALSE);
}
