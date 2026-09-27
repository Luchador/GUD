#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "studioassets.h"
#include "editorpath.h"

static int StudioFileCompare(const void *a, const void *b)
{
    const StudioFileEntry *left = a, *right = b;
    int order = lstrcmpi(left->filename, right->filename);
    return order ? order : strcmp(left->filename, right->filename);
}

BOOL StudioFileList(const char *projectdir, const char *folder, const char *extension,
    StudioFileEntry **entries, DWORD *count, const char **why)
{
    char path[MAX_PATH], pattern[MAX_PATH]; WIN32_FIND_DATA found; HANDLE search;
    StudioFileEntry *items = NULL; size_t used = 0, capacity = 0, suffix = strlen(extension);
    DWORD error;
    *entries = NULL; *count = 0; *why = "";
    if (!projectdir || !projectdir[0]) { return TRUE; }
    if (!EditorPathJoin(path, sizeof(path), projectdir, folder)
        || !EditorPathJoin(pattern, sizeof(pattern), path, "*"))
    { *why = "The project path is too long to list studio files."; return FALSE; }
    search = FindFirstFile(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) { return TRUE; }
        *why = "Could not read the Render Studio folder."; return FALSE;
    }
    do
    {
        size_t length = strlen(found.cFileName);
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || length <= suffix
            || lstrcmpi(found.cFileName + length - suffix, extension)) { continue; }
        if (used == capacity)
        {
            size_t next = capacity ? capacity * 2 : 16;
            StudioFileEntry *grown;
            if (next < capacity || next > SIZE_MAX / sizeof(*items) || next > INT32_MAX) { goto memory; }
            grown = realloc(items, next * sizeof(*items));
            if (!grown) { goto memory; }
            items = grown; capacity = next;
        }
        lstrcpyn(items[used++].filename, found.cFileName, MAX_PATH);
    }
    while (FindNextFile(search, &found));
    error = GetLastError(); FindClose(search);
    if (error != ERROR_NO_MORE_FILES)
    { free(items); *why = "Could not finish reading the Render Studio folder."; return FALSE; }
    if (used > 1) { qsort(items, used, sizeof(*items), StudioFileCompare); }
    *entries = items; *count = (DWORD)used;
    return TRUE;
memory:
    FindClose(search); free(items);
    *why = "Not enough memory to list studio files."; return FALSE;
}

BOOL StudioLoadImages(const char *projectdir, TexThumb **items,
    unsigned char **pixels, DWORD *count, const char **why)
{
    StudioFileEntry *files = NULL; DWORD total = 0;
    TexThumb *thumbs = NULL; unsigned char *block = NULL;
    char folder[MAX_PATH], path[MAX_PATH];
    const size_t stride = TEX_THUMB_MAX * TEX_THUMB_MAX * 4;
    *items = NULL; *pixels = NULL; *count = 0; *why = "";
    if (!StudioFileList(projectdir, "studio\\images", ".bmp", &files, &total, why)) { return FALSE; }
    if (!total) { free(files); return TRUE; }
    /* Each TexThumb is smaller than its pixel block; this also bounds its
     * allocation, browser row arithmetic, and the 32-bit pixel offsets. */
    if (total > UINT32_MAX / stride) { goto memory; }
    thumbs = calloc(total, sizeof(*thumbs)); block = calloc(total, stride);
    if (!thumbs || !block) { goto memory; }
    if (!EditorPathJoin(folder, sizeof(folder), projectdir, "studio\\images"))
    { free(files); free(thumbs); free(block); *why = "The studio images path is too long."; return FALSE; }
    for (DWORD i = 0; i < total; i++)
    {
        lstrcpyn(thumbs[i].label, files[i].filename, sizeof(thumbs[i].label));
        if (!EditorPathJoin(path, sizeof(path), folder, files[i].filename)
            || !TexLoadFileThumbnail(path, &thumbs[i], block + i * stride))
            *why = "Some studio BMPs could not be previewed.";
        thumbs[i].pixeloffset = (unsigned int)(i * stride);
    }
    free(files);
    *items = thumbs; *pixels = block; *count = total;
    return TRUE;
memory:
    free(files); free(thumbs); free(block);
    *why = "Not enough memory for studio image thumbnails."; return FALSE;
}
