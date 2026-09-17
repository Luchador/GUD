#ifndef GEDITOR_PROJECTREBASE_H
#define GEDITOR_PROJECTREBASE_H
#include "project.h"

typedef struct ProjectRebaseReport {
    DWORD checked, kept, updated, conflicts;
    DWORD imagesretained, imagesadded;
    char details[8192];
} ProjectRebaseReport;

/* These functions consume saved project files. Check never writes anything.
 * Create rechecks a private snapshot, validates it through the ROM exporter,
 * then publishes a new folder. It never replaces an existing destination. */
BOOL ProjectRebaseDestination(const GEditorProject *source, const char *parent,
    const char *name, char destination[MAX_PATH], const char **why);
BOOL ProjectRebaseCheck(const GEditorProject *source, const char *rompath,
    ProjectRebaseReport *report, const char **why);
BOOL ProjectRebaseCreate(const GEditorProject *source, const char *rompath,
    const char *parent, const char *name, GEditorProject *output,
    ProjectRebaseReport *report, const char **why);
#endif
