#include <windows.h>
#include <string.h>
#include "recentlevels.h"

#define RECENT_LEVELS_KEY "Software\\GUD\\GEditor\\Recent Levels"

static void RecentLevelsInsert(RecentLevels *recent, LONG levelid)
{
    DWORD index;
    for (index = 0; index < recent->count; index++)
    { if (recent->ids[index] == levelid) { break; } }
    if (index == recent->count)
    {
        if (recent->count < RECENT_LEVELS_MAX) { recent->count++; }
        else { index = RECENT_LEVELS_MAX - 1; }
    }
    memmove(recent->ids + 1, recent->ids, index * sizeof(recent->ids[0]));
    recent->ids[0] = levelid;
}

void RecentLevelsLoad(RecentLevels *recent, const char *projectpath)
{
    HKEY key;
    LONG ids[RECENT_LEVELS_MAX];
    DWORD length, type = 0, bytes = sizeof(ids);
    ZeroMemory(recent, sizeof(*recent));
    if (!projectpath || !projectpath[0]) { return; }
    length = GetFullPathNameA(projectpath, sizeof(recent->projectpath), recent->projectpath, NULL);
    if (!length || length >= sizeof(recent->projectpath))
    { recent->projectpath[0] = '\0'; return; }
    for (DWORD i = 0; i < length; i++)
    { if (recent->projectpath[i] == '/') { recent->projectpath[i] = '\\'; } }

    if (RegOpenKeyExA(HKEY_CURRENT_USER, RECENT_LEVELS_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    { return; }
    /* Registry value names are case-insensitive and can contain the full path.
     * Store only IDs, not names or a padded/native editor structure. */
    if (RegQueryValueExA(key, recent->projectpath, NULL, &type, (BYTE *)ids, &bytes) == ERROR_SUCCESS
        && type == REG_BINARY && bytes <= sizeof(ids) && bytes % sizeof(ids[0]) == 0)
    {
        for (int i = (int)(bytes / sizeof(ids[0])) - 1; i >= 0; i--)
        { RecentLevelsInsert(recent, ids[i]); }
    }
    RegCloseKey(key);
}

void RecentLevelsRemember(RecentLevels *recent, LONG levelid)
{
    HKEY key;
    RecentLevelsInsert(recent, levelid);
    /* A failed preference write must not prevent opening a level. */
    if (!recent->projectpath[0]
        || RegCreateKeyExA(HKEY_CURRENT_USER, RECENT_LEVELS_KEY, 0, NULL,
            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS) { return; }
    RegSetValueExA(key, recent->projectpath, 0, REG_BINARY,
        (const BYTE *)recent->ids, recent->count * sizeof(recent->ids[0]));
    RegCloseKey(key);
}
