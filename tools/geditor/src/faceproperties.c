/* Scrollable face controls shared by single- and multiple-face selections. */
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>

#include "faceproperties.h"
#include "browser.h"

#define FACEPROPERTIES_CLASS "GEditorFaceProperties"
#define FACEPROPERTIES_MARGIN 4
#define FACEPROPERTIES_PREVIEW_SIZE 64

enum { FACE_SUMMARY, FACE_ROOM_LABEL, FACE_ROOM, FACE_TEXTURE_LABEL, FACE_TEXTURE_THUMB, FACE_TEXTURE_FIND,
       FACE_DETAIL_LABEL, FACE_DETAIL_THUMB, FACE_DETAIL_FIND, FACE_DETAIL_INFO,
       FACE_RENDER_INFO, FACE_RENDER, FACE_RENDER_HELP,
       FACE_CULL_LABEL, FACE_CULL,
       FACE_WRAP_LABEL, FACE_U_LABEL, FACE_U, FACE_V_LABEL, FACE_V,
       FACE_SELECTION_HELP, FACE_CONTROL_COUNT };

typedef struct FacePropertiesState {
    HWND controls[FACE_CONTROL_COUNT];
    HWND tooltip;
    TexThumb thumbnail;
    unsigned char pixels[TEX_THUMB_MAX * TEX_THUMB_MAX * 4];
    DWORD textureid, roomcount;
    char roomtext[32];
    BOOL hasthumbnail, mixedtexture;
    TexThumb detailthumbnail;
    unsigned char detailpixels[TEX_THUMB_MAX * TEX_THUMB_MAX * 4];
    DWORD detailtextureid;
    BOOL hasdetailthumbnail, mixeddetailtexture, showdetail;
    int scroll, wheelremainder;
    BOOL updating;
} FacePropertiesState;

static FacePropertiesState *FacePropertiesGetState(HWND hwnd)
{
    return (FacePropertiesState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

static BOOL FacePropertiesRenderText(const BgDocument *document, const BgFaceRef *refs,
                                      DWORD count, char *text, size_t size, int *choiceout)
{
    static const char *types[] = {"Opaque", "Cutout", "Translucent (alpha blend)",
        "Custom / decal", "Cutout + blend", "Inherited / unknown", "Mixed"};
    size_t bytes = (size_t)count * sizeof(BgRenderState);
    BgRenderState *states = count && bytes / sizeof(*states) == count ? malloc(bytes) : NULL;
    int surface = -1;
    DWORD policy = BG_SURFACE_UNKNOWN;
    BOOL editable = TRUE;
    DWORD i;
    if (choiceout) { *choiceout = 0; }
    if (!states || !BgDocumentGetFaceRenderStates(document, refs, count, states))
    {
        snprintf(text, size, "Transparency: Unavailable");
        free(states);
        return FALSE;
    }
    for (i = 0; i < count; i++)
    {
        DWORD mode;
        int s = BgRenderGetTransparency(&states[i]);
        surface = surface < 0 ? s : surface == s ? surface : 6;
        policy = i == 0 ? states[i].surfacepolicy
            : policy == states[i].surfacepolicy ? policy : BG_SURFACE_UNKNOWN;
        editable &= BgRenderSurfacePreset(&states[i], BG_TRANSPARENCY_OPAQUE, &mode);
    }
    if (choiceout && policy <= BG_SURFACE_BLEND) { *choiceout = policy + 1; }
    if (policy == BG_SURFACE_AUTO) { snprintf(text, size, "Transparency: Auto (%s)", types[surface]); }
    else { snprintf(text, size, "Transparency: %s", policy <= BG_SURFACE_BLEND ? types[policy - 1] : "Mixed"); }
    free(states);
    return editable;
}

/* -1 means that this property differs across the selected faces, or that
 * some faces have no detail tile to which the property could apply. */
typedef struct FaceDetailSelection {
    int mode, textureid, shiftu, shiftv, minlod, offset;
} FaceDetailSelection;

static void FacePropertiesGetDetail(const BgDocument *document, const BgFaceRef *refs,
                                    DWORD count, FaceDetailSelection *selection)
{
    DWORD i;
    selection->mode = selection->textureid = selection->shiftu = selection->shiftv
        = selection->minlod = selection->offset = -1;
    for (i = 0; i < count; i++)
    {
        const BgDocumentFace *face = BgDocumentFindFace(document, &refs[i], NULL);
        BgDetailTexture detail;
        FaceDetailSelection current;
        if (!face) { continue; } /* The caller validates the selection first. */
        BgMaterialGetDetail(&face->material, &detail);
        current.mode = detail.mode;
        current.textureid = current.shiftu = current.shiftv = current.minlod = current.offset = -1;
        if (detail.mode == BG_DETAIL_BASE_IMAGE || detail.mode == BG_DETAIL_SEPARATE_IMAGE)
        {
            current.textureid = detail.textureid;
            current.shiftu = detail.shiftu;
            current.shiftv = detail.shiftv;
            current.minlod = detail.minlod;
            /* The other offset codes all produce a zero tile origin. */
            current.offset = detail.offset == 2;
        }
        if (i == 0) { *selection = current; }
        else
        {
            if (selection->mode != current.mode) { selection->mode = -1; }
            if (selection->textureid != current.textureid) { selection->textureid = -1; }
            if (selection->shiftu != current.shiftu) { selection->shiftu = -1; }
            if (selection->shiftv != current.shiftv) { selection->shiftv = -1; }
            if (selection->minlod != current.minlod) { selection->minlod = -1; }
            if (selection->offset != current.offset) { selection->offset = -1; }
        }
    }
}

static void FacePropertiesDetailScale(int shift, char *text, size_t size)
{
    /* RDP shifts 0..10 divide UVs; 11..15 multiply by 32, 16, 8, 4, 2. */
    if (shift < 0) { snprintf(text, size, "Mixed"); }
    else if (shift == 0) { snprintf(text, size, "1x"); }
    else if (shift <= 10) { snprintf(text, size, "1/%ux", 1u << shift); }
    else { snprintf(text, size, "%ux", 1u << (16 - shift)); }
}

static void FacePropertiesSetDetail(FacePropertiesState *state, const BgDocument *document,
                                    const BgFaceRef *refs, DWORD count, HWND browser)
{
    FaceDetailSelection detail;
    char label[96], info[384], u[24], v[24], lod[48], imageinfo[64] = "";
    FacePropertiesGetDetail(document, refs, count, &detail);
    state->showdetail = detail.mode != BG_DETAIL_NONE && detail.mode != BG_DETAIL_UNKNOWN;
    state->detailtextureid = detail.textureid < 0 ? BG_TEX_NONE : (DWORD)detail.textureid;
    state->mixeddetailtexture = detail.textureid < 0;
    state->hasdetailthumbnail = state->showdetail && state->detailtextureid != BG_TEX_NONE
        && BrowserCopyImageThumbnail(browser, state->detailtextureid,
                                     &state->detailthumbnail, state->detailpixels);
    if (detail.mode == BG_DETAIL_NONE) { snprintf(label, sizeof(label), "Detail texture: None"); }
    else if (detail.mode == BG_DETAIL_UNKNOWN) { snprintf(label, sizeof(label), "Detail texture: Unknown texture mode"); }
    else if (detail.textureid < 0) { snprintf(label, sizeof(label), "Detail texture: Mixed"); }
    else { snprintf(label, sizeof(label), "Detail texture: %04X", (unsigned int)detail.textureid); }
    FacePropertiesDetailScale(detail.shiftu, u, sizeof(u));
    FacePropertiesDetailScale(detail.shiftv, v, sizeof(v));
    if (detail.minlod < 0) { snprintf(lod, sizeof(lod), "Mixed"); }
    else { snprintf(lod, sizeof(lod), "%d/256 (%.1f%%)", detail.minlod, detail.minlod * 100.0 / 256.0); }
    if (state->hasdetailthumbnail)
    {
        snprintf(imageinfo, sizeof(imageinfo), "Image size: %d x %d\r\n",
            state->detailthumbnail.imagewidth, state->detailthumbnail.imageheight);
    }
    snprintf(info, sizeof(info), "%s\r\n%sU tile scale: %s\r\nV tile scale: %s\r\nMinimum LOD: %s\r\nTile offset: %s",
        detail.mode == BG_DETAIL_BASE_IMAGE ? "Base image reused (LOD tile)"
            : detail.mode == BG_DETAIL_SEPARATE_IMAGE ? "Separate detail image" : "Detail source: Mixed",
        imageinfo, u, v, lod, detail.offset < 0 ? "Mixed"
            : detail.offset ? "Half texel (generated mipmaps only)" : "None");
    SetWindowText(state->controls[FACE_DETAIL_LABEL], label);
    SetWindowText(state->controls[FACE_DETAIL_INFO], info);
    EnableWindow(state->controls[FACE_DETAIL_FIND], state->hasdetailthumbnail);
    InvalidateRect(state->controls[FACE_DETAIL_THUMB], NULL, FALSE);
}

static int FacePropertiesTextHeight(HWND control, int width)
{
    char text[512];
    RECT rect = {0, 0, width, 0};
    HDC dc = GetDC(control);
    HFONT previous = (HFONT)SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    GetWindowText(control, text, sizeof(text));
    DrawText(dc, text, -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, previous);
    ReleaseDC(control, dc);
    return rect.bottom;
}

static void FacePropertiesLayout(HWND hwnd, FacePropertiesState *state)
{
    RECT client, bounds[FACE_CONTROL_COUNT] = {{0}};
    SCROLLINFO info = {0};
    int width, y = FACEPROPERTIES_MARGIN, i, maximum;
    GetClientRect(hwnd, &client);
    width = client.right - FACEPROPERTIES_MARGIN * 2;
    if (width < 1) { width = 1; }
    for (i = 0; i < FACE_CONTROL_COUNT; i++)
    {
        int x = FACEPROPERTIES_MARGIN, w = width, height;
        BOOL visible = state->showdetail || i < FACE_DETAIL_THUMB || i > FACE_DETAIL_INFO;
        ShowWindow(state->controls[i], visible ? SW_SHOWNA : SW_HIDE);
        if (!visible) { continue; }
        if (i == FACE_TEXTURE_THUMB || i == FACE_DETAIL_THUMB)
        {
            int size = width > FACEPROPERTIES_PREVIEW_SIZE + 52
                ? FACEPROPERTIES_PREVIEW_SIZE : width - 52;
            if (size < 1) { size = 1; }
            SetRect(&bounds[i], x, y, x + size, y + size);
            x += size + 8;
            SetRect(&bounds[i + 1], x, y + (size - 24) / 2,
                    x + 44, y + (size - 24) / 2 + 24);
            ShowWindow(state->controls[i + 1], SW_SHOWNA);
            y += size + 12;
            i++; /* The Find button shares the thumbnail's row. */
            continue;
        }
        if (i == FACE_U_LABEL || i == FACE_V_LABEL) { w = 20; }
        if (i == FACE_U || i == FACE_V) { x += 24; w = width > 24 ? width - 24 : 1; }
        height = i == FACE_ROOM || i == FACE_RENDER || i == FACE_CULL || i == FACE_U || i == FACE_V ? 24
            : FacePropertiesTextHeight(state->controls[i], w);
        SetRect(&bounds[i], x, y, x + w, y + height);
        if (i != FACE_U_LABEL && i != FACE_V_LABEL)
        {
            y += height + (i == FACE_CULL ? 12 : 6);
        }
    }
    maximum = y - client.bottom;
    if (maximum < 0) { maximum = 0; }
    if (state->scroll > maximum) { state->scroll = maximum; }
    if (state->scroll < 0) { state->scroll = 0; }
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    info.nMax = y - 1;
    info.nPage = client.bottom;
    info.nPos = state->scroll;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);
    for (i = 0; i < FACE_CONTROL_COUNT; i++)
    {
        RECT *r = &bounds[i];
        /* The height of a native combo includes its opened list. */
        int height = i == FACE_ROOM || i == FACE_RENDER || i == FACE_CULL || i == FACE_U || i == FACE_V ? 160 : r->bottom - r->top;
        MoveWindow(state->controls[i], r->left, r->top - state->scroll,
                   r->right - r->left, height, TRUE);
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

static void FacePropertiesPaintThumbnail(const TexThumb *thumb, const unsigned char *source,
                                         BOOL available, BOOL mixed, const DRAWITEMSTRUCT *draw)
{
    RECT rect = draw->rcItem;
    HDC dc = draw->hDC;
    int saved = SaveDC(dc);
    FillRect(dc, &rect, GetSysColorBrush(COLOR_WINDOW));
    DrawEdge(dc, &rect, BDR_SUNKENOUTER, BF_RECT);
    InflateRect(&rect, -4, -4);
    if (available && rect.right > rect.left && rect.bottom > rect.top)
    {
        BITMAPINFO bmi = {0};
        unsigned char pixels[TEX_THUMB_MAX * TEX_THUMB_MAX * 4] = {0};
        int x, y, channel;
        int w = rect.right - rect.left, h = rect.bottom - rect.top;
        /* Use the browser's orientation and composite alpha on a checkerboard. */
        for (y = 0; y < thumb->h; y++)
        {
            for (x = 0; x < thumb->w; x++)
            {
                unsigned int offset = (y * TEX_THUMB_MAX + x) * 4;
                const unsigned char *src = source + offset;
                int background = ((x / 4 + y / 4) & 1) ? 192 : 240;
                for (channel = 0; channel < 3; channel++)
                {
                    pixels[offset + channel] = (unsigned char)((src[channel] * src[3]
                        + background * (255 - src[3]) + 127) / 255);
                }
            }
        }
        if (w * thumb->h > h * thumb->w) { w = h * thumb->w / thumb->h; }
        else { h = w * thumb->h / thumb->w; }
        if (w < 1) { w = 1; }
        if (h < 1) { h = 1; }
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = TEX_THUMB_MAX;
        bmi.bmiHeader.biHeight = -thumb->h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchDIBits(dc, rect.left + (rect.right - rect.left - w) / 2,
            rect.top + (rect.bottom - rect.top - h) / 2, w, h,
            0, 0, thumb->w, thumb->h, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
    }
    else
    {
        SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        SetTextColor(dc, GetSysColor(COLOR_GRAYTEXT));
        SetBkMode(dc, TRANSPARENT);
        DrawText(dc, mixed ? "Mixed" : "Image unavailable", -1, &rect,
                 DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    }
    RestoreDC(dc, saved);
}

static void FacePropertiesRevealControl(HWND hwnd, FacePropertiesState *state, HWND control)
{
    RECT client, bounds;
    GetClientRect(hwnd, &client);
    GetWindowRect(control, &bounds);
    MapWindowPoints(NULL, hwnd, (POINT *)&bounds, 2);
    if (bounds.top < 0) { state->scroll += bounds.top - FACEPROPERTIES_MARGIN; }
    else if (bounds.bottom > client.bottom)
    {
        state->scroll += bounds.bottom - client.bottom + FACEPROPERTIES_MARGIN;
    }
    else { return; }
    FacePropertiesLayout(hwnd, state);
}

static void FacePropertiesApplyRoom(HWND hwnd, FacePropertiesState *state, BOOL fromlist)
{
    HWND control = state->controls[FACE_ROOM];
    char number[32];
    DWORD room = 0;
    unsigned int i;
    if (fromlist)
    {
        LRESULT choice = SendMessage(control, CB_GETCURSEL, 0, 0);
        if (choice == CB_ERR) { return; }
        room = (DWORD)choice + 1;
    }
    else
    {
        GetWindowText(control, number, sizeof(number));
        if (lstrcmp(number, state->roomtext) == 0) { return; }
        for (i = 0; number[i] >= '0' && number[i] <= '9'; i++)
        {
            room = room * 10 + (number[i] - '0');
            if (room > state->roomcount) { break; }
        }
        if (i == 0 || number[i] || !room || room > state->roomcount)
        {
            MessageBox(hwnd, "Enter an existing room number.", "GEditor", MB_ICONERROR);
            SetWindowText(control, state->roomtext);
            return;
        }
    }
    SendMessage(GetParent(hwnd), FACEPROPERTIES_WM_ROOM_CHANGED, room, 0);
}

BOOL FacePropertiesHandleMessage(HWND panel, MSG *message)
{
    FacePropertiesState *state = FacePropertiesGetState(panel);
    HWND focus = GetFocus();
    HWND control;
    if (!state || message->message != WM_KEYDOWN) { return FALSE; }
    control = state->controls[FACE_ROOM];
    if (focus != control && !IsChild(control, focus)) { return FALSE; }
    /* Let the native combo accept/cancel an open list first. */
    if (SendMessage(control, CB_GETDROPPEDSTATE, 0, 0)) { return FALSE; }
    if (message->wParam == VK_RETURN)
    { FacePropertiesApplyRoom(panel, state, FALSE); return TRUE; }
    if (message->wParam == VK_ESCAPE)
    { SetWindowText(control, state->roomtext); return TRUE; }
    return FALSE;
}

static LRESULT CALLBACK FacePropertiesWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    FacePropertiesState *state = FacePropertiesGetState(hwnd);
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
        const char *labels[FACE_CONTROL_COUNT] = {
            "", "Room", "", "Texture", "", "Find",
            "Detail texture", "", "Find", "", "", "", "",
            "Backface culling", "",
            "Texture wrapping", "U", "", "V", "",
            ""
        };
        int i;
        state = (FacePropertiesState *)calloc(1, sizeof(*state));
        if (state == NULL) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        for (i = 0; i < FACE_CONTROL_COUNT; i++)
        {
            BOOL combo = i == FACE_ROOM || i == FACE_RENDER || i == FACE_CULL || i == FACE_U || i == FACE_V;
            BOOL button = i == FACE_TEXTURE_FIND || i == FACE_DETAIL_FIND;
            DWORD style = combo ? WS_TABSTOP | WS_VSCROLL | (i == FACE_ROOM ? CBS_DROPDOWN : CBS_DROPDOWNLIST)
                : button ? WS_TABSTOP | BS_PUSHBUTTON | BS_NOTIFY
                : i == FACE_TEXTURE_THUMB || i == FACE_DETAIL_THUMB ? SS_OWNERDRAW : SS_NOPREFIX;
            state->controls[i] = CreateWindowEx(0, combo ? "COMBOBOX" : button ? "BUTTON" : "STATIC", labels[i],
                WS_CHILD | WS_VISIBLE | style,
                0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)(i + 1), cs->hInstance, NULL);
            if (state->controls[i] == NULL) { return -1; }
            SendMessage(state->controls[i], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
            if (combo)
            {
                HWND control = state->controls[i];
                if (i == FACE_ROOM)
                { SendMessage(control, CB_LIMITTEXT, 5, 0); continue; }
                if (i == FACE_RENDER)
                {
                    SendMessage(control, CB_ADDSTRING, 0, (LPARAM)"Mixed / Keep current");
                    SendMessage(control, CB_ADDSTRING, 0, (LPARAM)"Auto");
                    SendMessage(control, CB_ADDSTRING, 0, (LPARAM)"Opaque");
                    SendMessage(control, CB_ADDSTRING, 0, (LPARAM)"Cutout");
                    SendMessage(control, CB_ADDSTRING, 0, (LPARAM)"Translucent (alpha blend)");
                    SendMessage(control, CB_SETDROPPEDWIDTH, 200, 0);
                    continue;
                }
                SendMessage(control, CB_ADDSTRING, 0, (LPARAM)"Mixed");
                SendMessage(control, CB_ADDSTRING, 0, (LPARAM)(i == FACE_CULL ? "On" : "Repeat"));
                SendMessage(control, CB_ADDSTRING, 0, (LPARAM)(i == FACE_CULL ? "Off" : "Clamp"));
                if (i != FACE_CULL) { SendMessage(control, CB_ADDSTRING, 0, (LPARAM)"Mirror"); }
                SendMessage(control, CB_SETDROPPEDWIDTH, 180, 0);
            }
        }
        EnableWindow(state->controls[FACE_TEXTURE_FIND], FALSE);
        EnableWindow(state->controls[FACE_DETAIL_FIND], FALSE);
        state->tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
            WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, hwnd, NULL, cs->hInstance, NULL);
        if (state->tooltip)
        {
            TOOLINFO tip = {0};
            tip.cbSize = sizeof(tip);
            tip.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
            tip.hwnd = hwnd;
            tip.uId = (UINT_PTR)state->controls[FACE_TEXTURE_FIND];
            tip.lpszText = "Show image in content browser";
            SendMessage(state->tooltip, TTM_ADDTOOL, 0, (LPARAM)&tip);
            tip.uId = (UINT_PTR)state->controls[FACE_DETAIL_FIND];
            SendMessage(state->tooltip, TTM_ADDTOOL, 0, (LPARAM)&tip);
        }
        return 0;
    }
    case WM_SIZE:
        if (state) { FacePropertiesLayout(hwnd, state); }
        return 0;
    case WM_COMMAND:
        if (state && !state->updating && LOWORD(wparam) == FACE_ROOM + 1)
        {
            if (HIWORD(wparam) == CBN_SETFOCUS)
            { FacePropertiesRevealControl(hwnd, state, (HWND)lparam); }
            else if (HIWORD(wparam) == CBN_SELENDOK)
            { FacePropertiesApplyRoom(hwnd, state, TRUE); }
            return 0;
        }
        if (state && !state->updating
            && (LOWORD(wparam) == FACE_TEXTURE_FIND + 1 || LOWORD(wparam) == FACE_DETAIL_FIND + 1))
        {
            BOOL detail = LOWORD(wparam) == FACE_DETAIL_FIND + 1;
            if (HIWORD(wparam) == BN_SETFOCUS)
            {
                FacePropertiesRevealControl(hwnd, state, (HWND)lparam);
            }
            else if (HIWORD(wparam) == BN_CLICKED && (detail ? state->hasdetailthumbnail : state->hasthumbnail))
            {
                SendMessage(GetParent(hwnd), FACEPROPERTIES_WM_REVEAL_IMAGE,
                    detail ? state->detailtextureid : state->textureid, 0);
            }
            return 0;
        }
        if (state && !state->updating
            && (LOWORD(wparam) == FACE_RENDER + 1 || LOWORD(wparam) == FACE_CULL + 1 || LOWORD(wparam) == FACE_U + 1 || LOWORD(wparam) == FACE_V + 1))
        {
            HWND control = (HWND)lparam;
            if (HIWORD(wparam) == CBN_SETFOCUS)
            {
                FacePropertiesRevealControl(hwnd, state, control);
            }
            else if (HIWORD(wparam) == CBN_SELENDOK)
            {
                BgFacePropertiesEdit edit = {0};
                int choice = (int)SendMessage(control, CB_GETCURSEL, 0, 0);
                if (choice > 0)
                {
                    if (control == state->controls[FACE_RENDER])
                    { edit.fields = BG_FACE_PROPERTY_TRANSPARENCY; edit.transparency = choice == 1 ? BG_TRANSPARENCY_AUTO : (BgTransparency)(choice - 2); }
                    else if (control == state->controls[FACE_CULL])
                    { edit.fields = BG_FACE_PROPERTY_CULL; edit.cullbackfaces = choice == 1; }
                    else if (control == state->controls[FACE_U])
                    { edit.fields = BG_FACE_PROPERTY_WRAP_U; edit.wrapu = (BgTextureWrap)(choice - 1); }
                    else
                    { edit.fields = BG_FACE_PROPERTY_WRAP_V; edit.wrapv = (BgTextureWrap)(choice - 1); }
                }
                /* Mixed / Keep current: a zero-field request just refreshes
                 * the controls from the unchanged document. */
                SendMessage(GetParent(hwnd), FACEPROPERTIES_WM_CHANGED, 0, (LPARAM)&edit);
            }
        }
        return 0;
    case WM_DRAWITEM:
        if (state && (wparam == FACE_TEXTURE_THUMB + 1 || wparam == FACE_DETAIL_THUMB + 1))
        {
            BOOL detail = wparam == FACE_DETAIL_THUMB + 1;
            FacePropertiesPaintThumbnail(detail ? &state->detailthumbnail : &state->thumbnail,
                detail ? state->detailpixels : state->pixels,
                detail ? state->hasdetailthumbnail : state->hasthumbnail,
                detail ? state->mixeddetailtexture : state->mixedtexture, (const DRAWITEMSTRUCT *)lparam);
            return TRUE;
        }
        break;
    case WM_MOUSEWHEEL:
        if (state)
        {
            state->wheelremainder += GET_WHEEL_DELTA_WPARAM(wparam);
            state->scroll -= 48 * (state->wheelremainder / WHEEL_DELTA);
            state->wheelremainder %= WHEEL_DELTA;
            FacePropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_VSCROLL:
        if (state)
        {
            SCROLLINFO info = {0};
            info.cbSize = sizeof(info);
            info.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &info);
            switch (LOWORD(wparam))
            {
            case SB_LINEUP: state->scroll -= 24; break;
            case SB_LINEDOWN: state->scroll += 24; break;
            case SB_PAGEUP: state->scroll -= info.nPage; break;
            case SB_PAGEDOWN: state->scroll += info.nPage; break;
            case SB_THUMBTRACK: state->scroll = info.nTrackPos; break;
            case SB_TOP: state->scroll = 0; break;
            case SB_BOTTOM: state->scroll = info.nMax; break;
            }
            FacePropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wparam, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_NCDESTROY:
        if (state && state->tooltip) { DestroyWindow(state->tooltip); }
        free(state);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

BOOL FacePropertiesRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = FacePropertiesWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = FACEPROPERTIES_CLASS;
    return RegisterClass(&wc) != 0;
}

HWND FacePropertiesCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, FACEPROPERTIES_CLASS, NULL,
        WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 1, 1, parent, NULL, instance, NULL);
}

BOOL FacePropertiesSetSelection(HWND panel, const BgDocument *document,
                                const BgFaceRef *refs, DWORD count, HWND browser)
{
    FacePropertiesState *state = FacePropertiesGetState(panel);
    const BgDocumentFace *first;
    int cull, wrapu, wrapv;
    unsigned short room;
    BOOL textured = TRUE, sametexture = TRUE, editable;
    DWORD i;
    char summary[256], texture[48], render[256];
    if (state == NULL || refs == NULL || count == 0
        || (first = BgDocumentFindFace(document, refs, NULL)) == NULL) { return FALSE; }
    room = first->room;
    cull = first->cullbackfaces ? 1 : 2;
    wrapu = BgMaterialGetWrap(&first->material, FALSE) + 1;
    wrapv = BgMaterialGetWrap(&first->material, TRUE) + 1;
    for (i = 0; i < count; i++)
    {
        const BgDocumentFace *face = BgDocumentFindFace(document, &refs[i], NULL);
        if (face == NULL) { return FALSE; }
        if (room != face->room) { room = 0; }
        if (cull != (face->cullbackfaces ? 1 : 2)) { cull = 0; }
        if (wrapu != (int)BgMaterialGetWrap(&face->material, FALSE) + 1) { wrapu = 0; }
        if (wrapv != (int)BgMaterialGetWrap(&face->material, TRUE) + 1) { wrapv = 0; }
        if (face->textureid == BG_TEX_NONE) { textured = FALSE; }
        if (face->textureid != first->textureid) { sametexture = FALSE; }
    }
    if (count == 1)
    {
        snprintf(summary, sizeof(summary), "Face: %lu\r\nLayer: %s",
            (unsigned long)first->id,
            first->layer == BG_GEOMETRY_SECONDARY ? "Secondary" : "Primary");
    }
    else { snprintf(summary, sizeof(summary), "%lu faces selected.\r\nMixed means their settings differ.", (unsigned long)count); }
    state->updating = TRUE;
    if (state->roomcount != document->roomcount)
    {
        HWND control = state->controls[FACE_ROOM];
        SendMessage(control, CB_RESETCONTENT, 0, 0);
        for (i = 1; i <= document->roomcount; i++)
        {
            char number[16];
            snprintf(number, sizeof(number), "%lu", (unsigned long)i);
            SendMessage(control, CB_ADDSTRING, 0, (LPARAM)number);
        }
        state->roomcount = document->roomcount;
    }
    if (room) { snprintf(state->roomtext, sizeof(state->roomtext), "%u", (unsigned int)room); }
    else { lstrcpyn(state->roomtext, "Mixed", sizeof(state->roomtext)); }
    SendMessage(state->controls[FACE_ROOM], CB_SETCURSEL, room ? room - 1 : -1, 0);
    SetWindowText(state->controls[FACE_ROOM], state->roomtext);
    state->textureid = first->textureid;
    state->mixedtexture = !sametexture;
    state->hasthumbnail = sametexture && BrowserCopyImageThumbnail(browser,
        state->textureid, &state->thumbnail, state->pixels);
    if (!sametexture) { lstrcpyn(texture, "Texture: Mixed", sizeof(texture)); }
    else if (state->hasthumbnail)
    {
        snprintf(texture, sizeof(texture), "Texture: %s", state->thumbnail.label);
    }
    else if (first->textureid == BG_TEX_NONE)
    {
        lstrcpyn(texture, "Texture: No Texture", sizeof(texture));
    }
    else { snprintf(texture, sizeof(texture), "Texture: %04X", (unsigned int)first->textureid); }
    SetWindowText(state->controls[FACE_SUMMARY], summary);
    {
        int choice;
        editable = FacePropertiesRenderText(document, refs, count, render, sizeof(render), &choice);
        SendMessage(state->controls[FACE_RENDER], CB_SETCURSEL, choice, 0);
    }
    EnableWindow(state->controls[FACE_RENDER], editable);
    SetWindowText(state->controls[FACE_RENDER_HELP], editable
        ? "Auto allows game optimizations. Explicit choices take priority. Auto restores the pre-override material. The current layer is kept."
        : "Read-only: the selection includes inherited or custom render state.");
    SetWindowText(state->controls[FACE_RENDER_INFO], render);
    SetWindowText(state->controls[FACE_TEXTURE_LABEL], texture);
    EnableWindow(state->controls[FACE_TEXTURE_FIND], state->hasthumbnail);
    InvalidateRect(state->controls[FACE_TEXTURE_THUMB], NULL, FALSE);
    FacePropertiesSetDetail(state, document, refs, count, browser);
    SendMessage(state->controls[FACE_CULL], CB_SETCURSEL, cull, 0);
    SendMessage(state->controls[FACE_U], CB_SETCURSEL, textured ? wrapu : -1, 0);
    SendMessage(state->controls[FACE_V], CB_SETCURSEL, textured ? wrapv : -1, 0);
    EnableWindow(state->controls[FACE_U], textured);
    EnableWindow(state->controls[FACE_V], textured);
    SetWindowText(state->controls[FACE_SELECTION_HELP], textured
        ? "Changes apply to all selected faces."
        : "Assign a texture to every selected face to edit wrapping.");
    state->updating = FALSE;
    FacePropertiesLayout(panel, state);
    return TRUE;
}
