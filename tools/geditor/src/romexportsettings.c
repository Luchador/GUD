#include "romexportsettings.h"
#include "editorpath.h"

#define ROM_EXPORT_SETTINGS_KEY "Software\\GUD\\GEditor\\Create ROM"

static struct {
    BOOL loaded;
    char name[ROM_EXPORT_NAME_MAX];
    char directory[MAX_PATH];
} g_RomExportSettings;

static BOOL ReadString(HKEY key, const char *value, char *out, DWORD capacity)
{
    DWORD type = 0, size = capacity;
    if (RegQueryValueExA(key, value, NULL, &type, (BYTE *)out, &size) == ERROR_SUCCESS
        && type == REG_SZ && size > 1 && size <= capacity
        && out[size - 1] == '\0' && strlen(out) + 1 == size) { return TRUE; }
    out[0] = '\0';
    return FALSE;
}

void RomExportSettingsLoad(const GEditorProject *project,
    char name[ROM_EXPORT_NAME_MAX], char directory[MAX_PATH])
{
    if (!g_RomExportSettings.loaded)
    {
        HKEY key;
        const char *why;
        g_RomExportSettings.loaded = TRUE;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, ROM_EXPORT_SETTINGS_KEY, 0,
                          KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
        {
            if (ReadString(key, "Name", g_RomExportSettings.name, sizeof(g_RomExportSettings.name))
                && !RomExportNameIsValid(g_RomExportSettings.name, &why))
            { g_RomExportSettings.name[0] = '\0'; }
            ReadString(key, "Directory", g_RomExportSettings.directory, sizeof(g_RomExportSettings.directory));
            RegCloseKey(key);
        }
    }
    lstrcpyn(name, g_RomExportSettings.name[0] ? g_RomExportSettings.name : project->name, ROM_EXPORT_NAME_MAX);
    lstrcpyn(directory, g_RomExportSettings.directory[0] ? g_RomExportSettings.directory : project->dir, MAX_PATH);
    EditorPathNormalize(directory);
}

void RomExportSettingsRemember(const char *name, const char *directory)
{
    HKEY key;
    char fullpath[MAX_PATH];
    const char *why;
    DWORD length;
    if (!RomExportNameIsValid(name, &why) || !directory || !directory[0]) { return; }
    length = GetFullPathNameA(directory, sizeof(fullpath), fullpath, NULL);
    if (!length || length >= sizeof(fullpath)) { return; }
    EditorPathNormalize(fullpath);
    g_RomExportSettings.loaded = TRUE;
    lstrcpyn(g_RomExportSettings.name, name, sizeof(g_RomExportSettings.name));
    lstrcpyn(g_RomExportSettings.directory, fullpath, sizeof(g_RomExportSettings.directory));

    /* A preference-write failure must not turn a successful export into an
     * error. The remembered values still work for the current session. */
    if (RegCreateKeyExA(HKEY_CURRENT_USER, ROM_EXPORT_SETTINGS_KEY, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) { return; }
    RegSetValueExA(key, "Name", 0, REG_SZ, (const BYTE *)g_RomExportSettings.name,
        (DWORD)strlen(g_RomExportSettings.name) + 1);
    RegSetValueExA(key, "Directory", 0, REG_SZ, (const BYTE *)g_RomExportSettings.directory,
        (DWORD)strlen(g_RomExportSettings.directory) + 1);
    RegCloseKey(key);
}
