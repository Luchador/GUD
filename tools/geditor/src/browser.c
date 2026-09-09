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
#include <commctrl.h>
#include <stdlib.h>

#include "browser.h"
#include "bgload.h"
#include "resource.h"

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
#define BROWSER_SECTION_MODELS 2
#define BROWSER_MAX_MODELS 512
#define BROWSER_MODEL_TAB_H 24
#define BROWSER_MODEL_TAB_COUNT 3
#define BROWSER_MODEL_CHARACTERS 0
#define BROWSER_MODEL_ITEMS 1
#define BROWSER_MODEL_PROPS 2
#define BROWSER_IMAGE_CELL_W (TEX_THUMB_MAX + 40) /* room for "No Texture" */
#define BROWSER_IMAGE_LABEL_H 16
#define BROWSER_IMAGE_CELL_H (TEX_THUMB_MAX + BROWSER_IMAGE_LABEL_H + 8)
#define BROWSER_IMAGE_MARGIN 4
#define BROWSER_MAX_LEVELS 64
#define BROWSER_ROW_H 16

typedef struct BrowserState {
    BrowserSection sections[BROWSER_SECTION_COUNT];
    BrowserLevelItem levels[BROWSER_MAX_LEVELS];
    int levelcount;
    TexThumb *images;             /* owned; freed on replace/destroy */
    unsigned char *imagepixels;   /* owned shared pixel block */
    int imagecount;
    TexThumb notexture; /* permanent item zero, independent of project images */
    unsigned char notexturepixels[TEX_THUMB_MAX * TEX_THUMB_MAX * 4];
    BrowserLevelItem models[BROWSER_MAX_MODELS];
    int modelcount;
    int modeltab;                        /* Characters is the default */
    int modelcounts[BROWSER_MODEL_TAB_COUNT];
    int modelscroll[BROWSER_MODEL_TAB_COUNT];
    int scroll[BROWSER_SECTION_COUNT];   /* pixels scrolled per body */
    int selectedlevel;
    int dragsection;                     /* thumb being dragged, or -1 */
    int dragstarty;
    int dragstartscroll;
    HIMAGELIST dragimage;
    DWORD dragtextureid;
    char dragmodel[64]; /* empty for image drags */
} BrowserState;

#define BROWSER_SCROLLBAR_W 8

/* Classify by the model name, independently of its project folder. */
static int BrowserModelCategory(const char *name)
{
    switch (name[0])
    {
    case 'C': case 'c': return BROWSER_MODEL_CHARACTERS;
    case 'G': case 'g': return BROWSER_MODEL_ITEMS;
    case 'P': case 'p': return BROWSER_MODEL_PROPS;
    }
    return -1;
}

/* Models reserve a fixed strip above their scrolling rows for the tabs. All
 * scrollbar calculations and content clipping use this same rectangle. */
static RECT BrowserContentRect(const BrowserState *state, int section)
{
    RECT rect = state->sections[section].bodyrc;

    if (section == BROWSER_SECTION_MODELS)
    {
        rect.top += BROWSER_MODEL_TAB_H;
        if (rect.top > rect.bottom) { rect.top = rect.bottom; }
    }
    return rect;
}

static RECT BrowserModelTabRect(const BrowserState *state, int tab)
{
    /* Give the longer Characters label half the strip; the other two share
     * the remaining half. Use the same rounded boundaries for hits and paint. */
    static const int boundaries[] = {0, 2, 3, 4};
    RECT rect = state->sections[BROWSER_SECTION_MODELS].bodyrc;
    int width = rect.right - rect.left;

    rect.right = rect.left + width * boundaries[tab + 1] / 4;
    rect.left += width * boundaries[tab] / 4;
    if (rect.bottom > rect.top + BROWSER_MODEL_TAB_H)
    {
        rect.bottom = rect.top + BROWSER_MODEL_TAB_H;
    }
    return rect;
}

static int BrowserHitModelTab(const BrowserState *state, POINT point)
{
    int tab;

    if (!state->sections[BROWSER_SECTION_MODELS].expanded) { return -1; }
    for (tab = 0; tab < BROWSER_MODEL_TAB_COUNT; tab++)
    {
        RECT rect = BrowserModelTabRect(state, tab);

        if (PtInRect(&rect, point)) { return tab; }
    }
    return -1;
}

/* Reserve the scrollbar gutter even when all images fit, so showing the
   scrollbar cannot itself change the number of columns. */
static int BrowserImageGridWidth(const RECT *body)
{
    int width = body->right - body->left
              - BROWSER_IMAGE_MARGIN * 2 - BROWSER_SCROLLBAR_W - 2;

    return width > 0 ? width : 1;
}

static int BrowserImageColumns(const RECT *body)
{
    int columns = BrowserImageGridWidth(body) / BROWSER_IMAGE_CELL_W;

    return columns > 0 ? columns : 1;
}

/*
 * Pixel height of a section's content. The image grid wraps to body width.
 * Models report the height of the active category only.
 */
static int BrowserContentHeight(const BrowserState *state, int section)
{
    if (section == BROWSER_SECTION_LEVELS)
    {
        return state->levelcount > 0 ? state->levelcount * BROWSER_ROW_H + 8 : 0;
    }

    if (section == BROWSER_SECTION_IMAGES)
    {
        int columns = BrowserImageColumns(&state->sections[section].bodyrc);
        int count = state->imagecount + 1; /* include the permanent No Texture item */
        int rows = count / columns + (count % columns != 0);

        return rows > 0 ? rows * BROWSER_IMAGE_CELL_H + BROWSER_IMAGE_MARGIN * 2 : 0;
    }

    if (section == BROWSER_SECTION_MODELS)
    {
        int count = state->modelcounts[state->modeltab];

        return count > 0 ? count * BROWSER_ROW_H + 8 : 0;
    }

    return 0;
}

static int BrowserMaxScroll(const BrowserState *state, int section)
{
    RECT rect = BrowserContentRect(state, section);
    int body = rect.bottom - rect.top;
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
    RECT rect = BrowserContentRect(state, section);
    int body = rect.bottom - rect.top;
    int content = BrowserContentHeight(state, section);
    int track;
    int thumb;
    int maxscroll;
    int y;

    if (!sec->expanded || content <= body || body <= 4)
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
    y = rect.top + 2
      + (maxscroll > 0 ? (track - thumb) * state->scroll[section] / maxscroll : 0);

    out->left = rect.right - BROWSER_SCROLLBAR_W - 2;
    out->right = rect.right - 2;
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

static void BrowserSelectModelTab(HWND hwnd, BrowserState *state, int tab)
{
    if (tab == state->modeltab) { return; }
    state->modelscroll[state->modeltab] = state->scroll[BROWSER_SECTION_MODELS];
    state->modeltab = tab;
    state->scroll[BROWSER_SECTION_MODELS] = state->modelscroll[tab];
    BrowserClampScroll(state, BROWSER_SECTION_MODELS);
    InvalidateRect(hwnd, &state->sections[BROWSER_SECTION_MODELS].bodyrc, FALSE);
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

        sec->bodyrc.left = 0;
        sec->bodyrc.right = client->right;
        sec->bodyrc.top = y;
        sec->bodyrc.bottom = y;
        if (sec->expanded)
        {
            int h = perbody;

            expandedseen++;
            if (expandedseen == expandedcount)
            {
                /* last expanded section absorbs the division remainder */
                h = bodyspace - perbody * (expandedcount - 1);
            }

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
 * Image grid: centered thumbnails with their IDs below. Thumbs are top-down BGRA in
 * the shared block; StretchDIBits takes them straight from memory via
 * a negative-height BITMAPINFO, so no per-item GDI bitmaps ever exist.
 */
static const TexThumb *BrowserImageAt(const BrowserState *state, int index,
                                     const unsigned char **pixels)
{
    if (index == 0)
    {
        *pixels = state->notexturepixels;
        return &state->notexture;
    }
    *pixels = state->imagepixels + state->images[index - 1].pixeloffset;
    return &state->images[index - 1];
}

static void BrowserPaintImageGrid(BrowserState *state, HDC hdc, const RECT *body)
{
    int columns = BrowserImageColumns(body);
    int width = BrowserImageGridWidth(body);
    int scroll = state->scroll[BROWSER_SECTION_IMAGES];
    int firstrow = scroll > BROWSER_IMAGE_MARGIN
        ? (scroll - BROWSER_IMAGE_MARGIN) / BROWSER_IMAGE_CELL_H : 0;
    int i;
    BITMAPINFO bmi;

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    for (i = firstrow * columns; i < state->imagecount + 1; i++)
    {
        const TexThumb *t;
        const unsigned char *pixels;
        RECT rc;
        int column = i % columns;
        int y = body->top + BROWSER_IMAGE_MARGIN
              + (i / columns) * BROWSER_IMAGE_CELL_H - scroll;

        if (y >= body->bottom)
        {
            break;
        }

        t = BrowserImageAt(state, i, &pixels);
        /* Share leftover width between columns, including rounding pixels. */
        rc.left = body->left + BROWSER_IMAGE_MARGIN + column * width / columns;
        rc.right = body->left + BROWSER_IMAGE_MARGIN + (column + 1) * width / columns;

        if (t->w > 0 && t->h > 0)
        {
            /* The thumb block is stored in GDI's native BGRA order,
               so this call needs no channel gymnastics. */
            bmi.bmiHeader.biWidth = TEX_THUMB_MAX;
            bmi.bmiHeader.biHeight = -t->h; /* negative: top-down */

            StretchDIBits(hdc,
                          rc.left + (rc.right - rc.left - t->w) / 2,
                          y + (TEX_THUMB_MAX - t->h) / 2,
                          t->w, t->h,
                          0, 0, t->w, t->h,
                          pixels,
                          &bmi, DIB_RGB_COLORS, SRCCOPY);
        }

        rc.top = y + TEX_THUMB_MAX + 4;
        rc.bottom = rc.top + BROWSER_IMAGE_LABEL_H;

        DrawText(hdc, t->label, -1, &rc,
                 DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

static void BrowserPaintModelTabs(const BrowserState *state, HDC hdc)
{
    static const char *names[BROWSER_MODEL_TAB_COUNT] = {"Characters", "Items", "Props"};
    int tab;

    SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
    for (tab = 0; tab < BROWSER_MODEL_TAB_COUNT; tab++)
    {
        RECT rect = BrowserModelTabRect(state, tab);
        BOOL active = tab == state->modeltab;

        FillRect(hdc, &rect, GetSysColorBrush(active ? COLOR_WINDOW : COLOR_BTNFACE));
        DrawEdge(hdc, &rect, BDR_RAISEDOUTER,
                 BF_LEFT | BF_TOP | BF_RIGHT | (active ? 0 : BF_BOTTOM));
        rect.left += 3;
        rect.right -= 3;
        DrawText(hdc, names[tab], -1, &rect,
                 DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

/* Filtered model rows, clipped below the fixed tab strip. */
static void BrowserPaintModelRows(BrowserState *state, HDC hdc, const RECT *body)
{
    int y = body->top + 4 - state->scroll[BROWSER_SECTION_MODELS];
    int i;

    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

    for (i = 0; i < state->modelcount; i++)
    {
        RECT rc;

        if (BrowserModelCategory(state->models[i].label) != state->modeltab)
        {
            continue;
        }
        rc.top = y;
        rc.bottom = y + BROWSER_ROW_H;
        y += BROWSER_ROW_H;
        if (rc.bottom < body->top || rc.top > body->bottom)
        {
            continue;
        }

        rc.left = 26;
        rc.right = body->right - BROWSER_SCROLLBAR_W - 6;
        DrawText(hdc, state->models[i].label, -1, &rc,
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

    track = BrowserContentRect(state, section);
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
            RECT body = BrowserContentRect(state, i);

            if (i == BROWSER_SECTION_MODELS) { BrowserPaintModelTabs(state, hdc); }
            if (body.bottom <= body.top) { continue; }
            if ((i == BROWSER_SECTION_LEVELS && state->levelcount > 0)
                || i == BROWSER_SECTION_IMAGES
                || (i == BROWSER_SECTION_MODELS && state->modelcounts[state->modeltab] > 0))
            {
                int saved = SaveDC(hdc);

                BrowserClampScroll(state, i);
                IntersectClipRect(hdc, body.left, body.top, body.right, body.bottom);

                if (i == BROWSER_SECTION_LEVELS)
                {
                    BrowserPaintLevelRows(state, hdc, &body);
                }
                else if (i == BROWSER_SECTION_IMAGES)
                {
                    BrowserPaintImageGrid(state, hdc, &body);
                }
                else
                {
                    BrowserPaintModelRows(state, hdc, &body);
                }

                RestoreDC(hdc, saved);

                BrowserPaintScrollbar(state, hdc, i);
            }
            else
            {
                RECT hint = body;

                hint.left += 26;
                hint.top += 6;
                SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
                DrawText(hdc, "(empty)", -1, &hint, DT_SINGLELINE | DT_TOP | DT_LEFT);
            }
        }
    }

    SelectObject(hdc, oldfont);
}

static int BrowserHitImage(const BrowserState *state, POINT point)
{
    const BrowserSection *section = &state->sections[BROWSER_SECTION_IMAGES];
    int width = BrowserImageGridWidth(&section->bodyrc);
    int columns = BrowserImageColumns(&section->bodyrc);
    int x = point.x - section->bodyrc.left - BROWSER_IMAGE_MARGIN;
    int y = point.y - section->bodyrc.top - BROWSER_IMAGE_MARGIN
          + state->scroll[BROWSER_SECTION_IMAGES];
    int image;

    if (!section->expanded || !PtInRect(&section->bodyrc, point)
        || x < 0 || x >= width || y < 0)
    {
        return -1;
    }
    /* Invert the painter's rounded column boundaries exactly. */
    image = (y / BROWSER_IMAGE_CELL_H) * columns
          + ((x + 1) * columns - 1) / width;
    return image < state->imagecount + 1 ? image : -1;
}


/* Image-list drag coordinates are relative to the drawing window's outer
 * rectangle, including its caption and borders. Use the editor frame as the
 * surface so desktop/monitor origins cannot offset the preview. Both starting
 * and moving the preview must use this same conversion. */
static BOOL BrowserImageDragPoint(HWND hwnd, POINT *point)
{
    RECT frame;

    if (!ClientToScreen(hwnd, point) || !GetWindowRect(GetParent(hwnd), &frame))
    {
        return FALSE;
    }
    point->x += 12 - frame.left;
    point->y += 18 - frame.top;
    return TRUE;
}


static void BrowserEndAssetDrag(HWND hwnd, BrowserState *state)
{
    if (state->dragimage != NULL)
    {
        ImageList_DragLeave(GetParent(hwnd));
        ImageList_EndDrag();
        ImageList_Destroy(state->dragimage);
        state->dragimage = NULL;
        state->dragmodel[0] = '\0';
        if (GetCapture() == hwnd) { ReleaseCapture(); }
    }
}


/* Invert the filtered, scrolled row layout; tabs and scrollbar are not rows. */
static int BrowserHitModel(const BrowserState *state, POINT point)
{
    RECT body = BrowserContentRect(state, BROWSER_SECTION_MODELS);
    int row, i;
    if (!state->sections[BROWSER_SECTION_MODELS].expanded || !PtInRect(&body, point) ||
        point.x >= body.right - BROWSER_SCROLLBAR_W - 6)
    {
        return -1;
    }
    row = point.y - body.top - 4 + state->scroll[BROWSER_SECTION_MODELS];
    if (row < 0)
    {
        return -1;
    }
    row /= BROWSER_ROW_H;
    for (i = 0; i < state->modelcount; i++)
    {
        if (BrowserModelCategory(state->models[i].label) == state->modeltab && row-- == 0)
        {
            return i;
        }
    }
    return -1;
}

/* Both asset drags share capture, cancellation and multi-monitor coordinates.
   Takes ownership of bitmap even when the drag cannot start. */
static BOOL BrowserStartAssetDrag(HWND hwnd, BrowserState *state, HBITMAP bitmap, int width,
                                  int height, POINT point)
{
    HIMAGELIST images = ImageList_Create(width, height, ILC_COLOR32, 1, 0);
    if (images == NULL || ImageList_Add(images, bitmap, NULL) < 0)
    {
        DeleteObject(bitmap);
        if (images != NULL)
        {
            ImageList_Destroy(images);
        }
        return FALSE;
    }
    DeleteObject(bitmap);
    if (!ImageList_BeginDrag(images, 0, 0, 0))
    {
        ImageList_Destroy(images);
        return FALSE;
    }
    if (!BrowserImageDragPoint(hwnd, &point) ||
        !ImageList_DragEnter(GetParent(hwnd), point.x, point.y))
    {
        ImageList_EndDrag();
        ImageList_Destroy(images);
        return FALSE;
    }
    state->dragimage = images;
    SetFocus(hwnd);
    SetCapture(hwnd);
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    return TRUE;
}

static void BrowserBeginModelDrag(HWND hwnd, BrowserState *state, int index, POINT point)
{
    const char *name = state->models[index].label;
    BITMAPINFO bmi = {0};
    HBITMAP bitmap;
    HDC dc;
    HGDIOBJ oldbitmap, oldfont;
    unsigned char *pixels;
    RECT rect = {0, 0, 200, 24};
    int i;

    if (state->modeltab == BROWSER_MODEL_ITEMS ||
        !SendMessage(GetParent(hwnd), BROWSER_WM_MODEL_DRAG_BEGIN, 0, (LPARAM)name))
    {
        return;
    }
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = rect.right;
    bmi.bmiHeader.biHeight = -rect.bottom;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (bitmap == NULL)
    {
        return;
    }
    dc = CreateCompatibleDC(NULL);
    if (dc == NULL)
    {
        DeleteObject(bitmap);
        return;
    }
    oldbitmap = SelectObject(dc, bitmap);
    oldfont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    FillRect(dc, &rect, GetSysColorBrush(COLOR_HIGHLIGHT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_HIGHLIGHTTEXT));
    rect.left = 6;
    rect.right -= 6;
    DrawText(dc, name, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, oldfont);
    SelectObject(dc, oldbitmap);
    DeleteDC(dc);
    /* GDI does not write alpha for its text/fill pixels. */
    for (i = 0; i < 200 * 24; i++)
    {
        pixels[i * 4 + 3] = 255;
    }
    if (BrowserStartAssetDrag(hwnd, state, bitmap, 200, 24, point))
    {
        lstrcpyn(state->dragmodel, name, sizeof(state->dragmodel));
    }
}

static void BrowserBeginImageDrag(HWND hwnd, BrowserState *state, int index, POINT point)
{
    const unsigned char *thumbpixels;
    const TexThumb *thumb = BrowserImageAt(state, index, &thumbpixels);
    BITMAPINFO bmi;
    HBITMAP bitmap;
    unsigned char *pixels;
    char *end = NULL;
    unsigned long textureid = index == 0 ? BG_TEX_NONE : strtoul(thumb->label, &end, 16);
    int x, y;

    if ((index != 0 && (end == thumb->label || *end != '\0' || textureid >= BG_TEX_NONE))
        || thumbpixels == NULL || thumb->w <= 0 || thumb->h <= 0
        || thumb->w > TEX_THUMB_MAX || thumb->h > TEX_THUMB_MAX
        || !SendMessage(GetParent(hwnd), BROWSER_WM_IMAGE_DRAG_BEGIN, textureid, 0))
    {
        return;
    }
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = TEX_THUMB_MAX;
    bmi.bmiHeader.biHeight = -TEX_THUMB_MAX;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (bitmap == NULL) { return; }

    /* A checkerboard keeps transparent images visible on any scene color. */
    for (y = 0; y < TEX_THUMB_MAX; y++)
    {
        for (x = 0; x < TEX_THUMB_MAX; x++)
        {
            unsigned char *dst = pixels + (y * TEX_THUMB_MAX + x) * 4;
            int sx = x - (TEX_THUMB_MAX - thumb->w) / 2;
            int sy = y - (TEX_THUMB_MAX - thumb->h) / 2;
            int background = ((x / 4 + y / 4) & 1) ? 192 : 240;
            int channel;

            dst[0] = dst[1] = dst[2] = (unsigned char)background;
            dst[3] = 255;
            if (sx >= 0 && sx < thumb->w && sy >= 0 && sy < thumb->h)
            {
                const unsigned char *src = thumbpixels + (sy * TEX_THUMB_MAX + sx) * 4;
                for (channel = 0; channel < 3; channel++)
                {
                    dst[channel] = (unsigned char)((src[channel] * src[3]
                        + background * (255 - src[3]) + 127) / 255);
                }
            }
        }
    }
    if (BrowserStartAssetDrag(hwnd, state, bitmap, TEX_THUMB_MAX, TEX_THUMB_MAX, point))
    {
        state->dragtextureid = (DWORD)textureid;
        state->dragmodel[0] = '\0';
    }
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

        lstrcpyn(state->notexture.label, "No Texture", sizeof(state->notexture.label));
        if (!TexLoadResourceThumbnail(((CREATESTRUCT *)lparam)->hInstance,
                IDR_NO_TEXTURE, &state->notexture, state->notexturepixels))
        {
            free(state);
            MessageBox(hwnd, "The No Texture thumbnail could not be loaded.", "GEditor", MB_ICONERROR);
            return -1;
        }

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

        /* Accept rapid image drags and tab clicks as ordinary clicks. */
        if (state != NULL)
        {
            RECT client;
            POINT point = {x, y};

            GetClientRect(hwnd, &client);
            BrowserLayoutSections(state, &client);
            if (BrowserHitImage(state, point) >= 0 || BrowserHitModelTab(state, point) >= 0
                || BrowserHitModel(state, point) >= 0)
            {
                return SendMessage(hwnd, WM_LBUTTONDOWN, wparam, lparam);
            }
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

            hit = BrowserHitModelTab(state, p);
            if (hit >= 0)
            {
                BrowserSelectModelTab(hwnd, state, hit);
                return 0;
            }

            /* Scrollbar first: the thumb and track live inside body
               rects, and a click there must not fall through. */
            for (i = 0; i < BROWSER_SECTION_COUNT; i++)
            {
                RECT thumb;
                RECT body = BrowserContentRect(state, i);

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
                    && y >= body.top && y < body.bottom)
                {
                    int page = body.bottom - body.top;

                    state->scroll[i] += (y < thumb.top) ? -page : page;
                    BrowserClampScroll(state, i);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }

            hit = BrowserHitImage(state, p);
            if (hit >= 0)
            {
                BrowserBeginImageDrag(hwnd, state, hit, p);
                return 0;
            }
            hit = BrowserHitModel(state, p);
            if (hit >= 0)
            {
                BrowserBeginModelDrag(hwnd, state, hit, p);
                return 0;
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
        if (state != NULL && state->dragimage != NULL)
        {
            POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};

            if (BrowserImageDragPoint(hwnd, &point))
            {
                ImageList_DragMove(point.x, point.y);
            }
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            return 0;
        }
        if (state != NULL && state->dragsection >= 0)
        {
            int i = state->dragsection;
            RECT rect = BrowserContentRect(state, i);
            int body = rect.bottom - rect.top;
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
        if (state != NULL && state->dragimage != NULL)
        {
            BrowserImageDrop drop;
            BrowserModelDrop modeldrop;

            lstrcpyn(modeldrop.name, state->dragmodel, sizeof(modeldrop.name));
            drop.textureid = state->dragtextureid;
            drop.screen.x = GET_X_LPARAM(lparam);
            drop.screen.y = GET_Y_LPARAM(lparam);
            ClientToScreen(hwnd, &drop.screen);
            /* Remove the preview and capture before hit testing or rebuilding
               the viewport. The frame receives a value, not a thumbnail pointer. */
            BrowserEndAssetDrag(hwnd, state);
            if (modeldrop.name[0] != '\0')
            {
                modeldrop.screen = drop.screen;
                SendMessage(GetParent(hwnd), BROWSER_WM_MODEL_DROP, 0, (LPARAM)&modeldrop);
            }
            else { SendMessage(GetParent(hwnd), BROWSER_WM_IMAGE_DROP, 0, (LPARAM)&drop); }
            return 0;
        }
        /* fall through */
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        if (state != NULL) { BrowserEndAssetDrag(hwnd, state); }
        if (state != NULL && state->dragsection >= 0)
        {
            state->dragsection = -1;

            if (GetCapture() == hwnd)
            {
                ReleaseCapture();
            }
        }
        return 0;

    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && state != NULL && state->dragimage != NULL)
        {
            BrowserEndAssetDrag(hwnd, state);
            return 0;
        }
        break;

    case WM_KILLFOCUS:
        if (state != NULL) { BrowserEndAssetDrag(hwnd, state); }
        break;

    case WM_MOUSEWHEEL:
        if (state != NULL && state->dragimage != NULL) { return 0; }
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
                    int step = i == BROWSER_SECTION_IMAGES
                        ? BROWSER_IMAGE_CELL_H : 3 * BROWSER_ROW_H;

                    state->scroll[i] -= notches * step;
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
        if (BrowserHitHeader(hwnd, p.x, p.y) >= 0
            || (state != NULL && BrowserHitModelTab(state, p) >= 0))
        {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE;
        }
        break;
    }

    case WM_SIZE:
        if (state != NULL)
        {
            RECT client;
            const RECT *body = &state->sections[BROWSER_SECTION_IMAGES].bodyrc;
            int oldcolumns = BrowserImageColumns(body);
            int scroll = state->scroll[BROWSER_SECTION_IMAGES];
            int row = scroll > BROWSER_IMAGE_MARGIN
                ? (scroll - BROWSER_IMAGE_MARGIN) / BROWSER_IMAGE_CELL_H : 0;
            int columns;
            int i;

            GetClientRect(hwnd, &client);
            BrowserLayoutSections(state, &client);
            columns = BrowserImageColumns(body);
            if (columns != oldcolumns)
            {
                /* Keep the previous top image's new row at the same vertical
                   offset, then clamp if the wider grid now fits entirely. */
                state->scroll[BROWSER_SECTION_IMAGES] =
                    (row * oldcolumns / columns) * BROWSER_IMAGE_CELL_H
                    + scroll - row * BROWSER_IMAGE_CELL_H;
            }
            for (i = 0; i < BROWSER_SECTION_COUNT; i++)
            {
                BrowserClampScroll(state, i);
            }
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc;
        BOOL dragging = state != NULL && state->dragimage != NULL;

        if (dragging) { ImageList_DragShowNolock(FALSE); }
        hdc = BeginPaint(hwnd, &ps);
        BrowserPaint(hwnd, hdc);
        EndPaint(hwnd, &ps);
        if (dragging) { ImageList_DragShowNolock(TRUE); }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        if (state != NULL)
        {
            BrowserEndAssetDrag(hwnd, state);
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

    BrowserEndAssetDrag(browser, state);
    free(state->images);
    free(state->imagepixels);

    state->images = items;
    state->imagepixels = pixelblock;
    state->imagecount = items != NULL ? count : 0;
    state->scroll[BROWSER_SECTION_IMAGES] = 0;

    InvalidateRect(browser, NULL, TRUE);
}


void BrowserSetModels(HWND browser, const BrowserLevelItem *items, int count)
{
    BrowserState *state = BrowserGetState(browser);
    int i;

    if (state == NULL)
    {
        return;
    }

    BrowserEndAssetDrag(browser, state);

    if (count > BROWSER_MAX_MODELS)
    {
        count = BROWSER_MAX_MODELS;
    }
    if (items == NULL || count < 0)
    {
        count = 0;
    }

    ZeroMemory(state->modelcounts, sizeof(state->modelcounts));
    ZeroMemory(state->modelscroll, sizeof(state->modelscroll));
    for (i = 0; i < count; i++)
    {
        int category = BrowserModelCategory(items[i].label);

        state->models[i] = items[i];
        if (category >= 0) { state->modelcounts[category]++; }
    }

    state->modelcount = count;
    state->scroll[BROWSER_SECTION_MODELS] = 0;

    InvalidateRect(browser, NULL, TRUE);
}
