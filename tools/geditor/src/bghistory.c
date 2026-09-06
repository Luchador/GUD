/*
 * Bounded undo/redo history for the editable background document.
 *
 * Each committed edit stores the complete pre-edit document. Undo and redo
 * transfer ownership between the live document and the opposite stack, so
 * stepping through history does not copy the level geometry again.
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "bghistory.h"


static void BgHistoryCopyAction(char out[BG_HISTORY_ACTION_MAX],
                                const char *action)
{
    if (action == NULL || action[0] == '\0')
    {
        action = "Edit";
    }

    lstrcpyn(out, action, BG_HISTORY_ACTION_MAX);
}


static void BgHistoryFreeEntries(BgHistoryEntry *entries, DWORD count)
{
    DWORD index;

    for (index = 0; index < count; index++)
    {
        BgDocumentFree(&entries[index].document);
    }
    free(entries);
}


static BOOL BgHistoryEnsureCapacity(BgHistoryEntry **entries,
                                    DWORD *capacity, DWORD needed)
{
    DWORD nextcapacity;
    BgHistoryEntry *grown;

    if (needed <= *capacity)
    {
        return TRUE;
    }
    if (needed > BG_HISTORY_LIMIT)
    {
        return FALSE;
    }

    nextcapacity = *capacity == 0 ? 8 : *capacity * 2;
    if (nextcapacity > BG_HISTORY_LIMIT)
    {
        nextcapacity = BG_HISTORY_LIMIT;
    }
    if (nextcapacity < needed)
    {
        nextcapacity = needed;
    }

    grown = (BgHistoryEntry *)realloc(*entries,
                (size_t)nextcapacity * sizeof(**entries));
    if (grown == NULL)
    {
        return FALSE;
    }

    *entries = grown;
    *capacity = nextcapacity;
    return TRUE;
}


static void BgHistoryClearStack(BgHistoryEntry *entries, DWORD *count)
{
    while (*count > 0)
    {
        (*count)--;
        BgDocumentFree(&entries[*count].document);
        ZeroMemory(&entries[*count], sizeof(entries[*count]));
    }
}


static void BgHistoryDiscardOldest(BgHistoryEntry *entries, DWORD *count)
{
    if (*count < BG_HISTORY_LIMIT)
    {
        return;
    }

    BgDocumentFree(&entries[0].document);
    memmove(entries, entries + 1,
            (size_t)(*count - 1) * sizeof(*entries));
    (*count)--;
    ZeroMemory(&entries[*count], sizeof(entries[*count]));
}


static void BgHistoryUpdateDirty(const BgHistory *history,
                                 BgDocument *document)
{
    document->dirty = history->currentrevision != history->savedrevision;
}


void BgHistoryReset(BgHistory *history, BgDocument *document)
{
    BgHistoryFree(history);
    history->currentrevision = 1;
    history->savedrevision = 1;
    history->nextrevision = 2;

    if (document != NULL)
    {
        document->dirty = FALSE;
    }
}


void BgHistoryFree(BgHistory *history)
{
    if (history == NULL)
    {
        return;
    }

    BgHistoryFreeEntries(history->undoentries, history->undocount);
    BgHistoryFreeEntries(history->redoentries, history->redocount);
    ZeroMemory(history, sizeof(*history));
}


BOOL BgHistoryBeginEdit(const BgHistory *history,
                        const BgDocument *document, const char *action,
                        BgHistoryTransaction *transaction,
                        const char **reasonout)
{
    ZeroMemory(transaction, sizeof(*transaction));
    *reasonout = "";

    if (history == NULL || history->currentrevision == 0)
    {
        *reasonout = "the bg undo history has not been initialized.";
        return FALSE;
    }

    if (!BgDocumentClone(document, &transaction->before, reasonout))
    {
        return FALSE;
    }

    transaction->revision = history->currentrevision;
    BgHistoryCopyAction(transaction->action, action);
    transaction->active = TRUE;
    return TRUE;
}


BOOL BgHistoryCommitEdit(BgHistory *history, BgDocument *document,
                         BgHistoryTransaction *transaction,
                         const char **reasonout)
{
    BgHistoryEntry *entry;
    DWORD needed;

    *reasonout = "";
    if (history == NULL || document == NULL || transaction == NULL
        || !transaction->active)
    {
        *reasonout = "there is no bg edit transaction to commit.";
        return FALSE;
    }
    if (transaction->revision != history->currentrevision)
    {
        *reasonout = "the bg document changed during the edit transaction.";
        return FALSE;
    }
    if (history->nextrevision == 0)
    {
        *reasonout = "the bg undo history revision counter is exhausted.";
        return FALSE;
    }

    needed = history->undocount < BG_HISTORY_LIMIT
        ? history->undocount + 1 : BG_HISTORY_LIMIT;
    if (!BgHistoryEnsureCapacity(&history->undoentries,
                                  &history->undocapacity, needed))
    {
        *reasonout = "out of memory extending the bg undo history.";
        return FALSE;
    }

    BgHistoryClearStack(history->redoentries, &history->redocount);
    BgHistoryDiscardOldest(history->undoentries, &history->undocount);

    entry = &history->undoentries[history->undocount++];
    ZeroMemory(entry, sizeof(*entry));
    entry->document = transaction->before;
    entry->revision = transaction->revision;
    BgHistoryCopyAction(entry->action, transaction->action);
    ZeroMemory(transaction, sizeof(*transaction));

    history->currentrevision = history->nextrevision++;
    BgHistoryUpdateDirty(history, document);
    return TRUE;
}


void BgHistoryCancelEdit(BgHistoryTransaction *transaction)
{
    if (transaction == NULL)
    {
        return;
    }

    if (transaction->active)
    {
        BgDocumentFree(&transaction->before);
    }
    ZeroMemory(transaction, sizeof(*transaction));
}


void BgHistoryRollbackEdit(BgHistoryTransaction *transaction,
                           BgDocument *document)
{
    if (transaction == NULL || document == NULL || !transaction->active)
    {
        return;
    }

    BgDocumentFree(document);
    *document = transaction->before;
    ZeroMemory(transaction, sizeof(*transaction));
}


BOOL BgHistoryCanUndo(const BgHistory *history)
{
    return history != NULL && history->undocount > 0;
}


BOOL BgHistoryCanRedo(const BgHistory *history)
{
    return history != NULL && history->redocount > 0;
}


const char *BgHistoryGetUndoAction(const BgHistory *history)
{
    return BgHistoryCanUndo(history)
        ? history->undoentries[history->undocount - 1].action : "";
}


const char *BgHistoryGetRedoAction(const BgHistory *history)
{
    return BgHistoryCanRedo(history)
        ? history->redoentries[history->redocount - 1].action : "";
}


BOOL BgHistoryUndo(BgHistory *history, BgDocument *document,
                   const char **reasonout)
{
    BgHistoryEntry previous;
    BgHistoryEntry *current;

    *reasonout = "";
    if (!BgHistoryCanUndo(history) || document == NULL)
    {
        *reasonout = "there is no bg edit to undo.";
        return FALSE;
    }
    if (!BgHistoryEnsureCapacity(&history->redoentries,
                                  &history->redocapacity,
                                  history->redocount + 1))
    {
        *reasonout = "out of memory extending the bg redo history.";
        return FALSE;
    }

    previous = history->undoentries[--history->undocount];
    ZeroMemory(&history->undoentries[history->undocount],
               sizeof(history->undoentries[history->undocount]));

    current = &history->redoentries[history->redocount++];
    ZeroMemory(current, sizeof(*current));
    current->document = *document;
    current->revision = history->currentrevision;
    BgHistoryCopyAction(current->action, previous.action);

    *document = previous.document;
    history->currentrevision = previous.revision;
    BgHistoryUpdateDirty(history, document);
    return TRUE;
}


BOOL BgHistoryRedo(BgHistory *history, BgDocument *document,
                   const char **reasonout)
{
    BgHistoryEntry next;
    BgHistoryEntry *current;

    *reasonout = "";
    if (!BgHistoryCanRedo(history) || document == NULL)
    {
        *reasonout = "there is no bg edit to redo.";
        return FALSE;
    }
    if (!BgHistoryEnsureCapacity(&history->undoentries,
                                  &history->undocapacity,
                                  history->undocount + 1))
    {
        *reasonout = "out of memory extending the bg undo history.";
        return FALSE;
    }

    next = history->redoentries[--history->redocount];
    ZeroMemory(&history->redoentries[history->redocount],
               sizeof(history->redoentries[history->redocount]));

    current = &history->undoentries[history->undocount++];
    ZeroMemory(current, sizeof(*current));
    current->document = *document;
    current->revision = history->currentrevision;
    BgHistoryCopyAction(current->action, next.action);

    *document = next.document;
    history->currentrevision = next.revision;
    BgHistoryUpdateDirty(history, document);
    return TRUE;
}


void BgHistoryMarkSaved(BgHistory *history, BgDocument *document)
{
    if (history == NULL || history->currentrevision == 0)
    {
        return;
    }

    history->savedrevision = history->currentrevision;
    if (document != NULL)
    {
        document->dirty = FALSE;
    }
}
