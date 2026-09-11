#include "uvcanvas.h"

#include <windowsx.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define UVCANVAS_CLASS "GEditorUVCanvas"
#define UVCANVAS_UNIT_FILL RGB(48, 48, 48)
#define UVCANVAS_UNIT_EDGE RGB(160, 160, 160)
#define UVCANVAS_FACE_EDGE RGB(128, 128, 128)
#define UVCANVAS_VERTEX_RADIUS 2 /* a filled 5x5 screen-pixel square */
#define UVCANVAS_MIN_SCALE 4.0
#define UVCANVAS_MAX_SCALE 1000000.0
#define UVCANVAS_ROTATE_RADIUS 56
#define UVCANVAS_PI 3.14159265358979323846

typedef struct UVCanvasNode {
    BgDocumentUVEdit source;
    int width, height, sourcecorner;
    BOOL selected;
} UVCanvasNode;

typedef struct UVCanvasState {
    /* UV coordinate at the canvas center, with U right and V up.
       One shared scale keeps a UV unit square at any window size. */
    double centeru, centerv;
    double pixelsperunit;
    int width, height;
    BOOL panning;
    POINT lastmouse;
    UVCanvasTriangle *triangles;
    int trianglecount;
    UVCanvasNode *nodes; /* sorted by stable source identity */
    int nodecount;
    BOOL boxing, boxadd, boxremove;
    POINT boxstart, boxend;
    TransformMode mode;
    int draghandle; /* 0: idle, 1: U, 2: V, 3: center or rotation ring */
    POINT dragstart, dragorigin;
    double pivot[2], values[2]; /* UV offsets, degrees, or scale factors */
    double lastangle, dragangle;
    BOOL limited; /* last requested transform exceeded native S/T range */
} UVCanvasState;

static void UVCanvasUpdatePreview(UVCanvasState *state);
static void UVCanvasNotify(HWND hwnd);

static int UVCanvasNodeCompare(const void *a, const void *b)
{
    const UVCanvasNode *x = a, *y = b;
    if (x->source.vertex.room != y->source.vertex.room)
    { return x->source.vertex.room < y->source.vertex.room ? -1 : 1; }
    if (x->source.vertex.index != y->source.vertex.index)
    { return x->source.vertex.index < y->source.vertex.index ? -1 : 1; }
    if (x->source.vertexid != y->source.vertexid)
    { return x->source.vertexid < y->source.vertexid ? -1 : 1; }
    return 0;
}

static int UVCanvasCornerCompare(const void *a, const void *b)
{
    const UVCanvasNode *x = a, *y = b;
    int order = UVCanvasNodeCompare(a, b);
    return order != 0 ? order : (x->sourcecorner > y->sourcecorner) - (x->sourcecorner < y->sourcecorner);
}

static void UVCanvasResetTransform(UVCanvasState *state)
{
    state->values[0] = state->values[1] = state->mode == TRANSFORM_SCALE ? 1.0 : 0.0;
    state->limited = FALSE;
}

/* Evaluate from the original S/T every time: previews never accumulate native
 * rounding error. Validate before conversion so no vertex can wrap or clip. */
static BOOL UVCanvasTransformST(const UVCanvasNode *node, TransformMode mode,
                                const double pivot[2], const double values[2], int st[2])
{
    double s, t;
    if (node->width <= 0 || node->height <= 0 || !isfinite(values[0])
        || !isfinite(values[1]) || !isfinite(pivot[0]) || !isfinite(pivot[1])) { return FALSE; }
    if (mode == TRANSFORM_MOVE)
    {
        s = node->source.s + round(values[0] * 32.0 * node->width);
        t = node->source.t + round(values[1] * 32.0 * node->height);
    }
    else
    {
        double u = node->source.s / (32.0 * node->width) - pivot[0];
        double v = node->source.t / (32.0 * node->height) - pivot[1];
        double x, y;
        if (mode == TRANSFORM_ROTATE)
        {
            double angle = remainder(values[0], 360.0) * UVCANVAS_PI / 180.0;
            x = u * cos(angle) - v * sin(angle);
            y = u * sin(angle) + v * cos(angle);
        }
        else if (mode == TRANSFORM_SCALE) { x = u * values[0]; y = v * values[1]; }
        else { return FALSE; }
        s = round((pivot[0] + x) * 32.0 * node->width);
        t = round((pivot[1] + y) * 32.0 * node->height);
    }
    if (!isfinite(s) || !isfinite(t) || s < -32768 || s > 32767 || t < -32768 || t > 32767)
    { return FALSE; }
    st[0] = (int)s;
    st[1] = (int)t;
    return TRUE;
}

static void UVCanvasNodeST(const UVCanvasState *state, const UVCanvasNode *node, int st[2])
{
    st[0] = node->source.s;
    st[1] = node->source.t;
    if (node->selected && state->draghandle)
    {
        UVCanvasTransformST(node, state->mode, state->pivot, state->values, st);
    }
}

static int UVCanvasSelectionPosition(const UVCanvasState *state, double uv[2])
{
    int i, count = 0;
    uv[0] = uv[1] = 0.0;
    for (i = 0; i < state->nodecount; i++)
    {
        const UVCanvasNode *node = &state->nodes[i];
        int st[2];
        if (!node->selected) { continue; }
        UVCanvasNodeST(state, node, st);
        uv[0] += st[0] / (32.0 * node->width);
        uv[1] += st[1] / (32.0 * node->height);
        count++;
    }
    if (count > 0) { uv[0] /= count; uv[1] /= count; }
    return count;
}

static UVCanvasState *UVCanvasGetState(HWND hwnd)
{
    return (UVCanvasState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

static void UVCanvasResize(HWND hwnd, UVCanvasState *state)
{
    RECT client;
    GetClientRect(hwnd, &client);
    state->width = client.right;
    state->height = client.bottom;

    /* Fit once at the first real layout. Subsequent resizes preserve the
       camera center and zoom, including a minimize/restore cycle. */
    if (state->pixelsperunit == 0.0 && state->width > 0 && state->height > 0)
    {
        int shortside = state->width < state->height ? state->width : state->height;
        state->pixelsperunit = fmax(UVCANVAS_MIN_SCALE, shortside * 0.7);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

/* Clip beyond the client edges before converting to GDI integers. Keeping
   clipped edges outside the client avoids drawing a false outline when
   zoomed far inside the square. */
static int UVCanvasClipCoordinate(double coordinate, int extent)
{
    if (coordinate < -2.0) { return -2; }
    if (coordinate > extent + 2.0) { return extent + 2; }
    return (int)floor(coordinate + 0.5);
}

static void UVCanvasProject(const UVCanvasState *state, const double uv[2], double screen[2])
{
    screen[0] = state->width * 0.5 + (uv[0] - state->centeru) * state->pixelsperunit;
    screen[1] = state->height * 0.5 - (uv[1] - state->centerv) * state->pixelsperunit;
}

/* Clip a line parametrically before converting to screen integers. Merely
   clamping its endpoints would bend diagonal UV edges at high zoom. */
static BOOL UVCanvasClipEdge(double direction, double distance, double *start, double *end)
{
    double position;
    if (direction == 0.0) { return distance >= 0.0; }
    position = distance / direction;
    if (direction < 0.0)
    {
        if (position > *end) { return FALSE; }
        if (position > *start) { *start = position; }
    }
    else
    {
        if (position < *start) { return FALSE; }
        if (position < *end) { *end = position; }
    }
    return TRUE;
}

static void UVCanvasDrawEdge(HDC dc, const UVCanvasState *state,
                             const double a[2], const double b[2])
{
    double dx = b[0] - a[0], dy = b[1] - a[1];
    double start = 0.0, end = 1.0;
    if (!UVCanvasClipEdge(-dx, a[0], &start, &end)
        || !UVCanvasClipEdge(dx, state->width - 1.0 - a[0], &start, &end)
        || !UVCanvasClipEdge(-dy, a[1], &start, &end)
        || !UVCanvasClipEdge(dy, state->height - 1.0 - a[1], &start, &end))
    {
        return;
    }
    MoveToEx(dc, (int)floor(a[0] + start * dx + 0.5),
                 (int)floor(a[1] + start * dy + 0.5), NULL);
    LineTo(dc, (int)floor(a[0] + end * dx + 0.5),
               (int)floor(a[1] + end * dy + 0.5));
}

static void UVCanvasDrawTriangles(HDC dc, const UVCanvasState *state)
{
    int triangle, corner;

    SetDCPenColor(dc, UVCANVAS_FACE_EDGE);
    for (triangle = 0; triangle < state->trianglecount; triangle++)
    {
        double screen[3][2];
        for (corner = 0; corner < 3; corner++)
        {
            UVCanvasProject(state, state->triangles[triangle].uv[corner], screen[corner]);
        }
        for (corner = 0; corner < 3; corner++)
        {
            UVCanvasDrawEdge(dc, state, screen[corner], screen[(corner + 1) % 3]);
        }
    }

    /* Draw all markers last so crossing edges cannot paint over them.
       Markers keep their screen size while their UV positions follow the camera. */
    for (triangle = 0; triangle < state->trianglecount; triangle++)
    {
        for (corner = 0; corner < 3; corner++)
        {
            double screen[2];
            int x, y;
            RECT marker;
            UVCanvasProject(state, state->triangles[triangle].uv[corner], screen);
            if (screen[0] < -UVCANVAS_VERTEX_RADIUS - 1.0
                || screen[0] > state->width + UVCANVAS_VERTEX_RADIUS
                || screen[1] < -UVCANVAS_VERTEX_RADIUS - 1.0
                || screen[1] > state->height + UVCANVAS_VERTEX_RADIUS)
            {
                continue;
            }
            x = (int)floor(screen[0] + 0.5);
            y = (int)floor(screen[1] + 0.5);
            SetRect(&marker, x - UVCANVAS_VERTEX_RADIUS, y - UVCANVAS_VERTEX_RADIUS,
                    x + UVCANVAS_VERTEX_RADIUS + 1, y + UVCANVAS_VERTEX_RADIUS + 1);
            SetDCBrushColor(dc, state->nodes[state->triangles[triangle].nodes[corner]].selected
                                  ? RGB(0, 220, 255) : RGB(0, 0, 0));
            FillRect(dc, &marker, (HBRUSH)GetStockObject(DC_BRUSH));
        }
    }
}

static BOOL UVCanvasGizmo(const UVCanvasState *state, POINT *origin)
{
    double uv[2], screen[2];
    int count = UVCanvasSelectionPosition(state, uv);
    if (count == 0 || (state->mode != TRANSFORM_MOVE && count < 2)) { return FALSE; }
    if (state->draghandle && state->mode != TRANSFORM_MOVE)
    { uv[0] = state->pivot[0]; uv[1] = state->pivot[1]; }
    UVCanvasProject(state, uv, screen);
    if (screen[0] < -80 || screen[0] > state->width + 80
        || screen[1] < -80 || screen[1] > state->height + 80) { return FALSE; }
    origin->x = (int)floor(screen[0] + 0.5);
    origin->y = (int)floor(screen[1] + 0.5);
    return TRUE;
}

static int UVCanvasPickHandle(const UVCanvasState *state, int x, int y)
{
    POINT origin;
    int dx, dy;
    if (!UVCanvasGizmo(state, &origin)) { return 0; }
    dx = x - origin.x; dy = y - origin.y;
    if (state->mode == TRANSFORM_ROTATE)
    { return fabs(hypot(dx, dy) - UVCANVAS_ROTATE_RADIUS) <= 7.0 ? 3 : 0; }
    if (abs(dx) <= 6 && abs(dy) <= 6) { return 3; }
    if (dx >= 10 && dx <= 76 && abs(dy) <= 7) { return 1; }
    if (-dy >= 10 && -dy <= 76 && abs(dx) <= 7) { return 2; }
    return 0;
}

static void UVCanvasDrawTools(HDC dc, const UVCanvasState *state)
{
    POINT origin;
    HGDIOBJ oldfont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);
    if (UVCanvasGizmo(state, &origin))
    {
        RECT center = { origin.x - 4, origin.y - 4, origin.x + 5, origin.y + 5 };
        if (state->mode == TRANSFORM_ROTATE)
        {
            HGDIOBJ oldbrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            SetDCPenColor(dc, state->limited ? RGB(255, 100, 80) : RGB(235, 195, 80));
            Ellipse(dc, origin.x - UVCANVAS_ROTATE_RADIUS, origin.y - UVCANVAS_ROTATE_RADIUS,
                    origin.x + UVCANVAS_ROTATE_RADIUS + 1, origin.y + UVCANVAS_ROTATE_RADIUS + 1);
            MoveToEx(dc, origin.x - 4, origin.y, NULL); LineTo(dc, origin.x + 5, origin.y);
            MoveToEx(dc, origin.x, origin.y - 4, NULL); LineTo(dc, origin.x, origin.y + 5);
            if (state->draghandle)
            {
                double start = atan2(origin.y - state->dragstart.y, state->dragstart.x - origin.x);
                double angle = start + remainder(state->values[0], 360.0) * UVCANVAS_PI / 180.0;
                MoveToEx(dc, origin.x, origin.y, NULL);
                LineTo(dc, origin.x + (int)round(cos(angle) * UVCANVAS_ROTATE_RADIUS),
                           origin.y - (int)round(sin(angle) * UVCANVAS_ROTATE_RADIUS));
            }
            SelectObject(dc, oldbrush);
        }
        else
        {
            SetDCPenColor(dc, RGB(235, 80, 80));
            MoveToEx(dc, origin.x + 10, origin.y, NULL); LineTo(dc, origin.x + 72, origin.y);
            if (state->mode == TRANSFORM_SCALE)
            {
                RECT cap = {origin.x + 67, origin.y - 5, origin.x + 77, origin.y + 6};
                SetDCBrushColor(dc, RGB(235, 80, 80)); FillRect(dc, &cap, (HBRUSH)GetStockObject(DC_BRUSH));
            }
            else
            {
                MoveToEx(dc, origin.x + 63, origin.y - 5, NULL); LineTo(dc, origin.x + 72, origin.y);
                LineTo(dc, origin.x + 63, origin.y + 5);
            }
            SetTextColor(dc, RGB(235, 80, 80)); TextOut(dc, origin.x + 78, origin.y - 7, "U", 1);
            SetDCPenColor(dc, RGB(90, 215, 100));
            MoveToEx(dc, origin.x, origin.y - 10, NULL); LineTo(dc, origin.x, origin.y - 72);
            if (state->mode == TRANSFORM_SCALE)
            {
                RECT cap = {origin.x - 5, origin.y - 77, origin.x + 6, origin.y - 67};
                SetDCBrushColor(dc, RGB(90, 215, 100)); FillRect(dc, &cap, (HBRUSH)GetStockObject(DC_BRUSH));
            }
            else
            {
                MoveToEx(dc, origin.x - 5, origin.y - 63, NULL); LineTo(dc, origin.x, origin.y - 72);
                LineTo(dc, origin.x + 5, origin.y - 63);
            }
            SetTextColor(dc, RGB(90, 215, 100)); TextOut(dc, origin.x - 4, origin.y - 92, "V", 1);
            SetDCBrushColor(dc, RGB(235, 195, 80)); FillRect(dc, &center, (HBRUSH)GetStockObject(DC_BRUSH));
        }
    }
    if (state->boxing)
    {
        HPEN pen = CreatePen(PS_DOT, 1, RGB(210, 210, 210));
        HGDIOBJ oldpen = SelectObject(dc, pen), oldbrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, min(state->boxstart.x, state->boxend.x), min(state->boxstart.y, state->boxend.y),
                      max(state->boxstart.x, state->boxend.x) + 1, max(state->boxstart.y, state->boxend.y) + 1);
        SelectObject(dc, oldbrush); SelectObject(dc, oldpen); DeleteObject(pen);
    }
    SelectObject(dc, oldfont);
}

static void UVCanvasDraw(HDC dc, const UVCanvasState *state)
{
    RECT client = { 0, 0, state->width, state->height };
    HGDIOBJ oldbrush, oldpen;
    double left, top, right, bottom;

    SetDCBrushColor(dc, UVCANVAS_BACKGROUND);
    FillRect(dc, &client, (HBRUSH)GetStockObject(DC_BRUSH));

    left = state->width * 0.5 - state->centeru * state->pixelsperunit;
    top = state->height * 0.5 + (state->centerv - 1.0) * state->pixelsperunit;
    right = left + state->pixelsperunit;
    bottom = top + state->pixelsperunit;
    oldbrush = SelectObject(dc, GetStockObject(DC_BRUSH));
    oldpen = SelectObject(dc, GetStockObject(DC_PEN));
    if (right >= 0.0 && bottom >= 0.0 && left < state->width && top < state->height)
    {
        SetDCBrushColor(dc, UVCANVAS_UNIT_FILL);
        SetDCPenColor(dc, UVCANVAS_UNIT_EDGE);
        Rectangle(dc, UVCanvasClipCoordinate(left, state->width),
                  UVCanvasClipCoordinate(top, state->height),
                  UVCanvasClipCoordinate(right, state->width) + 1,
                  UVCanvasClipCoordinate(bottom, state->height) + 1);
    }
    /* UVs may lie outside 0-1, even with the entire unit square offscreen. */
    UVCanvasDrawTriangles(dc, state);
    UVCanvasDrawTools(dc, state);
    SelectObject(dc, oldpen);
    SelectObject(dc, oldbrush);
}

static void UVCanvasPaint(HWND hwnd, const UVCanvasState *state)
{
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(hwnd, &paint);

    if (state != NULL && state->width > 0 && state->height > 0)
    {
        HDC buffer = CreateCompatibleDC(dc);
        HBITMAP bitmap = buffer != NULL
            ? CreateCompatibleBitmap(dc, state->width, state->height) : NULL;

        /* Draw a complete frame offscreen so wheel and drag updates do not
           flash the background. Still paint if buffer allocation fails. */
        if (bitmap != NULL)
        {
            HGDIOBJ oldbitmap = SelectObject(buffer, bitmap);
            UVCanvasDraw(buffer, state);
            BitBlt(dc, 0, 0, state->width, state->height, buffer, 0, 0, SRCCOPY);
            SelectObject(buffer, oldbitmap);
            DeleteObject(bitmap);
        }
        else { UVCanvasDraw(dc, state); }
        if (buffer != NULL) { DeleteDC(buffer); }
    }
    EndPaint(hwnd, &paint);
}

static void UVCanvasPan(HWND hwnd, UVCanvasState *state, int x, int y)
{
    state->centeru -= (x - state->lastmouse.x) / state->pixelsperunit;
    state->centerv += (y - state->lastmouse.y) / state->pixelsperunit;
    state->lastmouse.x = x;
    state->lastmouse.y = y;
    InvalidateRect(hwnd, NULL, FALSE);
}

static void UVCanvasEndPan(HWND hwnd, UVCanvasState *state)
{
    if (state == NULL || !state->panning) { return; }
    state->panning = FALSE;
    if (GetCapture() == hwnd) { ReleaseCapture(); }
    SetCursor(LoadCursor(NULL, IDC_ARROW));
}

static void UVCanvasZoom(HWND hwnd, UVCanvasState *state, WPARAM wparam, LPARAM lparam)
{
    POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
    RECT client = { 0, 0, state->width, state->height };
    double x, y, u, v, scale;

    ScreenToClient(hwnd, &mouse);
    if (state->boxing || state->draghandle) { return; }
    if (state->pixelsperunit == 0.0 || !PtInRect(&client, mouse)) { return; }
    if (state->panning) { UVCanvasPan(hwnd, state, mouse.x, mouse.y); }

    x = mouse.x - state->width * 0.5;
    y = mouse.y - state->height * 0.5;
    u = state->centeru + x / state->pixelsperunit;
    v = state->centerv - y / state->pixelsperunit;

    /* Fractional wheel deltas work with high-resolution wheels too. Keep
       the UV point under the pointer fixed while changing the scale. */
    scale = state->pixelsperunit
        * pow(1.2, GET_WHEEL_DELTA_WPARAM(wparam) / (double)WHEEL_DELTA);
    scale = fmax(UVCANVAS_MIN_SCALE, fmin(UVCANVAS_MAX_SCALE, scale));
    state->centeru = u - x / scale;
    state->centerv = v + y / scale;
    state->pixelsperunit = scale;
    InvalidateRect(hwnd, NULL, FALSE);
}

static void UVCanvasNotify(HWND hwnd)
{
    InvalidateRect(hwnd, NULL, FALSE);
    SendMessage(GetParent(hwnd), UVCANVAS_WM_SELECTION_CHANGED, 0, 0);
}

static void UVCanvasUpdatePreview(UVCanvasState *state)
{
    int triangle, corner;
    for (triangle = 0; triangle < state->trianglecount; triangle++)
    {
        UVCanvasTriangle *face = &state->triangles[triangle];
        for (corner = 0; corner < 3; corner++)
        {
            int st[2];
            UVCanvasNodeST(state, &state->nodes[face->nodes[corner]], st);
            face->uv[corner][0] = st[0] / (32.0 * face->width);
            face->uv[corner][1] = st[1] / (32.0 * face->height);
        }
    }
}

BOOL UVCanvasCancelInteraction(HWND canvas)
{
    UVCanvasState *state = UVCanvasGetState(canvas);
    BOOL active;
    if (state == NULL) { return FALSE; }
    active = state->boxing || state->draghandle != 0;
    state->boxing = FALSE;
    state->draghandle = 0;
    UVCanvasResetTransform(state);
    if (active)
    {
        UVCanvasUpdatePreview(state);
        if (GetCapture() == canvas) { ReleaseCapture(); }
        UVCanvasNotify(canvas);
    }
    return active;
}

static void UVCanvasSelect(HWND hwnd, UVCanvasState *state)
{
    int triangle, corner, besttriangle = -1, bestcorner = -1;
    double bestdistance = 49.0;
    BOOL box = abs(state->boxend.x - state->boxstart.x) >= 3
            || abs(state->boxend.y - state->boxstart.y) >= 3;
    int left = min(state->boxstart.x, state->boxend.x), right = max(state->boxstart.x, state->boxend.x);
    int top = min(state->boxstart.y, state->boxend.y), bottom = max(state->boxstart.y, state->boxend.y);
    if (!state->boxadd && !state->boxremove)
    {
        for (corner = 0; corner < state->nodecount; corner++) { state->nodes[corner].selected = FALSE; }
    }
    for (triangle = 0; triangle < state->trianglecount; triangle++)
    {
        UVCanvasTriangle *face = &state->triangles[triangle];
        for (corner = 0; corner < 3; corner++)
        {
            double screen[2];
            UVCanvasNode *node = &state->nodes[face->nodes[corner]];
            UVCanvasProject(state, face->uv[corner], screen);
            if (box)
            {
                if (screen[0] < left || screen[0] > right || screen[1] < top || screen[1] > bottom) { continue; }
                if (!node->selected) { node->width = face->width; node->height = face->height; }
                node->selected = !state->boxremove;
            }
            else
            {
                double dx = screen[0] - state->boxend.x, dy = screen[1] - state->boxend.y;
                double distance = dx * dx + dy * dy;
                if (distance < bestdistance)
                { bestdistance = distance; besttriangle = triangle; bestcorner = corner; }
            }
        }
    }
    if (!box && besttriangle >= 0)
    {
        UVCanvasTriangle *face = &state->triangles[besttriangle];
        UVCanvasNode *node = &state->nodes[face->nodes[bestcorner]];
        node->width = face->width; node->height = face->height;
        node->selected = !state->boxremove;
    }
    state->boxing = FALSE;
    if (GetCapture() == hwnd) { ReleaseCapture(); }
    UVCanvasNotify(hwnd);
}

static BOOL UVCanvasTryTransform(UVCanvasState *state, const double values[2])
{
    int i, st[2];
    for (i = 0; i < state->nodecount; i++)
    {
        const UVCanvasNode *node = &state->nodes[i];
        if (node->selected && !UVCanvasTransformST(node, state->mode, state->pivot, values, st))
        { state->limited = TRUE; return FALSE; }
    }
    state->values[0] = values[0]; state->values[1] = values[1];
    state->limited = FALSE;
    return TRUE;
}

static void UVCanvasDrag(HWND hwnd, UVCanvasState *state, int x, int y)
{
    int axis, i;
    double values[2] = {0, 0};
    double dx = x - state->dragstart.x, dy = state->dragstart.y - y;
    if (state->mode == TRANSFORM_ROTATE)
    {
        double u = x - state->dragorigin.x, v = state->dragorigin.y - y;
        double angle;
        if (hypot(u, v) < 3.0) { return; }
        angle = atan2(v, u);
        state->dragangle += remainder(angle - state->lastangle, 2.0 * UVCANVAS_PI) * 180.0 / UVCANVAS_PI;
        state->lastangle = angle;
        values[0] = state->dragangle;
    }
    else if (state->mode == TRANSFORM_SCALE)
    {
        values[0] = state->draghandle == 2 ? 1.0 : 1.0 + dx / 72.0;
        values[1] = state->draghandle == 1 ? 1.0 : 1.0 + dy / 72.0;
        if (state->draghandle == 3) { values[0] = values[1] = 1.0 + (dx + dy) / 144.0; }
    }
    else
    {
        values[0] = state->draghandle == 2 ? 0 : dx / state->pixelsperunit;
        values[1] = state->draghandle == 1 ? 0 : dy / state->pixelsperunit;
        for (axis = 0; axis < 2; axis++)
        {
            double lower = -1.0e30, upper = 1.0e30;
            for (i = 0; i < state->nodecount; i++)
            {
                const UVCanvasNode *node = &state->nodes[i];
                double dimension = axis ? node->height : node->width;
                int source = axis ? node->source.t : node->source.s;
                if (!node->selected) { continue; }
                lower = fmax(lower, (-32768.0 - source) / (32.0 * dimension));
                upper = fmin(upper, (32767.0 - source) / (32.0 * dimension));
            }
            values[axis] = fmax(lower, fmin(upper, values[axis]));
        }
    }
    /* Refuse an out-of-range group as a whole and retain its last valid
     * preview. Clipping individual vertices would distort rotation/scale. */
    UVCanvasTryTransform(state, values);
    UVCanvasUpdatePreview(state);
    UVCanvasNotify(hwnd);
}

static BOOL UVCanvasCommit(HWND hwnd, UVCanvasState *state)
{
    BgDocumentUVEdit *edits = malloc((size_t)state->nodecount * sizeof(*edits));
    UVCanvasEdit request;
    int i;
    BOOL result = TRUE;
    if (edits == NULL)
    {
        UVCanvasCancelInteraction(hwnd);
        MessageBox(GetParent(hwnd), "Out of memory transforming UV vertices.", "UV Editor", MB_ICONERROR);
        return FALSE;
    }
    request.vertices = edits; request.count = 0; request.action = NULL;
    if (state->mode == TRANSFORM_ROTATE) { request.action = "Rotate UV Vertices"; }
    else if (state->mode == TRANSFORM_SCALE) { request.action = "Scale UV Vertices"; }
    for (i = 0; i < state->nodecount; i++)
    {
        const UVCanvasNode *node = &state->nodes[i];
        int st[2];
        if (!node->selected) { continue; }
        UVCanvasNodeST(state, node, st);
        if (st[0] == node->source.s && st[1] == node->source.t) { continue; }
        edits[request.count] = node->source;
        edits[request.count].s = st[0]; edits[request.count].t = st[1];
        request.count++;
    }
    /* The synchronous commit rebuilds the scene and replaces our snapshot.
       Drop capture first and never access state/triangles after sending it. */
    UVCanvasCancelInteraction(hwnd);
    if (request.count > 0)
    {
        result = (BOOL)SendMessage(GetParent(hwnd), UVCANVAS_WM_COMMIT, 0, (LPARAM)&request);
    }
    free(edits);
    return result;
}

static LRESULT CALLBACK UVCanvasWndProc(HWND hwnd, UINT message,
                                        WPARAM wparam, LPARAM lparam)
{
    UVCanvasState *state = UVCanvasGetState(hwnd);

    switch (message)
    {
    case WM_CREATE:
        state = (UVCanvasState *)calloc(1, sizeof(*state));
        if (state == NULL) { return -1; }
        state->centeru = state->centerv = 0.5;
        UVCanvasResetTransform(state);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return 0;

    case WM_SIZE:
        if (state != NULL) { UVCanvasResize(hwnd, state); }
        return 0;

    case WM_PAINT:
        UVCanvasPaint(hwnd, state);
        return 0;

    case WM_ERASEBKGND:
        return 1; /* WM_PAINT fills the complete canvas. */

    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        if (state != NULL && !state->panning && state->pixelsperunit > 0)
        {
            int handle = (wparam & (MK_SHIFT | MK_CONTROL)) ? 0
                : UVCanvasPickHandle(state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            UVCanvasCancelInteraction(hwnd);
            UVCanvasSelectionPosition(state, state->pivot);
            UVCanvasGizmo(state, &state->dragorigin);
            state->draghandle = handle;
            state->dragstart.x = GET_X_LPARAM(lparam); state->dragstart.y = GET_Y_LPARAM(lparam);
            state->lastangle = atan2(state->dragorigin.y - state->dragstart.y, state->dragstart.x - state->dragorigin.x);
            state->dragangle = 0;
            state->boxing = state->draghandle == 0;
            state->boxstart = state->boxend = state->dragstart;
            state->boxadd = (wparam & MK_SHIFT) != 0;
            state->boxremove = (wparam & MK_CONTROL) != 0;
            SetCapture(hwnd);
            if (GetCapture() != hwnd) { UVCanvasCancelInteraction(hwnd); }
        }
        return 0;

    case WM_LBUTTONUP:
        if (state != NULL && state->draghandle)
        {
            UVCanvasDrag(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            UVCanvasCommit(hwnd, state);
        }
        else if (state != NULL && state->boxing)
        {
            state->boxend.x = GET_X_LPARAM(lparam); state->boxend.y = GET_Y_LPARAM(lparam);
            UVCanvasSelect(hwnd, state);
        }
        return 0;

    case WM_MBUTTONDOWN:
        if (state != NULL && state->pixelsperunit > 0.0 && !state->boxing && !state->draghandle)
        {
            SetFocus(hwnd);
            SetCapture(hwnd);
            state->panning = GetCapture() == hwnd;
            state->lastmouse.x = GET_X_LPARAM(lparam);
            state->lastmouse.y = GET_Y_LPARAM(lparam);
            if (state->panning) { SetCursor(LoadCursor(NULL, IDC_SIZEALL)); }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (state != NULL && (state->boxing || state->draghandle))
        {
            if (!(wparam & MK_LBUTTON)) { UVCanvasCancelInteraction(hwnd); }
            else if (state->draghandle)
            { UVCanvasDrag(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)); }
            else
            {
                state->boxend.x = GET_X_LPARAM(lparam); state->boxend.y = GET_Y_LPARAM(lparam);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        else if (state != NULL && state->panning)
        {
            if (wparam & MK_MBUTTON)
            {
                UVCanvasPan(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            }
            else { UVCanvasEndPan(hwnd, state); }
        }
        return 0;

    case WM_MBUTTONUP:
        if (state != NULL && state->panning)
        {
            UVCanvasPan(hwnd, state, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            UVCanvasEndPan(hwnd, state);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (state != NULL) { UVCanvasZoom(hwnd, state, wparam, lparam); }
        return 0;

    case WM_SETCURSOR:
        if (state != NULL && state->panning && LOWORD(lparam) == HTCLIENT)
        {
            SetCursor(LoadCursor(NULL, IDC_SIZEALL));
            return TRUE;
        }
        break;

    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
    case WM_KILLFOCUS:
    case WM_DESTROY:
        UVCanvasCancelInteraction(hwnd);
        UVCanvasEndPan(hwnd, state);
        break;

    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        if (state != NULL) { free(state->triangles); }
        if (state != NULL) { free(state->nodes); }
        free(state);
        break;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

HWND UVCanvasCreate(HWND parent, HINSTANCE instance)
{
    WNDCLASS wc;
    if (!GetClassInfo(instance, UVCANVAS_CLASS, &wc))
    {
        ZeroMemory(&wc, sizeof(wc));
        wc.lpfnWndProc = UVCanvasWndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = UVCANVAS_CLASS;
        if (!RegisterClass(&wc)) { return NULL; }
    }
    return CreateWindowEx(0, UVCANVAS_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0,
        parent, NULL, instance, NULL);
}

BOOL UVCanvasSetTriangles(HWND canvas, UVCanvasTriangle *triangles, int count)
{
    UVCanvasState *state = UVCanvasGetState(canvas);
    UVCanvasNode *nodes = NULL;
    int i, nodecount = 0;
    BOOL success = TRUE;
    if (state == NULL) { free(triangles); return FALSE; }
    UVCanvasCancelInteraction(canvas);
    if (triangles != NULL && count > 0)
    {
        if (count <= INT_MAX / 3) { nodes = calloc((size_t)count * 3, sizeof(*nodes)); }
        if (nodes == NULL)
        {
            free(triangles); triangles = NULL; count = 0; success = FALSE;
        }
        else
        {
            for (i = 0; i < count * 3; i++)
            {
                nodes[i].source = triangles[i / 3].source[i % 3];
                nodes[i].width = triangles[i / 3].width;
                nodes[i].height = triangles[i / 3].height;
                nodes[i].sourcecorner = i;
            }
            qsort(nodes, (size_t)count * 3, sizeof(*nodes), UVCanvasCornerCompare);
            for (i = 0; i < count * 3; i++)
            {
                UVCanvasNode node = nodes[i];
                const UVCanvasNode *old = state->nodecount > 0
                    ? bsearch(&node, state->nodes, state->nodecount, sizeof(*nodes), UVCanvasNodeCompare) : NULL;
                if (nodecount == 0 || UVCanvasNodeCompare(&node, &nodes[nodecount - 1]) != 0)
                {
                    node.selected = old != NULL && old->selected;
                    nodes[nodecount++] = node;
                }
                /* A source can appear with differently sized textures. Keep
                   the selected occurrence's basis while it is still present. */
                if (old != NULL && old->width == node.width && old->height == node.height)
                { nodes[nodecount - 1].width = old->width; nodes[nodecount - 1].height = old->height; }
                triangles[node.sourcecorner / 3].nodes[node.sourcecorner % 3] = nodecount - 1;
            }
        }
    }
    free(state->triangles);
    free(state->nodes);
    state->triangles = triangles;
    state->trianglecount = triangles != NULL && count > 0 ? count : 0;
    state->nodes = nodes;
    state->nodecount = nodecount;
    UVCanvasUpdatePreview(state);
    UVCanvasNotify(canvas);
    return success;
}

int UVCanvasGetSelection(HWND canvas, double uv[2])
{
    const UVCanvasState *state = UVCanvasGetState(canvas);
    uv[0] = uv[1] = 0;
    return state != NULL ? UVCanvasSelectionPosition(state, uv) : 0;
}

BOOL UVCanvasSetPosition(HWND canvas, const double uv[2], const char **reason)
{
    UVCanvasState *state = UVCanvasGetState(canvas);
    double position[2];
    int i;
    *reason = "Select exactly one UV vertex to set coordinates.";
    if (state == NULL || UVCanvasGetSelection(canvas, position) != 1) { return FALSE; }
    if (state->mode != TRANSFORM_MOVE) { *reason = "Switch to Move to set UV coordinates."; return FALSE; }
    UVCanvasCancelInteraction(canvas);
    for (i = 0; i < state->nodecount; i++)
    {
        const UVCanvasNode *node = &state->nodes[i];
        double s, t;
        if (!node->selected) { continue; }
        s = round(uv[0] * 32.0 * node->width);
        t = round(uv[1] * 32.0 * node->height);
        if (!isfinite(s) || !isfinite(t) || s < -32768 || s > 32767 || t < -32768 || t > 32767)
        {
            *reason = "UV coordinates exceed GoldenEye's signed 16-bit texture coordinate range.";
            return FALSE;
        }
        state->values[0] = (s - node->source.s) / (32.0 * node->width);
        state->values[1] = (t - node->source.t) / (32.0 * node->height);
        state->draghandle = 3;
        *reason = "";
        return UVCanvasCommit(canvas, state);
    }
    return FALSE;
}

void UVCanvasSetTransformMode(HWND canvas, TransformMode mode)
{
    UVCanvasState *state = UVCanvasGetState(canvas);
    if (state == NULL || mode < TRANSFORM_MOVE || mode > TRANSFORM_SCALE || state->mode == mode) { return; }
    UVCanvasCancelInteraction(canvas);
    UVCanvasEndPan(canvas, state);
    state->mode = mode;
    UVCanvasResetTransform(state);
    UVCanvasNotify(canvas);
}

TransformMode UVCanvasGetTransform(HWND canvas, double values[2], BOOL *limited)
{
    const UVCanvasState *state = UVCanvasGetState(canvas);
    if (values) { values[0] = state ? state->values[0] : 0; values[1] = state ? state->values[1] : 0; }
    if (limited) { *limited = state && state->limited; }
    return state ? state->mode : TRANSFORM_MOVE;
}

BOOL UVCanvasApplyTransform(HWND canvas, const double values[2], const char **reason)
{
    UVCanvasState *state = UVCanvasGetState(canvas);
    *reason = "Select at least two UV vertices to rotate or scale.";
    if (state == NULL || state->mode == TRANSFORM_MOVE) { return FALSE; }
    UVCanvasCancelInteraction(canvas);
    if (UVCanvasSelectionPosition(state, state->pivot) < 2) { return FALSE; }
    if (!UVCanvasTryTransform(state, values))
    {
        UVCanvasResetTransform(state);
        *reason = "The transform exceeds GoldenEye's signed 16-bit texture coordinate range.";
        return FALSE;
    }
    state->draghandle = 3;
    *reason = "";
    return UVCanvasCommit(canvas, state);
}

BOOL UVCanvasHasFaces(HWND canvas)
{
    const UVCanvasState *state = UVCanvasGetState(canvas);
    return state != NULL && state->trianglecount > 0;
}

BOOL UVCanvasProjectFaces(HWND canvas, UVProjection projection, const char **reason)
{
    static const char *actions[UV_PROJECTION_COUNT] = {
        "Planar UV Projection X", "Planar UV Projection Y", "Planar UV Projection Z", "Best Fit UV Projection"
    };
    UVCanvasState *state = UVCanvasGetState(canvas);
    UVProjectionVertex *vertices = NULL;
    UVProjectionFace *faces = NULL;
    BgDocumentUVEdit *edits = NULL;
    UVCanvasEdit request;
    int triangle, corner, i;
    BOOL result = FALSE;
    *reason = "Select background faces to project.";
    if (state == NULL || state->nodecount == 0 || projection < 0 || projection >= UV_PROJECTION_COUNT) { return FALSE; }
    UVCanvasCancelInteraction(canvas);
    vertices = calloc((size_t)state->nodecount, sizeof(*vertices));
    faces = malloc((size_t)state->trianglecount * sizeof(*faces));
    edits = malloc((size_t)state->nodecount * sizeof(*edits));
    if (vertices == NULL || faces == NULL || edits == NULL)
    { *reason = "Out of memory projecting UVs."; goto done; }
    for (triangle = 0; triangle < state->trianglecount; triangle++)
    {
        for (corner = 0; corner < 3; corner++)
        {
            int node = state->triangles[triangle].nodes[corner];
            faces[triangle].vertices[corner] = node;
            memcpy(vertices[node].position, state->triangles[triangle].position[corner], sizeof(vertices[node].position));
        }
    }
    if (!UVProjectionMap(vertices, state->nodecount, faces, state->trianglecount, projection, reason)) { goto done; }
    request.vertices = edits; request.count = 0; request.action = actions[projection];
    for (i = 0; i < state->nodecount; i++)
    {
        const UVCanvasNode *node = &state->nodes[i];
        double s = round(vertices[i].uv[0] * 32.0 * node->width);
        double t = round(vertices[i].uv[1] * 32.0 * node->height);
        if (!isfinite(s) || !isfinite(t) || s < -32768 || s > 32767 || t < -32768 || t > 32767)
        { *reason = "The projected UVs exceed GoldenEye's texture coordinate range."; goto done; }
        if (s == node->source.s && t == node->source.t) { continue; }
        edits[request.count] = node->source;
        edits[request.count].s = (int)s; edits[request.count].t = (int)t;
        request.count++;
    }
    /* Like movement, projection edits each shared source vertex once using
       its current texture-size basis. Rebuilding may replace state below. */
    result = request.count == 0 || (BOOL)SendMessage(GetParent(canvas), UVCANVAS_WM_COMMIT, 0, (LPARAM)&request);
done:
    free(vertices); free(faces); free(edits);
    return result;
}

/* Source identity and the selected texture's coordinate basis, never live S/T. */
typedef struct UVCanvasSelectionVertex {
    BgDocumentVertexRef vertex;
    DWORD vertexid;
    int width, height;
} UVCanvasSelectionVertex;

BOOL UVCanvasCaptureSelection(HWND canvas, void **data, size_t *size)
{
    const UVCanvasState *state = UVCanvasGetState(canvas);
    UVCanvasSelectionVertex *vertices;
    int i, count = 0, at = 0;
    *data = NULL; *size = 0;
    if (!state) { return TRUE; }
    for (i = 0; i < state->nodecount; i++) { count += state->nodes[i].selected != FALSE; }
    if (!count) { return TRUE; }
    vertices = calloc(count, sizeof(*vertices));
    if (!vertices) { return FALSE; }
    for (i = 0; i < state->nodecount; i++)
    {
        const UVCanvasNode *node = &state->nodes[i];
        if (!node->selected) { continue; }
        vertices[at].vertex = node->source.vertex;
        vertices[at].vertexid = node->source.vertexid;
        vertices[at].width = node->width;
        vertices[at++].height = node->height;
    }
    *data = vertices; *size = (size_t)count * sizeof(*vertices);
    return TRUE;
}

BOOL UVCanvasRestoreSelection(HWND canvas, const void *data, size_t size)
{
    UVCanvasState *state = UVCanvasGetState(canvas);
    const UVCanvasSelectionVertex *vertices = data;
    size_t i;
    int nodeindex;
    if (size % sizeof(*vertices) || (size && !data)) { return FALSE; }
    if (!state) { return size == 0; }
    UVCanvasCancelInteraction(canvas);
    for (nodeindex = 0; nodeindex < state->nodecount; nodeindex++) { state->nodes[nodeindex].selected = FALSE; }
    for (i = 0; i < size / sizeof(*vertices); i++)
    {
        UVCanvasNode key = {0}, *node;
        key.source.vertex = vertices[i].vertex;
        key.source.vertexid = vertices[i].vertexid;
        node = state->nodecount ? bsearch(&key, state->nodes, state->nodecount, sizeof(*node), UVCanvasNodeCompare) : NULL;
        if (node)
        {
            node->selected = TRUE;
            node->width = vertices[i].width;
            node->height = vertices[i].height;
        }
    }
    UVCanvasUpdatePreview(state);
    UVCanvasNotify(canvas);
    return TRUE;
}
