#ifndef GEDITOR_THEME_H
#define GEDITOR_THEME_H
#include <windows.h>

typedef enum EditorTheme { EDITOR_THEME_LIGHT, EDITOR_THEME_DARK } EditorTheme;
typedef enum ThemeColorRole {
    THEME_BACKGROUND, THEME_PANEL, THEME_INPUT, THEME_BUTTON,
    THEME_HOVER, THEME_PRESSED, THEME_BORDER, THEME_TEXT,
    THEME_MUTED, THEME_SELECTION, THEME_SELECTION_TEXT,
    THEME_MENU, THEME_TITLE, THEME_TITLE_TEXT, THEME_ERROR_BACKGROUND,
    THEME_MENU_BORDER, THEME_COLOR_COUNT
} ThemeColorRole;

/* Palette definitions live at the top of theme.c. Brush handles are borrowed. */
COLORREF ThemeColor(ThemeColorRole role);
HBRUSH ThemeBrush(ThemeColorRole role);
COLORREF ThemeSystemColor(int index);
HBRUSH ThemeSystemBrush(int index);
BOOL ThemeInitialize(EditorTheme theme);
void ThemeSet(EditorTheme theme);
void ThemeShutdown(void);
#endif
