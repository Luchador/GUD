#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "setupload.h"
#include "bghistory.h"
#include "objectflagcatalog.h"
#include "setupselection.h"

/* The tested history entries own only setup documents. */
void BgDocumentFree(BgDocument *document) { (void)document; abort(); }
void StanFileFree(StanFile *stan) { (void)stan; abort(); }

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
    for (int i = 0; i < OBJECTFLAGS_COUNT; i++)
    {
        SetupFile setup = {0}; const ObjectFlagDefinition *flag = &g_ObjectFlags[i]; BOOL changed;
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

static void TypeSpecificFlags(void)
{
    const char *expected[PROPDEF_MAX] = {0};
    expected[PROPDEF_DOOR] = "PROPFLAG_DOOR_REVERSE_SWING";
    expected[PROPDEF_MULTI_MONITOR] = "PROPFLAG_MONITOR_SECONDARY_SCREENS_DECAL";
    expected[PROPDEF_COLLECTABLE] = "PROPFLAG_CONCEAL_GUN";
    expected[PROPDEF_CCTV] = "PROPFLAG_CCTV_NO_DETECTION";
    expected[PROPDEF_VEHICLE] = "PROPFLAG_VEHICLE_INIT_HEADING";
    expected[PROPDEF_AIRCRAFT] = "PROPFLAG_AIRCRAFT_PROPELLER";
    for (int type = 0; type < PROPDEF_MAX; type++)
    {
        int count = 0;
        for (int i = 0; i < OBJECTFLAGS_COUNT; i++)
        {
            const ObjectFlagDefinition *flag = &g_ObjectFlags[i];
            if (flag->bank == 0 && flag->mask == 0x20000000u
                && ObjectFlagAppliesToType(flag, type))
            {
                assert(expected[type] && !strcmp(flag->name, expected[type]));
                count++;
            }
        }
        assert(count == (expected[type] != NULL));
    }
    puts("PASS: each object type exposes only its own 0x20000000 option; turret acquisition state has no editable alias.");
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

static void Similar(void)
{
    SetupObject objects[7] = {
        {.type=PROPDEF_PROP,.modelid=12}, {.type=PROPDEF_PROP,.modelid=13},
        {.type=PROPDEF_DOOR,.modelid=12}, {.type=PROPDEF_PROP,.modelid=12,.deleted=TRUE},
        {.type=PROPDEF_COLLECTABLE,.modelid=12,.flags=PROPFLAG_ASSIGNEDTOCHR},
        {.type=PROPDEF_PROP,.modelid=12,.flags=PROPFLAG_INSIDEANOTHEROBJ}, {.modelid=-1}};
    SetupCharacter characters[4] = {{.bodyid=12,.headid=3},{.bodyid=12,.headid=6},{.bodyid=13},{.bodyid=12,.deleted=TRUE}};
    SetupFile setup = {.objects=objects,.objectcount=7,.characters=characters,.charactercount=4};
    DWORD seeds[] = {0, SETUP_CHARACTER_SELECTION_BIT}, *out, count;
    const char *why;
    assert(SetupSelectionSimilar(&setup,seeds,1,&out,&count,&why));
    assert(count==2 && out[0]==0 && out[1]==2); free(out);
    assert(SetupSelectionSimilar(&setup,seeds+1,1,&out,&count,&why));
    assert(count==2 && out[0]==SETUP_CHARACTER_SELECTION_BIT && out[1]==(SETUP_CHARACTER_SELECTION_BIT|1)); free(out);
    assert(SetupSelectionSimilar(&setup,seeds,2,&out,&count,&why));
    assert(count==4 && out[0]==0 && out[1]==2 && out[2]==SETUP_CHARACTER_SELECTION_BIT); free(out);
    assert(!SetupSelectionSimilar(&setup,NULL,0,&out,&count,&why) && !out && !count);
    seeds[0]=1000;
    assert(!SetupSelectionSimilar(&setup,seeds,1,&out,&count,&why));
    puts("PASS: Similar matches object models / character bodies in separate namespaces, unions mixed seeds and excludes deleted/carried/contained records.");
}

static void Collective(const SetupFile *source, const char *dir)
{
    SetupFile setup = {0}, before = {0}, edited = {0};
    const char *why; BOOL changed; SetupFlagSummary flags;
    DWORD ids[] = {0,1,SETUP_CHARACTER_SELECTION_BIT,SETUP_CHARACTER_SELECTION_BIT|1};
    EditHistory history = {0}; EditHistoryTransaction transaction = {0}; EditHistoryAsset asset;
    BgDocument bg = {0}; StanFile stan = {0};
    assert(SetupFileClone(source,&setup,&why));
    assert(SetupSelectionFlags(&setup,ids,4,&flags));
    assert(flags.objects==2 && flags.characters==2 && flags.objecttype==-1);
    assert(flags.all[2]==0 && flags.any[2]==9);
    /* One undoable checkbox edit covers both objects, preserving character data. */
    EditHistoryReset(&history,&bg,&setup,&stan);
    assert(EditHistoryBeginSetupEdit(&history,&setup,"Change Model Flags",&transaction,&why));
    assert(SetupSelectionSetFlag(&setup,ids,4,1,PROPFLAG2_GUNFIRE_IMMUNE,TRUE,&changed,&why) && changed);
    assert(!memcmp(setup.characters,source->characters,2*sizeof(*source->characters)));
    assert((setup.objects[0].flags2 & PROPFLAG2_GUNFIRE_IMMUNE) && (setup.objects[1].flags2 & PROPFLAG2_GUNFIRE_IMMUNE));
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&transaction,&why));
    assert(history.undocount==1 && SetupFileClone(&setup,&edited,&why));
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why)); Same(&setup,source);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why)); Same(&setup,&edited);
    RoundTrip(dir,&setup);
    EditHistoryFree(&history); SetupFileFree(&edited); SetupFileFree(&setup);
    /* All sixteen saved guard bits, including mixed -> checked / clear.
     * Compare every byte to catch writes into adjacent fields or records. */
    for (DWORD mask=1;mask<=0x8000;mask<<=1)
    {
        assert(SetupFileClone(source,&setup,&why));
        for (int enabled=1;enabled>=0;enabled--)
        {
            assert(SetupFileClone(&setup,&before,&why));
            for (DWORD c=0;c<before.charactercount;c++)
            {
                DWORD at=before.characters[c].sourceoffset+20;
                unsigned short value=before.characters[c].flags;
                value=enabled ? value|mask : value&~mask;
                before.data[at]=value>>8; before.data[at+1]=value;
                before.characters[c].flags=value;
            }
            assert(SetupSelectionSetFlag(&setup,ids,4,2,mask,enabled,&changed,&why));
            Same(&setup,&before); RoundTrip(dir,&setup); SetupFileFree(&before);
            assert(SetupSelectionFlags(&setup,ids,4,&flags));
            assert((flags.all[2]&mask)==(enabled?mask:0) && (flags.any[2]&mask)==(enabled?mask:0));
            assert(SetupSelectionSetFlag(&setup,ids,4,2,mask,enabled,&changed,&why) && !changed);
        }
        SetupFileFree(&setup);
    }
    /* A late corrupt record rolls back the whole batch, including an earlier edit. */
    assert(SetupFileClone(source,&setup,&why));
    setup.objects[1].sourceoffset=0;
    assert(SetupFileClone(&setup,&before,&why));
    assert(!SetupSelectionSetFlag(&setup,ids,4,1,PROPFLAG2_GUNFIRE_IMMUNE,TRUE,&changed,&why) && !changed);
    Same(&setup,&before); assert(!setup.dirty);
    SetupFileFree(&setup); SetupFileFree(&before);
    assert(SetupFileClone(source,&setup,&why));
    setup.characters[1].flags^=2;
    assert(SetupFileClone(&setup,&before,&why));
    assert(!SetupSelectionSetFlag(&setup,ids,4,2,2,TRUE,&changed,&why) && !changed);
    Same(&setup,&before); assert(!setup.dirty);
    assert(!SetupSelectionSetFlag(&setup,ids,4,2,0x10000,TRUE,&changed,&why));
    assert(!SetupSelectionSetFlag(&setup,ids,4,2,3,TRUE,&changed,&why));
    SetupFileFree(&setup); SetupFileFree(&before);
    /* Hidden-by-flags objects stay editable as part of a group. */
    assert(SetupFileClone(source,&setup,&why));
    for (DWORD mask=8;mask<=128;mask<<=1)
        assert(SetupSelectionSetFlag(&setup,ids,4,1,mask,TRUE,&changed,&why));
    assert(setup.objects[0].deleted && setup.objects[1].deleted);
    assert(SetupSelectionSetFlag(&setup,ids,4,1,PROPFLAG2_NO_LOAD_A,FALSE,&changed,&why) && changed);
    assert(!setup.objects[0].deleted && !setup.objects[1].deleted);
    RoundTrip(dir,&setup); SetupFileFree(&setup);
    puts("PASS: collective object/character flags, mixed values, all guard bits, atomic rejection, save/reload, no-ops, hidden models and one-step undo/redo.");
}

int main(int argc, char **argv)
{
    SetupFile source = {0}; const char *why = "";
    assert(argc == 2);
    assert(SetupLoadProjectFile(argv[1], "UsetupflagsZ", &source, &why));
    assert(source.objectcount == 2 && source.charactercount == 2 && source.padcount == 1);
    Similar(); Collective(&source, argv[1]);
    TypeSpecificFlags(); AllBits(&source, argv[1]); History(&source, argv[1]); ModeExclusions(&source, argv[1]); Invalid(&source);
    SetupFileFree(&source); return 0;
}
