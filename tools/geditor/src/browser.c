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
#define BROWSER_MAX_LEVELS 64
#define BROWSER_ROW_H 16

typedef struct BrowserState {
    BrowserSection sections[BROWSER_SECTION_COUNT];
    BrowserLevelItem levels[BROWSER_MAX_LEVELS];
    int levelcount;
} BrowserState;

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
static void BrowserPaintLevelRows(BrowserState *state, HDC hdc, const RECT *body)
{
    int y = body->top + 4;
    int i;
    int fits = (body->bottom - y) / BROWSER_ROW_H;
    int shown = state->levelcount;

    if (fits < 1)
    {
        return;
    }

    if (shown > fits)
    {
        shown = fits - 1; /* reserve the last line for the hint */
    }

    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    for (i = 0; i < shown; i++)
    {
        RECT rc;

        rc.left = 26;
        rc.right = body->right - 4;
        rc.top = y;
        rc.bottom = y + BROWSER_ROW_H;

        DrawText(hdc, state->levels[i].label, -1, &rc,
                 DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);

        y += BROWSER_ROW_H;
    }

    if (shown < state->levelcount)
    {
        RECT rc;
        char more[32];

        rc.left = 26;
        rc.right = body->right - 4;
        rc.top = y;
        rc.bottom = y + BROWSER_ROW_H;

        wsprintf(more, "... +%d more", state->levelcount - shown);
        SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
        DrawText(hdc, more, -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }
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
            if (i == BROWSER_SECTION_LEVELS && state->levelcount > 0)
            {
                BrowserPaintLevelRows(state, hdc, &sec->bodyrc);
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

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return 0;

    case WM_LBUTTONDOWN:
    {
        int hit = BrowserHitHeader(hwnd, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));

        if (hit >= 0 && state != NULL)
        {
            state->sections[hit].expanded = !state->sections[hit].expanded;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

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

    InvalidateRect(browser, NULL, TRUE);
}