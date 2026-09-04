#ifndef GEDITOR_BROWSER_H
#define GEDITOR_BROWSER_H

#include <windows.h>


BOOL BrowserRegisterClass(HINSTANCE hinstance);
HWND BrowserCreate(HWND parent, HINSTANCE hinstance);

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