#include "uveditor.h"
#include "uvcanvas.h"
#include "viewport.h"
#include "resource.h"

#include <windowsx.h>
#include <commctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

static HWND g_UVEditor;
static HWND g_UVCanvas;
static int g_UVCylinderAxis;
static double g_UVCylinderTexelSize = 4.0;
static int g_UVTextureOpacity = 50; /* Retained when the window is reopened. */

static void UVEditorUpdateOpacity(void)
{
    char text[48];
    snprintf(text, sizeof(text), "Opacity: %d%%", g_UVTextureOpacity);
    SetDlgItemText(g_UVEditor, IDC_UV_TEXTURE_OPACITY_LABEL, text);
    UVCanvasSetTextureOpacity(g_UVCanvas, g_UVTextureOpacity);
}

static void UVEditorRefreshTexture(const char *projectdir, unsigned short textureid, BOOL shared, int count)
{
    TexPixel *pixels = NULL;
    int width = 0, height = 0;
    BOOL loaded = FALSE;
    char text[80];
    if (!count) { lstrcpy(text, "Select textured faces."); }
    else if (!shared) { lstrcpy(text, "Mixed textures"); }
    else if (textureid == BG_TEX_NONE) { lstrcpy(text, "No texture"); }
    else
    {
        pixels = malloc(256 * 256 * sizeof(*pixels));
        if (pixels && projectdir && TexLoadProjectImage(projectdir, textureid, pixels, &width, &height))
        { loaded = width > 0 && width <= 256 && height > 0 && height <= 256; }
        if (!loaded) { free(pixels); pixels = NULL; }
        snprintf(text, sizeof(text), "Image %04X unavailable", textureid);
    }
    /* Replacing or clearing the image is presentation-only: never rebuild UV
     * nodes or emit selection/history notifications when opacity changes. */
    if (!UVCanvasSetTexture(g_UVCanvas, pixels, width, height)) { loaded = FALSE; }
    if (loaded) { snprintf(text, sizeof(text), "Image %04X (%d x %d)", textureid, width, height); }
    SetDlgItemText(g_UVEditor, IDC_UV_TEXTURE_STATUS, text);
    EnableWindow(GetDlgItem(g_UVEditor, IDC_UV_TEXTURE_OPACITY), loaded);
    EnableWindow(GetDlgItem(g_UVEditor, IDC_UV_TEXTURE_OPACITY_LABEL), loaded);
}

static void UVEditorUpdateFields(void)
{
    double uv[2], values[2];
    BOOL limited;
    TransformMode mode = UVCanvasGetTransform(g_UVCanvas, values, &limited);
    int count = UVCanvasGetSelection(g_UVCanvas, uv), axis;
    char text[96];
    if (g_UVEditor == NULL) { return; }
    CheckDlgButton(g_UVEditor, IDC_UV_MOVE, mode == TRANSFORM_MOVE ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(g_UVEditor, IDC_UV_ROTATE, mode == TRANSFORM_ROTATE ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(g_UVEditor, IDC_UV_SCALE, mode == TRANSFORM_SCALE ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemText(g_UVEditor, IDC_UV_TRANSFORM,
        mode == TRANSFORM_ROTATE ? "Rotate" : mode == TRANSFORM_SCALE ? "Scale" : "Move");
    SetDlgItemText(g_UVEditor, IDC_UV_U_LABEL, mode == TRANSFORM_ROTATE ? "Angle" : "U");
    ShowWindow(GetDlgItem(g_UVEditor, IDC_UV_V_LABEL), mode == TRANSFORM_ROTATE ? SW_HIDE : SW_SHOW);
    ShowWindow(GetDlgItem(g_UVEditor, IDC_UV_V), mode == TRANSFORM_ROTATE ? SW_HIDE : SW_SHOW);
    for (axis = IDC_UV_PROJECT_X; axis <= IDC_UV_PROJECT_BEST; axis++)
    { EnableWindow(GetDlgItem(g_UVEditor, axis), UVCanvasHasFaces(g_UVCanvas)); }
    EnableWindow(GetDlgItem(g_UVEditor, IDC_UV_PROJECT_CYLINDER), UVCanvasHasFaces(g_UVCanvas));
    if (count == 0) { lstrcpy(text, "No UV vertices selected"); }
    else { snprintf(text, sizeof(text), "%d UV %s selected", count, count == 1 ? "vertex" : "vertices"); }
    SetDlgItemText(g_UVEditor, IDC_UV_SELECTION, text);
    for (axis = 0; axis < 2; axis++)
    {
        int id = axis ? IDC_UV_V : IDC_UV_U;
        BOOL enabled = mode == TRANSFORM_MOVE ? count >= 1 : count >= 2;
        if (mode == TRANSFORM_ROTATE && axis == 1) { enabled = FALSE; }
        text[0] = '\0';
        if (enabled) { snprintf(text, sizeof(text), "%.9g", mode == TRANSFORM_MOVE ? uv[axis] : values[axis]); }
        EnableWindow(GetDlgItem(g_UVEditor, id), enabled);
        SetDlgItemText(g_UVEditor, id, text);
    }
    SetDlgItemText(g_UVEditor, IDC_UV_HINT, limited
        ? "Texture coordinate limit reached.\r\nThe last valid transform is shown."
        : mode != TRANSFORM_MOVE && count < 2
        ? "Select at least two UV vertices.\r\nThe selection center is the pivot."
        : mode == TRANSFORM_ROTATE
        ? "Drag the ring to rotate; Ctrl snaps to 10 degrees.\r\nEnter applies an angle in degrees; positive turns counterclockwise.\r\nPivot: selection center."
        : mode == TRANSFORM_SCALE
        ? "Drag U or V to scale one axis, or the center to scale both.\r\nEnter applies the factors; 1 = unchanged.\r\nPivot: selection center."
        : count > 0
        ? "U and V show the selection average.\r\nEnter aligns selected vertices to the coordinate in that box.\r\nDrag handles to move the selection."
        : "Select UV vertices to view or set their coordinates.");
}

static void UVEditorSetMode(TransformMode mode)
{
    UVCanvasSetTransformMode(g_UVCanvas, mode);
    UVEditorUpdateFields();
    SetFocus(g_UVCanvas);
}

static BOOL UVEditorReadCoordinate(int id, double *value)
{
    char text[64], *end;
    GetDlgItemText(g_UVEditor, id, text, sizeof(text));
    errno = 0;
    *value = strtod(text, &end);
    if (end == text || errno == ERANGE || !isfinite(*value)) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    return *end == '\0';
}

static void UVEditorApplyFields(void)
{
    double uv[2] = {0, 0};
    TransformMode mode = UVCanvasGetTransform(g_UVCanvas, NULL, NULL);
    const char *reason = "";
    int axis;
    if (mode == TRANSFORM_MOVE)
    {
        axis = GetFocus() == GetDlgItem(g_UVEditor, IDC_UV_V) ? 1 : 0;
        int id = axis ? IDC_UV_V : IDC_UV_U;
        if (!UVEditorReadCoordinate(id, &uv[axis]))
        {
            MessageBox(g_UVEditor, "Enter a finite UV coordinate.", "UV Editor", MB_ICONERROR);
            SetFocus(GetDlgItem(g_UVEditor, id));
            return;
        }
        if (!UVCanvasSetCoordinate(g_UVCanvas, axis, uv[axis], &reason) && reason[0])
        { MessageBox(g_UVEditor, reason, "UV Editor", MB_ICONERROR); }
        UVEditorUpdateFields();
        return;
    }
    for (axis = 0; axis < (mode == TRANSFORM_ROTATE ? 1 : 2); axis++)
    {
        int id = axis ? IDC_UV_V : IDC_UV_U;
        if (!UVEditorReadCoordinate(id, &uv[axis]))
        {
            MessageBox(g_UVEditor, mode == TRANSFORM_ROTATE ? "Enter a finite rotation angle in degrees."
                : mode == TRANSFORM_SCALE ? "Enter a finite scale factor for U and V."
                : "Enter a finite number for each UV coordinate.", "UV Editor", MB_ICONERROR);
            SetFocus(GetDlgItem(g_UVEditor, id));
            return;
        }
    }
    if (!UVCanvasApplyTransform(g_UVCanvas, uv, &reason) && reason[0] != '\0')
    {
        MessageBox(g_UVEditor, reason, "UV Editor", MB_ICONERROR);
    }
    UVEditorUpdateFields();
}

static void UVEditorLayout(HWND hwnd)
{
    static const int tools[] = { IDC_UV_MOVE, IDC_UV_ROTATE, IDC_UV_SCALE, IDC_UV_PROJECT_LABEL, IDC_UV_PROJECT_X,
                                IDC_UV_PROJECT_Y, IDC_UV_PROJECT_Z, IDC_UV_PROJECT_BEST, IDC_UV_PROJECT_CYLINDER,
                                IDC_UV_CYLINDER_AXIS_LABEL, IDC_UV_CYLINDER_AXIS, IDC_UV_CYLINDER_SIZE_LABEL, IDC_UV_CYLINDER_SIZE };
    static const int widths[] = { 8, 9, 8, 5, 6, 6, 6, 8, 11, 5, 9, 14, 7 };
    RECT client;
    RECT units = { 8, 32, 140, 16 };
    HWND closebutton = GetDlgItem(hwnd, IDCANCEL);

    if (closebutton == NULL) { return; }
    GetClientRect(hwnd, &client);
    MapDialogRect(hwnd, &units);
    int margin = units.left, row = units.bottom;
    int panelleft = max(0, client.right - units.right);
    int panelwidth = client.right - panelleft;
    int editwidth = max(0, panelwidth - margin * 7);
    int x = margin, y = margin / 2, buttonheight = row + margin / 2, index;
    /* Wrap the tools when the window is narrowed, keeping every projection
       accessible without covering the canvas or transform panel. */
    for (index = 0; index < (int)(sizeof(tools) / sizeof(tools[0])); index++)
    {
        int width = margin * widths[index];
        if (tools[index] == IDC_UV_CYLINDER_AXIS_LABEL) { x = margin; y += buttonheight + margin / 2; }
        BOOL label = tools[index] == IDC_UV_PROJECT_LABEL || tools[index] == IDC_UV_CYLINDER_AXIS_LABEL
            || tools[index] == IDC_UV_CYLINDER_SIZE_LABEL;
        if (x > margin && x + width > panelleft - margin)
        { x = margin; y += buttonheight + margin / 2; }
        MoveWindow(GetDlgItem(hwnd, tools[index]), x,
                   y + (label ? margin / 2 : 0),
                   width, tools[index] == IDC_UV_CYLINDER_AXIS ? row * 7 : label ? row : buttonheight, TRUE);
        x += width + margin;
    }
    int hintwidth = max(0, panelleft - x - margin);
    ShowWindow(GetDlgItem(hwnd, IDC_UV_TOOL_HINT), hintwidth >= margin * 22 ? SW_SHOW : SW_HIDE);
    MoveWindow(GetDlgItem(hwnd, IDC_UV_TOOL_HINT), x, y + margin / 2, hintwidth, row, TRUE);
    int toolbarheight = max(units.top, y + buttonheight + margin / 2);
    if (g_UVCanvas != NULL)
    {
        int canvasheight = client.bottom - toolbarheight;
        if (canvasheight < 0) { canvasheight = 0; }
        MoveWindow(g_UVCanvas, 0, toolbarheight, panelleft, canvasheight, TRUE);
    }
    MoveWindow(GetDlgItem(hwnd, IDC_UV_TRANSFORM), panelleft + margin, margin, panelwidth - margin * 2, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UV_SELECTION), panelleft + margin, row + margin * 2, panelwidth - margin * 2, row * 2, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UV_U_LABEL), panelleft + margin, row * 3 + margin * 3, margin * 4, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UV_U), panelleft + margin * 6, row * 3 + margin * 3, editwidth, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UV_V_LABEL), panelleft + margin, row * 4 + margin * 4, margin * 4, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UV_V), panelleft + margin * 6, row * 4 + margin * 4, editwidth, row, TRUE);
    int hinttop = row * 6 + margin * 4;
    int closetop = client.bottom - margin - row;
    int previewtop = max(hinttop, closetop - row * 4 - margin * 3);
    int hintheight = max(0, min(row * 5, previewtop - margin - hinttop));
    MoveWindow(GetDlgItem(hwnd, IDC_UV_HINT), panelleft + margin, hinttop, panelwidth - margin * 2, hintheight, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UV_TEXTURE_LABEL), panelleft + margin, previewtop, panelwidth - margin * 2, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UV_TEXTURE_STATUS), panelleft + margin, previewtop + row + margin / 2, panelwidth - margin * 2, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UV_TEXTURE_OPACITY_LABEL), panelleft + margin, previewtop + row * 2 + margin, panelwidth - margin * 2, row, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_UV_TEXTURE_OPACITY), panelleft + margin, previewtop + row * 3 + margin,
               panelwidth - margin * 2, row + margin / 2, TRUE);
    MoveWindow(closebutton, panelleft + margin, closetop,
               panelwidth - margin * 2, row, TRUE);
}

static INT_PTR CALLBACK UVEditorDialogProc(HWND hwnd, UINT message,
                                           WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        g_UVEditor = hwnd;
        for (int i = 0; i < 4; i++)
        { SendDlgItemMessage(hwnd, IDC_UV_CYLINDER_AXIS, CB_ADDSTRING, 0, (LPARAM)(const char *[]){"Auto", "X", "Y", "Z"}[i]); }
        SendDlgItemMessage(hwnd, IDC_UV_CYLINDER_AXIS, CB_SETCURSEL, g_UVCylinderAxis, 0);
        char size[64]; snprintf(size, sizeof(size), "%.9g", g_UVCylinderTexelSize);
        SetDlgItemText(hwnd, IDC_UV_CYLINDER_SIZE, size);
        SendDlgItemMessage(hwnd, IDC_UV_CYLINDER_SIZE, EM_LIMITTEXT, 63, 0);
        CheckDlgButton(hwnd, IDC_UV_MOVE, BST_CHECKED);
        SendDlgItemMessage(hwnd, IDC_UV_U, EM_LIMITTEXT, 63, 0);
        SendDlgItemMessage(hwnd, IDC_UV_V, EM_LIMITTEXT, 63, 0);
        SendDlgItemMessage(hwnd, IDC_UV_TEXTURE_OPACITY, TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
        SendDlgItemMessage(hwnd, IDC_UV_TEXTURE_OPACITY, TBM_SETPAGESIZE, 0, 10);
        SendDlgItemMessage(hwnd, IDC_UV_TEXTURE_OPACITY, TBM_SETPOS, TRUE, g_UVTextureOpacity);
        UVEditorUpdateOpacity();
        UVEditorUpdateFields();
        UVEditorLayout(hwnd);
        return TRUE;

    case WM_SIZE:
        UVEditorLayout(hwnd);
        return TRUE;

    case WM_HSCROLL:
        if ((HWND)lparam == GetDlgItem(hwnd, IDC_UV_TEXTURE_OPACITY))
        {
            g_UVTextureOpacity = (int)SendDlgItemMessage(hwnd, IDC_UV_TEXTURE_OPACITY, TBM_GETPOS, 0, 0);
            UVEditorUpdateOpacity();
            return TRUE;
        }
        break;

    case UVCANVAS_WM_SELECTION_CHANGED:
        UVEditorUpdateFields();
        SendMessage(GetWindow(hwnd, GW_OWNER), UVEDITOR_WM_SELECTION_CHANGED, 0, 0);
        return TRUE;

    case UVCANVAS_WM_COMMIT:
    case UVCANVAS_WM_COMMIT_FACES:
    {
        LRESULT result = SendMessage(GetWindow(hwnd, GW_OWNER),
            message == UVCANVAS_WM_COMMIT_FACES ? UVEDITOR_WM_APPLY_FACES : UVEDITOR_WM_APPLY, 0, lparam);
        SetWindowLongPtr(hwnd, DWLP_MSGRESULT, result);
        return TRUE;
    }

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO *limits = (MINMAXINFO *)lparam;
        RECT minimum = { 0, 0, 340, 240 };

        MapDialogRect(hwnd, &minimum);
        AdjustWindowRectEx(&minimum, (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE),
                           FALSE, (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        limits->ptMinTrackSize.x = minimum.right - minimum.left;
        limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wparam) == IDC_UV_PROJECT_CYLINDER)
        {
            const char *reason = "";
            double size;
            if (!UVEditorReadCoordinate(IDC_UV_CYLINDER_SIZE, &size) || size <= 0)
            { MessageBox(hwnd, "Enter a positive texel size.", "UV Editor", MB_ICONERROR); return TRUE; }
            g_UVCylinderTexelSize = size;
            g_UVCylinderAxis = (int)SendDlgItemMessage(hwnd, IDC_UV_CYLINDER_AXIS, CB_GETCURSEL, 0, 0);
            if (!UVCanvasProjectCylinder(g_UVCanvas, g_UVCylinderAxis, size, &reason) && reason[0])
            { MessageBox(hwnd, reason, "UV Editor", MB_ICONERROR); }
            UVEditorUpdateFields();
            SetFocus(g_UVCanvas);
            return TRUE;
        }
        if (LOWORD(wparam) >= IDC_UV_PROJECT_X && LOWORD(wparam) <= IDC_UV_PROJECT_BEST)
        {
            const char *reason = "";
            UVProjection projection = (UVProjection)(LOWORD(wparam) - IDC_UV_PROJECT_X);
            if (!UVCanvasProjectFaces(g_UVCanvas, projection, &reason) && reason[0] != '\0')
            { MessageBox(hwnd, reason, "UV Editor", MB_ICONERROR); }
            UVEditorUpdateFields();
            SetFocus(g_UVCanvas);
            return TRUE;
        }
        if (LOWORD(wparam) == IDC_UV_MOVE || LOWORD(wparam) == IDC_UV_ROTATE || LOWORD(wparam) == IDC_UV_SCALE)
        {
            UVEditorSetMode(LOWORD(wparam) == IDC_UV_ROTATE ? TRANSFORM_ROTATE
                : LOWORD(wparam) == IDC_UV_SCALE ? TRANSFORM_SCALE : TRANSFORM_MOVE);
            return TRUE;
        }
        if (LOWORD(wparam) == IDOK)
        {
            if (GetFocus() == GetDlgItem(hwnd, IDC_UV_U) || GetFocus() == GetDlgItem(hwnd, IDC_UV_V))
            { UVEditorApplyFields(); }
            return TRUE;
        }
        if (LOWORD(wparam) == IDCANCEL)
        {
            DestroyWindow(hwnd);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        /* Modeless dialogs use DestroyWindow, not EndDialog or WM_QUIT. */
        DestroyWindow(hwnd);
        return TRUE;

    case WM_NCDESTROY:
        if (g_UVEditor == hwnd)
        {
            g_UVCanvas = NULL;
            g_UVEditor = NULL;
            SendMessage(GetWindow(hwnd, GW_OWNER), UVEDITOR_WM_VISIBILITY, FALSE, 0);
        }
        break;
    }
    return FALSE;
}

BOOL UVEditorShow(HWND owner, HINSTANCE instance)
{
    /* A different editor starts a fresh context: no stale BG/model corner
     * identities, selection history, drags or notifications cross owners. */
    if (g_UVEditor && !UVEditorIsOpen(owner)) { DestroyWindow(g_UVEditor); }
    if (g_UVEditor == NULL)
    {
        /* A top-level owned window always stays above its owner and follows
           its minimize/restore and destruction. CreateDialog keeps the main
           editor usable while the UV Editor is open. */
        g_UVEditor = CreateDialog(instance, MAKEINTRESOURCE(IDD_UV_EDITOR),
                                  owner, UVEditorDialogProc);
        if (g_UVEditor == NULL) { return FALSE; }
        g_UVCanvas = UVCanvasCreate(g_UVEditor, instance);
        if (g_UVCanvas == NULL)
        {
            DestroyWindow(g_UVEditor);
            return FALSE;
        }
        UVEditorUpdateOpacity();
        UVEditorLayout(g_UVEditor);
    }

    SendMessage(owner, UVEDITOR_WM_VISIBILITY, TRUE, 0);
    ShowWindow(g_UVEditor, SW_SHOWNORMAL);
    SetForegroundWindow(g_UVEditor);
    return TRUE;
}

void UVEditorRefreshSelection(HWND viewport, const BgDocument *document, const char *projectdir)
{
    BgFaceRef *refs;
    UVCanvasTriangle *triangles;
    int count, index, output = 0;
    unsigned short textureid = BG_TEX_NONE;
    BOOL shared = TRUE;

    if (!UVEditorIsOpen(GetParent(viewport)) || g_UVCanvas == NULL) { return; }
    if (ViewportGetTool(viewport) != EDITOR_TOOL_FACE_SELECT)
    { UVCanvasSetTriangles(g_UVCanvas, NULL, 0); UVEditorRefreshTexture(NULL, BG_TEX_NONE, TRUE, 0); return; }
    count = ViewportGetSelectedBgFaceCount(viewport);
    if (count <= 0)
    { UVCanvasSetTriangles(g_UVCanvas, NULL, 0); UVEditorRefreshTexture(NULL, BG_TEX_NONE, TRUE, 0); return; }

    refs = (BgFaceRef *)malloc((size_t)count * sizeof(*refs));
    triangles = (UVCanvasTriangle *)malloc((size_t)count * sizeof(*triangles));
    if (refs == NULL || triangles == NULL)
    {
        free(refs);
        free(triangles);
        UVCanvasSetTriangles(g_UVCanvas, NULL, 0);
        UVEditorRefreshTexture(NULL, BG_TEX_NONE, TRUE, 0);
        MessageBox(g_UVEditor, "Out of memory displaying the selected UVs.",
                   "UV Editor", MB_ICONERROR);
        return;
    }
    if (!ViewportGetSelectedBgFaces(viewport, refs, count))
    {
        free(refs);
        free(triangles);
        UVCanvasSetTriangles(g_UVCanvas, NULL, 0);
        UVEditorRefreshTexture(NULL, BG_TEX_NONE, TRUE, 0);
        return;
    }

    for (index = 0; index < count; index++)
    {
        const BgDocumentRoom *room;
        const BgDocumentFace *face = BgDocumentFindFace(document, &refs[index], &room);
        int corner, width, height;
        if (face == NULL || room == NULL || room->vertices == NULL
            || face->vertexindices[0] >= room->vertexcount
            || face->vertexindices[1] >= room->vertexcount
            || face->vertexindices[2] >= room->vertexcount)
        {
            shared = FALSE;
            continue;
        }
        if (index == 0) { textureid = face->textureid; }
        else if (textureid != face->textureid) { shared = FALSE; }
        ViewportGetTextureSize(viewport, face->textureid, &width, &height);
        triangles[output].width = width;
        triangles[output].height = height;
        triangles[output].face = refs[index];
        triangles[output].seams = face->uvseams;
        for (corner = 0; corner < 3; corner++)
        {
            const BgDocumentVertex *vertex = &room->vertices[face->vertexindices[corner]];
            triangles[output].source[corner].vertex.room = refs[index].room;
            triangles[output].source[corner].vertex.index = face->vertexindices[corner];
            triangles[output].source[corner].vertexid = vertex->id;
            triangles[output].source[corner].s = vertex->s;
            triangles[output].source[corner].t = vertex->t;
            /* Include room origins so selections spanning rooms share one
               world-space projection plane. Keep the calculation in double. */
            triangles[output].position[corner][0] = (room->origin[0] + (double)vertex->x) / document->levelscale;
            triangles[output].position[corner][1] = (room->origin[1] + (double)vertex->y) / document->levelscale;
            triangles[output].position[corner][2] = (room->origin[2] + (double)vertex->z) / document->levelscale;
            /* Authored S/T is in 1/32 texels. Read the source document:
               environment mapping rewrites the viewport's preview UVs. */
            triangles[output].uv[corner][0] = vertex->s / (32.0 * width);
            triangles[output].uv[corner][1] = vertex->t / (32.0 * height);
        }
        output++;
    }
    free(refs);
    if (!UVCanvasSetTriangles(g_UVCanvas, triangles, output))
    {
        UVEditorRefreshTexture(NULL, BG_TEX_NONE, TRUE, 0);
        MessageBox(g_UVEditor, "Out of memory displaying the selected UVs.", "UV Editor", MB_ICONERROR);
    }
    else { UVEditorRefreshTexture(projectdir, textureid, shared, output); }
}

void UVEditorSetOverlay(HWND owner, UVCanvasTriangle *triangles, int count,
    const char *projectdir, unsigned short texture, BOOL shared, const char *title)
{
    if (!UVEditorIsOpen(owner) || !g_UVCanvas) { free(triangles); return; }
    SetWindowText(g_UVEditor, title ? title : "UV Editor");
    SetDlgItemText(g_UVEditor, IDC_UV_CYLINDER_SIZE_LABEL, "Texel size (units):");
    if (!UVCanvasSetTriangles(g_UVCanvas, triangles, count))
    {
        UVEditorRefreshTexture(NULL, BG_TEX_NONE, TRUE, 0);
        MessageBox(g_UVEditor, "Out of memory displaying model UVs.", "UV Editor", MB_ICONERROR);
    }
    else UVEditorRefreshTexture(projectdir, texture, shared, count);
}

BOOL UVEditorHandleMessage(MSG *message)
{
    BOOL uvmessage;

    if (g_UVEditor == NULL || message == NULL) { return FALSE; }
    uvmessage = message->hwnd == g_UVEditor || IsChild(g_UVEditor, message->hwnd);

    /* Wheel messages may target the focus window instead of the hovered
       window. Route them before the main editor can scroll a panel behind
       the UV canvas, including when the Close button has keyboard focus. */
    if (message->message == WM_MOUSEWHEEL
        && (GetCapture() == NULL || GetCapture() == g_UVCanvas))
    {
        POINT mouse = { GET_X_LPARAM(message->lParam), GET_Y_LPARAM(message->lParam) };
        HWND target = WindowFromPoint(mouse);
        if (g_UVCanvas != NULL && target == g_UVCanvas)
        {
            SendMessage(g_UVCanvas, WM_MOUSEWHEEL, message->wParam, message->lParam);
            return TRUE;
        }
        if (uvmessage)
        {
            HWND owner = GetWindow(g_UVEditor, GW_OWNER);
            if (target == owner || IsChild(owner, target))
            {
                SendMessage(owner, WM_MOUSEWHEEL, message->wParam, message->lParam);
            }
            return TRUE;
        }
    }
    if (!uvmessage) { return FALSE; }
    /* Ctrl+T stays with the current model/background owner. */
    if (message->message == WM_KEYDOWN && message->wParam == 'T'
        && (GetKeyState(VK_CONTROL) & 0x8000)
        && !(GetKeyState(VK_MENU) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000))
    {
        SetForegroundWindow(g_UVEditor);
        return TRUE;
    }
    /* Save Project is shared with the main frame's accelerators. */
    if (message->message == WM_KEYDOWN
        && message->wParam == 'S'
        && (GetKeyState(VK_CONTROL) & 0x8000)
        && !(GetKeyState(VK_MENU) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000))
    { return FALSE; }


    if (message->message == WM_KEYDOWN)
    {
        char classname[32] = "";
        BOOL edit;
        GetClassName(message->hwnd, classname, sizeof(classname));
        edit = lstrcmpi(classname, "Edit") == 0;
        if (message->wParam == VK_ESCAPE)
        {
            if (UVCanvasCancelInteraction(g_UVCanvas)) { return TRUE; }
            if (edit) { UVEditorUpdateFields(); return TRUE; }
        }
        if (edit && message->wParam == VK_RETURN)
        { UVEditorApplyFields(); return TRUE; }
        if (!edit && (message->wParam == 'W' || message->wParam == 'E' || message->wParam == 'R')
            && !(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000))
        {
            UVEditorSetMode(message->wParam == 'E' ? TRANSFORM_ROTATE
                : message->wParam == 'R' ? TRANSFORM_SCALE : TRANSFORM_MOVE);
            return TRUE;
        }
        if (!edit && (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)
            && (message->wParam == 'Z' || message->wParam == 'Y'))
        {
            UVCanvasCancelInteraction(g_UVCanvas);
            SendMessage(GetWindow(g_UVEditor, GW_OWNER), UVEDITOR_WM_HISTORY,
                        message->wParam == 'Y' || (GetKeyState(VK_SHIFT) & 0x8000), 0);
            return TRUE;
        }
    }

    /* Keep Tab, Escape, and Alt+F4 local to this window. Dispatch any other
       input here so the main editor's accelerators cannot consume it. */
    if (!IsDialogMessage(g_UVEditor, message))
    {
        TranslateMessage(message);
        DispatchMessage(message);
    }
    return TRUE;
}

BOOL UVEditorIsOpen(HWND owner)
{
    return g_UVEditor != NULL && GetWindow(g_UVEditor, GW_OWNER) == owner;
}

BOOL UVEditorCaptureSelection(HWND owner, void **data, size_t *size)
{
    *data = NULL; *size = 0;
    if (!UVEditorIsOpen(owner)) return TRUE;
    return UVCanvasCaptureSelection(g_UVCanvas, data, size);
}

BOOL UVEditorRestoreSelection(HWND owner, const void *data, size_t size)
{
    if (!UVEditorIsOpen(owner)) return size == 0;
    return UVCanvasRestoreSelection(g_UVCanvas, data, size);
}

void UVEditorCancelInteraction(HWND owner)
{
    if (!UVEditorIsOpen(owner)) return;
    UVCanvasCancelInteraction(g_UVCanvas);
}
