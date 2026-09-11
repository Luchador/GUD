#ifndef GEDITOR_BGHISTORY_H
#define GEDITOR_BGHISTORY_H

#include <windows.h>

#include "bgdocument.h"
#include "setupload.h"
#include "stanload.h"

#define EDIT_HISTORY_ACTION_MAX 64
#define EDIT_HISTORY_LIMIT 64

typedef enum EditHistoryAsset {
    EDIT_HISTORY_ASSET_NONE = 0,
    EDIT_HISTORY_ASSET_BG,
    EDIT_HISTORY_ASSET_SETUP,
    EDIT_HISTORY_ASSET_STAN,
    EDIT_HISTORY_ASSET_SELECTION
} EditHistoryAsset;

/* An entry owns its selection snapshot and, for edits, one asset document. */
typedef struct EditHistoryEntry {
    EditHistoryAsset asset;
    void *selection;
    size_t selectionsize;
    BgDocument bgdocument;
    SetupFile setup;
    StanFile stan;
    ULONGLONG staterevision;
    ULONGLONG assetrevision;
    char action[EDIT_HISTORY_ACTION_MAX];
} EditHistoryEntry;

/* The chronological stack is shared by all editable level documents. Each
 * document has its own saved revision so saving or undoing one asset cannot
 * incorrectly mark another one clean or dirty. */
typedef struct EditHistory {
    void *selection; /* opaque, pointer-free viewport/UV snapshot */
    size_t selectionsize;
    EditHistoryEntry *undoentries;
    DWORD undocount;
    DWORD undocapacity;
    EditHistoryEntry *redoentries;
    DWORD redocount;
    DWORD redocapacity;
    ULONGLONG currentstaterevision;
    ULONGLONG currentbgrevision;
    ULONGLONG currentsetuprevision;
    ULONGLONG currentstanrevision;
    ULONGLONG savedbgrevision;
    ULONGLONG savedsetuprevision;
    ULONGLONG savedstanrevision;
    ULONGLONG nextrevision;
} EditHistory;

typedef struct EditHistoryTransaction {
    EditHistoryAsset asset;
    BgDocument beforebg;
    SetupFile beforesetup;
    StanFile beforestan;
    ULONGLONG staterevision;
    ULONGLONG assetrevision;
    char action[EDIT_HISTORY_ACTION_MAX];
    BOOL active;
} EditHistoryTransaction;

void EditHistoryReset(EditHistory *history, BgDocument *bgdocument,
                      SetupFile *setup, StanFile *stan);
void EditHistoryFree(EditHistory *history);

BOOL EditHistoryBeginBgEdit(const EditHistory *history,
                            const BgDocument *document, const char *action,
                            EditHistoryTransaction *transaction,
                            const char **reasonout);
BOOL EditHistoryBeginSetupEdit(const EditHistory *history,
                               const SetupFile *setup, const char *action,
                               EditHistoryTransaction *transaction,
                               const char **reasonout);
BOOL EditHistoryBeginStanEdit(const EditHistory *history,
                              const StanFile *stan, const char *action,
                              EditHistoryTransaction *transaction,
                              const char **reasonout);
BOOL EditHistoryCommitEdit(EditHistory *history, BgDocument *bgdocument,
                           SetupFile *setup, StanFile *stan,
                           EditHistoryTransaction *transaction,
                           const char **reasonout);
void EditHistoryCancelEdit(EditHistoryTransaction *transaction);
void EditHistoryRollbackEdit(EditHistoryTransaction *transaction,
                             BgDocument *bgdocument, SetupFile *setup, StanFile *stan);

BOOL EditHistoryCanUndo(const EditHistory *history);
BOOL EditHistoryCanRedo(const EditHistory *history);
const char *EditHistoryGetUndoAction(const EditHistory *history);
const char *EditHistoryGetRedoAction(const EditHistory *history);
BOOL EditHistoryUndo(EditHistory *history, BgDocument *bgdocument,
                     SetupFile *setup, StanFile *stan, EditHistoryAsset *assetout,
                     const char **reasonout);
BOOL EditHistoryRedo(EditHistory *history, BgDocument *bgdocument,
                     SetupFile *setup, StanFile *stan, EditHistoryAsset *assetout,
                     const char **reasonout);

/* record=FALSE synchronizes selection after loading or completing an edit;
 * TRUE records a user selection change. Identical snapshots are a no-op.
 * Copies data; failure leaves both history stacks and saved revisions intact. */
BOOL EditHistorySetSelection(EditHistory *history, const void *data, size_t size,
                             BOOL record, const char **reasonout);

void EditHistoryMarkBgSaved(EditHistory *history, BgDocument *document);
void EditHistoryMarkSetupSaved(EditHistory *history, SetupFile *setup);
void EditHistoryMarkStanSaved(EditHistory *history, StanFile *stan);

#endif /* GEDITOR_BGHISTORY_H */
