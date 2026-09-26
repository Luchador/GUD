/* Real preference load/save code with an in-memory registry. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "windows.h"
#include "editorunits.h"
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
{ assert(!strcmp(name,"Coordinate units")); *type=storedtype; *size=storedsize; memcpy(out,&stored,sizeof(stored)); return 0; }
static LONG RegCreateKeyExA(HKEY root, const char *name, DWORD unused, void *cls,
    DWORD options, DWORD access, void *security, HKEY *key, void *disposition)
{ *key=1; if(unavailable)return 1; exists=1; return 0; }
static LONG RegSetValueExA(HKEY key, const char *name, DWORD unused, DWORD type, const BYTE *data, DWORD size)
{ assert(type==REG_DWORD&&size==sizeof(DWORD)); memcpy(&stored,data,size); storedtype=type; storedsize=size; writes++; return 0; }
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
    return 0;
}
