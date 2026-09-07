#ifndef GEDITOR_BGHISTORY_H
#define GEDITOR_BGHISTORY_H

#include <windows.h>

#include "bgdocument.h"
#include "setupload.h"

#define EDIT_HISTORY_ACTION_MAX 64
#define EDIT_HISTORY_LIMIT 64

typedef enum EditHistoryAsset {
    EDIT_HISTORY_ASSET_NONE = 0,
    EDIT_HISTORY_ASSET_BG,
    EDIT_HISTORY_ASSET_SETUP
} EditHistoryAsset;

/* Only the document selected by asset owns allocated data in an entry. */
typedef struct EditHistoryEntry {
    EditHistoryAsset asset;
    BgDocument bgdocument;
    SetupFile setup;
    ULONGLONG staterevision;
    ULONGLONG assetrevision;
    char action[EDIT_HISTORY_ACTION_MAX];
} EditHistoryEntry;

/* The chronological stack is shared by all editable level documents. Each
 * document has its own saved revision so saving or undoing one asset cannot
 * incorrectly mark another one clean or dirty. */
typedef struct EditHistory {
    EditHistoryEntry *undoentries;
    DWORD undocount;
    DWORD undocapacity;
    EditHistoryEntry *redoentries;
    DWORD redocount;
    DWORD redocapacity;
    ULONGLONG currentstaterevision;
    ULONGLONG currentbgrevision;
    ULONGLONG currentsetuprevision;
    ULONGLONG savedbgrevision;
    ULONGLONG savedsetuprevision;
    ULONGLONG nextrevision;
} EditHistory;

typedef struct EditHistoryTransaction {
    EditHistoryAsset asset;
    BgDocument beforebg;
    SetupFile beforesetup;
    ULONGLONG staterevision;
    ULONGLONG assetrevision;
    char action[EDIT_HISTORY_ACTION_MAX];
    BOOL active;
} EditHistoryTransaction;

void EditHistoryReset(EditHistory *history, BgDocument *bgdocument,
                      SetupFile *setup);
void EditHistoryFree(EditHistory *history);

BOOL EditHistoryBeginBgEdit(const EditHistory *history,
                            const BgDocument *document, const char *action,
                            EditHistoryTransaction *transaction,
                            const char **reasonout);
BOOL EditHistoryBeginSetupEdit(const EditHistory *history,
                               const SetupFile *setup, const char *action,
                               EditHistoryTransaction *transaction,
                               const char **reasonout);
BOOL EditHistoryCommitEdit(EditHistory *history, BgDocument *bgdocument,
                           SetupFile *setup,
                           EditHistoryTransaction *transaction,
                           const char **reasonout);
void EditHistoryCancelEdit(EditHistoryTransaction *transaction);
void EditHistoryRollbackEdit(EditHistoryTransaction *transaction,
                             BgDocument *bgdocument, SetupFile *setup);

BOOL EditHistoryCanUndo(const EditHistory *history);
BOOL EditHistoryCanRedo(const EditHistory *history);
const char *EditHistoryGetUndoAction(const EditHistory *history);
const char *EditHistoryGetRedoAction(const EditHistory *history);
BOOL EditHistoryUndo(EditHistory *history, BgDocument *bgdocument,
                     SetupFile *setup, EditHistoryAsset *assetout,
                     const char **reasonout);
BOOL EditHistoryRedo(EditHistory *history, BgDocument *bgdocument,
                     SetupFile *setup, EditHistoryAsset *assetout,
                     const char **reasonout);

void EditHistoryMarkBgSaved(EditHistory *history, BgDocument *document);
void EditHistoryMarkSetupSaved(EditHistory *history, SetupFile *setup);

#endif /* GEDITOR_BGHISTORY_H */
