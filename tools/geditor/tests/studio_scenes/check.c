#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "studioscene.h"
#include "project.h"
#include "editorpath.h"

static const char *why = "";
static int short_write, fail_flush, fail_close;
#define OK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, #expr, why); abort(); } } while (0)

BOOL __real_WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
BOOL __real_FlushFileBuffers(HANDLE);
BOOL __real_CloseHandle(HANDLE);
BOOL __wrap_WriteFile(HANDLE file, const void *data, DWORD size, DWORD *written, void *overlapped)
{
    if (short_write) { short_write = 0; size /= 2; }
    return __real_WriteFile(file, data, size, written, overlapped);
}
BOOL __wrap_FlushFileBuffers(HANDLE file)
{
    if (fail_flush) { fail_flush = 0; return FALSE; }
    return __real_FlushFileBuffers(file);
}
BOOL __wrap_CloseHandle(HANDLE file)
{
    BOOL ok = __real_CloseHandle(file);
    if (fail_close) { fail_close = 0; return FALSE; }
    return ok;
}
static void Path(char out[MAX_PATH], const char *dir, const char *name)
{ OK(EditorPathJoin(out, MAX_PATH, dir, name)); }
static void Save(const char *dir, const char *name, const char *text)
{
    char path[MAX_PATH]; Path(path, dir, name);
    FILE *file = fopen(path, "wb");
    OK(file && fwrite(text, 1, strlen(text), file) == strlen(text) && !fclose(file));
}
static unsigned EntryCount(const char *folder)
{
    char path[MAX_PATH]; WIN32_FIND_DATA entry; unsigned count = 0;
    Path(path, folder, "*"); HANDLE search = FindFirstFile(path, &entry);
    OK(search != INVALID_HANDLE_VALUE);
    do { if (strcmp(entry.cFileName, ".") && strcmp(entry.cFileName, "..")) { count++; } }
    while (FindNextFile(search, &entry));
    OK(GetLastError() == ERROR_NO_MORE_FILES); FindClose(search); return count;
}

int main(int argc, char **argv)
{
    char project[MAX_PATH], scenes[MAX_PATH], path[MAX_PATH], filename[MAX_PATH], longname[STUDIO_SCENE_NAME_MAX + 1];
    StudioSceneEntry *entries = NULL; DWORD count = 0;
    OK(argc == 2);
    Path(project, argv[1], "Project"); OK(CreateDirectory(project, NULL));
    Path(scenes, project, "studio/scenes");
    OK(StudioSceneList(NULL, &entries, &count, &why) && !entries && !count);
    OK(!StudioSceneCreate(NULL, "Main", filename, &why) && why[0] && !filename[0]);
    const char *invalid[] = {NULL, "", ".rnd", ".", "..", "../escape", "sub\\escape", "C:drive", "a\nb",
        " space", "space ", ".hidden", "period.", "period..rnd", "x*y", "x?y", "x\"y", "x<y", "x>y", "x|y",
        "CON", "con.txt", "Prn.rnd", "AUX", "NUL", "COM1", "COM9.extra", "LPT1", "LPT9", "CONIN$", "CONOUT$"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
        OK(!StudioSceneCreate(project, invalid[i], filename, &why) && why[0] && !filename[0]);
    memset(longname, 's', sizeof(longname) - 1); longname[sizeof(longname) - 1] = 0;
    OK(!StudioSceneCreate(project, longname, filename, &why) && why[0]);
    OK(EntryCount(project) == 0); /* Rejected names create neither files nor folders. */
    OK(StudioSceneCreate(project, "Main", filename, &why) && !strcmp(filename, "Main.rnd"));
    OK(StudioSceneCreate(project, "Alpha.RND", filename, &why) && !strcmp(filename, "Alpha.rnd"));
    OK(!StudioSceneCreate(project, "Main.rnd", filename, &why) && why[0] && !filename[0]);
    OK(!StudioSceneCreate(project, "Main", filename, &why) && why[0]);
    Save(scenes, "zeta.RND", "existing future scene content");
    Save(scenes, "notes.txt", "not a scene"); Save(scenes, "saving.tmp", "partial scene");
    Save(scenes, ".rnd", "no scene name");
    Path(path, scenes, "Folder.rnd"); OK(CreateDirectory(path, NULL));
    OK(!StudioSceneCreate(project, "Folder", filename, &why) && why[0]);
    OK(StudioSceneList(project, &entries, &count, &why) && count == 3);
    OK(!strcmp(entries[0].filename, "Alpha.rnd") && !strcmp(entries[1].filename, "Main.rnd")
        && !strcmp(entries[2].filename, "zeta.RND")); free(entries);
    puts("PASS: scene creation, optional extension, filename validation, duplicate preservation and sorted .rnd-only listing.");

    unsigned before = EntryCount(scenes);
    test_fail_write = 1;
    OK(!StudioSceneCreate(project, "Write failure", filename, &why) && why[0] && !filename[0]);
    OK(EntryCount(scenes) == before);
    short_write = 1;
    OK(!StudioSceneCreate(project, "Short write", filename, &why) && why[0]);
    OK(EntryCount(scenes) == before);
    fail_flush = 1;
    OK(!StudioSceneCreate(project, "Flush failure", filename, &why) && why[0]);
    OK(EntryCount(scenes) == before);
    fail_close = 1;
    OK(!StudioSceneCreate(project, "Close failure", filename, &why) && why[0]);
    OK(EntryCount(scenes) == before);
    test_fail_move = 1;
    OK(!StudioSceneCreate(project, "Publish failure", filename, &why) && why[0]);
    OK(EntryCount(scenes) == before);
    test_publish_race = 1;
    OK(!StudioSceneCreate(project, "Concurrent scene", filename, &why) && why[0]);
    Path(path, scenes, "Concurrent scene.rnd");
    OK(GetFileAttributes(path) == FILE_ATTRIBUTE_DIRECTORY && RemoveDirectory(path));
    OK(EntryCount(scenes) == before);
    puts("PASS: failed/short writes, failed flush/close/publish and concurrent destination creation preserve existing entries and leave no temporary files.");

    longname[STUDIO_SCENE_NAME_MAX - 1] = 0;
    OK(StudioSceneCreate(project, longname, filename, &why));
    for (unsigned i = 0; i < 40; i++)
    {
        char name[32]; snprintf(name, sizeof(name), "Scene %02u", 40 - i);
        OK(StudioSceneCreate(project, name, filename, &why));
    }
    OK(StudioSceneList(project, &entries, &count, &why) && count == 44);
    for (DWORD i = 1; i < count; i++) { OK(lstrcmpi(entries[i - 1].filename, entries[i].filename) < 0); }
    free(entries);
    Path(project, argv[1], "Blocked"); OK(CreateDirectory(project, NULL));
    Path(path, project, "studio"); OK(CreateDirectory(path, NULL));
    Save(path, "scenes", "keep this file");
    OK(!StudioSceneCreate(project, "Blocked", filename, &why) && why[0]);
    OK(!StudioSceneList(project, &entries, &count, &why) && why[0] && !entries && !count);
    puts("PASS: long valid names, growing scene lists and scene-folder file collisions.");
    return 0;
}
