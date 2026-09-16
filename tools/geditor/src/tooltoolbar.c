#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <wincodec.h>
#include <stdlib.h>

#include "resource.h"
#include "tooltoolbar.h"

#define TOOLTOOLBAR_CLASS "GEditorToolToolbar"
#define TOOLTOOLBAR_BUTTON_SIZE 32
#define TOOLTOOLBAR_MARGIN 4
#define TOOLTOOLBAR_FIRST_ID 3001
#define TOOLTOOLBAR_SNAP_INDEX EDITOR_TOOL_COUNT
#define TOOLTOOLBAR_BUTTON_COUNT (EDITOR_TOOL_COUNT + 1)
#define TOOLTOOLBAR_MENU_FIRST_ID 3101
#define TOOLTOOLBAR_MENU_WIDTH 80
#define TOOLTOOLBAR_CORRECT_ID 3201
#define TOOLTOOLBAR_CORRECT_WIDTH 164

static const struct {
    const char *name;
    int images[2]; /* off, on */
} g_Tools[TOOLTOOLBAR_BUTTON_COUNT] = {
    { "Vertex Select (1)", { IDR_VERTEX_SELECT_OFF, IDR_VERTEX_SELECT_ON } },
    { "Edge Select (2)",   { IDR_EDGE_SELECT_OFF, IDR_EDGE_SELECT_ON } },
    { "Face Select (3)",   { IDR_FACE_SELECT_OFF, IDR_FACE_SELECT_ON } },
    { "Vertex Paint (4)",  { IDR_VERTEX_PAINT_OFF, IDR_VERTEX_PAINT_ON } },
    { "Vertex Snap (V; vertex mode only)", { IDR_VERTEX_SNAP_OFF, IDR_VERTEX_SNAP_ON } }
};

typedef struct ToolToolbarState {
    EditorTool tool;
    BOOL vertexsnap;
    HWND buttons[TOOLTOOLBAR_BUTTON_COUNT];
    HWND menus[TOOLTOOLBAR_MENU_COUNT];
    HWND correctattributes;
    HWND tooltip;
    HBITMAP images[TOOLTOOLBAR_BUTTON_COUNT][2];
} ToolToolbarState;

/* Decode embedded PNGs once at creation, so installed copies of the exe
   never need to locate the source asset folder. The supplied buttons are
   opaque; a top-down BGR DIB can be drawn directly by GDI. */
static HBITMAP ToolToolbarLoadImage(HINSTANCE instance,
                                   IWICImagingFactory *factory, int id)
{
    HRSRC resource = FindResource(instance, MAKEINTRESOURCE(id), RT_RCDATA);
    HGLOBAL data;
    BYTE *bytes;
    DWORD size;
    IWICStream *stream = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    HBITMAP bitmap = NULL;
    BITMAPINFO info;
    void *pixels;
    UINT width, height;
    BOOL success = FALSE;

    if (resource == NULL) { return NULL; }
    data = LoadResource(instance, resource);
    bytes = (BYTE *)LockResource(data);
    size = SizeofResource(instance, resource);
    if (bytes == NULL || size == 0) { return NULL; }

    if (FAILED(IWICImagingFactory_CreateStream(factory, &stream))
        || FAILED(IWICStream_InitializeFromMemory(stream, bytes, size))
        || FAILED(IWICImagingFactory_CreateDecoderFromStream(factory,
            (IStream *)stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder))
        || FAILED(IWICBitmapDecoder_GetFrame(decoder, 0, &frame))
        || FAILED(IWICBitmapFrameDecode_GetSize(frame, &width, &height))
        || width != TOOLTOOLBAR_BUTTON_SIZE || height != TOOLTOOLBAR_BUTTON_SIZE
        || FAILED(IWICImagingFactory_CreateFormatConverter(factory, &converter))
        || FAILED(IWICFormatConverter_Initialize(converter,
            (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppBGR,
            WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom)))
    {
        goto done;
    }

    ZeroMemory(&info, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = (LONG)width;
    info.bmiHeader.biHeight = -(LONG)height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &pixels, NULL, 0);
    success = bitmap != NULL
        && SUCCEEDED(IWICFormatConverter_CopyPixels(converter, NULL,
                         width * 4, width * height * 4, (BYTE *)pixels));

done:
    if (converter != NULL) { IWICFormatConverter_Release(converter); }
    if (frame != NULL) { IWICBitmapFrameDecode_Release(frame); }
    if (decoder != NULL) { IWICBitmapDecoder_Release(decoder); }
    if (stream != NULL) { IWICStream_Release(stream); }
    if (!success && bitmap != NULL) { DeleteObject(bitmap); bitmap = NULL; }
    return bitmap;
}

static BOOL ToolToolbarLoadImages(HINSTANCE instance, ToolToolbarState *state)
{
    IWICImagingFactory *factory = NULL;
    HRESULT initialized = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    BOOL success = FALSE;
    int tool, active;

    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
    {
        return FALSE;
    }
    if (SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory, NULL,
                      CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                      (void **)&factory)))
    {
        success = TRUE;
        for (tool = 0; tool < TOOLTOOLBAR_BUTTON_COUNT; tool++)
        {
            for (active = 0; active < 2; active++)
            {
                state->images[tool][active] = ToolToolbarLoadImage(
                    instance, factory, g_Tools[tool].images[active]);
                if (state->images[tool][active] == NULL) { success = FALSE; }
            }
        }
        IWICImagingFactory_Release(factory);
    }
    if (SUCCEEDED(initialized)) { CoUninitialize(); }
    return success;
}

/* The height query and WM_SIZE use the same wrapping calculation. */
static int ToolToolbarLayout(ToolToolbarState *state, int width)
{
    int x = TOOLTOOLBAR_MARGIN, y = TOOLTOOLBAR_MARGIN;
    for (int i = 0; i < TOOLTOOLBAR_BUTTON_COUNT + TOOLTOOLBAR_MENU_COUNT + 1; i++)
    {
        BOOL correct = i == TOOLTOOLBAR_BUTTON_COUNT + TOOLTOOLBAR_MENU_COUNT;
        BOOL menu = i >= TOOLTOOLBAR_BUTTON_COUNT;
        int buttonwidth = correct ? TOOLTOOLBAR_CORRECT_WIDTH
            : menu ? TOOLTOOLBAR_MENU_WIDTH : TOOLTOOLBAR_BUTTON_SIZE;
        if (correct && buttonwidth > width - 2 * TOOLTOOLBAR_MARGIN)
        { buttonwidth = width > 2 * TOOLTOOLBAR_MARGIN ? width - 2 * TOOLTOOLBAR_MARGIN : 1; }
        if (x > TOOLTOOLBAR_MARGIN && x + buttonwidth + TOOLTOOLBAR_MARGIN > width)
        {
            x = TOOLTOOLBAR_MARGIN;
            y += TOOLTOOLBAR_BUTTON_SIZE + TOOLTOOLBAR_MARGIN;
        }
        if (state)
        {
            HWND button = correct ? state->correctattributes
                : menu ? state->menus[i - TOOLTOOLBAR_BUTTON_COUNT] : state->buttons[i];
            MoveWindow(button, x, y, buttonwidth, TOOLTOOLBAR_BUTTON_SIZE, TRUE);
        }
        x += buttonwidth + TOOLTOOLBAR_MARGIN;
    }
    return y + TOOLTOOLBAR_BUTTON_SIZE + TOOLTOOLBAR_MARGIN;
}

int ToolToolbarGetHeight(int width)
{
    return ToolToolbarLayout(NULL, width);
}

static LRESULT CALLBACK ToolToolbarWndProc(HWND hwnd, UINT message,
                                           WPARAM wparam, LPARAM lparam)
{
    ToolToolbarState *state = (ToolToolbarState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    int tool;

    switch (message)
    {
    case WM_CREATE:
    {
        HINSTANCE instance = ((CREATESTRUCT *)lparam)->hInstance;

        state = (ToolToolbarState *)calloc(1, sizeof(*state));
        if (state == NULL) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        state->tool = EDITOR_TOOL_FACE_SELECT;
        if (!ToolToolbarLoadImages(instance, state))
        {
            MessageBox(hwnd, "Could not load the toolbar button graphics.",
                       "GEditor", MB_ICONERROR);
            return -1;
        }

        state->tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hwnd, NULL, instance, NULL);
        for (tool = 0; tool < TOOLTOOLBAR_BUTTON_COUNT; tool++)
        {
            TOOLINFO tip;

            state->buttons[tool] = CreateWindowEx(0, "BUTTON", g_Tools[tool].name,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                TOOLTOOLBAR_MARGIN + tool * (TOOLTOOLBAR_BUTTON_SIZE + TOOLTOOLBAR_MARGIN),
                TOOLTOOLBAR_MARGIN, TOOLTOOLBAR_BUTTON_SIZE, TOOLTOOLBAR_BUTTON_SIZE,
                hwnd, (HMENU)(INT_PTR)(TOOLTOOLBAR_FIRST_ID + tool), instance, NULL);
            if (state->buttons[tool] == NULL) { return -1; }
            if (tool == TOOLTOOLBAR_SNAP_INDEX) { EnableWindow(state->buttons[tool], FALSE); }
            ZeroMemory(&tip, sizeof(tip));
            tip.cbSize = sizeof(tip);
            tip.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            tip.hwnd = hwnd;
            tip.uId = (UINT_PTR)state->buttons[tool];
            tip.lpszText = (char *)g_Tools[tool].name;
            SendMessage(state->tooltip, TTM_ADDTOOL, 0, (LPARAM)&tip);
        }
        for (tool = 0; tool < TOOLTOOLBAR_MENU_COUNT; tool++)
        {
            static const WCHAR *names[] = { L"Vertex  \x25be", L"Edge  \x25be", L"Face  \x25be" };
            state->menus[tool] = CreateWindowExW(0, L"BUTTON", names[tool],
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                0, 0, TOOLTOOLBAR_MENU_WIDTH, TOOLTOOLBAR_BUTTON_SIZE,
                hwnd, (HMENU)(INT_PTR)(TOOLTOOLBAR_MENU_FIRST_ID + tool), instance, NULL);
            if (!state->menus[tool]) { return -1; }
            SendMessage(state->menus[tool], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
        }
        state->correctattributes = CreateWindowEx(0, "BUTTON", "Correct Face Attributes",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_MULTILINE,
            0, 0, TOOLTOOLBAR_CORRECT_WIDTH, TOOLTOOLBAR_BUTTON_SIZE,
            hwnd, (HMENU)(INT_PTR)TOOLTOOLBAR_CORRECT_ID, instance, NULL);
        if (!state->correctattributes) { return -1; }
        SendMessage(state->correctattributes, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
        SendMessage(state->correctattributes, BM_SETCHECK, BST_CHECKED, 0);
        return 0;
    }

    case WM_SIZE:
        if (state) { ToolToolbarLayout(state, LOWORD(lparam)); }
        return 0;

    case WM_COMMAND:
        tool = LOWORD(wparam) - TOOLTOOLBAR_MENU_FIRST_ID;
        if (state && HIWORD(wparam) == BN_CLICKED && tool >= 0 && tool < TOOLTOOLBAR_MENU_COUNT)
        {
            SendMessage(state->menus[tool], BM_SETSTATE, TRUE, 0);
            SendMessage(GetParent(hwnd), TOOLTOOLBAR_WM_MENU, tool, (LPARAM)state->menus[tool]);
            SendMessage(state->menus[tool], BM_SETSTATE, FALSE, 0);
            return 0;
        }
        tool = LOWORD(wparam) - TOOLTOOLBAR_FIRST_ID;
        if (HIWORD(wparam) == BN_CLICKED && tool >= 0 && tool < TOOLTOOLBAR_BUTTON_COUNT)
        {
            if (tool == TOOLTOOLBAR_SNAP_INDEX)
            {
                SendMessage(GetParent(hwnd), EDITTOOL_WM_TOGGLE_VERTEX_SNAP, 0, 0);
            }
            else
            {
                SendMessage(GetParent(hwnd), EDITTOOL_WM_SELECT, (WPARAM)tool, 0);
            }
            return 0;
        }
        break;

    case WM_DRAWITEM:
        if (state != NULL)
        {
            const DRAWITEMSTRUCT *draw = (const DRAWITEMSTRUCT *)lparam;

            tool = (int)draw->CtlID - TOOLTOOLBAR_FIRST_ID;
            if (draw->CtlType == ODT_BUTTON && tool >= 0 && tool < TOOLTOOLBAR_BUTTON_COUNT)
            {
                HDC source = CreateCompatibleDC(draw->hDC);
                HGDIOBJ previous;

                if (source == NULL) { return FALSE; }
                previous = SelectObject(source, state->images[tool][tool == TOOLTOOLBAR_SNAP_INDEX
                    ? state->vertexsnap : state->tool == (EditorTool)tool]);
                BitBlt(draw->hDC, draw->rcItem.left, draw->rcItem.top,
                    TOOLTOOLBAR_BUTTON_SIZE, TOOLTOOLBAR_BUTTON_SIZE, source, 0, 0, SRCCOPY);
                SelectObject(source, previous);
                DeleteDC(source);
                if ((draw->itemState & (ODS_FOCUS | ODS_NOFOCUSRECT)) == ODS_FOCUS)
                {
                    RECT focus = draw->rcItem;

                    InflateRect(&focus, -3, -3);
                    DrawFocusRect(draw->hDC, &focus);
                }
                return TRUE;
            }
        }
        break;

    case WM_NCDESTROY:
        if (state != NULL)
        {
            int active;

            if (state->tooltip != NULL) { DestroyWindow(state->tooltip); }
            for (tool = 0; tool < TOOLTOOLBAR_BUTTON_COUNT; tool++)
            {
                for (active = 0; active < 2; active++)
                {
                    if (state->images[tool][active] != NULL)
                    {
                        DeleteObject(state->images[tool][active]);
                    }
                }
            }
            free(state);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        break;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

BOOL ToolToolbarRegisterClass(HINSTANCE hinstance)
{
    WNDCLASS wc;
    INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_WIN95_CLASSES };

    if (!InitCommonControlsEx(&controls)) { return FALSE; }
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = ToolToolbarWndProc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = TOOLTOOLBAR_CLASS;
    return RegisterClass(&wc) != 0;
}

HWND ToolToolbarCreate(HWND parent, HINSTANCE hinstance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, TOOLTOOLBAR_CLASS, "Tools",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 16, TOOLTOOLBAR_HEIGHT, parent, NULL, hinstance, NULL);
}

void ToolToolbarSetTool(HWND toolbar, EditorTool tool)
{
    ToolToolbarState *state = (ToolToolbarState *)GetWindowLongPtr(toolbar, GWLP_USERDATA);
    int index;

    if (state == NULL || tool < 0 || tool >= EDITOR_TOOL_COUNT || tool == state->tool)
    {
        return;
    }
    state->tool = tool;
    if (tool != EDITOR_TOOL_VERTEX_SELECT) { state->vertexsnap = FALSE; }
    EnableWindow(state->buttons[TOOLTOOLBAR_SNAP_INDEX], tool == EDITOR_TOOL_VERTEX_SELECT);
    for (index = 0; index < TOOLTOOLBAR_BUTTON_COUNT; index++)
    {
        InvalidateRect(state->buttons[index], NULL, FALSE);
    }
}

void ToolToolbarSetVertexSnap(HWND toolbar, BOOL enabled)
{
    ToolToolbarState *state = (ToolToolbarState *)GetWindowLongPtr(toolbar, GWLP_USERDATA);

    if (state == NULL) { return; }
    state->vertexsnap = enabled && state->tool == EDITOR_TOOL_VERTEX_SELECT;
    InvalidateRect(state->buttons[TOOLTOOLBAR_SNAP_INDEX], NULL, FALSE);
}

BOOL ToolToolbarCorrectFaceAttributes(HWND toolbar)
{
    ToolToolbarState *state = (ToolToolbarState *)GetWindowLongPtr(toolbar, GWLP_USERDATA);
    return state && SendMessage(state->correctattributes, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

BOOL ToolToolbarHandleMessage(HWND toolbar, MSG *message)
{
    HWND frame;

    if (toolbar == NULL || message == NULL) { return FALSE; }
    frame = GetParent(toolbar);
    if (message->hwnd != frame && !IsChild(frame, message->hwnd)) { return FALSE; }

    if (message->message == WM_KEYDOWN
        && !(GetKeyState(VK_CONTROL) & 0x8000)
        && !(GetKeyState(VK_MENU) & 0x8000)
        && !(GetKeyState(VK_SHIFT) & 0x8000))
    {
        char classname[32] = "";
        int tool = -1;

        /* The Transform fields must still accept numbers, as must any
           other native edit controls added to the editor in future. */
        GetClassName(message->hwnd, classname, sizeof(classname));
        if (lstrcmpi(classname, "Edit") == 0) { return FALSE; }
        if (message->wParam == 'V')
        {
            /* Holding V must not repeatedly toggle the mode. */
            if (!(message->lParam & (1L << 30)))
            {
                SendMessage(frame, EDITTOOL_WM_TOGGLE_VERTEX_SNAP, 0, 0);
            }
            return TRUE;
        }
        if (message->wParam >= '1' && message->wParam <= '4')
        {
            tool = (int)(message->wParam - '1');
        }
        else if (message->wParam >= VK_NUMPAD1 && message->wParam <= VK_NUMPAD4)
        {
            tool = (int)(message->wParam - VK_NUMPAD1);
        }
        if (tool >= 0)
        {
            SendMessage(frame, EDITTOOL_WM_SELECT, (WPARAM)tool, 0);
            return TRUE;
        }
    }

    if (message->message == WM_KEYDOWN && IsChild(toolbar, message->hwnd)
        && (message->wParam == VK_RETURN
            || (message->wParam == VK_DOWN && GetDlgCtrlID(message->hwnd) >= TOOLTOOLBAR_MENU_FIRST_ID
                && GetDlgCtrlID(message->hwnd) < TOOLTOOLBAR_MENU_FIRST_ID + TOOLTOOLBAR_MENU_COUNT)))
    {
        SendMessage(message->hwnd, BM_CLICK, 0, 0);
        return TRUE;
    }
    return (message->hwnd == toolbar || IsChild(toolbar, message->hwnd))
        && IsDialogMessage(toolbar, message);
}
