#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"
#include "bghistory.h"
#include "modelload.h"
#include <src/propconstants.h>

/* Only setup documents are edited in this suite. */
void BgDocumentFree(BgDocument *d) { memset(d, 0, sizeof(*d)); }
void StanFileFree(StanFile *d) { memset(d, 0, sizeof(*d)); }

static DWORD Read32(const unsigned char *p)
{ return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3]; }
static SetupObjectPropertyEdit Request(const SetupFile *setup, DWORD index, SetupObjectProperty property, double value)
{
    SetupObjectPropertyEdit edit = {0};
    edit.objectindex = index; edit.sourceoffset = setup->objects[index].sourceoffset;
    edit.type = setup->objects[index].type; edit.property = property; edit.value = value;
    return edit;
}
static void Same(const SetupFile *a, const SetupFile *b)
{
    assert(a->size == b->size && a->objectcount == b->objectcount && a->charactercount == b->charactercount);
    assert(!memcmp(a->data, b->data, a->size));
    assert(!memcmp(a->objects, b->objects, a->objectcount * sizeof(*a->objects)));
    assert(!memcmp(a->characters, b->characters, a->charactercount * sizeof(*a->characters)));
}
static void RoundTrip(const char *dir, const SetupFile *setup)
{
    SetupFile loaded = {0}; const char *why;
    assert(SetupSaveProjectFile(dir, setup, &why));
    assert(SetupLoadProjectFile(dir, setup->name, &loaded, &why));
    Same(&loaded, setup); assert(!loaded.dirty);
    SetupFileFree(&loaded);
}
static void OnlyBytes(const SetupFile *before, const SetupFile *after, DWORD start, DWORD length)
{
    assert(before->size == after->size);
    assert(!memcmp(before->data, after->data, start));
    assert(!memcmp(before->data + start + length, after->data + start + length, before->size - start - length));
}

static DWORD FindType(const SetupFile *setup, unsigned char type)
{
    for (DWORD i = 0; i < setup->objectcount; i++) { if (setup->objects[i].type == type) { return i; } }
    assert(0); return 0;
}

static void CheckSpecificEdit(const char *dir, const SetupFile *source,
    SetupObjectPropertyEdit edit, DWORD relative, DWORD length)
{
    SetupFile setup = {0}; SetupObjectProperties properties;
    EditHistory history = {0}; EditHistoryTransaction tx; EditHistoryAsset asset;
    BgDocument bg = {0}; StanFile stan = {0}; const char *why; BOOL changed;
    assert(SetupFileClone(source, &setup, &why));
    EditHistoryReset(&history, &bg, &setup, &stan);
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Edit Object Property", &tx, &why));
    assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && changed);
    assert(SetupFileGetObjectProperties(&setup, edit.objectindex, &properties, &why));
    switch (edit.property)
    {
    case SETUP_OBJECT_DOOR_KEY_FLAGS:
    case SETUP_OBJECT_KEY_FLAGS: assert(properties.keyflags == (DWORD)edit.value); break;
    case SETUP_OBJECT_AMMO_TYPE: assert(properties.ammotype == (DWORD)edit.value); break;
    case SETUP_OBJECT_AMMO_QUANTITY: assert(properties.ammo[edit.slot].quantity == (unsigned short)edit.value); break;
    case SETUP_OBJECT_DOOR_TYPE: assert(properties.door.type == (unsigned short)edit.value); break;
    case SETUP_OBJECT_DOOR_FLAGS: assert(properties.door.flags == (unsigned short)edit.value); break;
    case SETUP_OBJECT_DOOR_SOUND: assert(properties.door.sound == (DWORD)edit.value); break;
    case SETUP_OBJECT_DOOR_CLOSE_DELAY: assert(properties.door.closeframes == (DWORD)floor(edit.value * 60.0 + 0.5)); break;
    case SETUP_OBJECT_DOOR_TRAVEL: assert(fabs(properties.door.travel - edit.value) <= 0.5 / 65536.0); break;
    case SETUP_OBJECT_DOOR_CLEARANCE: assert(fabs(properties.door.clearance - edit.value) <= 0.5 / 65536.0); break;
    case SETUP_OBJECT_DOOR_ACCEL: assert(fabs(properties.door.accel - edit.value) <= 0.5 / 65536.0); break;
    case SETUP_OBJECT_DOOR_DECEL: assert(fabs(properties.door.decel - edit.value) <= 0.5 / 65536.0); break;
    case SETUP_OBJECT_DOOR_SPEED: assert(fabs(properties.door.speed - edit.value) <= 0.5 / 65536.0); break;
    default: assert(0);
    }
    OnlyBytes(source, &setup, edit.sourceoffset + relative, length);
    assert(EditHistoryCommitEdit(&history, &bg, &setup, &stan, &tx, &why));
    RoundTrip(dir, &setup); EditHistoryMarkSetupSaved(&history, &setup);
    assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && !changed && !setup.dirty);
    assert(EditHistoryUndo(&history, &bg, &setup, &stan, &asset, &why) && setup.dirty);
    Same(&setup, source);
    assert(EditHistoryRedo(&history, &bg, &setup, &stan, &asset, &why) && !setup.dirty);
    RoundTrip(dir, &setup);
    EditHistoryFree(&history); SetupFileFree(&setup);
}

static void CheckKeysAndAmmo(const char *dir, const SetupFile *source)
{
    DWORD key = FindType(source, PROPDEF_KEY), magazine = FindType(source, PROPDEF_MAGAZINE);
    DWORD crate = FindType(source, PROPDEF_AMMO), prop = FindType(source, PROPDEF_PROP);
    const DWORD masks[] = {0, 1, 0x80000000u, 0xffffffffu};
    SetupObjectPropertyEdit edit;
    for (unsigned int i = 0; i < sizeof(masks) / sizeof(*masks); i++)
    {
        edit = Request(source, key, SETUP_OBJECT_KEY_FLAGS, masks[i]);
        CheckSpecificEdit(dir, source, edit, 0x80, 4);
    }
    for (int type = AMMO_NONE; type < AMMOTYPE_MAX; type++)
    {
        edit = Request(source, magazine, SETUP_OBJECT_AMMO_TYPE, type);
        CheckSpecificEdit(dir, source, edit, 0x80, 4);
    }
    for (DWORD slot = 0; slot < AMMOTYPE_GLOBAL_MAX; slot++)
    {
        edit = Request(source, crate, SETUP_OBJECT_AMMO_QUANTITY, 65535); edit.slot = slot;
        CheckSpecificEdit(dir, source, edit, 0x82 + slot * 4, 2);
        edit.value = 0; CheckSpecificEdit(dir, source, edit, 0x82 + slot * 4, 2);
    }
    SetupFile setup = {0}; const char *why; BOOL changed;
    assert(SetupFileClone(source, &setup, &why));
    const double invalid[] = {-1, 0.5, NAN, INFINITY, 4294967296.0};
    for (int property = SETUP_OBJECT_KEY_FLAGS; property <= SETUP_OBJECT_AMMO_QUANTITY; property++)
    {
        DWORD index = property == SETUP_OBJECT_KEY_FLAGS ? key : property == SETUP_OBJECT_AMMO_TYPE ? magazine : crate;
        for (unsigned int i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
        {
            edit = Request(&setup, index, property, invalid[i]);
            assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && !changed && !setup.dirty);
            Same(&setup, source);
        }
        edit = Request(&setup, prop, property, 0);
        assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why)); Same(&setup, source);
    }
    edit = Request(&setup, magazine, SETUP_OBJECT_AMMO_TYPE, AMMOTYPE_MAX);
    assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why));
    edit = Request(&setup, crate, SETUP_OBJECT_AMMO_QUANTITY, 65536);
    assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why));
    edit.value = 1; edit.slot = AMMOTYPE_GLOBAL_MAX;
    assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why));
    edit.slot = (DWORD)-1;
    assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why));
    assert(!setup.dirty); Same(&setup, source); SetupFileFree(&setup);
    puts("PASS: 32-bit keys, every ammo type and all 12 quantity slots; native byte preservation, save/reload, undo/redo and invalid edits.");
}

static void CheckDoors(const char *dir, const SetupFile *source)
{
    DWORD door = FindType(source, PROPDEF_DOOR), prop = FindType(source, PROPDEF_PROP);
    SetupObjectPropertyEdit edit;
    /* Native signed 16.16 offsets, including clearance beyond full travel. */
    for (int property = SETUP_OBJECT_DOOR_TRAVEL; property <= SETUP_OBJECT_DOOR_SPEED; property++)
    {
        double values[] = {1.0 / 65536.0, 0.1, 90, 1000, 2147483647.0 / 65536.0};
        for (unsigned int i = 0; i < sizeof(values) / sizeof(*values); i++)
        {
            edit = Request(source, door, property, values[i]);
            CheckSpecificEdit(dir, source, edit, 0x84 + (property - SETUP_OBJECT_DOOR_TRAVEL) * 4, 4);
        }
        if (property != SETUP_OBJECT_DOOR_ACCEL && property != SETUP_OBJECT_DOOR_DECEL)
        {
            edit.value = 0;
            CheckSpecificEdit(dir, source, edit, 0x84 + (property - SETUP_OBJECT_DOOR_TRAVEL) * 4, 4);
        }
    }
    for (int type = DOORTYPE_SLIDING; type <= DOORTYPE_AZTECCHAIR; type++)
    { CheckSpecificEdit(dir, source, Request(source, door, SETUP_OBJECT_DOOR_TYPE, type), 0x9a, 2); }
    for (int sound = DOOR_OPEN_SOUND_NONE; sound <= DOOR_OPEN_SOUND_METAL_4; sound++)
    { CheckSpecificEdit(dir, source, Request(source, door, SETUP_OBJECT_DOOR_SOUND, sound), 0xa4, 4); }
    const double delays[] = {0, 1.0 / 60.0, 0.1, 15, 25, 268435455.0 / 60.0, 2147483647.0 / 60.0};
    for (unsigned int i = 0; i < sizeof(delays) / sizeof(*delays); i++)
    { CheckSpecificEdit(dir, source, Request(source, door, SETUP_OBJECT_DOOR_CLOSE_DELAY, delays[i]), 0xa0, 4); }
    const DWORD masks[] = {0, 1, 0x80000000u, 0xffffffffu};
    for (unsigned int i = 0; i < sizeof(masks) / sizeof(*masks); i++)
    { CheckSpecificEdit(dir, source, Request(source, door, SETUP_OBJECT_DOOR_KEY_FLAGS, masks[i]), 0x9c, 4); }
    for (int bit = 0; bit < 4; bit++)
    {
        DWORD original = Read32(source->data + source->objects[door].sourceoffset + 0x98) >> 16;
        /* All unknown flag bits and the adjacent movement type survive toggles. */
        CheckSpecificEdit(dir, source, Request(source, door, SETUP_OBJECT_DOOR_FLAGS, original ^ (1u << bit)), 0x98, 2);
    }
    SetupFile setup = {0}; const char *why; BOOL changed;
    assert(SetupFileClone(source, &setup, &why));
    const double invalid[] = {-1, NAN, INFINITY, 4294967296.0};
    for (int property = SETUP_OBJECT_DOOR_TRAVEL; property <= SETUP_OBJECT_DOOR_KEY_FLAGS; property++)
    {
        for (unsigned int i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
        {
            edit = Request(&setup, door, property, invalid[i]);
            assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && !changed && !setup.dirty);
            Same(&setup, source);
        }
        edit = Request(&setup, prop, property, 1);
        assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why)); Same(&setup, source);
    }
    const struct { SetupObjectProperty property; double value; } bad[] = {
        {SETUP_OBJECT_DOOR_ACCEL, 0}, {SETUP_OBJECT_DOOR_DECEL, 0},
        {SETUP_OBJECT_DOOR_ACCEL, 0.1 / 65536.0}, {SETUP_OBJECT_DOOR_DECEL, 0.1 / 65536.0},
        {SETUP_OBJECT_DOOR_TRAVEL, 32768}, {SETUP_OBJECT_DOOR_CLEARANCE, 32768},
        {SETUP_OBJECT_DOOR_SPEED, 32768}, {SETUP_OBJECT_DOOR_CLOSE_DELAY, 2147483648.0 / 60.0},
        {SETUP_OBJECT_DOOR_TYPE, 10}, {SETUP_OBJECT_DOOR_TYPE, 1.5},
        {SETUP_OBJECT_DOOR_SOUND, 18}, {SETUP_OBJECT_DOOR_SOUND, 1.5},
        {SETUP_OBJECT_DOOR_FLAGS, 65536}, {SETUP_OBJECT_DOOR_FLAGS, 1.5},
        {SETUP_OBJECT_DOOR_KEY_FLAGS, 1.5}
    };
    for (unsigned int i = 0; i < sizeof(bad) / sizeof(*bad); i++)
    {
        edit = Request(&setup, door, bad[i].property, bad[i].value);
        assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && !changed && !setup.dirty);
        Same(&setup, source);
    }
    SetupFileFree(&setup);
    puts("PASS: all door types/sound presets, motion units, close delays, lock masks and flags; exact native offsets, save/reload, undo/redo, no-ops and invalid edits.");
}

int main(int argc, char **argv)
{
    SetupFile source = {0}, setup = {0}; const char *why;
    assert(argc == 2 && SetupLoadProjectFile(argv[1], "UsetuppropertiesZ", &source, &why));
    assert(source.objectcount == 21 && source.charactercount == 1);
    CheckKeysAndAmmo(argv[1], &source);
    CheckDoors(argv[1], &source);
    for (DWORD index = 0; index < source.objectcount; index++)
    {
        SetupObjectProperties view;
        SetupObjectPropertyEdit edit;
        BOOL changed;
        DWORD offset = source.objects[index].sourceoffset;
        assert(SetupFileClone(&source, &setup, &why));
        assert(SetupFileGetObjectProperties(&setup, index, &view, &why));
        assert(view.health == 1000.25 && view.object.modelid == 15);
        assert(strcmp(SetupObjectTypeName(view.object.type), "Unknown object"));
        edit = Request(&setup, index, SETUP_OBJECT_HEALTH, view.health);
        assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && !changed && !setup.dirty);
        Same(&setup, &source);
        edit.value = 512.5;
        assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && changed && setup.dirty);
        assert(Read32(setup.data + offset + 0x74) == 0x02008000);
        assert(Read32(setup.data + offset + 0x70) == 0xABCDEF01); /* Never maxdamage. */
        OnlyBytes(&source, &setup, offset + 0x74, 4);
        RoundTrip(argv[1], &setup);
        SetupFileFree(&setup);
        assert(SetupFileClone(&source, &setup, &why));
        edit = Request(&setup, index, SETUP_OBJECT_MODEL, 15);
        assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && !changed && !setup.dirty);
        edit.value = 8;
        assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && changed && setup.dirty);
        assert(setup.objects[index].modelid == 8 && view.object.type == setup.objects[index].type);
        OnlyBytes(&source, &setup, offset + 4, 2);
        RoundTrip(argv[1], &setup); SetupFileFree(&setup);
    }
    puts("PASS: all 21 ObjectRecord types; health/model edits preserve runtime and specialized fields, flags, pads and command indices through save/reload.");

    assert(SetupFileClone(&source, &setup, &why));
    SetupObjectPropertyEdit edit = Request(&setup, 0, SETUP_OBJECT_HEALTH, 0.1);
    SetupObjectProperties view; BOOL changed;
    assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why));
    assert(SetupFileGetObjectProperties(&setup, 0, &view, &why));
    assert(fabs(view.health - 0.1) <= 0.5 / 65536.0);
    assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && !changed);
    edit.value = 2147483647.0 / 65536.0;
    assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why));
    assert(Read32(setup.data + edit.sourceoffset + 0x74) == 0x7FFFFFFF);
    edit.value = 0;
    assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why));
    assert(Read32(setup.data + edit.sourceoffset + 0x74) == 0);
    SetupFileFree(&setup);

    /* Invalid edits cannot dirty the asset or touch another subtype/character. */
    const double badhealth[] = {-1, 32768, NAN, INFINITY, -INFINITY};
    const double badmodel[] = {-1, 0.5, 32767, 32768, NAN, INFINITY};
    assert(SetupFileClone(&source, &setup, &why));
    for (unsigned int i = 0; i < sizeof(badhealth) / sizeof(*badhealth); i++)
    {
        edit = Request(&setup, 0, SETUP_OBJECT_HEALTH, badhealth[i]);
        assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && !changed && !setup.dirty); Same(&setup, &source);
    }
    for (unsigned int i = 0; i < sizeof(badmodel) / sizeof(*badmodel); i++)
    {
        edit = Request(&setup, 0, SETUP_OBJECT_MODEL, badmodel[i]);
        assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && !changed && !setup.dirty); Same(&setup, &source);
    }
    edit = Request(&setup, 0, SETUP_OBJECT_HEALTH, 42);
    edit.type = 3;
    assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why)); Same(&setup, &source);
    edit.type = 1; edit.sourceoffset++;
    assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why)); Same(&setup, &source);
    edit = Request(&setup, 0, SETUP_OBJECT_HEALTH, 42); edit.objectindex = SETUP_CHARACTER_SELECTION_BIT;
    assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why)); Same(&setup, &source);
    edit.objectindex = setup.objectcount;
    assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why)); Same(&setup, &source);
    edit = Request(&setup, 0, (SetupObjectProperty)999, 42);
    assert(!SetupFileSetObjectProperty(&setup, &edit, &changed, &why)); Same(&setup, &source);
    DWORD oldsize = setup.size;
    setup.size = edit.sourceoffset + 0x80;
    assert(!SetupFileGetObjectProperties(&setup, 0, &view, &why)); /* Door suffix truncated. */
    setup.size = oldsize;
    setup.objects[0].deleted = TRUE;
    assert(!SetupFileGetObjectProperties(&setup, 0, &view, &why));
    setup.objects[0].deleted = FALSE;
    Same(&setup, &source); SetupFileFree(&setup);
    puts("PASS: native quantization/range, nonfinite values, unknown models, stale requests, deleted objects and truncated records.");

    /* Undo includes selections; save revisions stay independent. */
    EditHistory history = {0}; EditHistoryTransaction tx; EditHistoryAsset asset;
    BgDocument bg = {0}; StanFile stan = {0}; DWORD selection = 3;
    assert(SetupFileClone(&source, &setup, &why));
    EditHistoryReset(&history, &bg, &setup, &stan);
    assert(EditHistorySetSelection(&history, &selection, sizeof(selection), FALSE, &why));
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Change Object Health", &tx, &why));
    edit = Request(&setup, 3, SETUP_OBJECT_HEALTH, 42.25);
    assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && changed);
    assert(EditHistoryCommitEdit(&history, &bg, &setup, &stan, &tx, &why));
    assert(setup.dirty && !bg.dirty && !stan.dirty);
    RoundTrip(argv[1], &setup); EditHistoryMarkSetupSaved(&history, &setup);
    assert(!setup.dirty);
    selection = 4;
    assert(EditHistorySetSelection(&history, &selection, sizeof(selection), TRUE, &why));
    assert(EditHistoryUndo(&history, &bg, &setup, &stan, &asset, &why) && asset == EDIT_HISTORY_ASSET_SELECTION && !setup.dirty);
    assert(EditHistoryUndo(&history, &bg, &setup, &stan, &asset, &why) && asset == EDIT_HISTORY_ASSET_SETUP && setup.dirty);
    Same(&setup, &source); assert(*(DWORD *)history.selection == 3);
    assert(EditHistoryRedo(&history, &bg, &setup, &stan, &asset, &why) && !setup.dirty);
    assert(SetupFileGetObjectProperties(&setup, 3, &view, &why) && view.health == 42.25);
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Change Object Model", &tx, &why));
    edit = Request(&setup, 3, SETUP_OBJECT_MODEL, 8);
    assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why));
    EditHistoryRollbackEdit(&tx, &bg, &setup, &stan);
    assert(setup.objects[3].modelid == 15 && !setup.dirty);
    RoundTrip(argv[1], &setup);
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Change Object Model", &tx, &why));
    assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && changed);
    assert(EditHistoryCommitEdit(&history, &bg, &setup, &stan, &tx, &why));
    assert(EditHistoryUndo(&history, &bg, &setup, &stan, &asset, &why));
    assert(setup.objects[3].modelid == 15 && !setup.dirty);
    assert(EditHistoryRedo(&history, &bg, &setup, &stan, &asset, &why));
    assert(setup.objects[3].modelid == 8 && setup.dirty);
    RoundTrip(argv[1], &setup);
    EditHistoryFree(&history); SetupFileFree(&setup); SetupFileFree(&source);
    puts("PASS: mixed selection/property undo-redo, saved revisions and failed-preview rollback.");
    return 0;
}
