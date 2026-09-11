#ifndef GEDITOR_BROWSER_H
#define GEDITOR_BROWSER_H

#include <windows.h>

/*
 * Sent to the browser's parent when a level row is double-clicked.
 * wparam: level index. lparam: the row's label (char *), valid only
 * for the duration of the message.
 */
#define BROWSER_WM_LEVEL_OPEN (WM_APP + 1)

/* The parent accepts a drag only when the current tool/level supports it.
 * DROP is synchronous; lparam points to a request valid during SendMessage. */
#define BROWSER_WM_IMAGE_DRAG_BEGIN (WM_APP + 12)
#define BROWSER_WM_IMAGE_DROP       (WM_APP + 13)
/* Image context actions. wparam is the stable texture ID. */
#define BROWSER_WM_IMAGE_DELETE     (WM_APP + 27)
#define BROWSER_WM_IMAGE_REPLACE    (WM_APP + 28)
typedef struct BrowserImageDrop {
    DWORD textureid;
    POINT screen;
} BrowserImageDrop;

/* BEGIN lparam is the model name; DROP points to the copied name and screen
   position. Items remain browse-only. Both messages are synchronous. */
#define BROWSER_WM_MODEL_DRAG_BEGIN (WM_APP + 19)
#define BROWSER_WM_MODEL_DROP       (WM_APP + 20)
typedef struct BrowserModelDrop {
    char name[64];
    POINT screen;
} BrowserModelDrop;

/* Stable palette identities, independent of the displayed tab/row. Unsupported entries
 * still have a drag preview; only implemented kinds send placement requests. */
typedef enum BrowserObjectType {
    BROWSER_OBJECT_TRIANGLE, BROWSER_OBJECT_QUAD,
    BROWSER_OBJECT_SPAWN, BROWSER_OBJECT_INTRO_SPLINE,
    BROWSER_OBJECT_INTRO_CAMERA, BROWSER_OBJECT_OUTRO_CAMERA,
    BROWSER_OBJECT_DOOR, BROWSER_OBJECT_GLASS,
    BROWSER_OBJECT_WEAPON, BROWSER_OBJECT_AMMO,
    BROWSER_OBJECT_CCTV, BROWSER_OBJECT_ALARM,
    BROWSER_OBJECT_DRONE_GUN, BROWSER_OBJECT_TANK, BROWSER_OBJECT_PORTAL,
    BROWSER_OBJECT_KEY, BROWSER_OBJECT_SAFE,
    BROWSER_OBJECT_CIRCLE, BROWSER_OBJECT_CYLINDER,
    BROWSER_OBJECT_COUNT
} BrowserObjectType;
#define BROWSER_WM_OBJECT_DRAG_BEGIN (WM_APP + 29) /* wparam: BrowserObjectType */
#define BROWSER_WM_OBJECT_DROP       (WM_APP + 30) /* lparam: BrowserObjectDrop */
typedef struct BrowserObjectDrop {
    BrowserObjectType type;
    POINT screen;
} BrowserObjectDrop;

BOOL BrowserRegisterClass(HINSTANCE hinstance);
HWND BrowserCreate(HWND parent, HINSTANCE hinstance);

#include "texload.h"

/*
 * Hands the Images section its thumbnails. The browser TAKES OWNERSHIP
 * of both allocations (the item array and the shared pixel block) and
 * frees them on replacement, clearing, or destruction. NULLs clear.
 */
void BrowserSetImages(HWND browser, TexThumb *items, int count,
                      unsigned char *pixelblock);

/* Copy the browser's label and fixed-stride thumbnail into caller-owned
 * storage (TEX_THUMB_MAX * TEX_THUMB_MAX * 4 bytes). BG_TEX_NONE resolves to
 * the permanent No Texture entry. FALSE means the image is unavailable. */
BOOL BrowserCopyImageThumbnail(HWND browser, DWORD textureid, TexThumb *thumb,
                               unsigned char *pixels);

/* Expand Images, scroll the matching item into view, and highlight it. */
BOOL BrowserRevealImage(HWND browser, DWORD textureid);

/* One row in the Levels section. */
typedef struct BrowserLevelItem {
    char label[64];
} BrowserLevelItem;

/*
 * Replaces the Levels section's contents. Copies the items, so the
 * caller's array can live on the stack. NULL/0 clears the section.
 */
void BrowserSetLevels(HWND browser, const BrowserLevelItem *items, int count);

/* Same contract for Models (labels only, copied). Rows are grouped into the
 * Characters, Items, and Props tabs by their C/G/P model-name prefixes. */
void BrowserSetModels(HWND browser, const BrowserLevelItem *items, int count);

#endif
