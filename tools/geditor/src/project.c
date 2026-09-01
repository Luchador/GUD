/**
 * This file handles GEditor project file creation, loading, and saving.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "project.h"

#define GEP_MAGIC   "GEditor Project"
#define GEP_VERSION 1


/**
 * Writes .proj to its .gep path. Returns FALSE if the file could not be created or fully written.
 */
static BOOL ProjectWrite(const GEditorProject *proj)
{
    FILE *f;

    f = fopen(proj->geppath, "w");
    if (f == NULL)
    {
        return FALSE;
    }

    fprintf(f, "%s %d\n", GEP_MAGIC, GEP_VERSION);
    fprintf(f, "name = %s\n", proj->name);

    return fclose(f) == 0;
}


BOOL ProjectCreate(const char *name, const char *location, GEditorProject *proj)
{
    int written;

    ZeroMemory(proj, sizeof(*proj));

    if (name == NULL || name[0] == '\0' || location == NULL || location[0] == '\0')
    {
        return FALSE;
    }

    strncpy(proj->name, name, sizeof(proj->name) - 1);

    /**
     * Build the two paths, refusing anything snprintf had to truncate:
     * a cut-off path would point somewhere else entirely.
     */
    written = snprintf(proj->dir, sizeof(proj->dir), "%s\\%s", location, name);

    if (written < 0 || written >= (int)sizeof(proj->dir))
    {
        goto fail;
    }

    written = snprintf(proj->geppath, sizeof(proj->geppath), "%s\\%s.gep", proj->dir, name);

    if (written < 0 || written >= (int)sizeof(proj->geppath))
    {
        goto fail;
    }

    /* The project folder. */
    if (!CreateDirectory(proj->dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        goto fail;
    }

    /* Never clobber an existing project. */
    if (GetFileAttributes(proj->geppath) != INVALID_FILE_ATTRIBUTES)
    {
        goto fail;
    }

    if (!ProjectWrite(proj))
    {
        goto fail;
    }

    return TRUE;

fail:
    ZeroMemory(proj, sizeof(*proj));
    return FALSE;
}