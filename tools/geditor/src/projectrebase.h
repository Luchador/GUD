#ifndef GEDITOR_PROJECTREBASE_H
#define GEDITOR_PROJECTREBASE_H
#include "project.h"

typedef struct ProjectRebaseReport {
    DWORD checked, kept, updated, conflicts;
    DWORD imagesretained, imagesadded, imagespreserved;
    DWORD levelsremoved, resourcesremoved;
    DWORD modelskept, modelsupdated, resolved;
    char details[8192];
} ProjectRebaseReport;

typedef enum ProjectRebaseChoice {
    PROJECT_REBASE_STOP,
    PROJECT_REBASE_KEEP_PROJECT,
    PROJECT_REBASE_USE_ROM
} ProjectRebaseChoice;
typedef struct ProjectRebaseOptions {
    BOOL keepBaseImages;
    ProjectRebaseChoice levelConflicts, modelConflicts;
} ProjectRebaseOptions;

/* These functions consume saved project files. Check never writes anything.
 * Create rechecks a private snapshot, validates it through the ROM exporter,
 * then publishes a new folder. It never replaces an existing destination.
 * keepBaseImages explicitly chooses the old base for differing shared image
 * IDs, including native mipmaps and surface/detail settings. New IDs still
 * undergo the normal imported-image conflict checks. */
BOOL ProjectRebaseDestination(const GEditorProject *source, const char *parent,
    const char *name, char destination[MAX_PATH], const char **why);
BOOL ProjectRebaseCheck(const GEditorProject *source, const char *rompath,
    BOOL keepBaseImages, ProjectRebaseReport *report, const char **why);
BOOL ProjectRebaseCreate(const GEditorProject *source, const char *rompath,
    BOOL keepBaseImages, const char *parent, const char *name, GEditorProject *output,
    ProjectRebaseReport *report, const char **why);
/* Choices apply only to competing saved asset edits, never to incompatible
 * catalogs, coordinate scales, corrupt files or field-level settings. */
BOOL ProjectRebaseCheckWithOptions(const GEditorProject *source, const char *rompath,
    const ProjectRebaseOptions *options, ProjectRebaseReport *report, const char **why);
BOOL ProjectRebaseCreateWithOptions(const GEditorProject *source, const char *rompath,
    const ProjectRebaseOptions *options, const char *parent, const char *name,
    GEditorProject *output, ProjectRebaseReport *report, const char **why);
#endif
