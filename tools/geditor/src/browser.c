/*
 * GEditor content browser panel.
 *
 * Deliberately the opposite of the viewport in one way: it paints with
 * plain GDI, not OpenGL. The two children demonstrate the two ways a
 * Windows program draws - same WM_PAINT protocol, different brushes.
 *
 * The panel is organised as an accordion of sections (Images, Models,
 * ...). Each section is a full-width header bar; clicking it toggles
 * the section's body. Headers are laid out so they can never leave the
 * panel: the space left after ALL headers are placed is what bodies
 * share, so collapsed sections' bars pin to the top or bottom rather
 * than being pushed off.
 */

#include <windows.h>
#include <windowsx.h>  /* GET_X_LPARAM / GET_Y_LPARAM */
#include <stdlib.h>

#include "browser.h"

#define BROWSER_CLASS    "GEditorBrowser"
#define BROWSER_HEADER_H 26

typedef struct BrowserSection {
    const char *name;
    BOOL expanded;
    RECT headerrc;   /* filled by BrowserLayoutSections */
    RECT bodyrc;     /* valid only when expanded        */
} BrowserSection;

#define BROWSER_SECTION_COUNT 3
#define BROWSER_SECTION_LEVELS 0
#define BROWSER_SECTION_IMAGES 1
#define BROWSER_IMAGE_ROW_H 36
#define BROWSER_MAX_LEVELS 64
#define BROWSER_ROW_H 16

typedef struct BrowserState {
    BrowserSection sections[BROWSER_SECTION_COUNT];
    BrowserLevelItem levels[BROWSER_MAX_LEVELS];
    int levelcount;
    TexThumb *images;             /* owned; freed on replace/destroy */
    unsigned char *imagepixels;   /* owned shared pixel block */
    int imagecount;
    int scroll[BROWSER_SECTION_COUNT];   /* pixels scrolled per body */
    int selectedlevel;
    int dragsection;                     /* thumb being dragged, or -1 */
    int dragstarty;
    int dragstartscroll;
} BrowserState;

#define BROWSER_SCROLLBAR_W 8

/*
 * Pixel height of a section's content, independent of the body rect.
 * Images and Models report 0 until they have content to show.
 */
static int BrowserContentHeight(const BrowserState *state, int section)
{
    if (section == BROWSER_SECTION_LEVELS)
    {
        return state->levelcount > 0 ? state->levelcount * BROWSER_ROW_H + 8 : 0;
    }

    if (section == BROWSER_SECTION_IMAGES)
    {
        return state->imagecount > 0 ? state->imagecount * BROWSER_IMAGE_ROW_H + 8 : 0;
    }

    return 0;
}

static int BrowserMaxScroll(const BrowserState *state, int section)
{
    const BrowserSection *sec = &state->sections[section];
    int body = sec->bodyrc.bottom - sec->bodyrc.top;
    int content = BrowserContentHeight(state, section);

    return content > body ? content - body : 0;
}

/*
 * Where the scrollbar thumb sits for a section, in client coordinates.
 * Returns FALSE when the section needs no scrollbar.
 */
static BOOL BrowserThumbRect(const BrowserState *state, int section, RECT *out)
{
    const BrowserSection *sec = &state->sections[section];
    int body = sec->bodyrc.bottom - sec->bodyrc.top;
    int content = BrowserContentHeight(state, section);
    int track;
    int thumb;
    int maxscroll;
    int y;

    if (!sec->expanded || content <= body || body <= 0)
    {
        return FALSE;
    }

    track = body - 4;
    thumb = track * body / content;   /* proportional */
    if (thumb < 20)
    {
        thumb = 20;                   /* never vanishingly small */
    }
    if (thumb > track)
    {
        thumb = track;
    }

    maxscroll = content - body;
    y = sec->bodyrc.top + 2
      + (maxscroll > 0 ? (track - thumb) * state->scroll[section] / maxscroll : 0);

    out->left = sec->bodyrc.right - BROWSER_SCROLLBAR_W - 2;
    out->right = sec->bodyrc.right - 2;
    out->top = y;
    out->bottom = y + thumb;

    return TRUE;
}

static void BrowserClampScroll(BrowserState *state, int section)
{
    int maxscroll = BrowserMaxScroll(state, section);

    if (state->scroll[section] > maxscroll)
    {
        state->scroll[section] = maxscroll;
    }
    if (state->scroll[section] < 0)
    {
        state->scroll[section] = 0;
    }
}

static BrowserState *BrowserGetState(HWND hwnd)
{
    return (BrowserState *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

/*
 * The accordion layout.
 *
 * Every header is placed first, in order, each taking BROWSER_HEADER_H.
 * The height left over - client height minus ALL headers - is body
 * space, split evenly among the expanded sections (the last one takes
 * the rounding remainder). Because body space is computed net of every
 * header, a section low in the list always has room reserved for its
 * bar: expand everything above it and its header lands exactly at the
 * bottom edge, never beyond it.
 */
static void BrowserLayoutSections(BrowserState *state, const RECT *client)
{
    int expandedcount = 0;
    int bodyspace;
    int perbody = 0;
    int y = 0;
    int i;
    int expandedseen = 0;

    for (i = 0; i < BROWSER_SECTION_COUNT; i++)
    {
        if (state->sections[i].expanded)
        {
            expandedcount++;
        }
    }

    bodyspace = client->bottom - BROWSER_SECTION_COUNT * BROWSER_HEADER_H;
    if (bodyspace < 0)
    {
        bodyspace = 0; /* window shorter than the bars: they just stack */
    }
    if (expandedcount > 0)
    {
        perbody = bodyspace / expandedcount;
    }

    for (i = 0; i < BROWSER_SECTION_COUNT; i++)
    {
        BrowserSection *sec = &state->sections[i];

        sec->headerrc.left = 0;
        sec->headerrc.right = client->right;
        sec->headerrc.top = y;
        sec->headerrc.bottom = y + BROWSER_HEADER_H;
        y = sec->headerrc.bottom;

        if (sec->expanded)
        {
            int h = perbody;

            expandedseen++;
            if (expandedseen == expandedcount)
            {
                /* last expanded section absorbs the division remainder */
                h = bodyspace - perbody * (expandedcount - 1);
            }

            sec->bodyrc.left = 0;
            sec->bodyrc.right = client->right;
            sec->bodyrc.top = y;
            sec->bodyrc.bottom = y + h;
            y = sec->bodyrc.bottom;
        }
    }
}


static int BrowserHitLevelRow(HWND hwnd, int x, int y)
{
    BrowserState *state = BrowserGetState(hwnd);
    BrowserSection *sec;
    RECT client;
    POINT p;
    int index;

    if (state == NULL || state->levelcount == 0)
    {
        return -1;
    }

    GetClientRect(hwnd, &client);
    BrowserLayoutSections(state, &client);

    sec = &state->sections[BROWSER_SECTION_LEVELS];
    p.x = x;
    p.y = y;

    if (!sec->expanded || !PtInRect(&sec->bodyrc, p))
    {
        return -1;
    }

    if (x >= sec->bodyrc.right - BROWSER_SCROLLBAR_W - 2)
    {
        return -1; /* that's the scrollbar, not a row */
    }

    index = (y - sec->bodyrc.top - 4 + state->scroll[BROWSER_SECTION_LEVELS])
          / BROWSER_ROW_H;

    if (index < 0 || index >= state->levelcount)
    {
        return -1;
    }

    return index;
}


/* Section index whose header contains the point, or -1. */
static int BrowserHitHeader(HWND hwnd, int x, int y)
{
    BrowserState *state = BrowserGetState(hwnd);
    RECT client;
    POINT p;
    int i;

    if (state == NULL)
    {
        return -1;
    }

    GetClientRect(hwnd, &client);
    BrowserLayoutSections(state, &client);

    p.x = x;
    p.y = y;

    for (i = 0; i < BROWSER_SECTION_COUNT; i++)
    {
        if (PtInRect(&state->sections[i].headerrc, p))
        {
            return i;
        }
    }

    return -1;
}

/* Small filled triangle: points right when collapsed, down when open. */
static void BrowserPaintArrow(HDC hdc, const RECT *header, BOOL expanded)
{
    POINT pts[3];
    int cx = header->left + 13;
    int cy = (header->top + header->bottom) / 2;
    HBRUSH brush = GetSysColorBrush(COLOR_BTNTEXT);
    HPEN pen;
    HGDIOBJ oldbrush;
    HGDIOBJ oldpen;

    if (expanded)
    {
        pts[0].x = cx - 4; pts[0].y = cy - 2;
        pts[1].x = cx + 4; pts[1].y = cy - 2;
        pts[2].x = cx;     pts[2].y = cy + 3;
    }
    else
    {
        pts[0].x = cx - 2; pts[0].y = cy - 4;
        pts[1].x = cx - 2; pts[1].y = cy + 4;
        pts[2].x = cx + 3; pts[2].y = cy;
    }

    pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNTEXT));
    oldbrush = SelectObject(hdc, brush);
    oldpen = SelectObject(hdc, pen);

    Polygon(hdc, pts, 3);

    SelectObject(hdc, oldbrush);
    SelectObject(hdc, oldpen);
    DeleteObject(pen); /* created objects are ours to free; stock ones are not */
}

/*
 * Draws the level rows top-down inside the body rect, clipping to it.
 * When rows do not fit, the last visible line becomes a "+N more"
 * hint; scrolling is a later feature.
 */
/*
 * Draws the level rows offset by the section's scroll position. The
 * caller has already clipped the DC to the body rect, so rows that
 * hang over either edge are cut cleanly instead of painted over the
 * neighbouring section.
 */
static void BrowserPaintLevelRows(BrowserState *state, HDC hdc, const RECT *body)
{
    int y = body->top + 4 - state->scroll[BROWSER_SECTION_LEVELS];
    int i;

    for (i = 0; i < state->levelcount; i++, y += BROWSER_ROW_H)
    {
        RECT rc;

        if (y + BROWSER_ROW_H < body->top || y > body->bottom)
        {
            continue; /* entirely outside the body: nothing to draw */
        }

        rc.left = 26;
        rc.right = body->right - BROWSER_SCROLLBAR_W - 6;
        rc.top = y;
        rc.bottom = y + BROWSER_ROW_H;

        if (i == state->selectedlevel)
        {
            RECT fill = rc;

            fill.left = body->left;
            FillRect(hdc, &fill, GetSysColorBrush(COLOR_HIGHLIGHT));
            SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
        }
        else
        {
            SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
        }

        DrawText(hdc, state->levels[i].label, -1, &rc,
                 DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

/*
 * Image rows: thumbnail left, label right. Thumbs are top-down RGBA in
 * the shared block; StretchDIBits takes them straight from memory via
 * a negative-height BITMAPINFO, so no per-item GDI bitmaps ever exist.
 */
static void BrowserPaintImageRows(BrowserState *state, HDC hdc, const RECT *body)
{
    int y = body->top + 4 - state->scroll[BROWSER_SECTION_IMAGES];
    int i;
    BITMAPINFO bmi;

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    for (i = 0; i < state->imagecount; i++, y += BROWSER_IMAGE_ROW_H)
    {
        const TexThumb *t;
        RECT rc;

        if (y + BROWSER_IMAGE_ROW_H < body->top || y > body->bottom)
        {
            continue;
        }

        t = &state->images[i];

        if (t->w > 0 && t->h > 0)
        {
            /* Thumb rows are RGBA; GDI DIBs want BGRA, so red and blue
               swap in the header's eyes - x8 masks don't exist in
               BI_RGB, so we pre-swapped at load instead: the block is
               stored ready for this call. */
            bmi.bmiHeader.biWidth = TEX_THUMB_MAX;
            bmi.bmiHeader.biHeight = -t->h; /* negative: top-down */

            StretchDIBits(hdc,
                          26, y + (BROWSER_IMAGE_ROW_H - 4 - t->h) / 2 + 2,
                          t->w, t->h,
                          0, 0, t->w, t->h,
                          state->imagepixels + t->pixeloffset,
                          &bmi, DIB_RGB_COLORS, SRCCOPY);
        }

        rc.left = 26 + TEX_THUMB_MAX + 8;
        rc.right = body->right - BROWSER_SCROLLBAR_W - 6;
        rc.top = y;
        rc.bottom = y + BROWSER_IMAGE_ROW_H;

        DrawText(hdc, t->label, -1, &rc,
                 DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

/* Slim track-and-thumb indicator on the body's right edge. */
static void BrowserPaintScrollbar(BrowserState *state, HDC hdc, int section)
{
    RECT thumb;
    RECT track;

    if (!BrowserThumbRect(state, section, &thumb))
    {
        return;
    }

    track = state->sections[section].bodyrc;
    track.left = thumb.left;
    track.right = thumb.right;

    FillRect(hdc, &track, GetSysColorBrush(COLOR_BTNFACE));
    FillRect(hdc, &thumb, GetSysColorBrush(COLOR_BTNSHADOW));
}

static void BrowserPaint(HWND hwnd, HDC hdc)
{
    BrowserState *state = BrowserGetState(hwnd);
    RECT client;
    HFONT font;
    HFONT oldfont;
    int i;

    GetClientRect(hwnd, &client);

    /* Base coat: anything not covered by a header or body below. */
    FillRect(hdc, &client, GetSysColorBrush(COLOR_WINDOW));

    if (state == NULL)
    {
        return;
    }

    BrowserLayoutSections(state, &client);

    font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    oldfont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    for (i = 0; i < BROWSER_SECTION_COUNT; i++)
    {
        BrowserSection *sec = &state->sections[i];
        RECT text = sec->headerrc;

        FillRect(hdc, &sec->headerrc, GetSysColorBrush(COLOR_BTNFACE));
        BrowserPaintArrow(hdc, &sec->headerrc, sec->expanded);

        text.left += 26;
        SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
        DrawText(hdc, sec->name, -1, &text, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

        if (sec->expanded && sec->bodyrc.bottom > sec->bodyrc.top)
        {
            if ((i == BROWSER_SECTION_LEVELS && state->levelcount > 0)
                || (i == BROWSER_SECTION_IMAGES && state->imagecount > 0))
            {
                int saved = SaveDC(hdc);

                BrowserClampScroll(state, i);
                IntersectClipRect(hdc, sec->bodyrc.left, sec->bodyrc.top,
                                  sec->bodyrc.right, sec->bodyrc.bottom);

                if (i == BROWSER_SECTION_LEVELS)
                {
                    BrowserPaintLevelRows(state, hdc, &sec->bodyrc);
                }
                else
                {
                    BrowserPaintImageRows(state, hdc, &sec->bodyrc);
                }

                RestoreDC(hdc, saved);

                BrowserPaintScrollbar(state, hdc, i);
            }
            else
            {
                RECT hint = sec->bodyrc;

                hint.left += 26;
                hint.top += 6;
                SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
                DrawText(hdc, "(empty)", -1, &hint, DT_SINGLELINE | DT_TOP | DT_LEFT);
            }
        }
    }

    SelectObject(hdc, oldfont);
}

static LRESULT CALLBACK BrowserWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    BrowserState *state = BrowserGetState(hwnd);

    switch (msg)
    {
    case WM_CREATE:
        state = (BrowserState *)calloc(1, sizeof(*state));

        if (state == NULL)
        {
            return -1;
        }

        state->sections[0].name = "Levels";
        state->sections[0].expanded = TRUE;
        state->sections[1].name = "Images";
        state->sections[1].expanded = TRUE;
        state->sections[2].name = "Models";
        state->sections[2].expanded = TRUE;
        state->dragsection = -1;
        state->selectedlevel = -1;

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return 0;

    case WM_LBUTTONDBLCLK:
    {
        int x = GET_X_LPARAM(lparam);
        int y = GET_Y_LPARAM(lparam);
        int row = BrowserHitLevelRow(hwnd, x, y);

        if (row >= 0)
        {
            state->selectedlevel = row;
            InvalidateRect(hwnd, NULL, FALSE);

            /* Tell the frame which level was opened. The label pointer
               is only valid for the duration of this SendMessage. */
            SendMessage(GetParent(hwnd), BROWSER_WM_LEVEL_OPEN, (WPARAM)row, (LPARAM)state->levels[row].label);
            return 0;
        }

        /* A double-click on a header behaves like a second click, so
           rapid clicking toggles twice instead of eating a click. */
        {
            int hit = BrowserHitHeader(hwnd, x, y);

            if (hit >= 0 && state != NULL)
            {
                state->sections[hit].expanded = !state->sections[hit].expanded;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lparam);
        int y = GET_Y_LPARAM(lparam);
        int hit;
        int i;

        if (state != NULL)
        {
            RECT client;
            POINT p;

            GetClientRect(hwnd, &client);
            BrowserLayoutSections(state, &client);
            p.x = x;
            p.y = y;

            /* Scrollbar first: the thumb and track live inside body
               rects, and a click there must not fall through. */
            for (i = 0; i < BROWSER_SECTION_COUNT; i++)
            {
                RECT thumb;

                if (!BrowserThumbRect(state, i, &thumb))
                {
                    continue;
                }

                if (PtInRect(&thumb, p))
                {
                    state->dragsection = i;
                    state->dragstarty = y;
                    state->dragstartscroll = state->scroll[i];
                    SetCapture(hwnd);
                    return 0;
                }

                /* The track above/below the thumb pages the view. */
                if (x >= thumb.left && x < thumb.right
                    && y >= state->sections[i].bodyrc.top
                    && y < state->sections[i].bodyrc.bottom)
                {
                    int body = state->sections[i].bodyrc.bottom
                             - state->sections[i].bodyrc.top;

                    state->scroll[i] += (y < thumb.top) ? -body : body;
                    BrowserClampScroll(state, i);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
        }

        hit = BrowserHitHeader(hwnd, x, y);

        if (hit >= 0 && state != NULL)
        {
            state->sections[hit].expanded = !state->sections[hit].expanded;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (state != NULL && state->dragsection >= 0)
        {
            int i = state->dragsection;
            int body = state->sections[i].bodyrc.bottom - state->sections[i].bodyrc.top;
            int content = BrowserContentHeight(state, i);
            int track = body - 4;
            int thumb = content > 0 ? track * body / content : track;
            int range;

            if (thumb < 20)
            {
                thumb = 20;
            }

            range = track - thumb;

            if (range > 0)
            {
                int dy = GET_Y_LPARAM(lparam) - state->dragstarty;
                int maxscroll = BrowserMaxScroll(state, i);

                /* thumb pixels -> content pixels, same ratio the
                   painter uses in the other direction */
                state->scroll[i] = state->dragstartscroll + dy * maxscroll / range;
                BrowserClampScroll(state, i);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;

    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
        if (state != NULL && state->dragsection >= 0)
        {
            state->dragsection = -1;

            if (msg == WM_LBUTTONUP)
            {
                ReleaseCapture();
            }
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (state != NULL)
        {
            RECT client;
            POINT p;
            int i;

            /* Wheel coordinates are screen coordinates. */
            p.x = GET_X_LPARAM(lparam);
            p.y = GET_Y_LPARAM(lparam);
            ScreenToClient(hwnd, &p);

            GetClientRect(hwnd, &client);
            BrowserLayoutSections(state, &client);

            for (i = 0; i < BROWSER_SECTION_COUNT; i++)
            {
                BrowserSection *sec = &state->sections[i];

                if (sec->expanded && PtInRect(&sec->bodyrc, p))
                {
                    int notches = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;

                    state->scroll[i] -= notches * 3 * BROWSER_ROW_H;
                    BrowserClampScroll(state, i);
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                }
            }
        }
        return 0;

    case WM_SETCURSOR:
    {
        POINT p;

        GetCursorPos(&p);
        ScreenToClient(hwnd, &p);
        if (BrowserHitHeader(hwnd, p.x, p.y) >= 0)
        {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE;
        }
        break;
    }

    case WM_SIZE:
        /* Rects are derived from the client size at paint time; all a
           resize needs is a repaint. */
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        BrowserPaint(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        if (state != NULL)
        {
            free(state->images);
            free(state->imagepixels);
        }
        if (state != NULL)
        {
            free(state);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

BOOL BrowserRegisterClass(HINSTANCE hinstance)
{
    WNDCLASS wc;

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = BrowserWndProc;
    wc.style         = CS_DBLCLKS;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL; /* painted in WM_PAINT */
    wc.lpszClassName = BROWSER_CLASS;

    return RegisterClass(&wc) != 0;
}

HWND BrowserCreate(HWND parent, HINSTANCE hinstance)
{
    return CreateWindowEx(
        0,
        BROWSER_CLASS,
        NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 16, 16, /* placeholder; the parent's layout positions it */
        parent, NULL, hinstance, NULL);
}


void BrowserSetLevels(HWND browser, const BrowserLevelItem *items, int count)
{
    BrowserState *state = BrowserGetState(browser);
    int i;

    if (state == NULL)
    {
        return;
    }

    if (count > BROWSER_MAX_LEVELS)
    {
        count = BROWSER_MAX_LEVELS;
    }
    if (items == NULL)
    {
        count = 0;
    }

    for (i = 0; i < count; i++)
    {
        state->levels[i] = items[i];
    }

    state->levelcount = count;
    state->scroll[BROWSER_SECTION_LEVELS] = 0;
    state->selectedlevel = -1;

    InvalidateRect(browser, NULL, TRUE);
}


void BrowserSetImages(HWND browser, TexThumb *items, int count,
                      unsigned char *pixelblock)
{
    BrowserState *state = BrowserGetState(browser);

    if (state == NULL)
    {
        /* No state to own them: honour the contract by freeing. */
        free(items);
        free(pixelblock);
        return;
    }

    free(state->images);
    free(state->imagepixels);

    state->images = items;
    state->imagepixels = pixelblock;
    state->imagecount = items != NULL ? count : 0;
    state->scroll[BROWSER_SECTION_IMAGES] = 0;

    InvalidateRect(browser, NULL, TRUE);
}