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


/**
 * Strip trailing newline, carriage returns, space and tabs.
 * This means .gep files can be edited with plaintext editors.
 */
static void TrimRight(char *s)
{
    size_t n = strlen(s);
 
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'
                  || s[n - 1] == ' '  || s[n - 1] == '\t'))
    {
        s[--n] = '\0';
    }
}


/**
  * Reads a .gep file into proj. Returns FALSE if the file cannot be
  * opened, is not a GEditor project, comes from a newer format version,
  * or carries no name.
  *
  * Lines with '=' are skipped. Unknown keys are ignored so a future .gep
  * with different keys still opens.
  */
BOOL ProjectRead(const char *geppath, GEditorProject *proj)
{
    char line[512];
    int version = 0;
    FILE *f;
    char *lastslash;
 
    ZeroMemory(proj, sizeof(*proj));
 
    f = fopen(geppath, "r");

    if (f == NULL)
    {
        return FALSE;
    }
 
    /**
     * Read header first to ensure this is a GEditor project file. Versions <= ours are accepted.
     */
    if (fgets(line, sizeof(line), f) == NULL || sscanf(line, GEP_MAGIC " %d", &version) != 1 || version < 1 || version > GEP_VERSION)
    {
        fclose(f);
        return FALSE;
    }
 
    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *eq = strchr(line, '=');
        char *key = line;
        char *value;
 
        if (eq == NULL)
        {
            continue; /* blank line or comment */
        }
 
        *eq = '\0'; /* split in place: key | value */
        value = eq + 1;
 
        TrimRight(key);
        while (*value == ' ' || *value == '\t')
        {
            value++;
        }
        TrimRight(value);
 
        if (strcmp(key, "name") == 0)
        {
            strncpy(proj->name, value, sizeof(proj->name) - 1);
        }
        /* unknown keys: ignored */
    }
 
    fclose(f);
 
    if (proj->name[0] == '\0')
    {
        ZeroMemory(proj, sizeof(*proj));
        return FALSE;
    }
 
    /**
     * Derive file paths from where the file actually is so moving the .gep file doesn't break
     * the project file.
     */
    strncpy(proj->geppath, geppath, sizeof(proj->geppath) - 1);
    strncpy(proj->dir, geppath, sizeof(proj->dir) - 1);
 
    lastslash = strrchr(proj->dir, '\\');
    if (lastslash == NULL)
    {
        lastslash = strrchr(proj->dir, '/');
    }
    if (lastslash != NULL)
    {
        *lastslash = '\0';
    }
    else
    {
        proj->dir[0] = '.';  /* bare filename: project dir is the cwd */
        proj->dir[1] = '\0';
    }
 
    return TRUE;
}

/*
 * Closes a project: today, that just means forgetting it. The struct
 * is zeroed so name[0] == '\0' is once again a reliable "no project
 * open" test everywhere.
 *
 * The .gep on disk needs nothing - reads and writes open and close it
 * within one call, so no handle is ever held between operations. When
 * projects gain unsaved state (a loaded level, editor settings), this
 * is the function that will flush it before forgetting.
 */
void ProjectClose(GEditorProject *proj)
{
    ZeroMemory(proj, sizeof(*proj));
}