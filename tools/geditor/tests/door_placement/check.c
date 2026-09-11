#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"
#include "bghistory.h"
#include "modelload.h"
#include <src/propconstants.h>

void BgDocumentFree(BgDocument *d) { memset(d, 0, sizeof(*d)); }
void StanFileFree(StanFile *d) { memset(d, 0, sizeof(*d)); }
static DWORD Read(const unsigned char *p)
{ return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3]; }
static void Write(unsigned char *p, DWORD v)
{ p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void Near(double a, double b) { assert(fabs(a-b) < .0003); }
static void Same(const SetupFile *a, const SetupFile *b)
{
    assert(a->size == b->size && a->objectcount == b->objectcount && a->padcount == b->padcount
        && a->boundpadcount == b->boundpadcount && a->charactercount == b->charactercount);
    assert(!memcmp(a->data, b->data, a->size));
    assert(!memcmp(a->pads, b->pads, a->padcount * sizeof(*a->pads)));
    assert(!memcmp(a->boundpads, b->boundpads, a->boundpadcount * sizeof(*a->boundpads)));
}
static void RoundTrip(const char *dir, const SetupFile *setup)
{
    SetupFile saved = {0}; const char *why;
    assert(SetupSaveProjectFile(dir, setup, &why));
    assert(SetupLoadProjectFile(dir, setup->name, &saved, &why));
    Same(setup, &saved); assert(!saved.dirty); SetupFileFree(&saved);
}
static void OldReferences(const SetupFile *source, const SetupFile *placed)
{
    DWORD target;
    for (int i = 0; i < 40; i += 4)
    {
        if (i != 12 && i != 28) { assert(Read(source->data+i) == Read(placed->data+i)); }
    }
    /* All original payloads stay at their file-relative offsets; command and
     * bound lists are copied byte-for-byte without shifting native indices. */
    assert(!memcmp(source->data + 40, placed->data + 40, source->size - 40));
    DWORD commands = Read(source->data+12), end = source->objects[1].sourceoffset + 256 + 28 + 28;
    assert(!memcmp(source->data + commands, placed->data + Read(placed->data+12), end - commands));
    assert(!memcmp(source->data + Read(source->data+28), placed->data + Read(placed->data+28), 68));
    assert(SetupObjectRelativeTarget(placed, placed->objects[0].sourceoffset, 2, &target) && target == 1);
    assert(SetupObjectRelativeTarget(placed, placed->objects[1].sourceoffset, -2, &target) && target == 0);
    assert(!memcmp(source->pads, placed->pads, sizeof(*source->pads)));
    assert(!memcmp(source->boundpads, placed->boundpads, sizeof(*source->boundpads)));
}
static void Place(const char *dir, const SetupFile *source, int model, float scale, const double facing[3], BOOL mp)
{
    SetupFile setup = {0}, before = {0}, placed = {0}; const char *why;
    EditHistory history = {0}; EditHistoryTransaction tx; EditHistoryAsset asset;
    BgDocument bg = {0}; StanFile stan = {0};
    double position[3] = {300, -20, -400}; DWORD selection;
    assert(SetupFileClone(source, &setup, &why));
    if (mp) { strcpy(setup.name, "Ump_setupdoorsZ"); }
    assert(SetupFileClone(&setup, &before, &why));
    EditHistoryReset(&history, &bg, &setup, &stan);
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Add Door", &tx, &why));
    assert(SetupFileAddDoor(&setup, model, scale, position, facing, &selection, &why));
    assert(selection == source->objectcount && setup.objectcount == source->objectcount + 1);
    assert(setup.padcount == source->padcount && setup.boundpadcount == source->boundpadcount + 1);
    assert(setup.charactercount == source->charactercount && setup.dirty);
    OldReferences(source, &setup);
    const SetupObject *door = &setup.objects[selection];
    assert(door->type == PROPDEF_DOOR && door->modelid == model && door->pad == 1 && door->extrascale == 256);
    assert(door->flags == (PROPFLAG_FORCE_COLLISIONS | PROPFLAG_NO_PORTAL_CLOSE));
    assert(door->flags2 == 0 && !door->nonsolid && !door->deleted);
    SetupObjectProperties properties;
    assert(SetupFileGetObjectProperties(&setup, selection, &properties, &why));
    assert(properties.health == 1000 && !properties.keyflags && properties.door.type == DOORTYPE_SLIDING);
    assert(properties.door.travel == 1 && properties.door.clearance > .94 && properties.door.clearance < 1);
    assert(properties.door.accel > 0 && properties.door.decel > 0 && properties.door.speed > 0);
    assert(properties.door.closeframes == 300 && properties.door.sound == DOOR_OPEN_SOUND_METAL);
    DWORD record = door->sourceoffset;
    assert(Read(setup.data + record + 256) == 48); /* Full 64-word command, then native terminator. */
    for (DWORD at = record + 0xa8; at < record + 256; at += 4) { assert(!Read(setup.data + at)); }
    assert(!Read(setup.data + record + 0x80)); /* No accidental linked door. */
    const SetupBoundPad *pad = &setup.boundpads[door->pad];
    float corners[8][3];
    SetupPadGetBoxCorners(&pad->pad, pad->xmin,pad->xmax,pad->ymin,pad->ymax,pad->zmin,pad->zmax,1/scale,corners);
    for (int axis = 0; axis < 3; axis++) { Near(pad->pad.pos[axis]/scale, position[axis]); }
    Near((pad->xmax-pad->xmin)/scale,12); Near((pad->ymax-pad->ymin)/scale,100); Near((pad->zmax-pad->zmin)/scale,200);
    for (int i = 0; i < 8; i++) { Near(corners[i][1], position[1] + (i & 4 ? 200 : 0)); }
    double horizontal = hypot(facing[0], facing[2]);
    if (horizontal > 1e-8)
    {
        Near(pad->pad.up[0], -facing[2]/horizontal); Near(pad->pad.up[2], facing[0]/horizontal);
        /* Model +Z = pad up x look faces the viewer. */
        Near(-pad->pad.up[2]*facing[0]+pad->pad.up[0]*facing[2], -horizontal);
    }
    else { Near(pad->pad.up[0],1); Near(pad->pad.up[2],0); }
    Near(pad->pad.look[1],1);
    DWORD padrecord = Read(setup.data+28)+68;
    DWORD plink = Read(setup.data+padrecord+36);
    assert(plink < setup.size && setup.data[plink] == 0 && !pad->pad.stanname[0]);
    assert(!Read(setup.data+padrecord+68+36)); /* Bound-pad null terminator. */
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why));
    RoundTrip(dir,&setup); EditHistoryMarkSetupSaved(&history,&setup);
    assert(SetupFileClone(&setup,&placed,&why));
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why) && setup.dirty); Same(&setup,&before);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why) && !setup.dirty); Same(&setup,&placed);
    /* New doors use the existing inspector, transforms and deletion path. */
    SetupObjectPropertyEdit edit = {0}; BOOL changed;
    edit.objectindex=selection; edit.sourceoffset=setup.objects[selection].sourceoffset; edit.type=PROPDEF_DOOR;
    edit.property=SETUP_OBJECT_DOOR_KEY_FLAGS; edit.value=0x80000001u;
    assert(SetupFileSetObjectProperty(&setup,&edit,&changed,&why) && changed);
    const double offset[3]={10,20,30};
    assert(SetupFileTranslateModel(&setup,selection,scale,offset,&why));
    SetupPadRef ref; Rotation rotation;
    assert(SetupFileGetModelPad(&setup,selection,&ref) && ref.bound);
    RotationAxis(&rotation,1,0.5);
    assert(SetupFileRotatePad(&setup,&ref,&rotation,&changed,&why));
    const double bounds[6]={-10,10,-70,70,0,240};
    assert(SetupFileSetModelBounds(&setup,selection,scale,bounds,&why));
    RoundTrip(dir,&setup);
    DWORD second;
    assert(SetupFileAddDoor(&setup,model,scale,position,facing,&second,&why) && second == selection+1);
    assert(setup.objects[second].pad != setup.objects[selection].pad);
    assert(SetupFileDeleteObject(&setup,second,&why) && setup.objects[second].deleted);
    RoundTrip(dir,&setup);
    /* The shared append helper must still create ordinary props/characters. */
    assert(SetupFileAddModel(&setup,FALSE,model,scale,position,&second,&why));
    assert(setup.objects[second].type == PROPDEF_PROP && setup.objects[second].pad == 1);
    assert(SetupFileAddModel(&setup,TRUE,0,scale,position,&second,&why));
    assert(second & SETUP_CHARACTER_SELECTION_BIT);
    assert(setup.characters[second & ~SETUP_CHARACTER_SELECTION_BIT].pad == 2);
    RoundTrip(dir,&setup);
    SetupFileFree(&setup); SetupFileFree(&before); SetupFileFree(&placed); EditHistoryFree(&history);
}
int main(int argc,char **argv)
{
    SetupFile source={0},setup={0}; const char *why,*name; int model=-1; DWORD selection;
    assert(argc==2 && SetupLoadProjectFile(argv[1],"UsetupdoorsZ",&source,&why));
    for(int i=0;ModelGetPropDefinition(i,&name,NULL);i++) { if(!strcmp(name,SETUP_DEFAULT_DOOR_MODEL)) { model=i;break; } }
    assert(model>=0);
    const float scales[]={.15019713f,.53931433f,1.20648f,1};
    const double facing[][3]={{0,0,-1},{1,.5,1},{-1,0,0},{0,-1,0}};
    for(int mp=0;mp<2;mp++) for(int scale=0;scale<4;scale++) for(int face=0;face<4;face++)
    { Place(argv[1],&source,model,scales[scale],facing[face],mp); }
    puts("PASS: solo/MP door creation, level-scaled dimensions, floor anchors, orientation, native defaults, original command/pad links, save/reload and undo/redo.");
    puts("PASS: placed-door properties, translation, rotation, bounds, repeated placement, deletion and ordinary prop/character creation.");
    const double pos[3]={0,0,0},bad[3]={NAN,0,0};
    assert(SetupFileClone(&source,&setup,&why));
    assert(!SetupFileAddDoor(&setup,model,0,pos,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddDoor(&setup,model,1,bad,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddDoor(&setup,model,1,pos,bad,&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddDoor(&setup,-1,1,pos,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddDoor(&setup,model,INFINITY,pos,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddDoor(&setup,model,1e-30f,pos,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddDoor(&setup,model,1,NULL,facing[0],&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddDoor(&setup,model,1,pos,NULL,&selection,&why)); Same(&setup,&source);
    assert(!SetupFileAddDoor(&setup,model,1,pos,facing[0],NULL,&why)); Same(&setup,&source);
    DWORD oldcount=setup.boundpadcount;
    setup.boundpadcount=32768; assert(!SetupFileAddDoor(&setup,model,1,pos,facing[0],&selection,&why)); setup.boundpadcount=oldcount;
    Same(&setup,&source);
    DWORD old=Read(setup.data+12);
    Write(setup.data+12,setup.size+4); assert(!SetupFileAddDoor(&setup,model,1,pos,facing[0],&selection,&why)); Write(setup.data+12,old);
    Same(&setup,&source); assert(!setup.dirty);
    /* A legal NULL propDefs list can gain its first door. */
    Write(setup.data+12,0); free(setup.objects); setup.objects=NULL; setup.objectcount=0;
    free(setup.characters); setup.characters=NULL; setup.charactercount=0;
    assert(SetupFileAddDoor(&setup,model,1,pos,facing[0],&selection,&why) && selection==0 && setup.objectcount==1);
    RoundTrip(argv[1],&setup);
    SetupFileFree(&setup); SetupFileFree(&source);
    puts("PASS: invalid placement/range/table requests are atomic; empty object lists work.");
    return 0;
}
