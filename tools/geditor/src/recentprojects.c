#include <windows.h>
#include <string.h>

#include "recentprojects.h"

#define RECENT_PROJECTS_KEY "Software\\GUD\\GEditor\\Recent Projects"

/* Use absolute paths so the list works regardless of the launch directory.
   Copy first: path can refer to an existing entry which is about to move. */
static BOOL RecentProjectsInsert(RecentProjects *recent, const char *path)
{
    char fullpath[MAX_PATH];
    DWORD length;
    DWORD index;

    if (path == NULL || path[0] == '\0') { return FALSE; }
    length = GetFullPathNameA(path, sizeof(fullpath), fullpath, NULL);
    if (length == 0 || length >= sizeof(fullpath)) { return FALSE; }
    for (index = 0; index < length; index++)
    {
        if (fullpath[index] == '/') { fullpath[index] = '\\'; }
    }

    for (index = 0; index < recent->count; index++)
    {
        if (lstrcmpiA(recent->paths[index], fullpath) == 0) { break; }
    }
    if (index == recent->count)
    {
        if (recent->count < RECENT_PROJECTS_MAX) { recent->count++; }
        else { index = RECENT_PROJECTS_MAX - 1; }
    }
    memmove(recent->paths[1], recent->paths[0],
            index * sizeof(recent->paths[0]));
    memcpy(recent->paths[0], fullpath, length + 1);
    return TRUE;
}


void RecentProjectsLoad(RecentProjects *recent)
{
    HKEY key;
    int index;

    ZeroMemory(recent, sizeof(*recent));
    if (RegOpenKeyExA(HKEY_CURRENT_USER, RECENT_PROJECTS_KEY, 0,
                      KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) { return; }

    /* Insert oldest first so the saved order and newest spelling survive. */
    for (index = RECENT_PROJECTS_MAX - 1; index >= 0; index--)
    {
        char name[2] = { (char)('1' + index), '\0' };
        char path[MAX_PATH];
        DWORD type = 0;
        DWORD size = sizeof(path);

        if (RegQueryValueExA(key, name, NULL, &type, (BYTE *)path, &size)
                == ERROR_SUCCESS
            && type == REG_SZ && size > 1 && size <= sizeof(path)
            && path[size - 1] == '\0' && strlen(path) + 1 == size)
        {
            RecentProjectsInsert(recent, path);
        }
    }
    RegCloseKey(key);
}


void RecentProjectsRemember(RecentProjects *recent, const char *path)
{
    HKEY key;
    DWORD index;

    if (!RecentProjectsInsert(recent, path)) { return; }
    /* A preference-write failure must not prevent opening the project.
       The in-memory list still works for the rest of this session. */
    if (RegCreateKeyExA(HKEY_CURRENT_USER, RECENT_PROJECTS_KEY, 0, NULL,
                        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL,
                        &key, NULL) != ERROR_SUCCESS) { return; }

    for (index = 0; index < RECENT_PROJECTS_MAX; index++)
    {
        char name[2] = { (char)('1' + index), '\0' };
        if (index < recent->count)
        {
            RegSetValueExA(key, name, 0, REG_SZ,
                (const BYTE *)recent->paths[index],
                (DWORD)strlen(recent->paths[index]) + 1);
        }
        else
        {
            RegDeleteValueA(key, name);
        }
    }
    RegCloseKey(key);
}
