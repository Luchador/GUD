#ifndef GEDITOR_PROJECTREBASE_H
#define GEDITOR_PROJECTREBASE_H
#include "project.h"

typedef struct ProjectRebaseReport {
    DWORD checked, kept, updated, conflicts;
    DWORD imagesretained, imagesadded, imagespreserved, imagesupdated;
    DWORD levelsremoved, resourcesremoved;
    DWORD modelskept, modelsupdated, resolved;
    DWORD setupskept;
    char details[8192];
} ProjectRebaseReport;

typedef enum ProjectRebaseChoice {
    PROJECT_REBASE_STOP,
    PROJECT_REBASE_KEEP_PROJECT,
    PROJECT_REBASE_USE_ROM
} ProjectRebaseChoice;
typedef struct ProjectRebaseOptions {
    /* levelConflicts applies to BG/stan/text. Saved setups always keep project. */
    ProjectRebaseChoice imageConflicts, levelConflicts, modelConflicts;
} ProjectRebaseOptions;

/* These functions consume saved project files. Check never writes anything.
 * Create rechecks a private snapshot, validates it through the ROM exporter,
 * then publishes a new folder. It never replaces an existing destination.
 * keepBaseImages chooses Keep project for images; FALSE uses strict checks.
 * The dialog uses WithOptions to choose Keep project or Use new ROM for each
 * asset category, including competing imports at the same image ID.
 * Keep project for models retains every saved .gmodel override, including
 * one whose native geometry already matches the old base.z64.
 * Saved solo/MP setup files always survive byte-for-byte while their resource
 * exists in the new ROM, including when they match the old base. Setup
 * conflicts keep the project under every option, including STOP and USE_ROM. */
BOOL ProjectRebaseDestination(const GEditorProject *source, const char *parent,
    const char *name, char destination[MAX_PATH], const char **why);
BOOL ProjectRebaseCheck(const GEditorProject *source, const char *rompath,
    BOOL keepBaseImages, ProjectRebaseReport *report, const char **why);
BOOL ProjectRebaseCreate(const GEditorProject *source, const char *rompath,
    BOOL keepBaseImages, const char *parent, const char *name, GEditorProject *output,
    ProjectRebaseReport *report, const char **why);
/* Choices resolve saved assets, never incompatible catalogs, coordinate
 * scales, corrupt files or field-level settings. Keep project also protects
 * saved models already baked into the old base, as described above. */
BOOL ProjectRebaseCheckWithOptions(const GEditorProject *source, const char *rompath,
    const ProjectRebaseOptions *options, ProjectRebaseReport *report, const char **why);
BOOL ProjectRebaseCreateWithOptions(const GEditorProject *source, const char *rompath,
    const ProjectRebaseOptions *options, const char *parent, const char *name,
    GEditorProject *output, ProjectRebaseReport *report, const char **why);
#endif
