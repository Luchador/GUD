#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "setupload.h"
#include "bghistory.h"

/* The tested history entries own only setup documents. */
void BgDocumentFree(BgDocument *document) { (void)document; abort(); }
void StanFileFree(StanFile *stan) { (void)stan; abort(); }

typedef struct Flag { unsigned int bank; DWORD mask; } Flag;
static const Flag flags[] = {
#define OBJECT_FLAG(bank, symbol, label, description) {bank, (DWORD)symbol},
#include "objectflagdefs.h"
#undef OBJECT_FLAG
};
static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }
static void Same(const SetupFile *a, const SetupFile *b)
{
    assert(a->size == b->size && !memcmp(a->data, b->data, a->size));
    assert(a->objectcount == b->objectcount && a->charactercount == b->charactercount);
    assert(!memcmp(a->objects, b->objects, a->objectcount * sizeof(*a->objects)));
    assert(!memcmp(a->characters, b->characters, a->charactercount * sizeof(*a->characters)));
}

static void RoundTrip(const char *dir, const SetupFile *setup)
{
    SetupFile saved = {0}; const char *why = "";
    assert(SetupSaveProjectFile(dir, setup, &why));
    assert(SetupLoadProjectFile(dir, setup->name, &saved, &why));
    Same(&saved, setup); assert(!saved.dirty);
    SetupFileFree(&saved);
}

static void AllBits(const SetupFile *source, const char *dir)
{
    const char *why = "";
    for (unsigned int i = 0; i < sizeof(flags) / sizeof(*flags); i++)
    {
        SetupFile setup = {0}; const Flag *flag = &flags[i]; BOOL changed;
        DWORD offset = source->objects[0].sourceoffset + 8 + flag->bank * 4;
        DWORD initial = Read32(source->data + offset);
        assert(SetupFileClone(source, &setup, &why));
        assert(SetupFileSetObjectFlag(&setup, 0, flag->bank, flag->mask,
            (initial & flag->mask) != 0, &changed, &why) && !changed && !setup.dirty);
        Same(&setup, source);
        for (int pass = 0; pass < 2; pass++)
        {
            BOOL enabled = pass ? (initial & flag->mask) != 0 : (initial & flag->mask) == 0;
            DWORD expected = pass ? initial : initial ^ flag->mask;
            assert(SetupFileSetObjectFlag(&setup, 0, flag->bank, flag->mask, enabled, &changed, &why));
            assert(changed && setup.dirty && Read32(setup.data + offset) == expected);
            assert(setup.size == source->size && !memcmp(setup.data, source->data, offset));
            assert(!memcmp(setup.data + offset + 4, source->data + offset + 4, setup.size - offset - 4));
            assert((flag->bank ? setup.objects[0].flags2 : setup.objects[0].flags) == expected);
            assert(setup.objects[0].nonsolid == source->objects[0].nonsolid);
            RoundTrip(dir, &setup);
        }
        Same(&setup, source); SetupFileFree(&setup);
    }
    puts("PASS: each flag sets/clears only its native bit; aliases, both high bits, no-op edits and save/reload preserve all other data.");
}

static void History(const SetupFile *source, const char *dir)
{
    SetupFile setup = {0}, edited = {0}; EditHistory history = {0};
    EditHistoryTransaction transaction = {0}; EditHistoryAsset asset;
    BgDocument bg = {0}; StanFile stan = {0};
    const char *why = ""; BOOL changed;
    assert(SetupFileClone(source, &setup, &why));
    EditHistoryReset(&history, &bg, &setup, &stan);
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Change Object Flag", &transaction, &why));
    assert(SetupFileSetObjectFlag(&setup, 1, 1, PROPFLAG2_GUNFIRE_IMMUNE, TRUE, &changed, &why) && changed);
    assert(EditHistoryCommitEdit(&history, &bg, &setup, &stan, &transaction, &why));
    assert(history.undocount == 1 && setup.dirty);
    assert(SetupFileClone(&setup, &edited, &why));
    RoundTrip(dir, &setup); EditHistoryMarkSetupSaved(&history, &setup); assert(!setup.dirty);
    assert(EditHistoryUndo(&history, &bg, &setup, &stan, &asset, &why));
    assert(asset == EDIT_HISTORY_ASSET_SETUP && setup.dirty); Same(&setup, source);
    RoundTrip(dir, &setup);
    assert(EditHistoryRedo(&history, &bg, &setup, &stan, &asset, &why));
    assert(!setup.dirty); Same(&setup, &edited); RoundTrip(dir, &setup);
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Change Object Flag", &transaction, &why));
    assert(SetupFileSetObjectFlag(&setup, 1, 0, PROPFLAG_IS_DOUBLE, TRUE, &changed, &why) && changed);
    EditHistoryRollbackEdit(&transaction, &bg, &setup, &stan);
    Same(&setup, &edited); assert(!setup.dirty && history.undocount == 1);
    EditHistoryFree(&history); SetupFileFree(&setup); SetupFileFree(&edited);
    puts("PASS: object flag undo/redo, rollback and dirty state stay correct across saving.");
}

static void ModeExclusions(const SetupFile *source, const char *dir)
{
    SetupFile setup = {0}, deleted = {0}; const char *why = ""; BOOL changed;
    assert(SetupFileClone(source, &setup, &why));
    assert(SetupFileClone(source, &deleted, &why));
    assert(SetupFileDeleteObject(&deleted, 0, &why));
    for (DWORD mask = 8; mask <= 128; mask <<= 1)
    { assert(SetupFileSetObjectFlag(&setup, 0, 1, mask, TRUE, &changed, &why)); }
    assert(setup.objects[0].deleted); Same(&setup, &deleted); RoundTrip(dir, &setup);
    assert(SetupFileSetObjectFlag(&setup, 0, 1, PROPFLAG2_NO_LOAD_A, FALSE, &changed, &why));
    assert(changed && !setup.objects[0].deleted); RoundTrip(dir, &setup);
    SetupFileFree(&setup); SetupFileFree(&deleted);
    puts("PASS: mode-exclusion flags agree with Delete Object and reloading, including re-enabling a mode.");
}

static void Invalid(const SetupFile *source)
{
    SetupFile setup = {0}; const char *why = ""; BOOL changed;
    assert(SetupFileClone(source, &setup, &why));
#define REJECT(index, bank, mask) do { \
    assert(!SetupFileSetObjectFlag(&setup, index, bank, mask, TRUE, &changed, &why)); \
    assert(!changed && why[0] && !setup.dirty); \
    assert(!memcmp(setup.data, source->data, source->size)); \
} while (0)
    REJECT(setup.objectcount, 0, 1); REJECT(SETUP_CHARACTER_SELECTION_BIT, 0, 1);
    REJECT(0, 2, 1); REJECT(0, 0, 0); REJECT(0, 0, 3);
    for (int i = 0; i < 5; i++)
    {
        DWORD offsets[] = {0, 168, setup.size - 4, setup.size + 1, 0xffffffffu};
        setup.objects[0].sourceoffset = offsets[i]; REJECT(0, 0, 1);
    }
    setup.objects[0] = source->objects[0];
    setup.objects[0].flags ^= 1; REJECT(0, 0, 1);
    setup.objects[0] = source->objects[0];
    setup.objects[0].type = PROPDEF_GUARD; REJECT(0, 0, 1);
#undef REJECT
    SetupFileFree(&setup);
    puts("PASS: invalid masks, banks, character indices and corrupt/stale source records are rejected without mutation.");
}

int main(int argc, char **argv)
{
    SetupFile source = {0}; const char *why = "";
    assert(argc == 2);
    assert(SetupLoadProjectFile(argv[1], "UsetupflagsZ", &source, &why));
    assert(source.objectcount == 2 && source.charactercount == 1 && source.padcount == 1);
    AllBits(&source, argv[1]); History(&source, argv[1]); ModeExclusions(&source, argv[1]); Invalid(&source);
    SetupFileFree(&source); return 0;
}
