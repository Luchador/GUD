#include "theme.h"
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
#include <stdio.h>
#include <ctype.h>

#include "browser.h"
#include "bgload.h"
#include "resource.h"
#include "modelthumbnail.h"
#include "viewport.h"

#define BROWSER_CLASS    "GEditorBrowser"
#define BROWSER_HEADER_H 26

typedef struct BrowserSection {
    const char *name;
    BOOL expanded;
    RECT headerrc;   /* filled by BrowserLayoutSections */
    RECT bodyrc;     /* valid only when expanded        */
} BrowserSection;

#define BROWSER_SECTION_COUNT 3
#define BROWSER_SECTION_OBJECTS 0
#define BROWSER_SECTION_IMAGES 1
#define BROWSER_SECTION_MODELS 2
#define BROWSER_OBJECT_COLUMNS 2
#define BROWSER_OBJECT_TAB_H 24
#define BROWSER_OBJECT_TAB_COUNT 3
#define BROWSER_OBJECT_TAB_OBJECTS 0
#define BROWSER_OBJECT_TAB_PRIMITIVES 1
#define BROWSER_OBJECT_TAB_SPECIAL 2
#define BROWSER_OBJECT_TILE_H 38
#define BROWSER_OBJECT_GAP 4
#define BROWSER_OBJECT_MARGIN 4
#define BROWSER_OBJECT_ICON_SIZE 32

/* Indexed by the stable BrowserObjectType; display order is defined separately. */
static const struct {
    const char *label;
    int icon;
    int tab;
} g_BrowserObjects[BROWSER_OBJECT_COUNT] = {
    { "Triangle",     IDR_OBJECT_TRIANGLE,     BROWSER_OBJECT_TAB_PRIMITIVES },
    { "Quad",         IDR_OBJECT_QUAD,         BROWSER_OBJECT_TAB_PRIMITIVES },
    { "Spawn Point",  IDR_OBJECT_SPAWN,        BROWSER_OBJECT_TAB_SPECIAL },
    { "Intro Spline", IDR_OBJECT_INTRO_SPLINE, BROWSER_OBJECT_TAB_SPECIAL },
    { "Intro Camera", IDR_OBJECT_INTRO,        BROWSER_OBJECT_TAB_SPECIAL },
    { "Outro Camera", IDR_OBJECT_OUTRO,        BROWSER_OBJECT_TAB_SPECIAL },
    { "Door",         IDR_OBJECT_DOOR,         BROWSER_OBJECT_TAB_OBJECTS },
    { "Glass",        IDR_OBJECT_GLASS,        BROWSER_OBJECT_TAB_OBJECTS },
    { "Weapon",       IDR_OBJECT_WEAPON,       BROWSER_OBJECT_TAB_OBJECTS },
    { "Ammo",         IDR_OBJECT_AMMO,         BROWSER_OBJECT_TAB_OBJECTS },
    { "CCTV Camera",  IDR_OBJECT_CCTV,         BROWSER_OBJECT_TAB_OBJECTS },
    { "Alarm",        IDR_OBJECT_ALARM,        BROWSER_OBJECT_TAB_OBJECTS },
    { "Drone Gun",    IDR_OBJECT_DRONE_GUN,    BROWSER_OBJECT_TAB_OBJECTS },
    { "Tank",         IDR_OBJECT_TANK,         BROWSER_OBJECT_TAB_OBJECTS },
    { "Portal",       IDR_OBJECT_PORTAL,       BROWSER_OBJECT_TAB_SPECIAL },
    { "Key",          IDR_OBJECT_KEY,          BROWSER_OBJECT_TAB_OBJECTS },
    { "Safe",         IDR_OBJECT_SAFE,         BROWSER_OBJECT_TAB_OBJECTS },
    { "Circle",       IDR_OBJECT_CIRCLE,       BROWSER_OBJECT_TAB_PRIMITIVES },
    { "Cylinder",     IDR_OBJECT_CYLINDER,     BROWSER_OBJECT_TAB_PRIMITIVES },
    { "Armor",        IDR_OBJECT_ARMOR,        BROWSER_OBJECT_TAB_OBJECTS },
    { "Monitor",      IDR_OBJECT_MONITOR,      BROWSER_OBJECT_TAB_OBJECTS },
    { "Pad",          IDR_OBJECT_PAD,          BROWSER_OBJECT_TAB_SPECIAL },
    { "Occluder",     IDR_OBJECT_OCCLUDER,     BROWSER_OBJECT_TAB_SPECIAL }
};

/* Row-major order within each tab, independent of drag-and-drop identities. */
static const BrowserObjectType g_BrowserObjectOrder[BROWSER_OBJECT_COUNT] = {
    BROWSER_OBJECT_DOOR, BROWSER_OBJECT_GLASS,
    BROWSER_OBJECT_WEAPON, BROWSER_OBJECT_AMMO,
    BROWSER_OBJECT_ARMOR, BROWSER_OBJECT_MONITOR,
    BROWSER_OBJECT_CCTV, BROWSER_OBJECT_ALARM,
    BROWSER_OBJECT_DRONE_GUN, BROWSER_OBJECT_TANK,
    BROWSER_OBJECT_KEY, BROWSER_OBJECT_SAFE,
    BROWSER_OBJECT_TRIANGLE, BROWSER_OBJECT_QUAD,
    BROWSER_OBJECT_CIRCLE, BROWSER_OBJECT_CYLINDER,
    BROWSER_OBJECT_PAD,
    BROWSER_OBJECT_SPAWN, BROWSER_OBJECT_INTRO_SPLINE,
    BROWSER_OBJECT_INTRO_CAMERA, BROWSER_OBJECT_OUTRO_CAMERA,
    BROWSER_OBJECT_PORTAL, BROWSER_OBJECT_OCCLUDER
};
#define BROWSER_MAX_MODELS 1024
#define BROWSER_MODEL_TAB_H 24
#define BROWSER_MODEL_TAB_COUNT 3
#define BROWSER_MODEL_CHARACTERS 0
#define BROWSER_MODEL_ITEMS 1
#define BROWSER_MODEL_PROPS 2
#define BROWSER_IMAGE_DISPLAY_SCALE 2
#define BROWSER_IMAGE_PREVIEW_SIZE (TEX_THUMB_MAX * BROWSER_IMAGE_DISPLAY_SCALE)
#define BROWSER_IMAGE_CELL_W (BROWSER_IMAGE_PREVIEW_SIZE + 16) /* preview and label padding */
#define BROWSER_IMAGE_LABEL_H 16
#define BROWSER_IMAGE_CELL_H (BROWSER_IMAGE_PREVIEW_SIZE + BROWSER_IMAGE_LABEL_H + 8)
#define BROWSER_IMAGE_MARGIN 4
#define BROWSER_MODEL_TIMER 0x4d54
#define BROWSER_SEARCH_H 30
#define BROWSER_SEARCH_ID 100

typedef struct BrowserState {
    BOOL fileimages; /* Standalone studio grid: filename identity, no game actions. */
    HWND search[BROWSER_SECTION_COUNT];
    char filter[BROWSER_SECTION_COUNT][MAX_PATH];
    BrowserSection sections[BROWSER_SECTION_COUNT];
    TexThumb objecticons[BROWSER_OBJECT_COUNT];
    unsigned char objectpixels[BROWSER_OBJECT_COUNT][TEX_THUMB_MAX * TEX_THUMB_MAX * 4];
    int objecttab;                       /* Objects is the default */
    int objectscroll[BROWSER_OBJECT_TAB_COUNT];
    int hoverobject;
    int pressedobject;
    POINT objectpresspoint;
    BOOL dragobject; /* object palette, distinct from image/model drags */
    TexThumb *images;             /* owned; freed on replace/destroy */
    unsigned char *imagepixels;   /* owned shared pixel block */
    int imagecount;
    TexThumb notexture; /* permanent item zero, independent of project images */
    unsigned char notexturepixels[TEX_THUMB_MAX * TEX_THUMB_MAX * 4];
    BrowserModelItem models[BROWSER_MAX_MODELS];
    ModelThumbnail *modelthumbnails;
    char modelproject[MAX_PATH];
    HWND modelrenderer;
    int selectedmodel;
    DWORD modelrefreshafter;
    int modelcount;
    int modeltab;                        /* Characters is the default */
    int modelcounts[BROWSER_MODEL_TAB_COUNT];
    int modelscroll[BROWSER_MODEL_TAB_COUNT];
    int scroll[BROWSER_SECTION_COUNT];   /* pixels scrolled per body */
    int selectedimage;                   /* grid index, including No Texture */
    HWND tooltip;
    int tooltipimage;                    /* the one image registered as a tool */
    char tooltiptext[384];
    int dragsection;                     /* thumb being dragged, or -1 */
    int dragstarty;
    int dragstartscroll;
    HIMAGELIST dragimage;
    DWORD dragtextureid;
    char dragmodel[64]; /* empty for image drags */
} BrowserState;

#define BROWSER_SCROLLBAR_W 8

static int BrowserImageCount(const BrowserState *state)
{ return state->imagecount + (state->fileimages ? 0 : 1); }

static BOOL BrowserNameMatches(const char *name, const char *filter)
{
    if (!*filter) { return TRUE; }
    for (; *name; name++)
    {
        const char *a = name, *b = filter;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
        if (!*b) { return TRUE; }
    }
    return FALSE;
}

static BOOL BrowserImageMatches(const BrowserState *state, int index)
{
    const char *name = !state->fileimages && index == 0 ? state->notexture.label
        : state->images[index - (state->fileimages ? 0 : 1)].label;
    return BrowserNameMatches(name, state->filter[BROWSER_SECTION_IMAGES]);
}

static int BrowserVisibleImageCount(const BrowserState *state)
{
    int count = 0;
    for (int i = 0; i < BrowserImageCount(state); i++)
        if (BrowserImageMatches(state, i)) { count++; }
    return count;
}

/* Display cells are temporary; selections, drags and actions retain the
 * original asset index so filtering can never change an asset's identity. */
static int BrowserImageCell(const BrowserState *state, int index)
{
    int cell = 0;
    if (index < 0 || index >= BrowserImageCount(state) || !BrowserImageMatches(state, index)) { return -1; }
    for (int i = 0; i < index; i++) if (BrowserImageMatches(state, i)) { cell++; }
    return cell;
}

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

static BOOL BrowserModelMatches(const BrowserState *state, int index)
{
    return BrowserModelCategory(state->models[index].label) == state->modeltab
        && BrowserNameMatches(state->models[index].label, state->filter[BROWSER_SECTION_MODELS]);
}

static void BrowserCountModels(BrowserState *state)
{
    ZeroMemory(state->modelcounts, sizeof(state->modelcounts));
    for (int i = 0; i < state->modelcount; i++)
    {
        int category = BrowserModelCategory(state->models[i].label);
        if (category >= 0 && BrowserNameMatches(state->models[i].label, state->filter[BROWSER_SECTION_MODELS]))
            state->modelcounts[category]++;
    }
}

/* Tabbed sections reserve a fixed strip above their scrolling rows. All
 * scrollbar calculations and content clipping use this same rectangle. */
static RECT BrowserContentRect(const BrowserState *state, int section)
{
    RECT rect = state->sections[section].bodyrc;

    if (!state->fileimages && (section == BROWSER_SECTION_IMAGES || section == BROWSER_SECTION_MODELS))
        rect.top += BROWSER_SEARCH_H;
    if (section == BROWSER_SECTION_MODELS || section == BROWSER_SECTION_OBJECTS)
    {
        rect.top += section == BROWSER_SECTION_OBJECTS ? BROWSER_OBJECT_TAB_H : BROWSER_MODEL_TAB_H;
    }
    if (rect.top > rect.bottom) { rect.top = rect.bottom; }
    return rect;
}

static RECT BrowserObjectTabRect(const BrowserState *state, int tab)
{
    RECT rect = state->sections[BROWSER_SECTION_OBJECTS].bodyrc;
    int width = rect.right - rect.left;

    rect.right = rect.left + width * (tab + 1) / BROWSER_OBJECT_TAB_COUNT;
    rect.left += width * tab / BROWSER_OBJECT_TAB_COUNT;
    if (rect.bottom > rect.top + BROWSER_OBJECT_TAB_H)
    {
        rect.bottom = rect.top + BROWSER_OBJECT_TAB_H;
    }
    return rect;
}

static int BrowserHitObjectTab(const BrowserState *state, POINT point)
{
    int tab;

    if (!state->sections[BROWSER_SECTION_OBJECTS].expanded) { return -1; }
    for (tab = 0; tab < BROWSER_OBJECT_TAB_COUNT; tab++)
    {
        RECT rect = BrowserObjectTabRect(state, tab);

        if (PtInRect(&rect, point)) { return tab; }
    }
    return -1;
}

static RECT BrowserModelTabRect(const BrowserState *state, int tab)
{
    /* Give the longer Characters label half the strip; the other two share
     * the remaining half. Use the same rounded boundaries for hits and paint. */
    static const int boundaries[] = {0, 2, 3, 4};
    RECT rect = state->sections[BROWSER_SECTION_MODELS].bodyrc;
    int width = rect.right - rect.left;

    if (!state->fileimages) { rect.top += BROWSER_SEARCH_H; }
    if (rect.top > rect.bottom) { rect.top = rect.bottom; }
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
 * Tabbed sections report the height of the active category only.
 */
static int BrowserContentHeight(const BrowserState *state, int section)
{
    if (section == BROWSER_SECTION_OBJECTS)
    {
        int i, count = 0, rows;
        for (i = 0; i < BROWSER_OBJECT_COUNT; i++)
        {
            if (g_BrowserObjects[i].tab == state->objecttab) { count++; }
        }
        rows = (count + BROWSER_OBJECT_COLUMNS - 1) / BROWSER_OBJECT_COLUMNS;
        return rows > 0 ? BROWSER_OBJECT_MARGIN * 2
            + rows * (BROWSER_OBJECT_TILE_H + BROWSER_OBJECT_GAP) - BROWSER_OBJECT_GAP : 0;
    }

    if (section == BROWSER_SECTION_IMAGES)
    {
        int columns = BrowserImageColumns(&state->sections[section].bodyrc);
        int count = BrowserVisibleImageCount(state);
        int rows = count / columns + (count % columns != 0);

        return rows > 0 ? rows * BROWSER_IMAGE_CELL_H + BROWSER_IMAGE_MARGIN * 2 : 0;
    }

    if (section == BROWSER_SECTION_MODELS)
    {
        int count = state->modelcounts[state->modeltab];
        int columns = BrowserImageColumns(&state->sections[section].bodyrc);
        int rows = (count + columns - 1) / columns;
        return rows > 0 ? rows * BROWSER_IMAGE_CELL_H + BROWSER_IMAGE_MARGIN * 2 : 0;
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

static void BrowserScrollTo(HWND hwnd, BrowserState *state, int section, int position)
{
    int previous = state->scroll[section];
    state->scroll[section] = position;
    BrowserClampScroll(state, section);
    if (state->scroll[section] != previous)
    {
        /* Tabs and the other accordion sections have not changed. The
         * content rectangle includes this section's scrollbar as well. */
        RECT content = BrowserContentRect(state, section);
        InvalidateRect(hwnd, &content, FALSE);
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

static void BrowserHideImageTooltip(HWND hwnd, BrowserState *state)
{
    if (state->tooltip && state->tooltipimage >= 0)
    {
        TOOLINFO tool = {0};
        tool.cbSize = sizeof(tool);
        tool.hwnd = hwnd;
        tool.uId = (UINT_PTR)state->tooltipimage + 1;
        SendMessage(state->tooltip, TTM_POP, 0, 0);
        SendMessage(state->tooltip, TTM_DELTOOL, 0, (LPARAM)&tool);
    }
    state->tooltipimage = -1;
}

static void BrowserLayoutSearches(BrowserState *state)
{
    for (int i = BROWSER_SECTION_IMAGES; i <= BROWSER_SECTION_MODELS; i++)
    {
        HWND edit = state->search[i];
        RECT rect = state->sections[i].bodyrc, old;
        BOOL visible = state->sections[i].expanded
            && rect.bottom - rect.top >= BROWSER_SEARCH_H && rect.right - rect.left > 8;
        if (!edit) { continue; }
        if (visible)
        {
            rect.left += 4; rect.right -= 4; rect.top += 4; rect.bottom = rect.top + BROWSER_SEARCH_H - 8;
            GetWindowRect(edit, &old);
            MapWindowPoints(NULL, GetParent(edit), (POINT *)&old, 2);
            if (!EqualRect(&rect, &old))
                SetWindowPos(edit, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
        }
        else if (GetFocus() == edit) { SetFocus(GetParent(edit)); }
        if (!!(GetWindowLongPtr(edit, GWL_STYLE) & WS_VISIBLE) != visible)
            ShowWindow(edit, visible ? SW_SHOWNA : SW_HIDE);
    }
}

/* Reserve every header, then give Objects enough height for its active tab
 * when space permits. At short heights it shares the available space and
 * scrolls. The remaining expanded sections split the rest evenly. */
static void BrowserLayoutSections(BrowserState *state, const RECT *client)
{
    int expandedcount = 0, bodyspace, perbody = 0, objectheight = 0;
    int remaining, y = 0, i;
    if (state->fileimages)
    {
        for (i = 0; i < BROWSER_SECTION_COUNT; i++)
        {
            SetRectEmpty(&state->sections[i].headerrc);
            SetRectEmpty(&state->sections[i].bodyrc);
            state->sections[i].expanded = i == BROWSER_SECTION_IMAGES;
        }
        state->sections[BROWSER_SECTION_IMAGES].bodyrc = *client;
        return;
    }
    for (i = 0; i < BROWSER_SECTION_COUNT; i++)
    {
        if (state->sections[i].expanded) { expandedcount++; }
    }
    bodyspace = client->bottom - BROWSER_SECTION_COUNT * BROWSER_HEADER_H;
    if (bodyspace < 0) { bodyspace = 0; }
    remaining = expandedcount;
    if (state->sections[BROWSER_SECTION_OBJECTS].expanded)
    {
        int preferred = BROWSER_OBJECT_TAB_H + BrowserContentHeight(state, BROWSER_SECTION_OBJECTS);
        objectheight = bodyspace / expandedcount;
        if (expandedcount == 1) { objectheight = bodyspace; }
        else if (bodyspace >= preferred * expandedcount)
        { objectheight = preferred; }
        /* Reserve modest useful space for the other bodies before showing
         * all rows, rather than leaving Objects partially clipped. */
        else if (bodyspace >= preferred + (expandedcount - 1) * 100)
        { objectheight = preferred; }
        bodyspace -= objectheight;
        remaining--;
    }
    if (remaining > 0) { perbody = bodyspace / remaining; }
    for (i = 0; i < BROWSER_SECTION_COUNT; i++)
    {
        BrowserSection *sec = &state->sections[i];
        SetRect(&sec->headerrc, 0, y, client->right, y + BROWSER_HEADER_H);
        y = sec->headerrc.bottom;
        SetRect(&sec->bodyrc, 0, y, client->right, y);
        if (sec->expanded)
        {
            int height = objectheight;
            if (i != BROWSER_SECTION_OBJECTS)
            {
                height = remaining == 1 ? bodyspace : perbody;
                bodyspace -= height;
                remaining--;
            }
            sec->bodyrc.bottom = y + height;
            y = sec->bodyrc.bottom;
        }
    }
    BrowserLayoutSearches(state);
}

static void BrowserSelectObjectTab(HWND hwnd, BrowserState *state, int tab)
{
    RECT client;
    int i;
    if (tab == state->objecttab || state->dragimage || state->pressedobject >= 0) { return; }
    state->objectscroll[state->objecttab] = state->scroll[BROWSER_SECTION_OBJECTS];
    state->objecttab = tab;
    state->scroll[BROWSER_SECTION_OBJECTS] = state->objectscroll[tab];
    state->hoverobject = -1;
    GetClientRect(hwnd, &client);
    BrowserLayoutSections(state, &client);
    for (i = 0; i < BROWSER_SECTION_COUNT; i++) { BrowserClampScroll(state, i); }
    InvalidateRect(hwnd, NULL, FALSE);
}

/* Shared tile geometry for painting, mouse hits, and drag previews. Reserve
 * the scrollbar gutter even when all rows fit so columns stay stable.
 * index is a BrowserObjectType in the active tab, not a visible tile index. */
static RECT BrowserObjectRect(const BrowserState *state, int index)
{
    RECT rect = BrowserContentRect(state, BROWSER_SECTION_OBJECTS);
    int i, tile = 0, column;
    int width = rect.right - rect.left - BROWSER_OBJECT_MARGIN * 2
        - BROWSER_SCROLLBAR_W - 2 + BROWSER_OBJECT_GAP;
    for (i = 0; i < BROWSER_OBJECT_COUNT; i++)
    {
        int object = g_BrowserObjectOrder[i];
        if (object == index) { break; }
        if (g_BrowserObjects[object].tab == state->objecttab) { tile++; }
    }
    column = tile % BROWSER_OBJECT_COLUMNS;
    if (width < BROWSER_OBJECT_COLUMNS * BROWSER_OBJECT_GAP)
    { width = BROWSER_OBJECT_COLUMNS * BROWSER_OBJECT_GAP; }
    rect.left += BROWSER_OBJECT_MARGIN + column * width / BROWSER_OBJECT_COLUMNS;
    rect.right = state->sections[BROWSER_SECTION_OBJECTS].bodyrc.left + BROWSER_OBJECT_MARGIN
        + (column + 1) * width / BROWSER_OBJECT_COLUMNS - BROWSER_OBJECT_GAP;
    rect.top += BROWSER_OBJECT_MARGIN + (tile / BROWSER_OBJECT_COLUMNS)
        * (BROWSER_OBJECT_TILE_H + BROWSER_OBJECT_GAP) - state->scroll[BROWSER_SECTION_OBJECTS];
    rect.bottom = rect.top + BROWSER_OBJECT_TILE_H;
    return rect;
}

static int BrowserHitObject(const BrowserState *state, POINT point)
{
    RECT body = BrowserContentRect(state, BROWSER_SECTION_OBJECTS);
    int i;
    if (!state->sections[BROWSER_SECTION_OBJECTS].expanded || !PtInRect(&body, point)) { return -1; }
    for (i = 0; i < BROWSER_OBJECT_COUNT; i++)
    {
        RECT rect;
        if (g_BrowserObjects[i].tab != state->objecttab) { continue; }
        rect = BrowserObjectRect(state, i);
        if (PtInRect(&rect, point)) { return i; }
    }
    return -1;
}

static void BrowserPaintObjectTile(const BrowserState *state, HDC dc,
                                   int index, const RECT *rect, BOOL active)
{
    COLORREF background = ThemeSystemColor(active ? COLOR_HIGHLIGHT : COLOR_BTNFACE);
    COLORREF foreground = ThemeSystemColor(active ? COLOR_HIGHLIGHTTEXT : COLOR_BTNTEXT);
    HGDIOBJ oldbrush = SelectObject(dc, ThemeSystemBrush(active ? COLOR_HIGHLIGHT : COLOR_BTNFACE));
    HGDIOBJ oldpen = SelectObject(dc, GetStockObject(DC_PEN));
    const TexThumb *icon = &state->objecticons[index];
    unsigned char pixels[TEX_THUMB_MAX * TEX_THUMB_MAX * 4] = {0};
    BITMAPINFO bmi = {0};
    RECT label = *rect;
    int x, y, saved = SaveDC(dc);
    IntersectClipRect(dc, rect->left, rect->top, rect->right, rect->bottom);
    SetDCPenColor(dc, ThemeSystemColor(active ? COLOR_HIGHLIGHT : COLOR_BTNSHADOW));
    RoundRect(dc, rect->left, rect->top, rect->right, rect->bottom, 6, 6);
    /* Composite straight-alpha PNG pixels over the tile before drawing with
     * GDI. Keep the source artwork unchanged and preserve its aspect ratio. */
    for (y = 0; y < icon->h; y++) for (x = 0; x < icon->w; x++)
    {
        int offset = (y * TEX_THUMB_MAX + x) * 4;
        const unsigned char *src = state->objectpixels[index] + offset;
        pixels[offset] = (src[0] * src[3] + GetBValue(background) * (255 - src[3]) + 127) / 255;
        pixels[offset + 1] = (src[1] * src[3] + GetGValue(background) * (255 - src[3]) + 127) / 255;
        pixels[offset + 2] = (src[2] * src[3] + GetRValue(background) * (255 - src[3]) + 127) / 255;
        pixels[offset + 3] = 255;
    }
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = TEX_THUMB_MAX;
    bmi.bmiHeader.biHeight = -TEX_THUMB_MAX;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(dc, rect->left + 3 + (BROWSER_OBJECT_ICON_SIZE - icon->w) / 2,
        rect->top + (BROWSER_OBJECT_TILE_H - icon->h) / 2, icon->w, icon->h,
        0, 0, icon->w, icon->h, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
    label.left += 3 + BROWSER_OBJECT_ICON_SIZE + 3;
    label.right -= 12;
    if (label.right > label.left)
    {
        SetTextColor(dc, foreground);
        SetBkMode(dc, TRANSPARENT);
        DrawText(dc, g_BrowserObjects[index].label, -1, &label,
            DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    /* Two columns of three dots make the drag affordance visible. */
    for (y = -4; y <= 4; y += 4) for (x = 0; x <= 3; x += 3)
    {
        RECT dot = {rect->right - 9 + x, (rect->top + rect->bottom) / 2 + y,
                    rect->right - 8 + x, (rect->top + rect->bottom) / 2 + y + 1};
        FillRect(dc, &dot, ThemeSystemBrush(active ? COLOR_HIGHLIGHTTEXT : COLOR_BTNSHADOW));
    }
    RestoreDC(dc, saved);
    SelectObject(dc, oldbrush);
    SelectObject(dc, oldpen);
}

static void BrowserPaintObjects(const BrowserState *state, HDC dc, const RECT *body)
{
    int i;
    for (i = 0; i < BROWSER_OBJECT_COUNT; i++)
    {
        RECT rect;
        if (g_BrowserObjects[i].tab != state->objecttab) { continue; }
        rect = BrowserObjectRect(state, i);
        if (rect.bottom <= body->top || rect.top >= body->bottom) { continue; }
        BrowserPaintObjectTile(state, dc, i, &rect,
            i == state->hoverobject || i == state->pressedobject);
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
    HBRUSH brush = ThemeSystemBrush(COLOR_BTNTEXT);
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

    pen = CreatePen(PS_SOLID, 1, ThemeSystemColor(COLOR_BTNTEXT));
    oldbrush = SelectObject(hdc, brush);
    oldpen = SelectObject(hdc, pen);

    Polygon(hdc, pts, 3);

    SelectObject(hdc, oldbrush);
    SelectObject(hdc, oldpen);
    DeleteObject(pen); /* created objects are ours to free; stock ones are not */
}

/*
 * Image grid: centered thumbnails with their IDs below. Thumbs are top-down BGRA in
 * the shared block; StretchDIBits takes them straight from memory via
 * a negative-height BITMAPINFO, so no per-item GDI bitmaps ever exist.
 */
static const TexThumb *BrowserImageAt(const BrowserState *state, int index,
                                     const unsigned char **pixels)
{
    if (!state->fileimages && index == 0)
    {
        *pixels = state->notexturepixels;
        return &state->notexture;
    }
    if (!state->fileimages) { index--; }
    *pixels = state->imagepixels + state->images[index].pixeloffset;
    return &state->images[index];
}

static int BrowserFindImage(const BrowserState *state, DWORD textureid)
{
    int i;
    if (state->fileimages) { return -1; }
    if (textureid == BG_TEX_NONE) { return 0; }
    if (textureid > BG_TEX_NONE) { return -1; }
    for (i = 0; i < state->imagecount; i++)
    {
        const char *label = state->images[i].label;
        char *end;
        unsigned long id = strtoul(label, &end, 16);
        if (end != label && *end == '\0' && id == textureid) { return i + 1; }
    }
    return -1;
}

static void BrowserPaintImageGrid(BrowserState *state, HDC hdc, const RECT *body)
{
    int columns = BrowserImageColumns(body);
    int width = BrowserImageGridWidth(body);
    int scroll = state->scroll[BROWSER_SECTION_IMAGES];
    int firstrow = scroll > BROWSER_IMAGE_MARGIN
        ? (scroll - BROWSER_IMAGE_MARGIN) / BROWSER_IMAGE_CELL_H : 0;
    int i, cell = -1;
    int oldstretch = SetStretchBltMode(hdc, COLORONCOLOR);
    BITMAPINFO bmi;

    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetTextColor(hdc, ThemeSystemColor(COLOR_WINDOWTEXT));

    for (i = 0; i < BrowserImageCount(state); i++)
    {
        const TexThumb *t;
        const unsigned char *pixels;
        RECT rc;
        int column, y;
        if (!BrowserImageMatches(state, i)) { continue; }
        cell++;
        if (cell < firstrow * columns) { continue; }
        column = cell % columns;
        y = body->top + BROWSER_IMAGE_MARGIN + (cell / columns) * BROWSER_IMAGE_CELL_H - scroll;

        if (y >= body->bottom)
        {
            break;
        }

        t = BrowserImageAt(state, i, &pixels);
        /* Share leftover width between columns, including rounding pixels. */
        rc.left = body->left + BROWSER_IMAGE_MARGIN + column * width / columns;
        rc.right = body->left + BROWSER_IMAGE_MARGIN + (column + 1) * width / columns;
        rc.top = y;
        rc.bottom = y + BROWSER_IMAGE_CELL_H - 2;
        if (i == state->selectedimage)
        {
            FillRect(hdc, &rc, ThemeSystemBrush(COLOR_HIGHLIGHT));
            SetTextColor(hdc, ThemeSystemColor(COLOR_HIGHLIGHTTEXT));
        }
        else { SetTextColor(hdc, ThemeSystemColor(COLOR_WINDOWTEXT)); }

        if (t->w > 0 && t->h > 0)
        {
            int displaywidth = t->w * BROWSER_IMAGE_DISPLAY_SCALE;
            int displayheight = t->h * BROWSER_IMAGE_DISPLAY_SCALE;
            /* The thumb block is stored in GDI's native BGRA order,
               so this call needs no channel gymnastics. Enlarge only the
               destination; the shared thumbnails and image data stay native. */
            bmi.bmiHeader.biWidth = TEX_THUMB_MAX;
            bmi.bmiHeader.biHeight = -t->h; /* negative: top-down */

            StretchDIBits(hdc,
                          rc.left + (rc.right - rc.left - displaywidth) / 2,
                          y + (BROWSER_IMAGE_PREVIEW_SIZE - displayheight) / 2,
                          displaywidth, displayheight,
                          0, 0, t->w, t->h,
                          pixels,
                          &bmi, DIB_RGB_COLORS, SRCCOPY);
        }
        else if (state->fileimages)
        {
            RECT preview = rc; preview.bottom = y + BROWSER_IMAGE_PREVIEW_SIZE;
            DrawText(hdc, "No preview", -1, &preview, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
        }

        rc.top = y + BROWSER_IMAGE_PREVIEW_SIZE + 4;
        rc.bottom = rc.top + BROWSER_IMAGE_LABEL_H;

        DrawText(hdc, t->label, -1, &rc,
                 DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    if (oldstretch) { SetStretchBltMode(hdc, oldstretch); }
}

static void BrowserPaintTab(HDC hdc, RECT rect, const char *name, BOOL active)
{
    SetTextColor(hdc, ThemeSystemColor(COLOR_BTNTEXT));
    FillRect(hdc, &rect, ThemeSystemBrush(active ? COLOR_WINDOW : COLOR_BTNFACE));
    DrawEdge(hdc, &rect, BDR_RAISEDOUTER,
             BF_LEFT | BF_TOP | BF_RIGHT | (active ? 0 : BF_BOTTOM));
    rect.left += 3;
    rect.right -= 3;
    DrawText(hdc, name, -1, &rect,
             DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

static void BrowserPaintObjectTabs(const BrowserState *state, HDC hdc)
{
    static const char *names[BROWSER_OBJECT_TAB_COUNT] = {"Objects", "Primitives", "Special"};
    int tab;

    for (tab = 0; tab < BROWSER_OBJECT_TAB_COUNT; tab++)
    {
        BrowserPaintTab(hdc, BrowserObjectTabRect(state, tab), names[tab], tab == state->objecttab);
    }
}

static void BrowserPaintModelTabs(const BrowserState *state, HDC hdc)
{
    static const char *names[BROWSER_MODEL_TAB_COUNT] = {"Characters", "Items", "Props"};
    int tab;

    for (tab = 0; tab < BROWSER_MODEL_TAB_COUNT; tab++)
    {
        BrowserPaintTab(hdc, BrowserModelTabRect(state, tab), names[tab], tab == state->modeltab);
    }
}

/* The grid and its hit testing share the image browser's column boundaries. */
static BOOL BrowserModelRect(const BrowserState *state, int index, RECT *rect)
{
    RECT body = BrowserContentRect(state, BROWSER_SECTION_MODELS);
    int columns = BrowserImageColumns(&body), width = BrowserImageGridWidth(&body), cell = 0;
    if (!state->sections[BROWSER_SECTION_MODELS].expanded || index < 0 || index >= state->modelcount
        || !BrowserModelMatches(state, index)) { return FALSE; }
    for (int i = 0; i < index; i++)
        if (BrowserModelMatches(state, i)) { cell++; }
    rect->left = body.left + BROWSER_IMAGE_MARGIN + (cell % columns) * width / columns;
    rect->right = body.left + BROWSER_IMAGE_MARGIN + (cell % columns + 1) * width / columns;
    rect->top = body.top + BROWSER_IMAGE_MARGIN + (cell / columns) * BROWSER_IMAGE_CELL_H
        - state->scroll[BROWSER_SECTION_MODELS];
    rect->bottom = rect->top + BROWSER_IMAGE_CELL_H - 2;
    return rect->bottom > body.top && rect->top < body.bottom;
}

static void BrowserPaintModelTile(const BrowserState *state, HDC dc, int index, RECT rect)
{
    const ModelThumbnail *thumbnail = state->modelthumbnails ? &state->modelthumbnails[index] : NULL;
    BOOL selected = index == state->selectedmodel;
    if (selected) { FillRect(dc, &rect, ThemeSystemBrush(COLOR_HIGHLIGHT)); }
    SetTextColor(dc, ThemeSystemColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    if (thumbnail && thumbnail->pixels)
    {
        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = MODEL_THUMBNAIL_SIZE; bmi.bmiHeader.biHeight = -MODEL_THUMBNAIL_SIZE;
        bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32; bmi.bmiHeader.biCompression = BI_RGB;
        StretchDIBits(dc, rect.left + (rect.right - rect.left - MODEL_THUMBNAIL_SIZE) / 2,
            rect.top, MODEL_THUMBNAIL_SIZE, MODEL_THUMBNAIL_SIZE, 0, 0,
            MODEL_THUMBNAIL_SIZE, MODEL_THUMBNAIL_SIZE, thumbnail->pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
    }
    else
    {
        RECT preview = rect; preview.bottom = preview.top + MODEL_THUMBNAIL_SIZE;
        DrawText(dc, thumbnail && thumbnail->pending ? "..." : "No preview", -1,
            &preview, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }
    rect.top += MODEL_THUMBNAIL_SIZE + 4;
    DrawText(dc, state->models[index].label, -1, &rect,
        DT_SINGLELINE | DT_CENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

static void BrowserPaintModelRows(BrowserState *state, HDC hdc, const RECT *body)
{
    int oldstretch = SetStretchBltMode(hdc, COLORONCOLOR);
    for (int i = 0; i < state->modelcount; i++)
    {
        RECT rect;
        if (BrowserModelRect(state, i, &rect)) { BrowserPaintModelTile(state, hdc, i, rect); }
    }
    SetStretchBltMode(hdc, oldstretch);
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

    FillRect(hdc, &track, ThemeSystemBrush(COLOR_BTNFACE));
    FillRect(hdc, &thumb, ThemeSystemBrush(COLOR_BTNSHADOW));
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
    FillRect(hdc, &client, ThemeSystemBrush(COLOR_WINDOW));

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

        if (RectVisible(hdc, &sec->headerrc))
        {
            FillRect(hdc, &sec->headerrc, ThemeSystemBrush(COLOR_BTNFACE));
            BrowserPaintArrow(hdc, &sec->headerrc, sec->expanded);
            text.left += 26;
            SetTextColor(hdc, ThemeSystemColor(COLOR_BTNTEXT));
            DrawText(hdc, sec->name, -1, &text, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        }

        if (sec->expanded && sec->bodyrc.bottom > sec->bodyrc.top && RectVisible(hdc, &sec->bodyrc))
        {
            RECT body = BrowserContentRect(state, i);

            if (i == BROWSER_SECTION_MODELS) { BrowserPaintModelTabs(state, hdc); }
            if (i == BROWSER_SECTION_OBJECTS) { BrowserPaintObjectTabs(state, hdc); }
            if (body.bottom <= body.top) { continue; }
            if (i == BROWSER_SECTION_OBJECTS
                || (i == BROWSER_SECTION_IMAGES && BrowserVisibleImageCount(state) > 0)
                || (i == BROWSER_SECTION_MODELS && state->modelcounts[state->modeltab] > 0))
            {
                int saved = SaveDC(hdc);

                BrowserClampScroll(state, i);
                IntersectClipRect(hdc, body.left, body.top, body.right, body.bottom);

                if (i == BROWSER_SECTION_OBJECTS)
                {
                    BrowserPaintObjects(state, hdc, &body);
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
                SetTextColor(hdc, ThemeSystemColor(COLOR_GRAYTEXT));
                DrawText(hdc, state->filter[i][0] ? "No matches" : "(empty)", -1, &hint, DT_SINGLELINE | DT_TOP | DT_LEFT);
            }
        }
    }

    SelectObject(hdc, oldfont);
}

static void BrowserPaintBuffered(HWND hwnd, HDC hdc, const RECT *dirty)
{
    int width = dirty->right - dirty->left, height = dirty->bottom - dirty->top;
    HDC buffer;
    HBITMAP bitmap;
    BOOL copied = FALSE;
    if (width <= 0 || height <= 0) { return; }
    buffer = CreateCompatibleDC(hdc);
    bitmap = buffer ? CreateCompatibleBitmap(hdc, width, height) : NULL;
    if (bitmap)
    {
        HGDIOBJ previous = SelectObject(buffer, bitmap);
        if (previous && previous != HGDI_ERROR)
        {
            /* Paint in client coordinates into a buffer covering only the
             * dirty rectangle, then present the finished image in one copy.
             * The paint DC retains Windows' actual update-region clipping. */
            if (SetWindowOrgEx(buffer, dirty->left, dirty->top, NULL))
            {
                IntersectClipRect(buffer, dirty->left, dirty->top, dirty->right, dirty->bottom);
                BrowserPaint(hwnd, buffer);
                copied = BitBlt(hdc, dirty->left, dirty->top, width, height,
                    buffer, dirty->left, dirty->top, SRCCOPY);
            }
            SelectObject(buffer, previous);
        }
        DeleteObject(bitmap);
    }
    if (buffer) { DeleteDC(buffer); }
    /* Keep the browser usable if GDI cannot allocate the temporary buffer. */
    if (!copied) { BrowserPaint(hwnd, hdc); }
}

static int BrowserHitImage(const BrowserState *state, POINT point)
{
    const BrowserSection *section = &state->sections[BROWSER_SECTION_IMAGES];
    RECT body = BrowserContentRect(state, BROWSER_SECTION_IMAGES);
    int width = BrowserImageGridWidth(&body);
    int columns = BrowserImageColumns(&body);
    int x = point.x - body.left - BROWSER_IMAGE_MARGIN;
    int y = point.y - body.top - BROWSER_IMAGE_MARGIN
          + state->scroll[BROWSER_SECTION_IMAGES];
    int image;

    if (!section->expanded || !PtInRect(&body, point)
        || x < 0 || x >= width || y < 0)
    {
        return -1;
    }
    /* Invert the painter's rounded column boundaries exactly. */
    image = (y / BROWSER_IMAGE_CELL_H) * columns
          + ((x + 1) * columns - 1) / width;
    for (int i = 0; i < BrowserImageCount(state); i++)
        if (BrowserImageMatches(state, i) && image-- == 0) { return i; }
    return -1;
}

static int BrowserHitModel(const BrowserState *state, POINT point)
{
    RECT body = BrowserContentRect(state, BROWSER_SECTION_MODELS);
    int width = BrowserImageGridWidth(&body), columns = BrowserImageColumns(&body);
    int x = point.x - body.left - BROWSER_IMAGE_MARGIN;
    int y = point.y - body.top - BROWSER_IMAGE_MARGIN + state->scroll[BROWSER_SECTION_MODELS];
    if (!state->sections[BROWSER_SECTION_MODELS].expanded || !PtInRect(&body, point)
        || x < 0 || x >= width || y < 0) { return -1; }
    int cell = (y / BROWSER_IMAGE_CELL_H) * columns + ((x + 1) * columns - 1) / width;
    for (int i = 0; i < state->modelcount; i++)
        if (BrowserModelMatches(state, i) && cell-- == 0) { return i; }
    return -1;
}

static void BrowserUpdateImageTooltip(HWND hwnd, BrowserState *state, WPARAM wparam, LPARAM lparam)
{
    RECT client;
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    TRACKMOUSEEVENT tracking = {0};
    MSG event = {0};
    int index;
    if (state->tooltip == NULL) { return; }
    GetClientRect(hwnd, &client);
    BrowserLayoutSections(state, &client);
    index = BrowserHitImage(state, point);
    int model = index < 0 ? BrowserHitModel(state, point) : -1;
    if (model >= 0) { index = BrowserImageCount(state) + model; }
    if (index != state->tooltipimage)
    {
        BrowserHideImageTooltip(hwnd, state);
        if (index >= 0)
        {
            TOOLINFO tool = {0};
            RECT body = BrowserContentRect(state, BROWSER_SECTION_IMAGES);
            int columns = BrowserImageColumns(&body), width = BrowserImageGridWidth(&body);
            int cell = model < 0 ? BrowserImageCell(state, index) : 0;
            int column = cell % columns;
            if (model >= 0)
            {
                lstrcpyn(state->tooltiptext, state->models[model].label, sizeof(state->tooltiptext));
            }
            else if (state->fileimages)
            {
                const TexThumb *thumb = &state->images[index];
                if (thumb->w && thumb->h)
                    snprintf(state->tooltiptext, sizeof(state->tooltiptext), "%s\r\n%d x %d pixels",
                        thumb->label, thumb->imagewidth, thumb->imageheight);
                else
                    snprintf(state->tooltiptext, sizeof(state->tooltiptext), "%s\r\nPreview unavailable.", thumb->label);
            }
            else if (index == 0)
            {
                lstrcpyn(state->tooltiptext, "No Texture\r\nRemoves the texture from a face.", sizeof(state->tooltiptext));
            }
            else { TexFormatThumbnailInfo(&state->images[index - 1], state->tooltiptext, sizeof(state->tooltiptext)); }
            tool.cbSize = sizeof(tool);
            tool.hwnd = hwnd;
            tool.uId = (UINT_PTR)index + 1;
            tool.lpszText = state->tooltiptext;
            tool.rect.left = body.left + BROWSER_IMAGE_MARGIN + column * width / columns;
            tool.rect.right = body.left + BROWSER_IMAGE_MARGIN + (column + 1) * width / columns;
            tool.rect.top = body.top + BROWSER_IMAGE_MARGIN + (cell / columns) * BROWSER_IMAGE_CELL_H
                - state->scroll[BROWSER_SECTION_IMAGES];
            tool.rect.bottom = tool.rect.top + BROWSER_IMAGE_CELL_H;
            if (model >= 0)
            {
                body = BrowserContentRect(state, BROWSER_SECTION_MODELS);
                BrowserModelRect(state, model, &tool.rect);
            }
            IntersectRect(&tool.rect, &tool.rect, &body);
            if (SendMessage(state->tooltip, TTM_ADDTOOL, 0, (LPARAM)&tool)) { state->tooltipimage = index; }
        }
    }
    tracking.cbSize = sizeof(tracking);
    tracking.dwFlags = TME_LEAVE;
    tracking.hwndTrack = hwnd;
    TrackMouseEvent(&tracking);
    /* Relay after changing the tool: each new image gets its own hover delay.
     * Native tooltips handle placement, wrapping and automatic dismissal. */
    event.hwnd = hwnd;
    event.message = WM_MOUSEMOVE;
    event.wParam = wparam;
    event.lParam = lparam;
    event.time = GetMessageTime();
    event.pt = point;
    ClientToScreen(hwnd, &event.pt);
    SendMessage(state->tooltip, TTM_RELAYEVENT, 0, (LPARAM)&event);
}


/* With the manifested common-controls v6, desktop image-list drags take
 * screen coordinates, including negative positions on monitors left/above
 * the primary. Subtracting the virtual-screen origin here shifts the preview
 * a whole monitor away. Share the same cursor offset for enter and move. */
static BOOL BrowserImageDragPoint(HWND hwnd, POINT *point)
{
    if (!ClientToScreen(hwnd, point))
    {
        return FALSE;
    }
    point->x += 12;
    point->y += 18;
    return TRUE;
}


static void BrowserEndAssetDrag(HWND hwnd, BrowserState *state)
{
    if (state->dragimage != NULL)
    {
        ImageList_DragLeave(GetDesktopWindow());
        ImageList_EndDrag();
        ImageList_Destroy(state->dragimage);
        state->dragimage = NULL;
        state->dragmodel[0] = '\0';
    }
    state->dragobject = FALSE;
    state->pressedobject = -1;
    if (state->dragsection < 0 && GetCapture() == hwnd) { ReleaseCapture(); }
    InvalidateRect(hwnd, &state->sections[BROWSER_SECTION_OBJECTS].bodyrc, FALSE);
}





/* All browser drags share capture, cancellation and multi-monitor coordinates.
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
        !ImageList_DragEnter(GetDesktopWindow(), point.x, point.y))
    {
        ImageList_EndDrag();
        ImageList_Destroy(images);
        return FALSE;
    }
    state->dragimage = images;
    SetFocus(hwnd);
    /* Palette clicks already own capture while waiting for the drag threshold.
       Recapturing the same window sends WM_CAPTURECHANGED and cancels the drag. */
    if (GetCapture() != hwnd) { SetCapture(hwnd); }
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    return TRUE;
}

/* The mouse must first cross the system threshold. Implemented palette kinds
 * ask the frame to prepare for placement; other entries remain preview-only. */
static void BrowserBeginObjectDrag(HWND hwnd, BrowserState *state, int index, POINT point)
{
    RECT rect = BrowserObjectRect(state, index);
    BITMAPINFO bmi = {0};
    unsigned char *pixels;
    HBITMAP bitmap;
    HDC dc;
    HGDIOBJ oldbitmap, oldfont;
    int width = rect.right - rect.left, i;
    if (width < 1) { return; }
    if ((index == BROWSER_OBJECT_OCCLUDER || index == BROWSER_OBJECT_PAD || index == BROWSER_OBJECT_TRIANGLE || index == BROWSER_OBJECT_QUAD
            || index == BROWSER_OBJECT_CIRCLE || index == BROWSER_OBJECT_CYLINDER || index == BROWSER_OBJECT_PORTAL
            || index == BROWSER_OBJECT_SPAWN || index == BROWSER_OBJECT_INTRO_CAMERA || index == BROWSER_OBJECT_OUTRO_CAMERA
            || index == BROWSER_OBJECT_DOOR || index == BROWSER_OBJECT_GLASS
            || index == BROWSER_OBJECT_CCTV || index == BROWSER_OBJECT_ALARM || index == BROWSER_OBJECT_DRONE_GUN
            || index == BROWSER_OBJECT_WEAPON || index == BROWSER_OBJECT_AMMO || index == BROWSER_OBJECT_ARMOR || index == BROWSER_OBJECT_TANK || index == BROWSER_OBJECT_SAFE)
        && !SendMessage(GetParent(hwnd), BROWSER_WM_OBJECT_DRAG_BEGIN, index, 0))
    {
        state->pressedobject = -1;
        if (GetCapture() == hwnd) { ReleaseCapture(); }
        return;
    }
    OffsetRect(&rect, -rect.left, -rect.top);
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -BROWSER_OBJECT_TILE_H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (bitmap == NULL) { return; }
    dc = CreateCompatibleDC(NULL);
    if (dc == NULL) { DeleteObject(bitmap); return; }
    oldbitmap = SelectObject(dc, bitmap);
    oldfont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    FillRect(dc, &rect, ThemeSystemBrush(COLOR_WINDOW));
    BrowserPaintObjectTile(state, dc, index, &rect, TRUE);
    SelectObject(dc, oldfont);
    SelectObject(dc, oldbitmap);
    DeleteDC(dc);
    for (i = 0; i < width * BROWSER_OBJECT_TILE_H; i++) { pixels[i * 4 + 3] = 255; }
    if (BrowserStartAssetDrag(hwnd, state, bitmap, width, BROWSER_OBJECT_TILE_H, point))
    {
        state->dragobject = TRUE;
        state->dragmodel[0] = '\0';
    }
}

static void BrowserBeginModelDrag(HWND hwnd, BrowserState *state, int index, POINT point)
{
    const char *name = state->models[index].label;
    BITMAPINFO bmi = {0};
    HBITMAP bitmap;
    HDC dc;
    HGDIOBJ oldbitmap, oldfont;
    unsigned char *pixels;
    RECT rect = {0, 0, BROWSER_IMAGE_CELL_W, BROWSER_IMAGE_CELL_H};
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
    FillRect(dc, &rect, ThemeSystemBrush(COLOR_HIGHLIGHT));
    SetBkMode(dc, TRANSPARENT);
    BrowserPaintModelTile(state, dc, index, rect);
    SelectObject(dc, oldfont);
    SelectObject(dc, oldbitmap);
    DeleteDC(dc);
    /* GDI does not write alpha for its text/fill pixels. */
    for (i = 0; i < BROWSER_IMAGE_CELL_W * BROWSER_IMAGE_CELL_H; i++)
    {
        pixels[i * 4 + 3] = 255;
    }
    if (BrowserStartAssetDrag(hwnd, state, bitmap, BROWSER_IMAGE_CELL_W, BROWSER_IMAGE_CELL_H, point))
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


static BOOL BrowserModelContextMenu(HWND hwnd, BrowserState *state, POINT screen)
{
    RECT body = state->sections[BROWSER_SECTION_MODELS].bodyrc;
    POINT point = screen;
    HMENU menu;
    UINT command;
    BOOL canimport = state->modelproject[0] != '\0';

    /* The same action is available over models, blank space, and category tabs. */
    if (screen.x == -1 && screen.y == -1) { return FALSE; }
    ScreenToClient(hwnd, &point);
    body.right -= BROWSER_SCROLLBAR_W + 2;
    if (!state->sections[BROWSER_SECTION_MODELS].expanded
        || !PtInRect(&body, point)) { return FALSE; }
    BrowserHideImageTooltip(hwnd, state);
    BrowserEndAssetDrag(hwnd, state);
    SetFocus(hwnd);
    menu = CreatePopupMenu();
    if (!menu) { return TRUE; }
    AppendMenu(menu, MF_STRING | (canimport ? MF_ENABLED : MF_GRAYED), 1, "Import Model...");
    command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             screen.x, screen.y, 0, hwnd, NULL);
    DestroyMenu(menu);
    if (command == 1 && canimport)
    {
        SendMessage(GetParent(hwnd), BROWSER_WM_MODEL_IMPORT, 0, 0);
    }
    return TRUE;
}

static BOOL BrowserAppendSurfaceMenu(HMENU menu, const char *label, UINT first,
    unsigned int current, BOOL available)
{
    HMENU choices = CreatePopupMenu();
    if (!choices) { return FALSE; }
    for (UINT i = 0; i <= 12; i++)
    {
        if (!AppendMenu(choices, MF_STRING | (available && current == i ? MF_CHECKED : 0),
            first + i, TexInfoSurfaceName(i)))
        { DestroyMenu(choices); return FALSE; }
    }
    if (!AppendMenu(menu, MF_POPUP | MF_STRING | (available ? MF_ENABLED : MF_GRAYED),
        (UINT_PTR)choices, label))
    { DestroyMenu(choices); return FALSE; }
    return TRUE;
}

static BOOL BrowserAppendImageActions(HMENU menu, const TexImageInfo *info)
{
    return AppendMenu(menu, MF_STRING, 1, "Delete image")
        && AppendMenu(menu, MF_STRING, 2, "Replace image")
        && AppendMenu(menu, MF_STRING, 3, "Reimport")
        && AppendMenu(menu, MF_STRING, 4, "Export image")
        && AppendMenu(menu, MF_SEPARATOR, 0, NULL)
        && BrowserAppendSurfaceMenu(menu, "Hit sound", 100, info->hitsound, info->surfacevalid)
        && BrowserAppendSurfaceMenu(menu, "Bullet hole", 200, info->hittexture, info->surfacevalid)
        && AppendMenu(menu, MF_SEPARATOR, 0, NULL)
        && AppendMenu(menu, MF_STRING, 6, "Flip vertical")
        && AppendMenu(menu, MF_STRING, 7, "Flip horizontal");
}

static BOOL BrowserDispatchSurfaceCommand(HWND hwnd, UINT command, DWORD textureid)
{
    if (command >= 100 && command <= 112)
    { SendMessage(GetParent(hwnd), BROWSER_WM_IMAGE_HIT_SOUND, textureid, command - 100); return TRUE; }
    if (command >= 200 && command <= 212)
    { SendMessage(GetParent(hwnd), BROWSER_WM_IMAGE_BULLET_HOLE, textureid, command - 200); return TRUE; }
    return FALSE;
}

static LRESULT CALLBACK BrowserWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    BrowserState *state = BrowserGetState(hwnd);

    if (state && (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK
        || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_MOUSEWHEEL
        || msg == WM_SIZE || msg == WM_KILLFOCUS || msg == WM_CANCELMODE))
    {
        BrowserHideImageTooltip(hwnd, state);
    }

    switch (msg)
    {
    case WM_TIMER:
        if (wparam == BROWSER_MODEL_TIMER && state && state->modelthumbnails)
        {
            /* Mouse capture belongs to active drags/painting. Leave those
             * interactions uninterrupted, and coalesce rapid model edits. */
            if (GetCapture() || !IsWindowEnabled(GetAncestor(hwnd, GA_ROOT))
                || IsIconic(GetAncestor(hwnd, GA_ROOT))
                || (LONG)(GetTickCount() - state->modelrefreshafter) < 0) { return 0; }
            int next = -1;
            for (int i = 0; i < state->modelcount; i++) if (state->modelthumbnails[i].pending)
            {
                RECT rect;
                if (next < 0) { next = i; }
                if (BrowserModelRect(state, i, &rect)) { next = i; break; }
            }
            if (next < 0) { KillTimer(hwnd, BROWSER_MODEL_TIMER); return 0; }
            ModelThumbnailUpdate(&state->modelthumbnails[next], hwnd, &state->modelrenderer,
                state->modelproject, state->models[next].label);
            RECT rect, body = BrowserContentRect(state, BROWSER_SECTION_MODELS);
            if (BrowserModelRect(state, next, &rect))
            {
                IntersectRect(&rect, &rect, &body);
                InvalidateRect(hwnd, &rect, FALSE);
            }
            return 0;
        }
        break;
    case WM_CREATE:
        state = (BrowserState *)calloc(1, sizeof(*state));

        if (state == NULL)
        {
            return -1;
        }

        state->fileimages = ((CREATESTRUCT *)lparam)->lpCreateParams != NULL;
        state->sections[BROWSER_SECTION_OBJECTS].name = "Objects";
        state->sections[BROWSER_SECTION_IMAGES].name = "Images";
        state->sections[BROWSER_SECTION_MODELS].name = "Models";
        state->hoverobject = -1;
        state->pressedobject = -1;
        {
            int i;
            for (i = 0; i < BROWSER_SECTION_COUNT; i++) { state->sections[i].expanded = TRUE; }
            for (i = 0; !state->fileimages && i < BROWSER_OBJECT_COUNT; i++)
            {
                if (!TexLoadResourceThumbnail(((CREATESTRUCT *)lparam)->hInstance,
                    g_BrowserObjects[i].icon, &state->objecticons[i], state->objectpixels[i]))
                {
                    free(state);
                    MessageBox(hwnd, "An Object panel icon could not be loaded.", "GEditor", MB_ICONERROR);
                    return -1;
                }
            }
        }
        state->dragsection = -1;
        state->selectedimage = -1;
        state->selectedmodel = -1;
        state->tooltipimage = -1;

        lstrcpyn(state->notexture.label, "No Texture", sizeof(state->notexture.label));
        if (!state->fileimages && !TexLoadResourceThumbnail(((CREATESTRUCT *)lparam)->hInstance,
                IDR_NO_TEXTURE, &state->notexture, state->notexturepixels))
        {
            free(state);
            MessageBox(hwnd, "The No Texture thumbnail could not be loaded.", "GEditor", MB_ICONERROR);
            return -1;
        }

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        for (int i = BROWSER_SECTION_IMAGES; !state->fileimages && i <= BROWSER_SECTION_MODELS; i++)
        {
            state->search[i] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 0, 0,
                hwnd, (HMENU)(INT_PTR)(BROWSER_SEARCH_ID + i), ((CREATESTRUCT *)lparam)->hInstance, NULL);
            SendMessage(state->search[i], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
            SendMessage(state->search[i], EM_SETLIMITTEXT, sizeof(state->filter[i]) - 1, 0);
            SendMessageW(state->search[i], EM_SETCUEBANNER, TRUE,
                (LPARAM)(i == BROWSER_SECTION_IMAGES ? L"Search images..." : L"Search models..."));
        }
        state->tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
            WS_POPUP | TTS_NOPREFIX, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, hwnd, NULL, ((CREATESTRUCT *)lparam)->hInstance, NULL);
        if (state->tooltip)
        {
            SendMessage(state->tooltip, TTM_SETMAXTIPWIDTH, 0, 380);
            SendMessage(state->tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 500);
            SendMessage(state->tooltip, TTM_SETDELAYTIME, TTDT_RESHOW, 500);
            SendMessage(state->tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 15000);
        }
        return 0;

    case WM_COMMAND:
        if (state && HIWORD(wparam) == EN_CHANGE)
        {
            int section = LOWORD(wparam) - BROWSER_SEARCH_ID;
            if (section >= BROWSER_SECTION_IMAGES && section <= BROWSER_SECTION_MODELS
                && (HWND)lparam == state->search[section])
            {
                GetWindowText(state->search[section], state->filter[section], sizeof(state->filter[section]));
                BrowserHideImageTooltip(hwnd, state);
                BrowserEndAssetDrag(hwnd, state);
                state->scroll[section] = 0;
                if (section == BROWSER_SECTION_MODELS)
                {
                    ZeroMemory(state->modelscroll, sizeof(state->modelscroll));
                    BrowserCountModels(state);
                    if (state->selectedmodel >= 0 && !BrowserModelMatches(state, state->selectedmodel))
                        state->selectedmodel = -1;
                }
                else if (state->selectedimage >= 0 && !BrowserImageMatches(state, state->selectedimage))
                    state->selectedimage = -1;
                InvalidateRect(hwnd, &state->sections[section].bodyrc, FALSE);
                return 0;
            }
        }
        break;

    case WM_LBUTTONDBLCLK:
    {
        int x = GET_X_LPARAM(lparam);
        int y = GET_Y_LPARAM(lparam);
        int row;

        /* Accept rapid image drags and tab clicks as ordinary clicks. */
        if (state != NULL)
        {
            RECT client;
            POINT point = {x, y};

            GetClientRect(hwnd, &client);
            BrowserLayoutSections(state, &client);
            row = BrowserHitModel(state, point);
            if (row >= 0)
            {
                char name[sizeof(state->models[row].label)];
                lstrcpyn(name, state->models[row].label, sizeof(name));
                BrowserEndAssetDrag(hwnd, state);
                SendMessage(GetParent(hwnd), BROWSER_WM_MODEL_OPEN, 0, (LPARAM)name);
                return 0;
            }
            if (BrowserHitImage(state, point) >= 0 || BrowserHitModelTab(state, point) >= 0
                || BrowserHitObjectTab(state, point) >= 0
                || BrowserHitObject(state, point) >= 0)
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

            hit = BrowserHitObjectTab(state, p);
            if (hit >= 0)
            {
                BrowserSelectObjectTab(hwnd, state, hit);
                return 0;
            }

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

                    BrowserScrollTo(hwnd, state, i, state->scroll[i] + ((y < thumb.top) ? -page : page));
                    return 0;
                }
            }

            hit = BrowserHitObject(state, p);
            if (hit >= 0)
            {
                SetFocus(hwnd);
                state->pressedobject = hit;
                state->objectpresspoint = p;
                SetCapture(hwnd);
                InvalidateRect(hwnd, &state->sections[BROWSER_SECTION_OBJECTS].bodyrc, FALSE);
                return 0;
            }
            hit = BrowserHitImage(state, p);
            if (hit >= 0)
            {
                state->selectedimage = hit;
                InvalidateRect(hwnd, &state->sections[BROWSER_SECTION_IMAGES].bodyrc, FALSE);
                if (!state->fileimages) { BrowserBeginImageDrag(hwnd, state, hit, p); }
                else { SetFocus(hwnd); }
                return 0;
            }
            hit = BrowserHitModel(state, p);
            if (hit >= 0)
            {
                state->selectedmodel = hit;
                InvalidateRect(hwnd, &state->sections[BROWSER_SECTION_MODELS].bodyrc, FALSE);
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
        if (state != NULL && state->dragimage == NULL && state->dragsection < 0)
        {
            POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            int hover = BrowserHitObject(state, point);
            TRACKMOUSEEVENT tracking = {sizeof(tracking), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tracking);
            if (hover != state->hoverobject)
            {
                state->hoverobject = hover;
                InvalidateRect(hwnd, &state->sections[BROWSER_SECTION_OBJECTS].bodyrc, FALSE);
            }
            if (state->pressedobject >= 0 && (wparam & MK_LBUTTON)
                && (abs(point.x - state->objectpresspoint.x) >= GetSystemMetrics(SM_CXDRAG)
                    || abs(point.y - state->objectpresspoint.y) >= GetSystemMetrics(SM_CYDRAG)))
            {
                BrowserBeginObjectDrag(hwnd, state, state->pressedobject, point);
            }
        }
        if (state && state->dragimage == NULL && state->dragsection < 0
            && !(wparam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)))
        {
            BrowserUpdateImageTooltip(hwnd, state, wparam, lparam);
        }
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
                BrowserScrollTo(hwnd, state, i, state->dragstartscroll + dy * maxscroll / range);
            }
        }
        return 0;

    case WM_MOUSELEAVE:
        if (state)
        {
            BrowserHideImageTooltip(hwnd, state);
            state->hoverobject = -1;
            InvalidateRect(hwnd, &state->sections[BROWSER_SECTION_OBJECTS].bodyrc, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (state != NULL && state->dragimage != NULL)
        {
            BrowserImageDrop drop;
            BrowserModelDrop modeldrop;

            if (state->dragobject)
            {
                BrowserObjectDrop objectdrop;
                objectdrop.type = (BrowserObjectType)state->pressedobject;
                objectdrop.screen.x = GET_X_LPARAM(lparam);
                objectdrop.screen.y = GET_Y_LPARAM(lparam);
                ClientToScreen(hwnd, &objectdrop.screen);
                BrowserEndAssetDrag(hwnd, state);
                if (objectdrop.type == BROWSER_OBJECT_OCCLUDER || objectdrop.type == BROWSER_OBJECT_PAD || objectdrop.type == BROWSER_OBJECT_TRIANGLE || objectdrop.type == BROWSER_OBJECT_QUAD
                    || objectdrop.type == BROWSER_OBJECT_CIRCLE || objectdrop.type == BROWSER_OBJECT_CYLINDER
                    || objectdrop.type == BROWSER_OBJECT_PORTAL
                    || objectdrop.type == BROWSER_OBJECT_SPAWN || objectdrop.type == BROWSER_OBJECT_INTRO_CAMERA
                    || objectdrop.type == BROWSER_OBJECT_OUTRO_CAMERA || objectdrop.type == BROWSER_OBJECT_DOOR
                    || objectdrop.type == BROWSER_OBJECT_GLASS || objectdrop.type == BROWSER_OBJECT_CCTV
                    || objectdrop.type == BROWSER_OBJECT_ALARM || objectdrop.type == BROWSER_OBJECT_DRONE_GUN
                    || objectdrop.type == BROWSER_OBJECT_WEAPON || objectdrop.type == BROWSER_OBJECT_AMMO || objectdrop.type == BROWSER_OBJECT_ARMOR || objectdrop.type == BROWSER_OBJECT_TANK || objectdrop.type == BROWSER_OBJECT_SAFE)
                { SendMessage(GetParent(hwnd), BROWSER_WM_OBJECT_DROP, 0, (LPARAM)&objectdrop); }
                return 0;
            }
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

    case WM_CONTEXTMENU:
        if (state && state->fileimages) { return 0; }
        if (state != NULL)
        {
            POINT screen = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) }, point;
            int index;
            DWORD textureid;
            HMENU menu;
            UINT command;
            if (state->dragobject || state->pressedobject >= 0)
            {
                BrowserEndAssetDrag(hwnd, state);
                return 0;
            }
            if (BrowserModelContextMenu(hwnd, state, screen)) { return 0; }
            if (screen.x == -1 && screen.y == -1)
            {
                index = state->selectedimage;
                if (index <= 0 || index > state->imagecount) { return 0; }
                /* Keyboard context menus open alongside the selected row. */
                BrowserRevealImage(hwnd, (DWORD)strtoul(state->images[index - 1].label, NULL, 16));
                RECT body = BrowserContentRect(state, BROWSER_SECTION_IMAGES);
                int cell = BrowserImageCell(state, index);
                point.x = body.left + BROWSER_IMAGE_MARGIN;
                point.y = body.top + BROWSER_IMAGE_MARGIN
                    + (cell / BrowserImageColumns(&body)) * BROWSER_IMAGE_CELL_H
                    - state->scroll[BROWSER_SECTION_IMAGES] + BROWSER_IMAGE_CELL_H / 2;
                screen = point; ClientToScreen(hwnd, &screen);
            }
            else
            {
                RECT body = BrowserContentRect(state, BROWSER_SECTION_IMAGES);
                point = screen; ScreenToClient(hwnd, &point);
                /* Empty-space actions belong only to the image content,
                   not section headers, other panels, or the scrollbar. */
                body.right -= BROWSER_SCROLLBAR_W + 2;
                if (!state->sections[BROWSER_SECTION_IMAGES].expanded
                    || !PtInRect(&body, point)) { return 0; }
                index = BrowserHitImage(state, point);
                if (index >= 0)
                {
                    /* Right-click gaps around a thumbnail as empty space.
                       Its image and label still open the image's own menu. */
                    const unsigned char *pixels;
                    const TexThumb *thumb = BrowserImageAt(state, index, &pixels);
                    RECT content = BrowserContentRect(state, BROWSER_SECTION_IMAGES);
                    const RECT *grid = &content;
                    int columns = BrowserImageColumns(grid), width = BrowserImageGridWidth(grid);
                    int cell = BrowserImageCell(state, index);
                    int column = cell % columns;
                    int left = grid->left + BROWSER_IMAGE_MARGIN + column * width / columns;
                    int right = grid->left + BROWSER_IMAGE_MARGIN + (column + 1) * width / columns;
                    int top = grid->top + BROWSER_IMAGE_MARGIN
                        + (cell / columns) * BROWSER_IMAGE_CELL_H - state->scroll[BROWSER_SECTION_IMAGES];
                    int imagewidth = thumb->w * BROWSER_IMAGE_DISPLAY_SCALE;
                    int imageheight = thumb->h * BROWSER_IMAGE_DISPLAY_SCALE;
                    RECT image = {left + (right - left - imagewidth) / 2,
                                  top + (BROWSER_IMAGE_PREVIEW_SIZE - imageheight) / 2, 0, 0};
                    RECT label = {left, top + BROWSER_IMAGE_PREVIEW_SIZE + 4,
                                  right, top + BROWSER_IMAGE_PREVIEW_SIZE + 4 + BROWSER_IMAGE_LABEL_H};
                    image.right = image.left + imagewidth;
                    image.bottom = image.top + imageheight;
                    if (!PtInRect(&image, point) && !PtInRect(&label, point)) { index = -1; }
                }
            }
            /* No Texture has no image actions; blank space offers import. */
            if (index == 0 || index > state->imagecount) { return 0; }
            textureid = index > 0 ? (DWORD)strtoul(state->images[index - 1].label, NULL, 16) : BG_TEX_NONE;
            BrowserHideImageTooltip(hwnd, state);
            BrowserEndAssetDrag(hwnd, state);
            if (index > 0) { state->selectedimage = index; }
            SetFocus(hwnd); InvalidateRect(hwnd, NULL, FALSE);
            menu = CreatePopupMenu();
            if (menu == NULL) { return 0; }
            if (index < 0)
            {
                AppendMenu(menu, MF_STRING, 5, "Import image");
            }
            else if (!BrowserAppendImageActions(menu, &state->images[index - 1].info))
            { DestroyMenu(menu); return 0; }
            command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                     screen.x, screen.y, 0, hwnd, NULL);
            DestroyMenu(menu);
            if (index > 0 && BrowserDispatchSurfaceCommand(hwnd, command, textureid)) { return 0; }
            if (command == 5)
            {
                SendMessage(GetParent(hwnd), BROWSER_WM_IMAGE_IMPORT, 0, 0);
            }
            else if (command == 6 || command == 7)
            {
                SendMessage(GetParent(hwnd), command == 6 ? BROWSER_WM_IMAGE_FLIP_VERTICAL
                    : BROWSER_WM_IMAGE_FLIP_HORIZONTAL, textureid, 0);
            }
            else if (command >= 1 && command <= 4)
            {
                SendMessage(GetParent(hwnd), command == 1 ? BROWSER_WM_IMAGE_DELETE
                    : command == 2 ? BROWSER_WM_IMAGE_REPLACE
                    : command == 3 ? BROWSER_WM_IMAGE_REIMPORT : BROWSER_WM_IMAGE_EXPORT, textureid, 0);
            }
        }
        return 0;

    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && state != NULL
            && (state->dragimage != NULL || state->pressedobject >= 0))
        {
            BrowserEndAssetDrag(hwnd, state);
            return 0;
        }
        break;

    case WM_KILLFOCUS:
        if (state != NULL) { BrowserEndAssetDrag(hwnd, state); }
        break;

    case WM_MOUSEWHEEL:
        if (state != NULL && (state->dragimage != NULL || state->pressedobject >= 0)) { return 0; }
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
            if (state->hoverobject >= 0)
            {
                state->hoverobject = -1;
                InvalidateRect(hwnd, &state->sections[BROWSER_SECTION_OBJECTS].bodyrc, FALSE);
            }

            for (i = 0; i < BROWSER_SECTION_COUNT; i++)
            {
                BrowserSection *sec = &state->sections[i];

                if (sec->expanded && PtInRect(&sec->bodyrc, p))
                {
                    int notches = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
                    int step = i == BROWSER_SECTION_IMAGES || i == BROWSER_SECTION_MODELS
                        ? BROWSER_IMAGE_CELL_H : 48;

                    BrowserScrollTo(hwnd, state, i, state->scroll[i] - notches * step);
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
            || (state != NULL && (BrowserHitModelTab(state, p) >= 0
                || BrowserHitObjectTab(state, p) >= 0 || BrowserHitObject(state, p) >= 0)))
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

            if (state->dragobject || state->pressedobject >= 0) { BrowserEndAssetDrag(hwnd, state); }
            state->hoverobject = -1;
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
        BrowserPaintBuffered(hwnd, hdc, &ps.rcPaint);
        EndPaint(hwnd, &ps);
        if (dragging) { ImageList_DragShowNolock(TRUE); }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        if (state != NULL)
        {
            BrowserHideImageTooltip(hwnd, state);
            if (state->tooltip) { DestroyWindow(state->tooltip); }
            BrowserEndAssetDrag(hwnd, state);
            KillTimer(hwnd, BROWSER_MODEL_TIMER);
            for (int i = 0; state->modelthumbnails && i < state->modelcount; i++)
                ModelThumbnailFree(&state->modelthumbnails[i]);
            free(state->modelthumbnails);
            if (state->modelrenderer) { ViewportDestroyThumbnail(state->modelrenderer); }
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
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 16, 16, /* placeholder; the parent's layout positions it */
        parent, NULL, hinstance, NULL);
}

HWND BrowserCreateImagePanel(HWND parent, HINSTANCE hinstance, int controlid)
{
    return CreateWindowEx(WS_EX_CLIENTEDGE, BROWSER_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 16, 16,
        parent, (HMENU)(INT_PTR)controlid, hinstance, (void *)1);
}

BOOL BrowserHandleMessage(HWND browser, MSG *message)
{
    BrowserState *state = BrowserGetState(browser);
    int section;
    if (!state || !message || message->message < WM_KEYFIRST || message->message > WM_KEYLAST) { return FALSE; }
    for (section = BROWSER_SECTION_IMAGES; section <= BROWSER_SECTION_MODELS; section++)
        if (state->search[section] && message->hwnd == state->search[section]) { break; }
    if (section > BROWSER_SECTION_MODELS) { return FALSE; }
    if (message->message == WM_KEYDOWN)
    {
        if (message->wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
        { SendMessage(message->hwnd, EM_SETSEL, 0, -1); return TRUE; }
        if (message->wParam == VK_ESCAPE)
        { SetWindowText(message->hwnd, ""); return TRUE; }
        if (message->wParam == VK_RETURN)
        { SetFocus(browser); return TRUE; }
        if (message->wParam == VK_TAB)
        {
            HWND other = state->search[section == BROWSER_SECTION_IMAGES ? BROWSER_SECTION_MODELS : BROWSER_SECTION_IMAGES];
            SetFocus(other && IsWindowVisible(other) ? other : browser);
            return TRUE;
        }
    }
    /* Text editing (including Ctrl+Z/C/X/V) must precede editor accelerators. */
    TranslateMessage(message);
    DispatchMessage(message);
    return TRUE;
}

void BrowserSetImages(HWND browser, TexThumb *items, int count,
                      unsigned char *pixelblock)
{
    BrowserState *state = BrowserGetState(browser);
    DWORD selectedid = BG_TEX_NONE;
    char selectedfile[MAX_PATH] = "";
    BOOL hadselection = FALSE;

    if (state == NULL)
    {
        /* No state to own them: honour the contract by freeing. */
        free(items);
        free(pixelblock);
        return;
    }

    if (state->fileimages && state->selectedimage >= 0 && state->selectedimage < state->imagecount)
        lstrcpyn(selectedfile, state->images[state->selectedimage].label, sizeof(selectedfile));
    else if (!state->fileimages && state->selectedimage >= 0 && state->selectedimage <= state->imagecount)
    {
        hadselection = TRUE;
        if (state->selectedimage > 0)
        { selectedid = (DWORD)strtoul(state->images[state->selectedimage - 1].label, NULL, 16); }
    }
    BrowserHideImageTooltip(browser, state);
    BrowserEndAssetDrag(browser, state);
    free(state->images);
    free(state->imagepixels);

    state->images = items;
    state->imagepixels = pixelblock;
    state->imagecount = items != NULL ? count : 0;
    if (items == NULL) { state->scroll[BROWSER_SECTION_IMAGES] = 0; }
    state->selectedimage = hadselection && items != NULL ? BrowserFindImage(state, selectedid) : -1;
    if (selectedfile[0])
        for (int i = 0; i < state->imagecount; i++)
            if (!lstrcmpi(selectedfile, state->images[i].label)) { state->selectedimage = i; break; }
    if (!items && state->search[BROWSER_SECTION_IMAGES]) { SetWindowText(state->search[BROWSER_SECTION_IMAGES], ""); }
    BrowserClampScroll(state, BROWSER_SECTION_IMAGES);

    InvalidateRect(browser, NULL, TRUE);
}


BOOL BrowserCopyImageThumbnail(HWND browser, DWORD textureid, TexThumb *thumb,
                               unsigned char *pixels)
{
    BrowserState *state = BrowserGetState(browser);
    const TexThumb *source;
    const unsigned char *sourcepixels;
    int index;
    if (state == NULL || thumb == NULL || pixels == NULL
        || (index = BrowserFindImage(state, textureid)) < 0) { return FALSE; }
    source = BrowserImageAt(state, index, &sourcepixels);
    if (sourcepixels == NULL || source->w <= 0 || source->h <= 0
        || source->w > TEX_THUMB_MAX || source->h > TEX_THUMB_MAX) { return FALSE; }
    *thumb = *source;
    thumb->pixeloffset = 0;
    CopyMemory(pixels, sourcepixels, TEX_THUMB_MAX * TEX_THUMB_MAX * 4);
    return TRUE;
}

BOOL BrowserRevealImage(HWND browser, DWORD textureid)
{
    BrowserState *state = BrowserGetState(browser);
    RECT client, body;
    int index, cell, top, height;
    if (state == NULL || (index = BrowserFindImage(state, textureid)) < 0) { return FALSE; }
    BrowserHideImageTooltip(browser, state);
    /* An explicit reveal must also work when a search currently hides it. */
    if (!BrowserImageMatches(state, index)) { SetWindowText(state->search[BROWSER_SECTION_IMAGES], ""); }
    cell = BrowserImageCell(state, index);
    state->sections[BROWSER_SECTION_IMAGES].expanded = TRUE;
    GetClientRect(browser, &client);
    BrowserLayoutSections(state, &client);
    body = BrowserContentRect(state, BROWSER_SECTION_IMAGES);
    top = BROWSER_IMAGE_MARGIN + (cell / BrowserImageColumns(&body)) * BROWSER_IMAGE_CELL_H;
    height = body.bottom - body.top;
    /* Center the row when possible, including after expanding a closed section. */
    state->scroll[BROWSER_SECTION_IMAGES] = top
        - (height > BROWSER_IMAGE_CELL_H ? (height - BROWSER_IMAGE_CELL_H) / 2 : 0);
    state->selectedimage = index;
    BrowserClampScroll(state, BROWSER_SECTION_IMAGES);
    InvalidateRect(browser, NULL, FALSE);
    return TRUE;
}


static int BrowserCompareModels(const void *left, const void *right)
{
    return lstrcmpi(((const BrowserModelItem *)left)->label, ((const BrowserModelItem *)right)->label);
}

void BrowserSetModels(HWND browser, const BrowserModelItem *items, int count, const char *project)
{
    BrowserState *state = BrowserGetState(browser);
    if (!state) { return; }
    BrowserHideImageTooltip(browser, state);
    BrowserEndAssetDrag(browser, state);
    KillTimer(browser, BROWSER_MODEL_TIMER);
    for (int i = 0; state->modelthumbnails && i < state->modelcount; i++)
        ModelThumbnailFree(&state->modelthumbnails[i]);
    free(state->modelthumbnails); state->modelthumbnails = NULL;
    if (state->modelrenderer) { ViewportDestroyThumbnail(state->modelrenderer); state->modelrenderer = NULL; }
    if (!items || count < 0) { count = 0; }
    if (count > BROWSER_MAX_MODELS) { count = BROWSER_MAX_MODELS; }
    lstrcpyn(state->modelproject, project ? project : "", sizeof(state->modelproject));
    ZeroMemory(state->modelscroll, sizeof(state->modelscroll));
    for (int i = 0; i < count; i++) { state->models[i] = items[i]; }
    qsort(state->models, count, sizeof(state->models[0]), BrowserCompareModels);
    state->modelcount = count; state->selectedmodel = -1;
    if (!items && state->search[BROWSER_SECTION_MODELS]) { SetWindowText(state->search[BROWSER_SECTION_MODELS], ""); }
    BrowserCountModels(state);
    state->scroll[BROWSER_SECTION_MODELS] = 0;
    if (count && state->modelproject[0])
    {
        state->modelthumbnails = calloc(count, sizeof(*state->modelthumbnails));
        if (state->modelthumbnails)
        {
            for (int i = 0; i < count; i++) { state->modelthumbnails[i].pending = TRUE; }
            state->modelrefreshafter = GetTickCount();
            SetTimer(browser, BROWSER_MODEL_TIMER, 30, NULL);
        }
    }
    InvalidateRect(browser, NULL, FALSE);
}

void BrowserRefreshModelThumbnail(HWND browser, const char *name)
{
    BrowserState *state = BrowserGetState(browser);
    if (!state || !state->modelthumbnails || !name) { return; }
    for (int i = 0; i < state->modelcount; i++) if (!lstrcmpi(state->models[i].label, name))
    {
        state->modelthumbnails[i].pending = TRUE;
        state->modelrefreshafter = GetTickCount() + 150;
        SetTimer(browser, BROWSER_MODEL_TIMER, 30, NULL);
        break;
    }
}

void BrowserRefreshModelImage(HWND browser, DWORD textureid)
{
    BrowserState *state = BrowserGetState(browser);
    if (!state || !state->modelthumbnails) { return; }
    for (int i = 0; i < state->modelcount; i++)
        if (ModelThumbnailUsesImage(&state->modelthumbnails[i], textureid))
            state->modelthumbnails[i].pending = TRUE;
    state->modelrefreshafter = GetTickCount() + 150;
    SetTimer(browser, BROWSER_MODEL_TIMER, 30, NULL);
}
