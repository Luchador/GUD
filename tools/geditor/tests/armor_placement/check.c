#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "setupload.h"
#include "bghistory.h"
#include "modelload.h"
#include <src/propconstants.h>

#define _mkword(hi, lo) (((DWORD)(hi) << 16) | (unsigned short)(lo))
#define _mkshort(hi, lo) (((hi) << 8) | (lo))
#include "stock.h"

void BgDocumentFree(BgDocument *d) { memset(d, 0, sizeof(*d)); }
void StanFileFree(StanFile *d) { memset(d, 0, sizeof(*d)); }
static DWORD Read(const unsigned char *p)
{ return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3]; }
static void Same(const SetupFile *a, const SetupFile *b)
{
    assert(a->size == b->size && a->objectcount == b->objectcount && a->padcount == b->padcount
        && a->boundpadcount == b->boundpadcount && a->charactercount == b->charactercount);
    assert(!memcmp(a->data, b->data, a->size));
}
static void RoundTrip(const char *dir, const SetupFile *setup)
{
    SetupFile saved = {0}; const char *why;
    /* ROM export copies these native .set bytes into the setup resource. */
    assert(SetupSaveProjectFile(dir, setup, &why));
    assert(SetupLoadProjectFile(dir, setup->name, &saved, &why));
    Same(setup, &saved); assert(!saved.dirty); SetupFileFree(&saved);
}
static int ArmorModel(void)
{
    const char *name;
    for (int i = 0; ModelGetPropDefinition(i, &name, NULL); i++)
    { if (!strcmp(name, SETUP_DEFAULT_ARMOR_MODEL)) { return i; } }
    assert(0); return -1;
}
static void Place(const char *dir, const SetupFile *source, float scale, BOOL mp)
{
    SetupFile setup = {0}, before = {0}, placed = {0}; const char *why;
    EditHistory history = {0}; EditHistoryTransaction tx; EditHistoryAsset asset;
    BgDocument bg = {0}; StanFile stan = {0}; SetupObjectProperties properties;
    const double position[3] = {300, 180, -400}; DWORD selection, target;
    assert(SetupFileClone(source, &setup, &why));
    if (mp) { strcpy(setup.name, "Ump_setuparmorZ"); }
    assert(SetupFileClone(&setup, &before, &why));
    EditHistoryReset(&history, &bg, &setup, &stan);
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Add Armor", &tx, &why));
    assert(SetupFileAddArmor(&setup, ArmorModel(), scale, position, &selection, &why));
    assert(selection == source->objectcount && setup.objectcount == source->objectcount + 1);
    assert(setup.padcount == source->padcount + 1 && setup.boundpadcount == source->boundpadcount);
    assert(setup.charactercount == source->charactercount && setup.dirty);
    for (int at = 0; at < 40; at += 4)
    { if (at != 12 && at != 24) { assert(Read(setup.data + at) == Read(source->data + at)); } }
    assert(!memcmp(source->data + 40, setup.data + 40, source->size - 40));
    assert(SetupObjectRelativeTarget(&setup, setup.objects[0].sourceoffset, 2, &target) && target == 1);
    assert(SetupObjectRelativeTarget(&setup, setup.objects[1].sourceoffset, -2, &target) && target == 0);
    assert(SetupFileGetObjectProperties(&setup, selection, &properties, &why));
    DWORD record = properties.object.sourceoffset;
    assert(sizeof(stock) == 136);
    for (unsigned i = 0; i < sizeof(stock) / sizeof(*stock); i++)
    {
        DWORD expected = stock[i];
        if (i == 1) { expected = (expected & 0xffff0000u) | source->padcount; }
        assert(Read(setup.data + record + i * 4) == expected);
    }
    assert(properties.object.type == PROPDEF_ARMOUR && !properties.object.flags2);
    assert(Read(setup.data + record + sizeof(stock)) == PROPDEF_END);
    const SetupPad *pad = &setup.pads[source->padcount];
    for (int axis = 0; axis < 3; axis++) { assert(fabs(pad->pos[axis] / scale - position[axis]) < .0005); }
    assert(pad->up[1] == 1 && pad->look[2] == 1 && !pad->stanname[0]);
    DWORD pads = Read(setup.data + 24);
    DWORD link = Read(setup.data + pads + source->padcount * 44 + 36);
    assert(link && link < setup.size && !setup.data[link]);
    assert(!Read(setup.data + pads + setup.padcount * 44 + 36));
    assert(EditHistoryCommitEdit(&history, &bg, &setup, &stan, &tx, &why));
    RoundTrip(dir, &setup); EditHistoryMarkSetupSaved(&history, &setup);
    assert(SetupFileClone(&setup, &placed, &why));
    assert(EditHistoryUndo(&history, &bg, &setup, &stan, &asset, &why) && setup.dirty); Same(&setup, &before);
    assert(EditHistoryRedo(&history, &bg, &setup, &stan, &asset, &why) && !setup.dirty); Same(&setup, &placed);
    /* Shared transform/property editing must retain the pickup payload. */
    const double offset[3] = {10, 20, 30}; SetupPadRef ref; BOOL changed;
    Rotation rotation; RotationAxis(&rotation, 1, .5);
    assert(SetupFileTranslateModel(&setup, selection, scale, offset, &why));
    assert(SetupFileGetModelPad(&setup, selection, &ref));
    assert(SetupFileRotatePad(&setup, &ref, &rotation, &changed, &why) && changed);
    SetupObjectPropertyEdit edit = {0};
    edit.objectindex = selection; edit.sourceoffset = setup.objects[selection].sourceoffset;
    edit.type = PROPDEF_ARMOUR; edit.property = SETUP_OBJECT_HEALTH; edit.value = 250;
    assert(SetupFileSetObjectProperty(&setup, &edit, &changed, &why) && changed);
    record = setup.objects[selection].sourceoffset;
    assert(!memcmp(setup.data + record + 128, placed.data + placed.objects[selection].sourceoffset + 128, 8));
    assert(!memcmp(source->pads, setup.pads, sizeof(*source->pads) * source->padcount));
    RoundTrip(dir, &setup);
    DWORD second;
    assert(SetupFileAddArmor(&setup, ArmorModel(), scale, position, &second, &why) && second == selection + 1);
    assert(setup.objects[second].pad != setup.objects[selection].pad);
    assert(SetupFileDeleteObject(&setup, second, &why) && setup.objects[second].deleted);
    RoundTrip(dir, &setup);
    SetupFileFree(&setup); SetupFileFree(&before); SetupFileFree(&placed); EditHistoryFree(&history);
}

int main(int argc, char **argv)
{
    SetupFile source = {0}, setup = {0}; const char *why; DWORD selection;
    assert(argc == 2 && SetupLoadProjectFile(argv[1], "UsetuparmorZ", &source, &why));
    const float scales[] = {.15019713f, .53931433f, 1.20648f, 1};
    assert(ArmorModel() == 115);
    for (int mp = 0; mp < 2; mp++) for (int s = 0; s < 4; s++) { Place(argv[1], &source, scales[s], mp); }
    puts("PASS: armor matches stock game record; solo/MP placement, scales, links and private pads.");
    puts("PASS: native save/reload, undo/redo, transforms, health edits, repeated placement and deletion.");
    const double pos[3] = {0, 0, 0}, bad[3] = {NAN, 0, 0};
    int model = ArmorModel();
    assert(SetupFileClone(&source, &setup, &why));
    assert(!SetupFileAddArmor(&setup, model, 0, pos, &selection, &why)); Same(&setup, &source);
    assert(!SetupFileAddArmor(&setup, model, 1, bad, &selection, &why)); Same(&setup, &source);
    assert(!SetupFileAddArmor(&setup, -1, 1, pos, &selection, &why)); Same(&setup, &source);
    assert(!SetupFileAddArmor(&setup, model, 1, NULL, &selection, &why)); Same(&setup, &source);
    assert(!SetupFileAddArmor(&setup, model, 1, pos, NULL, &why)); Same(&setup, &source);
    DWORD count = setup.padcount; setup.padcount = 10000;
    assert(!SetupFileAddArmor(&setup, model, 1, pos, &selection, &why)); setup.padcount = count; Same(&setup, &source);
    assert(!setup.dirty); SetupFileFree(&setup);
    assert(SetupLoadProjectFile(argv[1], "UsetupemptyZ", &setup, &why));
    assert(!setup.objectcount && !setup.padcount);
    assert(SetupFileAddArmor(&setup, model, 1, pos, &selection, &why) && selection == 0 && setup.padcount == 1);
    RoundTrip(argv[1], &setup);
    SetupFileFree(&setup); SetupFileFree(&source);
    puts("PASS: invalid placement is atomic; an empty setup can gain its first armor pickup.");
    return 0;
}
