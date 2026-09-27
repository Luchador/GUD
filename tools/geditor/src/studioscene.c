#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "studioscene.h"
#include "project.h"
#include "editorpath.h"

static BOOL StudioSceneFail(const char **why, const char *text)
{ *why = text; return FALSE; }

static BOOL StudioSceneFilename(const char *name, char filename[MAX_PATH], const char **why)
{
    size_t length = name ? strlen(name) : 0;
    char device[16]; size_t token = 0;
    if (length >= 4 && !lstrcmpi(name + length - 4, ".rnd")) { length -= 4; }
    if (!length) { return StudioSceneFail(why, "Enter a scene name."); }
    if (length >= STUDIO_SCENE_NAME_MAX)
        return StudioSceneFail(why, "The scene name must be fewer than 128 characters.");
    if (name[0] == ' ' || name[0] == '.' || name[length - 1] == ' ' || name[length - 1] == '.')
        return StudioSceneFail(why, "A scene name cannot begin or end with a space or period.");
    for (size_t i = 0; i < length; i++)
        if ((unsigned char)name[i] < 32 || strchr("\\/:*?\"<>|", name[i]))
            return StudioSceneFail(why, "A scene name cannot contain \\ / : * ? \" < > or |.");
    /* Windows device names remain reserved even when an extension is added. */
    while (token < length && name[token] != '.' && token < sizeof(device) - 1)
    {
        char ch = name[token];
        device[token++] = ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch;
    }
    device[token] = 0;
    if (!lstrcmpi(device, "CON") || !lstrcmpi(device, "PRN") || !lstrcmpi(device, "AUX")
        || !lstrcmpi(device, "NUL") || !lstrcmpi(device, "CONIN$") || !lstrcmpi(device, "CONOUT$")
        || (token == 4 && ((device[3] >= '1' && device[3] <= '9')
            || (unsigned char)device[3] == 0xb9 || (unsigned char)device[3] == 0xb2 || (unsigned char)device[3] == 0xb3)
            && (!strncmp(device, "COM", 3) || !strncmp(device, "LPT", 3))))
        return StudioSceneFail(why, "That name is reserved by Windows.");
    memcpy(filename, name, length); memcpy(filename + length, ".rnd", 5);
    return TRUE;
}

BOOL StudioSceneCreate(const char *projectdir, const char *name, char filename[MAX_PATH], const char **why)
{
    /* Names live in filenames. The ASCII JSON header is independent of the
     * Windows filename encoding and leaves room for future scene fields. */
    static const char empty[] = "{\n  \"format\": \"GEditor Render Studio\",\n  \"version\": 1,\n  \"objects\": []\n}\n";
    char leaf[MAX_PATH], folder[MAX_PATH], path[MAX_PATH], temporary[MAX_PATH];
    HANDLE file; DWORD written = 0; BOOL ok;
    *why = ""; filename[0] = 0;
    if (!StudioSceneFilename(name, leaf, why)) { return FALSE; }
    if (!projectdir || !projectdir[0]) { return StudioSceneFail(why, "Open a project first."); }
    if (!EditorPathJoin(folder, sizeof(folder), projectdir, "studio\\scenes")
        || !EditorPathJoin(path, sizeof(path), folder, leaf))
        return StudioSceneFail(why, "The project path and scene name are too long.");
    if (!ProjectEnsureStudioFolders(projectdir, why)) { return FALSE; }
    if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
        return StudioSceneFail(why, "That scene already exists. Enter another name.");
    if (!GetTempFileName(folder, "rnd", 0, temporary))
        return StudioSceneFail(why, "Could not create a scene file. Check the scene folder's permissions and path length.");
    file = CreateFile(temporary, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        DeleteFile(temporary);
        return StudioSceneFail(why, "Could not open the new scene file for writing.");
    }
    ok = WriteFile(file, empty, sizeof(empty) - 1, &written, NULL) && written == sizeof(empty) - 1;
    if (ok) { ok = FlushFileBuffers(file); }
    if (!CloseHandle(file)) { ok = FALSE; }
    /* No REPLACE_EXISTING: a same-name scene created since the check wins. */
    if (!ok || !MoveFileEx(temporary, path, MOVEFILE_WRITE_THROUGH))
    {
        DeleteFile(temporary);
        return StudioSceneFail(why, GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES
            ? "That scene already exists. Enter another name."
            : "The scene could not be fully saved. Check free disk space and folder permissions.");
    }
    lstrcpyn(filename, leaf, MAX_PATH);
    return TRUE;
}

static int StudioSceneCompare(const void *a, const void *b)
{
    const StudioSceneEntry *left = a, *right = b;
    int order = lstrcmpi(left->filename, right->filename);
    return order ? order : strcmp(left->filename, right->filename);
}

BOOL StudioSceneList(const char *projectdir, StudioSceneEntry **entries, DWORD *count, const char **why)
{
    char pattern[MAX_PATH]; WIN32_FIND_DATA found; HANDLE search;
    StudioSceneEntry *items = NULL; size_t used = 0, capacity = 0;
    DWORD error;
    *entries = NULL; *count = 0; *why = "";
    if (!projectdir || !projectdir[0]) { return TRUE; }
    if (!EditorPathJoin(pattern, sizeof(pattern), projectdir, "studio\\scenes\\*"))
        return StudioSceneFail(why, "The project path is too long to list scenes.");
    search = FindFirstFile(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND
            || StudioSceneFail(why, "Could not read the studio/scenes folder.");
    do
    {
        size_t length = strlen(found.cFileName);
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || length <= 4
            || lstrcmpi(found.cFileName + length - 4, ".rnd")) { continue; }
        if (used == capacity)
        {
            size_t next = capacity ? capacity * 2 : 16;
            StudioSceneEntry *grown;
            if (next < capacity || next > SIZE_MAX / sizeof(*items) || next > UINT32_MAX) { goto memory; }
            grown = realloc(items, next * sizeof(*items));
            if (!grown) { goto memory; }
            items = grown; capacity = next;
        }
        lstrcpyn(items[used++].filename, found.cFileName, MAX_PATH);
    }
    while (FindNextFile(search, &found));
    error = GetLastError(); FindClose(search);
    if (error != ERROR_NO_MORE_FILES)
    { free(items); return StudioSceneFail(why, "Could not finish reading the studio/scenes folder."); }
    if (used > 1) { qsort(items, used, sizeof(*items), StudioSceneCompare); }
    *entries = items; *count = (DWORD)used;
    return TRUE;
memory:
    FindClose(search); free(items);
    return StudioSceneFail(why, "Not enough memory to list studio scenes.");
}
