#ifndef GEDITOR_EDITORPATH_H
#define GEDITOR_EDITORPATH_H

#include <stdio.h>
#include <string.h>

static inline int EditorPathSeparator(char ch)
{ return ch == '\\' || ch == '/'; }

/* Collapse separators in place, retaining the leading pair used by UNC and
 * extended Windows paths. Never strip the separator from a drive root. */
static inline void EditorPathNormalize(char *path)
{
    size_t from = 0, to = 0;
    if (EditorPathSeparator(path[0]) && EditorPathSeparator(path[1]))
    {
        path[to++] = '\\'; path[to++] = '\\';
        from = 2;
        while (EditorPathSeparator(path[from])) { from++; }
    }
    while (path[from])
    {
        char ch = path[from++];
        if (EditorPathSeparator(ch))
        {
            if (to && path[to - 1] == '\\') { continue; }
            ch = '\\';
        }
        path[to++] = ch;
    }
    path[to] = '\0';
}

/* The output buffer must not overlap either input. */
static inline int EditorPathJoin(char *out, size_t capacity,
                                  const char *directory, const char *name)
{
    size_t length = strlen(directory);
    const char *separator = length && EditorPathSeparator(directory[length - 1]) ? "" : "\\";
    int written = snprintf(out, capacity, "%s%s%s", directory, separator, name);
    if (written < 0 || (size_t)written >= capacity)
    {
        if (capacity) { out[0] = '\0'; }
        return 0;
    }
    EditorPathNormalize(out);
    return 1;
}

#endif
