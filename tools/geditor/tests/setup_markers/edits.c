#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"
#include "bghistory.h"

/* These tests never create BG or stan history entries. */
void BgDocumentFree(BgDocument *document) { (void)document; abort(); }
void StanFileFree(StanFile *stan) { (void)stan; abort(); }
static DWORD Get(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Put(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void Float(unsigned char *p,float f) { union {float f;DWORD u;}v;v.f=f;Put(p,v.u); }
static void Near(double a,double b) { assert(fabs(a-b)<0.0002); }
static void Same(const SetupFile *a,const SetupFile *b) { assert(a->size==b->size&&!memcmp(a->data,b->data,a->size)); }

static void PadScaleSizes(const SetupFile *source, const char *dir)
{
    /* Train, Bunker, Facility, plus identity: promotion must not cause a
       size jump, and scaling a saved bound pad must not apply level scale again. */
    const float levels[] = {.15019713f, .53931433f, 1.20648f, 1.0f};
    for (unsigned int i = 0; i < sizeof(levels) / sizeof(*levels); i++)
    {
        SetupFile setup = {0}, saved = {0};
        SetupPadRef ref = {0, FALSE};
        Scaling scale = {0};
        const char *why = "";
        assert(SetupFileClone(source, &setup, &why));
        strcpy(setup.name, "UsetuppadscaleZ");
        RotationAxis(&scale.axes, 0, 0);
        scale.factor[0] = 2; scale.factor[1] = 3; scale.factor[2] = .5;
        assert(SetupFileScalePad(&setup, &ref, levels[i], &scale, &why) && ref.bound);
        assert(!memcmp(setup.pads, source->pads, source->padcount * sizeof(*source->pads)));
        assert(SetupSaveProjectFile(dir, &setup, &why));
        assert(SetupLoadProjectFile(dir, setup.name, &saved, &why)); Same(&setup, &saved);
        for (int pass = 1; pass <= 2; pass++)
        {
            const SetupBoundPad *pad = &saved.boundpads[ref.index];
            float corners[8][3];
            SetupPadGetBoxCorners(&pad->pad, pad->xmin, pad->xmax, pad->ymin, pad->ymax,
                pad->zmin, pad->zmax, 1 / levels[i], corners);
            for (int axis = 0; axis < 3; axis++)
            {
                double squared = 0;
                for (int component = 0; component < 3; component++)
                {
                    double difference = corners[1 << axis][component] - corners[0][component];
                    squared += difference * difference;
                }
                Near(sqrt(squared), 10 * pow(scale.factor[axis], pass));
                Near((corners[0][axis] + corners[7][axis]) * .5,
                    source->pads[0].pos[axis] / levels[i]);
            }
            if (pass == 1) { assert(SetupFileScalePad(&saved, &ref, levels[i], &scale, &why)); }
        }
        SetupFileFree(&saved); SetupFileFree(&setup);
    }
    puts("PASS: pad scale promotion, saved bounds and repeated scaling retain consistent world sizes.");
}

static DWORD IntroAt(const SetupFile *setup, DWORD index)
{
    const DWORD words[] = {3,4,4,8,2,2,10,3,2,1};
    DWORD at = Get(setup->data + 8);
    for (DWORD i = 0; i < index; i++)
    {
        DWORD type = Get(setup->data + at);
        assert(type < 9); at += 4 * words[type];
    }
    assert(at + 4 <= setup->size);
    return at;
}

static DWORD SpawnCount(const SetupFile *setup)
{
    DWORD count = 0;
    for (DWORD i = 0;; i++)
    {
        DWORD at = IntroAt(setup, i), type = Get(setup->data + at);
        if (type == 9) { return count; }
        if (type == 0 && Get(setup->data + at + 8) == 0) { count++; }
    }
}

static void SpawnEdits(const SetupFile *source, const char *dir)
{
    SetupFile setup = {0}, before = {0}, saved = {0};
    SetupMarkerRef selected = {SETUP_MARKER_SPAWN, 0};
    SetupMarker *markers = NULL, oldstart;
    SetupSwirlPath oldpath = {0}, newpath = {0};
    EditHistory history = {0}; EditHistoryTransaction transaction = {0};
    EditHistoryAsset asset; BgDocument bg = {0}; StanFile stan = {0};
    DWORD count, at, pads;
    double position[3] = {300, 20, -400};
    const char *why = "";
    assert(SetupFileClone(source, &setup, &why));
    assert(SetupFileBuildMarkers(&setup, .5f, &markers, &count, &why));
    oldstart = markers[0]; free(markers);
    assert(SetupFileBuildSwirlPath(&setup, &oldstart, &oldpath, &why));
    assert(!SetupFileDeleteSpawn(&setup, &selected, &why) && strstr(why, "at least one")); Same(&setup, source);
    EditHistoryReset(&history, &bg, &setup, &stan);
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Place Player Start", &transaction, &why));
    assert(SetupFilePlaceSpawn(&setup, .5f, position, &selected, &why));
    assert(selected.command == 0 && SpawnCount(&setup) == 1 && setup.padcount == source->padcount + 1);
    assert(!memcmp(setup.pads, source->pads, source->padcount * sizeof(*source->pads)));
    assert(SetupFileBuildMarkers(&setup, .5f, &markers, &count, &why) && count == 3);
    for (int axis = 0; axis < 3; axis++)
    { Near(markers[0].position[axis], position[axis]); Near(markers[0].look[axis], oldstart.look[axis]); }
    assert(SetupFileBuildSwirlPath(&setup, &markers[0], &newpath, &why)); free(markers);
    assert(newpath.pointcount == oldpath.pointcount);
    for (DWORD i = 0; i < oldpath.pointcount; i++)
    for (int axis = 0; axis < 3; axis++)
    {
        Near(newpath.points[i].position[axis] - oldpath.points[i].position[axis], position[axis] - oldstart.position[axis]);
        Near(newpath.points[i].look[axis], oldpath.points[i].look[axis]);
    }
    /* Cameras and the entire authored swirl block are copied verbatim. */
    assert(!memcmp(setup.data + IntroAt(&setup, 1), source->data + IntroAt(source, 1), 232));
    assert(EditHistoryCommitEdit(&history, &bg, &setup, &stan, &transaction, &why));
    assert(SetupFileClone(&setup, &saved, &why));
    assert(EditHistoryUndo(&history, &bg, &setup, &stan, &asset, &why)); Same(&setup, source);
    assert(EditHistoryRedo(&history, &bg, &setup, &stan, &asset, &why)); Same(&setup, &saved);
    SetupFileFree(&saved);
    assert(SetupSaveProjectFile(dir, &setup, &why));
    assert(SetupLoadProjectFile(dir, setup.name, &saved, &why)); Same(&setup, &saved); SetupFileFree(&saved);
    assert(SetupFileClone(&setup, &before, &why));
    { double bad[3] = {INFINITY, 0, 0};
      assert(!SetupFilePlaceSpawn(&setup, .5f, bad, &selected, &why)); Same(&setup, &before); }
    SetupFileFree(&before);
    SetupSwirlPathFree(&oldpath); SetupSwirlPathFree(&newpath); EditHistoryFree(&history);

    /* Multiplayer appends private pads, keeps authored starts, and never
       allows the game's fixed 16-entry start array to overflow. */
    strcpy(setup.name, "Ump_setupspawnZ");
    for (DWORD expected = 2; expected <= 16; expected++)
    {
        assert(SetupFilePlaceSpawn(&setup, .5f, position, &selected, &why));
        assert(SpawnCount(&setup) == expected);
    }
    assert(SetupFileClone(&setup, &before, &why));
    assert(!SetupFilePlaceSpawn(&setup, .5f, position, &selected, &why) && strstr(why, "16")); Same(&setup, &before);
    SetupFileFree(&before);
    assert(SetupSaveProjectFile(dir, &setup, &why));
    assert(SetupLoadProjectFile(dir, setup.name, &saved, &why)); Same(&setup, &saved); SetupFileFree(&saved);
    EditHistoryReset(&history, &bg, &setup, &stan);
    assert(SetupFileClone(&setup, &before, &why));
    assert(EditHistoryBeginSetupEdit(&history, &setup, "Delete Spawn Point", &transaction, &why));
    selected.command = 0; pads = setup.padcount;
    assert(SetupFileDeleteSpawn(&setup, &selected, &why) && SpawnCount(&setup) == 15 && setup.padcount == pads);
    assert(!memcmp(setup.pads, before.pads, pads * sizeof(*setup.pads)));
    assert(Get(setup.data + IntroAt(&setup, 0)) == 6); /* camera shifts into command zero */
    assert(!memcmp(setup.data + IntroAt(&setup, 0), before.data + IntroAt(&before, 1), 232));
    assert(EditHistoryCommitEdit(&history, &bg, &setup, &stan, &transaction, &why));
    assert(SetupFileClone(&setup, &saved, &why));
    assert(EditHistoryUndo(&history, &bg, &setup, &stan, &asset, &why)); Same(&setup, &before);
    assert(EditHistoryRedo(&history, &bg, &setup, &stan, &asset, &why)); Same(&setup, &saved);
    SetupFileFree(&before); SetupFileFree(&saved); EditHistoryFree(&history);

    /* An authored demo start must survive solo replacement. Convert one
       of the extra fixture starts to demo slot 1, then collapse normal starts. */
    at = IntroAt(&setup, 7); assert(Get(setup.data + at) == 0);
    Put(setup.data + at + 8, 1);
    selected.command = 7;
    assert(SetupFileClone(&setup, &before, &why));
    assert(!SetupFileDeleteSpawn(&setup, &selected, &why)); Same(&setup, &before);
    strcpy(setup.name, "UsetupspawnZ");
    assert(SetupFilePlaceSpawn(&setup, .5f, position, &selected, &why) && SpawnCount(&setup) == 1);
    at = IntroAt(&setup, 7); assert(Get(setup.data + at) == 0 && Get(setup.data + at + 8) == 1);
    assert(!memcmp(setup.data + at, before.data + IntroAt(&before, 7), 12));
    assert(!SetupFileDeleteSpawn(&setup, &selected, &why) && SpawnCount(&setup) == 1);
    SetupFileFree(&before);
    assert(SetupSaveProjectFile(dir, &setup, &why));
    assert(SetupLoadProjectFile(dir, setup.name, &saved, &why)); Same(&setup, &saved); SetupFileFree(&saved);
    SetupFileFree(&setup);

    /* Dam-style swirl room hints must no longer refer to the old area. */
    assert(SetupFileClone(source, &setup, &why));
    Put(setup.data + IntroAt(&setup, 2) + 28, 1);
    assert(SetupFilePlaceSpawn(&setup, .5f, position, &selected, &why));
    assert(Get(setup.data + IntroAt(&setup, 2) + 28) == 0xffffffffu);
    assert(!memcmp(setup.pads, source->pads, source->padcount * sizeof(*source->pads)));
    SetupFileFree(&setup);

    /* A valid setup with no intro can acquire its first spawn. */
    assert(SetupFileClone(source, &setup, &why)); Put(setup.data + 8, 0);
    assert(SetupFilePlaceSpawn(&setup, .5f, position, &selected, &why) && selected.command == 0 && SpawnCount(&setup) == 1);
    assert(Get(setup.data + IntroAt(&setup, 1)) == 9);
    SetupFileFree(&setup);
    puts("PASS: solo replacement/swirl following, multiplayer add/delete and limits, demo/shared-pad preservation, saved assets and undo/redo.");
}

void MarkerEdits(const char *dir)
{
    unsigned char bytes[524]={0};char file[MAX_PATH+64];FILE *f;
    SetupFile setup={0},before={0},saved={0};const char *why="";
    SetupMarker *markers=NULL,anchor={0};DWORD count,padcount,size;BOOL changed;
    SetupMarkerRef start={SETUP_MARKER_SPAWN,0},intro={SETUP_MARKER_INTRO,1};
    SetupMarkerRef outro={SETUP_MARKER_OUTRO,0},swirl={SETUP_MARKER_SWIRL,3};
    Rotation rotation;double move[3]={3,4,5};
    EditHistory history={0};EditHistoryTransaction transaction={0};EditHistoryAsset asset;
    BgDocument bg={0};StanFile stan={0};
    Put(bytes+8,40);Put(bytes+12,288);Put(bytes+24,320);Put(bytes+28,452);
    Put(bytes+40,0);Put(bytes+44,0);
    Put(bytes+52,6);Put(bytes+56,1200);Put(bytes+60,(DWORD)-500);Put(bytes+64,800);
    for(DWORD i=0;i<6;i++)
    {
        unsigned char *p=bytes+92+i*32;Put(p,3);Put(p+4,i==5?1:2);
        Put(p+8,i*10*65536);Put(p+12,i==2?10*65536:123);Put(p+20,32768);Put(p+24,20*65536);Put(p+28,0xffffffff);
    }
    Put(bytes+284,9);Put(bytes+288,46);Put(bytes+292,100);Put(bytes+296,200);Put(bytes+300,300);Put(bytes+316,48);
    for(DWORD i=0;i<2;i++)
    {
        unsigned char *p=bytes+320+i*44;Float(p,10);Float(p+4,20);Float(p+8,30);
        Float(p+16,1);Float(p+24,1);Put(p+36,520);
    }
    memcpy(bytes+520,"p0",3);
    snprintf(file,sizeof(file),"%s/setup/UsetupmarkerZ.set",dir);
    f=fopen(file,"wb");assert(f&&fwrite(bytes,1,sizeof(bytes),f)==sizeof(bytes)&&!fclose(f));
    assert(SetupLoadProjectFile(dir,"UsetupmarkerZ",&setup,&why));
    PadScaleSizes(&setup, dir);
    SpawnEdits(&setup, dir);
    assert(SetupFileClone(&setup,&before,&why));EditHistoryReset(&history,&bg,&setup,&stan);
    assert(SetupFileBuildMarkers(&setup,.5f,&markers,&count,&why)&&count==3);
    assert(markers[0].command==0&&markers[1].command==1&&markers[2].command==0);
    anchor=markers[0];anchor.position[1]=5;free(markers);
    /* The actual setup history owns the entire edit, including raw pad bytes. */
    assert(EditHistoryBeginSetupEdit(&history,&setup,"Move Start Point",&transaction,&why));
    assert(SetupFileTransformMarker(&setup,&start,&anchor,.5f,move,NULL,&changed,&why)&&changed);
    Near(setup.pads[0].pos[0],11.5);Near(setup.pads[0].pos[1],22);Near(setup.pads[0].pos[2],32.5);
    assert(!setup.pads[0].stanname[0]);
    assert(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&transaction,&why)&&setup.dirty);
    assert(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why)&&asset==EDIT_HISTORY_ASSET_SETUP&&!setup.dirty);Same(&setup,&before);
    assert(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why)&&setup.dirty);Near(setup.pads[0].pos[0],11.5);
    RotationAxis(&rotation,1,90);
    assert(SetupFileTransformMarker(&setup,&start,&anchor,.5f,NULL,&rotation,&changed,&why)&&changed);
    Near(setup.pads[0].look[0],0);Near(setup.pads[0].look[2],-1);
    RotationAxis(&rotation,0,30);assert(!SetupFileTransformMarker(&setup,&start,&anchor,.5f,NULL,&rotation,&changed,&why));
    /* Cameras use hundredths of world units, and private room pads use level scale. */
    padcount=setup.padcount;
    assert(SetupFileTransformMarker(&setup,&intro,&anchor,.5f,move,NULL,&changed,&why)&&changed);
    assert(Get(setup.data+56)==1500&&Get(setup.data+60)==(DWORD)-100&&Get(setup.data+64)==1300);
    assert(setup.padcount==padcount+1&&Get(setup.data+76)==padcount);
    Near(setup.pads[padcount].pos[0],7.5);Near(setup.pads[padcount].pos[1],-.5);Near(setup.pads[0].pos[0],11.5);
    size=setup.size;
    assert(SetupFileTransformMarker(&setup,&intro,&anchor,.5f,move,NULL,&changed,&why)&&setup.size==size&&setup.padcount==padcount+1);
    assert(SetupFileTransformMarker(&setup,&outro,&anchor,.5f,move,NULL,&changed,&why)&&changed&&setup.padcount==padcount+2);
    { double angles[3]={30,45,0};RotationEuler(&rotation,angles); }
    assert(SetupFileTransformMarker(&setup,&intro,&anchor,.5f,NULL,&rotation,&changed,&why)&&changed);
    assert(SetupFileTransformMarker(&setup,&outro,&anchor,.5f,NULL,&rotation,&changed,&why)&&changed);
    Near((LONG)Get(setup.data+68)/65536.0,-3.141592653589793/4);Near((LONG)Get(setup.data+72)/65536.0,3.141592653589793/6);
    /* Translate a facing-relative control without rescaling the world delta. */
    SetupFileFree(&before);assert(SetupFileClone(&setup,&before,&why));
    assert(SetupFileTransformMarker(&setup,&swirl,&anchor,.5f,move,NULL,&changed,&why)&&changed);
    assert((LONG)Get(setup.data+132)==5*65536); /* +X local offset becomes -Z in world. */
    assert((LONG)Get(setup.data+136)==123+4*65536);
    assert((LONG)Get(setup.data+140)==3*65536);
    assert(!memcmp(setup.data+92,before.data+92,32)&&!memcmp(setup.data+156,before.data+156,128));
    { double huge[3]={1000000000,0,0};assert(SetupFileClone(&setup,&saved,&why));
      assert(!SetupFileTransformMarker(&setup,&swirl,&anchor,.5f,huge,NULL,&changed,&why));Same(&setup,&saved);SetupFileFree(&saved); }
    /* Rotation moves neighbours, leaves the selected point and metadata intact. */
    SetupFileFree(&before);assert(SetupFileClone(&setup,&before,&why));RotationAxis(&rotation,1,90);
    assert(SetupFileTransformMarker(&setup,&swirl,&anchor,.5f,NULL,&rotation,&changed,&why)&&changed);
    assert(!memcmp(setup.data+124,before.data+124,32));
    assert(!memcmp(setup.data+188,before.data+188,100));
    assert(!memcmp(setup.data+92,before.data+92,8)&&!memcmp(setup.data+112,before.data+112,12));
    assert(memcmp(setup.data+100,before.data+100,12)&&memcmp(setup.data+164,before.data+164,12));
    assert(SetupSaveProjectFile(dir,&setup,&why));
    assert(SetupLoadProjectFile(dir,"UsetupmarkerZ",&saved,&why));Same(&setup,&saved);
    assert(SetupFileBuildMarkers(&saved,.5f,&markers,&count,&why)&&count==3);
    assert(markers[1].command==1&&markers[2].command==0);Near(markers[1].position[0],18);free(markers);
    { SetupSwirlPath path={0};assert(SetupFileBuildSwirlPath(&saved,&anchor,&path,&why)&&path.pointcount==5);SetupSwirlPathFree(&path); }
    /* Clone/cancel is the same isolation used by viewport drag previews. */
    assert(EditHistoryBeginSetupEdit(&history,&setup,"Cancelled marker edit",&transaction,&why));
    assert(SetupFileTransformMarker(&setup,&outro,&anchor,.5f,move,NULL,&changed,&why));
    EditHistoryRollbackEdit(&transaction,NULL,&setup,NULL);Same(&setup,&saved);
    SetupFileFree(&before);SetupFileFree(&saved);SetupFileFree(&setup);EditHistoryFree(&history);
    puts("PASS: native marker transforms, private camera room pads/reuse, atomic range rejection, setup save/reload, undo/redo and cancelled edits.");
}
