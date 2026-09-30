/* Real preference load/save code with an in-memory registry. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "windows.h"
#include "editorunits.h"
typedef DWORD COLORREF;
typedef void *HBRUSH;
#include "theme.h"
static EditorTheme g_EditorTheme = EDITOR_THEME_DARK;
static EditorTheme applied_theme;
static int theme_applies;
void ThemeSet(EditorTheme theme) { applied_theme=theme; theme_applies++; }
static DWORD theme_stored, theme_type=4, theme_size=sizeof(DWORD);
static int theme_exists;
typedef int HKEY;
typedef unsigned char BYTE;
#define HKEY_CURRENT_USER 1
#define KEY_QUERY_VALUE 2
#define KEY_SET_VALUE 4
#define ERROR_SUCCESS 0
#define REG_DWORD 4
#define REG_OPTION_NON_VOLATILE 0
#define EDITOR_SETTINGS_KEY "Software\\GUD\\GEditor\\Editor Settings"
static EditorCoordinateUnits g_CoordinateUnits = EDITOR_UNITS_NATIVE;
static DWORD stored, storedtype=REG_DWORD, storedsize=sizeof(DWORD);
static int exists, unavailable, writes;
static LONG RegOpenKeyExA(HKEY root, const char *name, DWORD options, DWORD access, HKEY *key)
{ assert(root==HKEY_CURRENT_USER&&!strcmp(name,EDITOR_SETTINGS_KEY)); *key=1; return exists?0:1; }
static LONG RegQueryValueExA(HKEY key, const char *name, void *unused, DWORD *type, BYTE *out, DWORD *size)
{
    if (!strcmp(name,"Theme")) {
        if (!theme_exists) return 1;
        *type=theme_type; *size=theme_size; memcpy(out,&theme_stored,sizeof(theme_stored)); return 0;
    }
    assert(!strcmp(name,"Coordinate units")); *type=storedtype; *size=storedsize; memcpy(out,&stored,sizeof(stored)); return 0;
}
static LONG RegCreateKeyExA(HKEY root, const char *name, DWORD unused, void *cls,
    DWORD options, DWORD access, void *security, HKEY *key, void *disposition)
{ *key=1; if(unavailable)return 1; exists=1; return 0; }
static LONG RegSetValueExA(HKEY key, const char *name, DWORD unused, DWORD type, const BYTE *data, DWORD size)
{
    assert(type==REG_DWORD&&size==sizeof(DWORD));
    if (!strcmp(name,"Theme")) { memcpy(&theme_stored,data,size); theme_type=type; theme_size=size; theme_exists=1; }
    else { assert(!strcmp(name,"Coordinate units")); memcpy(&stored,data,size); storedtype=type; storedsize=size; }
    writes++; return 0;
}
static void RegCloseKey(HKEY key) {}
#include "preferences.inc"

int main(void)
{
    EditorSettingsLoad(); assert(EditorSettingsGetUnits()==EDITOR_UNITS_NATIVE);
    EditorSettingsSetUnits(EDITOR_UNITS_WORLD); assert(writes==1&&stored==EDITOR_UNITS_WORLD);
    g_CoordinateUnits=EDITOR_UNITS_NATIVE; EditorSettingsLoad(); assert(EditorSettingsGetUnits()==EDITOR_UNITS_WORLD);
    EditorSettingsSetUnits(EDITOR_UNITS_WORLD); assert(writes==1);
    EditorSettingsSetUnits(EDITOR_UNITS_NATIVE); EditorSettingsLoad(); assert(EditorSettingsGetUnits()==EDITOR_UNITS_NATIVE);
    int before=writes; EditorSettingsSetUnits((EditorCoordinateUnits)99); assert(writes==before);
    stored=99; EditorSettingsLoad(); assert(EditorSettingsGetUnits()==EDITOR_UNITS_NATIVE);
    stored=EDITOR_UNITS_WORLD; storedtype=1; EditorSettingsLoad(); assert(EditorSettingsGetUnits()==EDITOR_UNITS_NATIVE);
    storedtype=REG_DWORD; storedsize=1; EditorSettingsLoad(); assert(EditorSettingsGetUnits()==EDITOR_UNITS_NATIVE);
    unavailable=1; EditorSettingsSetUnits(EDITOR_UNITS_WORLD);
    assert(EditorSettingsGetUnits()==EDITOR_UNITS_WORLD&&writes==before);
    puts("PASS: native default, saved choice/reload, unchanged and invalid choices, malformed registry and session-only fallback.");
    unavailable=0; exists=0; theme_exists=0; EditorSettingsLoad();
    assert(EditorSettingsGetTheme()==EDITOR_THEME_DARK);
    before=writes; EditorSettingsSetTheme(EDITOR_THEME_LIGHT);
    assert(writes==before+1 && applied_theme==EDITOR_THEME_LIGHT && theme_applies==1);
    EditorSettingsLoad(); assert(EditorSettingsGetTheme()==EDITOR_THEME_LIGHT);
    EditorSettingsSetTheme(EDITOR_THEME_LIGHT); assert(writes==before+1 && theme_applies==1);
    EditorSettingsSetTheme((EditorTheme)99); assert(writes==before+1 && theme_applies==1);
    EditorSettingsSetTheme(EDITOR_THEME_DARK); EditorSettingsLoad();
    assert(EditorSettingsGetTheme()==EDITOR_THEME_DARK && theme_applies==2);
    theme_stored=99; EditorSettingsLoad(); assert(EditorSettingsGetTheme()==EDITOR_THEME_DARK);
    theme_stored=EDITOR_THEME_LIGHT; theme_type=1; EditorSettingsLoad(); assert(EditorSettingsGetTheme()==EDITOR_THEME_DARK);
    theme_type=REG_DWORD; theme_size=1; EditorSettingsLoad(); assert(EditorSettingsGetTheme()==EDITOR_THEME_DARK);
    before=writes; unavailable=1; EditorSettingsSetTheme(EDITOR_THEME_LIGHT);
    assert(EditorSettingsGetTheme()==EDITOR_THEME_LIGHT && applied_theme==EDITOR_THEME_LIGHT && writes==before);
    puts("PASS: dark default, immediate Light/Dark application, persistence, invalid values and unavailable registry.");
    return 0;
}
