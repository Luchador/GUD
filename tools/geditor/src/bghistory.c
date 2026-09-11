/*
 * Bounded chronological undo/redo history for editable level documents.
 *
 * Each entry owns a selection snapshot and the document for any changed asset.
 * Undo and redo transfer that ownership between the live document and the
 * opposite stack, avoiding another potentially large clone while stepping.
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "bghistory.h"


static void EditHistoryCopyAction(char out[EDIT_HISTORY_ACTION_MAX],
                                  const char *action)
{
    if (action == NULL || action[0] == '\0')
    {
        action = "Edit";
    }

    lstrcpyn(out, action, EDIT_HISTORY_ACTION_MAX);
}


static void EditHistoryFreeEntry(EditHistoryEntry *entry)
{
    if (entry->asset == EDIT_HISTORY_ASSET_BG)
    {
        BgDocumentFree(&entry->bgdocument);
    }
    else if (entry->asset == EDIT_HISTORY_ASSET_SETUP)
    {
        SetupFileFree(&entry->setup);
    }
    else if (entry->asset == EDIT_HISTORY_ASSET_STAN)
    {
        StanFileFree(&entry->stan);
    }
    free(entry->selection);
    ZeroMemory(entry, sizeof(*entry));
}


static void EditHistoryFreeEntries(EditHistoryEntry *entries, DWORD count)
{
    DWORD index;

    for (index = 0; index < count; index++)
    {
        EditHistoryFreeEntry(&entries[index]);
    }
    free(entries);
}


static BOOL EditHistoryEnsureCapacity(EditHistoryEntry **entries,
                                      DWORD *capacity, DWORD needed)
{
    DWORD nextcapacity;
    EditHistoryEntry *grown;

    if (needed <= *capacity)
    {
        return TRUE;
    }
    if (needed > EDIT_HISTORY_LIMIT)
    {
        return FALSE;
    }

    nextcapacity = *capacity == 0 ? 8 : *capacity * 2;
    if (nextcapacity > EDIT_HISTORY_LIMIT)
    {
        nextcapacity = EDIT_HISTORY_LIMIT;
    }
    if (nextcapacity < needed)
    {
        nextcapacity = needed;
    }

    grown = (EditHistoryEntry *)realloc(*entries,
                (size_t)nextcapacity * sizeof(**entries));
    if (grown == NULL)
    {
        return FALSE;
    }

    *entries = grown;
    *capacity = nextcapacity;
    return TRUE;
}


static void EditHistoryClearStack(EditHistoryEntry *entries, DWORD *count)
{
    while (*count > 0)
    {
        (*count)--;
        EditHistoryFreeEntry(&entries[*count]);
    }
}


static void EditHistoryDiscardOldest(EditHistoryEntry *entries,
                                     DWORD *count)
{
    if (*count < EDIT_HISTORY_LIMIT)
    {
        return;
    }

    EditHistoryFreeEntry(&entries[0]);
    memmove(entries, entries + 1,
            (size_t)(*count - 1) * sizeof(*entries));
    (*count)--;
    ZeroMemory(&entries[*count], sizeof(entries[*count]));
}


static void EditHistoryUpdateDirty(const EditHistory *history,
                                   BgDocument *bgdocument, SetupFile *setup, StanFile *stan)
{
    if (bgdocument != NULL)
    {
        bgdocument->dirty = history->currentbgrevision
                         != history->savedbgrevision;
    }
    if (setup != NULL)
    {
        setup->dirty = history->currentsetuprevision
                    != history->savedsetuprevision;
    }
    if (stan != NULL)
    {
        stan->dirty = history->currentstanrevision != history->savedstanrevision;
    }
}


void EditHistoryReset(EditHistory *history, BgDocument *bgdocument,
                      SetupFile *setup, StanFile *stan)
{
    EditHistoryFree(history);
    history->currentstaterevision = 1;
    history->currentbgrevision = 1;
    history->currentsetuprevision = 1;
    history->currentstanrevision = 1;
    history->savedbgrevision = 1;
    history->savedsetuprevision = 1;
    history->savedstanrevision = 1;
    history->nextrevision = 2;
    EditHistoryUpdateDirty(history, bgdocument, setup, stan);
}


void EditHistoryFree(EditHistory *history)
{
    if (history == NULL)
    {
        return;
    }

    EditHistoryFreeEntries(history->undoentries, history->undocount);
    EditHistoryFreeEntries(history->redoentries, history->redocount);
    free(history->selection);
    ZeroMemory(history, sizeof(*history));
}


/* Selection-only steps use the same chronological stack, but never change an
 * asset revision. Keeping snapshots opaque avoids coupling history to windows. */
BOOL EditHistorySetSelection(EditHistory *history, const void *data, size_t size,
                             BOOL record, const char **reasonout)
{
    void *copy = NULL;
    DWORD needed;
    EditHistoryEntry *entry;
    *reasonout = "";
    if (!history || !history->currentstaterevision || (size && !data))
    { *reasonout = "the selection history has not been initialized."; return FALSE; }
    if (size == history->selectionsize && (!size || !memcmp(data, history->selection, size)))
    { return TRUE; }
    if (size)
    {
        copy = malloc(size);
        if (!copy) { *reasonout = "out of memory remembering the selection."; return FALSE; }
        memcpy(copy, data, size);
    }
    if (record)
    {
        needed = history->undocount < EDIT_HISTORY_LIMIT ? history->undocount + 1 : EDIT_HISTORY_LIMIT;
        if (!history->nextrevision || !EditHistoryEnsureCapacity(&history->undoentries,
                &history->undocapacity, needed))
        { free(copy); *reasonout = "could not extend the selection history."; return FALSE; }
        EditHistoryClearStack(history->redoentries, &history->redocount);
        EditHistoryDiscardOldest(history->undoentries, &history->undocount);
        entry = &history->undoentries[history->undocount++];
        ZeroMemory(entry, sizeof(*entry));
        entry->asset = EDIT_HISTORY_ASSET_SELECTION;
        entry->selection = history->selection;
        entry->selectionsize = history->selectionsize;
        entry->staterevision = history->currentstaterevision;
        EditHistoryCopyAction(entry->action, "Change Selection");
        history->currentstaterevision = history->nextrevision++;
    }
    else { free(history->selection); }
    history->selection = copy;
    history->selectionsize = size;
    return TRUE;
}


static BOOL EditHistoryBegin(const EditHistory *history,
                             EditHistoryAsset asset, const char *action,
                             EditHistoryTransaction *transaction,
                             const char **reasonout)
{
    ZeroMemory(transaction, sizeof(*transaction));
    *reasonout = "";

    if (history == NULL || history->currentstaterevision == 0)
    {
        *reasonout = "the edit undo history has not been initialized.";
        return FALSE;
    }

    transaction->asset = asset;
    transaction->staterevision = history->currentstaterevision;
    transaction->assetrevision = asset == EDIT_HISTORY_ASSET_BG
        ? history->currentbgrevision : asset == EDIT_HISTORY_ASSET_STAN
        ? history->currentstanrevision : history->currentsetuprevision;
    EditHistoryCopyAction(transaction->action, action);
    return TRUE;
}


BOOL EditHistoryBeginBgEdit(const EditHistory *history,
                            const BgDocument *document, const char *action,
                            EditHistoryTransaction *transaction,
                            const char **reasonout)
{
    if (!EditHistoryBegin(history, EDIT_HISTORY_ASSET_BG, action,
                          transaction, reasonout))
    {
        return FALSE;
    }
    if (!BgDocumentClone(document, &transaction->beforebg, reasonout))
    {
        ZeroMemory(transaction, sizeof(*transaction));
        return FALSE;
    }

    transaction->active = TRUE;
    return TRUE;
}


BOOL EditHistoryBeginSetupEdit(const EditHistory *history,
                               const SetupFile *setup, const char *action,
                               EditHistoryTransaction *transaction,
                               const char **reasonout)
{
    if (!EditHistoryBegin(history, EDIT_HISTORY_ASSET_SETUP, action,
                          transaction, reasonout))
    {
        return FALSE;
    }
    if (!SetupFileClone(setup, &transaction->beforesetup, reasonout))
    {
        ZeroMemory(transaction, sizeof(*transaction));
        return FALSE;
    }

    transaction->active = TRUE;
    return TRUE;
}


BOOL EditHistoryBeginStanEdit(const EditHistory *history,
                               const StanFile *stan, const char *action,
                               EditHistoryTransaction *transaction,
                               const char **reasonout)
{
    if (!EditHistoryBegin(history, EDIT_HISTORY_ASSET_STAN, action,
                          transaction, reasonout))
    {
        return FALSE;
    }
    if (!StanFileClone(stan, &transaction->beforestan, reasonout))
    {
        ZeroMemory(transaction, sizeof(*transaction));
        return FALSE;
    }

    transaction->active = TRUE;
    return TRUE;
}


BOOL EditHistoryCommitEdit(EditHistory *history, BgDocument *bgdocument,
                           SetupFile *setup, StanFile *stan,
                           EditHistoryTransaction *transaction,
                           const char **reasonout)
{
    EditHistoryEntry *entry;
    void *selection = NULL;
    DWORD needed;
    ULONGLONG currentassetrevision;
    ULONGLONG newrevision;

    *reasonout = "";
    if (history == NULL || transaction == NULL || !transaction->active
        || (transaction->asset != EDIT_HISTORY_ASSET_BG
            && transaction->asset != EDIT_HISTORY_ASSET_SETUP
            && transaction->asset != EDIT_HISTORY_ASSET_STAN))
    {
        *reasonout = "there is no edit transaction to commit.";
        return FALSE;
    }

    currentassetrevision = transaction->asset == EDIT_HISTORY_ASSET_BG
        ? history->currentbgrevision : transaction->asset == EDIT_HISTORY_ASSET_STAN
        ? history->currentstanrevision : history->currentsetuprevision;
    if (transaction->staterevision != history->currentstaterevision
        || transaction->assetrevision != currentassetrevision)
    {
        *reasonout = "a level document changed during the edit transaction.";
        return FALSE;
    }
    if (history->nextrevision == 0)
    {
        *reasonout = "the edit undo history revision counter is exhausted.";
        return FALSE;
    }

    needed = history->undocount < EDIT_HISTORY_LIMIT
        ? history->undocount + 1 : EDIT_HISTORY_LIMIT;
    if (!EditHistoryEnsureCapacity(&history->undoentries,
                                    &history->undocapacity, needed))
    {
        *reasonout = "out of memory extending the edit undo history.";
        return FALSE;
    }

    if (history->selectionsize)
    {
        selection = malloc(history->selectionsize);
        if (!selection)
        { *reasonout = "out of memory remembering the selection before the edit."; return FALSE; }
        memcpy(selection, history->selection, history->selectionsize);
    }

    EditHistoryClearStack(history->redoentries, &history->redocount);
    EditHistoryDiscardOldest(history->undoentries, &history->undocount);

    entry = &history->undoentries[history->undocount++];
    ZeroMemory(entry, sizeof(*entry));
    entry->selection = selection;
    entry->selectionsize = history->selectionsize;
    entry->asset = transaction->asset;
    entry->staterevision = transaction->staterevision;
    entry->assetrevision = transaction->assetrevision;
    if (entry->asset == EDIT_HISTORY_ASSET_BG)
    {
        entry->bgdocument = transaction->beforebg;
    }
    else if (entry->asset == EDIT_HISTORY_ASSET_STAN)
    {
        entry->stan = transaction->beforestan;
    }
    else
    {
        entry->setup = transaction->beforesetup;
    }
    EditHistoryCopyAction(entry->action, transaction->action);
    ZeroMemory(transaction, sizeof(*transaction));

    newrevision = history->nextrevision++;
    history->currentstaterevision = newrevision;
    if (entry->asset == EDIT_HISTORY_ASSET_BG)
    {
        history->currentbgrevision = newrevision;
    }
    else if (entry->asset == EDIT_HISTORY_ASSET_STAN)
    {
        history->currentstanrevision = newrevision;
    }
    else
    {
        history->currentsetuprevision = newrevision;
    }
    EditHistoryUpdateDirty(history, bgdocument, setup, stan);
    return TRUE;
}


void EditHistoryCancelEdit(EditHistoryTransaction *transaction)
{
    if (transaction == NULL)
    {
        return;
    }

    if (transaction->active)
    {
        if (transaction->asset == EDIT_HISTORY_ASSET_BG)
        {
            BgDocumentFree(&transaction->beforebg);
        }
        else if (transaction->asset == EDIT_HISTORY_ASSET_SETUP)
        {
            SetupFileFree(&transaction->beforesetup);
        }
        else if (transaction->asset == EDIT_HISTORY_ASSET_STAN)
        {
            StanFileFree(&transaction->beforestan);
        }
    }
    ZeroMemory(transaction, sizeof(*transaction));
}


void EditHistoryRollbackEdit(EditHistoryTransaction *transaction,
                             BgDocument *bgdocument, SetupFile *setup, StanFile *stan)
{
    if (transaction == NULL || !transaction->active)
    {
        return;
    }

    if (transaction->asset == EDIT_HISTORY_ASSET_BG && bgdocument != NULL)
    {
        BgDocumentFree(bgdocument);
        *bgdocument = transaction->beforebg;
    }
    else if (transaction->asset == EDIT_HISTORY_ASSET_SETUP && setup != NULL)
    {
        SetupFileFree(setup);
        *setup = transaction->beforesetup;
    }
    else if (transaction->asset == EDIT_HISTORY_ASSET_STAN && stan != NULL)
    {
        StanFileFree(stan);
        *stan = transaction->beforestan;
    }
    else
    {
        EditHistoryCancelEdit(transaction);
        return;
    }

    ZeroMemory(transaction, sizeof(*transaction));
}


BOOL EditHistoryCanUndo(const EditHistory *history)
{
    return history != NULL && history->undocount > 0;
}


BOOL EditHistoryCanRedo(const EditHistory *history)
{
    return history != NULL && history->redocount > 0;
}


const char *EditHistoryGetUndoAction(const EditHistory *history)
{
    return EditHistoryCanUndo(history)
        ? history->undoentries[history->undocount - 1].action : "";
}


const char *EditHistoryGetRedoAction(const EditHistory *history)
{
    return EditHistoryCanRedo(history)
        ? history->redoentries[history->redocount - 1].action : "";
}


static void EditHistoryMoveLiveToEntry(EditHistoryEntry *entry,
                                       EditHistoryAsset asset,
                                       BgDocument *bgdocument,
                                       SetupFile *setup, StanFile *stan)
{
    entry->asset = asset;
    if (asset == EDIT_HISTORY_ASSET_BG)
    {
        entry->bgdocument = *bgdocument;
    }
    else if (asset == EDIT_HISTORY_ASSET_STAN)
    {
        entry->stan = *stan;
    }
    else if (asset == EDIT_HISTORY_ASSET_SETUP)
    {
        entry->setup = *setup;
    }
}


static void EditHistoryMoveEntryToLive(EditHistoryEntry *entry,
                                       BgDocument *bgdocument,
                                       SetupFile *setup, StanFile *stan)
{
    if (entry->asset == EDIT_HISTORY_ASSET_BG)
    {
        *bgdocument = entry->bgdocument;
        ZeroMemory(&entry->bgdocument, sizeof(entry->bgdocument));
    }
    else if (entry->asset == EDIT_HISTORY_ASSET_STAN)
    {
        *stan = entry->stan;
        ZeroMemory(&entry->stan, sizeof(entry->stan));
    }
    else if (entry->asset == EDIT_HISTORY_ASSET_SETUP)
    {
        *setup = entry->setup;
        ZeroMemory(&entry->setup, sizeof(entry->setup));
    }
}


static BOOL EditHistoryStep(EditHistory *history,
                            EditHistoryEntry *fromentries, DWORD *fromcount,
                            EditHistoryEntry **toentries, DWORD *tocount,
                            DWORD *tocapacity,
                            BgDocument *bgdocument, SetupFile *setup, StanFile *stan,
                            EditHistoryAsset *assetout,
                            const char **reasonout)
{
    EditHistoryEntry previous;
    EditHistoryEntry *current;
    ULONGLONG currentassetrevision;

    *reasonout = "";
    if (history == NULL || *fromcount == 0
        || bgdocument == NULL || setup == NULL || stan == NULL)
    {
        *reasonout = "there is no edit history step available.";
        return FALSE;
    }
    if (!EditHistoryEnsureCapacity(toentries, tocapacity, *tocount + 1))
    {
        *reasonout = "out of memory extending the edit history.";
        return FALSE;
    }

    previous = fromentries[--(*fromcount)];
    ZeroMemory(&fromentries[*fromcount], sizeof(fromentries[*fromcount]));
    currentassetrevision = previous.asset == EDIT_HISTORY_ASSET_BG
        ? history->currentbgrevision : previous.asset == EDIT_HISTORY_ASSET_STAN
        ? history->currentstanrevision : history->currentsetuprevision;

    current = &(*toentries)[(*tocount)++];
    ZeroMemory(current, sizeof(*current));
    EditHistoryMoveLiveToEntry(current, previous.asset, bgdocument, setup, stan);
    current->selection = history->selection;
    current->selectionsize = history->selectionsize;
    history->selection = previous.selection;
    history->selectionsize = previous.selectionsize;
    current->staterevision = history->currentstaterevision;
    current->assetrevision = currentassetrevision;
    EditHistoryCopyAction(current->action, previous.action);

    EditHistoryMoveEntryToLive(&previous, bgdocument, setup, stan);
    history->currentstaterevision = previous.staterevision;
    if (previous.asset == EDIT_HISTORY_ASSET_BG)
    {
        history->currentbgrevision = previous.assetrevision;
    }
    else if (previous.asset == EDIT_HISTORY_ASSET_STAN)
    {
        history->currentstanrevision = previous.assetrevision;
    }
    else if (previous.asset == EDIT_HISTORY_ASSET_SETUP)
    {
        history->currentsetuprevision = previous.assetrevision;
    }
    EditHistoryUpdateDirty(history, bgdocument, setup, stan);

    if (assetout != NULL)
    {
        *assetout = previous.asset;
    }
    return TRUE;
}


BOOL EditHistoryUndo(EditHistory *history, BgDocument *bgdocument,
                     SetupFile *setup, StanFile *stan, EditHistoryAsset *assetout,
                     const char **reasonout)
{
    if (assetout != NULL)
    {
        *assetout = EDIT_HISTORY_ASSET_NONE;
    }
    if (!EditHistoryCanUndo(history))
    {
        *reasonout = "there is no edit to undo.";
        return FALSE;
    }

    return EditHistoryStep(history,
        history->undoentries, &history->undocount,
        &history->redoentries, &history->redocount,
        &history->redocapacity,
        bgdocument, setup, stan, assetout, reasonout);
}


BOOL EditHistoryRedo(EditHistory *history, BgDocument *bgdocument,
                     SetupFile *setup, StanFile *stan, EditHistoryAsset *assetout,
                     const char **reasonout)
{
    if (assetout != NULL)
    {
        *assetout = EDIT_HISTORY_ASSET_NONE;
    }
    if (!EditHistoryCanRedo(history))
    {
        *reasonout = "there is no edit to redo.";
        return FALSE;
    }

    return EditHistoryStep(history,
        history->redoentries, &history->redocount,
        &history->undoentries, &history->undocount,
        &history->undocapacity,
        bgdocument, setup, stan, assetout, reasonout);
}


void EditHistoryMarkBgSaved(EditHistory *history, BgDocument *document)
{
    if (history == NULL || history->currentstaterevision == 0)
    {
        return;
    }

    history->savedbgrevision = history->currentbgrevision;
    if (document != NULL)
    {
        document->dirty = FALSE;
    }
}


void EditHistoryMarkSetupSaved(EditHistory *history, SetupFile *setup)
{
    if (history == NULL || history->currentstaterevision == 0)
    {
        return;
    }

    history->savedsetuprevision = history->currentsetuprevision;
    if (setup != NULL)
    {
        setup->dirty = FALSE;
    }
}


void EditHistoryMarkStanSaved(EditHistory *history, StanFile *stan)
{
    if (history == NULL || history->currentstaterevision == 0) { return; }
    history->savedstanrevision = history->currentstanrevision;
    if (stan != NULL) { stan->dirty = FALSE; }
}
