#ifndef GEDITOR_BROWSER_H
#define GEDITOR_BROWSER_H

#include <windows.h>

/*
 * Sent to the browser's parent when a level row is double-clicked.
 * wparam: level index. lparam: the row's label (char *), valid only
 * for the duration of the message.
 */
#define BROWSER_WM_LEVEL_OPEN (WM_APP + 1)

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

/* One row in the Levels section. */
typedef struct BrowserLevelItem {
    char label[64];
} BrowserLevelItem;

/*
 * Replaces the Levels section's contents. Copies the items, so the
 * caller's array can live on the stack. NULL/0 clears the section.
 */
void BrowserSetLevels(HWND browser, const BrowserLevelItem *items, int count);

#endif