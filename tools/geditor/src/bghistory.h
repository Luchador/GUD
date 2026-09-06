#ifndef GEDITOR_BGHISTORY_H
#define GEDITOR_BGHISTORY_H

#include <windows.h>

#include "bgdocument.h"

#define BG_HISTORY_ACTION_MAX 64
#define BG_HISTORY_LIMIT 64

typedef struct BgHistoryEntry {
    BgDocument document;
    ULONGLONG revision;
    char action[BG_HISTORY_ACTION_MAX];
} BgHistoryEntry;

typedef struct BgHistory {
    BgHistoryEntry *undoentries;
    DWORD undocount;
    DWORD undocapacity;
    BgHistoryEntry *redoentries;
    DWORD redocount;
    DWORD redocapacity;
    ULONGLONG currentrevision;
    ULONGLONG savedrevision;
    ULONGLONG nextrevision;
} BgHistory;

/* A transaction captures the complete pre-edit document before any mutation.
   Successful edits commit it to the undo stack; failed edits can restore it. */
typedef struct BgHistoryTransaction {
    BgDocument before;
    ULONGLONG revision;
    char action[BG_HISTORY_ACTION_MAX];
    BOOL active;
} BgHistoryTransaction;

void BgHistoryReset(BgHistory *history, BgDocument *document);
void BgHistoryFree(BgHistory *history);

BOOL BgHistoryBeginEdit(const BgHistory *history,
                        const BgDocument *document, const char *action,
                        BgHistoryTransaction *transaction,
                        const char **reasonout);
BOOL BgHistoryCommitEdit(BgHistory *history, BgDocument *document,
                         BgHistoryTransaction *transaction,
                         const char **reasonout);
void BgHistoryCancelEdit(BgHistoryTransaction *transaction);
void BgHistoryRollbackEdit(BgHistoryTransaction *transaction,
                           BgDocument *document);

BOOL BgHistoryCanUndo(const BgHistory *history);
BOOL BgHistoryCanRedo(const BgHistory *history);
const char *BgHistoryGetUndoAction(const BgHistory *history);
const char *BgHistoryGetRedoAction(const BgHistory *history);
BOOL BgHistoryUndo(BgHistory *history, BgDocument *document,
                   const char **reasonout);
BOOL BgHistoryRedo(BgHistory *history, BgDocument *document,
                   const char **reasonout);

void BgHistoryMarkSaved(BgHistory *history, BgDocument *document);

#endif /* GEDITOR_BGHISTORY_H */
