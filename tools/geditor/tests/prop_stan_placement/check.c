#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "objectload.h"
#include "bghistory.h"
#include "setupmeta.h"
#include "setup_compare.h"

static const char *why, *dir;
static const float levelscale = .21847887f;
static void Require(BOOL ok) { if (!ok) { fprintf(stderr, "%s\n", why); abort(); } }
static void LoadGlobalReferences(SetupFile *setup)
{
    /* The editor loads the shared-script catalog before it reuses pads. */
    unsigned char data[128] = {0};
    RomFile rom = {.data=data, .size=sizeof(data)};
    rom.info.entrycount = 2;
    rom.info.entries[0] = (RomManifestEntry){0x434d4150,0,128,0x80000000};
    rom.info.entries[1] = (RomManifestEntry){0x4149474c,16,32,0};
    SetupMetaWrite32(data+16,0x80000030); SetupMetaWrite32(data+20,1);
    SetupMetaWrite32(data+24,8); SetupMetaWrite32(data+28,1);
    SetupMetaWrite32(data+48,0x80000050); SetupMetaWrite32(data+52,2); data[80]=4;
    Require(SetupFileSetGlobalReferences(setup, &rom, &why));
}
static SetupPad *Pad(SetupFile *s, DWORD index)
{
    SetupPadRef ref;
    Require(SetupFileGetModelPad(s, index, &ref));
    return ref.bound ? &s->boundpads[ref.index].pad : &s->pads[ref.index];
}
static DWORD Resolve(const StanFile *stan, const SetupPad *pad)
{
    float position[3];
    for (int i = 0; i < 3; i++) { position[i] = pad->pos[i] / levelscale; }
    return StanResolvePadTile(stan, pad->stanname, position);
}
static void SamePose(const SetupObjectGeometry *before, const SetupObjectGeometry *after,
                     const double delta[3])
{
    assert(before->tricount == after->tricount);
    for (DWORD i = 0; i < before->tricount; i++)
    {
        assert(before->objectindices[i] == after->objectindices[i]);
        for (int j = 0; j < 3; j++)
        {
            const BgVertex *a = &before->tris[i*3+j], *b = &after->tris[i*3+j];
            BOOL moved = before->objectindices[i] == 0;
            assert(fabs(b->x - a->x - (moved ? delta[0] : 0)) < .02);
            assert(fabs(b->y - a->y - (moved ? delta[1] : 0)) < .02);
            assert(fabs(b->z - a->z - (moved ? delta[2] : 0)) < .02);
        }
    }
}
static void DisconnectedMoves(void)
{
    StanTile tiles[2] = {
        {.id=0x100, .sourceoffset=12, .room=1, .pointcount=3, .extreme={0,1,2},
         .points={{0,0,0,0},{0,0,100,0},{100,0,0,0}}},
        {.id=0x201, .sourceoffset=44, .room=2, .pointcount=3, .extreme={0,1,2},
         .points={{300,0,0,0},{300,0,100,0},{400,0,0,0}}}
    };
    StanFile stan = {.tiles=tiles, .tilecount=2, .levelscale=1};
    const float from[3] = {10,50,10}, valid[3] = {310,50,10}, gap[3] = {200,50,10};
    char name[16];
    assert(StanResolveMovedPadName(&stan, "p1a", from, valid, name));
    assert(!strcmp(name, "p2a1") && StanResolvePadTile(&stan, name, valid) == 1);
    assert(!StanResolveMovedPadName(&stan, "p1a", from, gap, name));
    /* q-prefixed authored references are valid even though the automatic
     * nearest-walkable search omits them. */
    tiles[0].id = 0x800100;
    assert(StanResolveMovedPadName(&stan, "q1a", from, from, name));
    assert(!strcmp(name, "q1a"));
}
static void Depot(StanFile *stan, BOOL bound, BOOL named)
{
    SetupFile s = {0}, baseline = {0}, after = {0}, saved = {0};
    SetupObjectGeometry before = {0}, moved = {0}, reloaded = {0};
    EditHistory history = {0}; EditHistoryTransaction tx = {0}; EditHistoryAsset asset;
    BgDocument bg = {0}; StanFile historyStan = {0};
    SetupPadRef ref;
    const double offset[3] = {50, 0, 0};
    Require(SetupLoadProjectFile(dir, "UsetupmoveZ", &s, &why));
    LoadGlobalReferences(&s);
    if (bound) for (int i = 0; i < 2; i++)
    {
        s.objects[i].pad = 10000;
        s.data[s.objects[i].sourceoffset+6] = 10000 >> 8;
        s.data[s.objects[i].sourceoffset+7] = 10000 & 255;
    }
    Require(SetupFileGetModelPad(&s, 0, &ref));
    if (named) { Require(SetupFileSetPadStanName(&s, &ref, "p2037c", &why)); }
    SetupPad other = *Pad(&s, 1);
    Require(ObjectLoadSetupGeometry(dir, &s, stan, levelscale, &before, &why));
    assert(before.tricount == 8 && Resolve(stan, Pad(&s, 0)) != STAN_TILE_NONE);
    float target[3];
    for (int i = 0; i < 3; i++) { target[i] = Pad(&s, 0)->pos[i] / levelscale + offset[i]; }
    /* This is the reported regression: nearest-3D lookup fails above a valid
     * warehouse floor because it starts at a disconnected raised surface. */
    assert(StanResolvePadTile(stan, "", target) == STAN_TILE_NONE);
    assert(StanResolvePadTile(stan, "p2037c", target) != STAN_TILE_NONE);
    Require(SetupFileClone(&s, &baseline, &why));
    EditHistoryReset(&history, NULL, &s, NULL);
    Require(EditHistoryBeginSetupEdit(&history, &s, "Move Object", &tx, &why));
    Require(ObjectTranslateSetupModel(dir, &s, stan, levelscale, &before, 0, offset, &moved, &why));
    SamePose(&before, &moved, offset);
    assert(s.objects[0].pad != s.objects[1].pad);
    assert(!memcmp(&other, Pad(&s, 1), sizeof(other)));
    assert(!strcmp(Pad(&s, 0)->stanname, "p2037c"));
    assert(fabs(Pad(&s, 0)->pos[1] / levelscale - 248.49382) < .01);
    Require(EditHistoryCommitEdit(&history, NULL, &s, NULL, &tx, &why));
    Require(SetupFileClone(&s, &after, &why));
    Require(EditHistoryUndo(&history, &bg, &s, &historyStan, &asset, &why));
    SetupAssertNativeEqual(&s, &baseline);
    Require(EditHistoryRedo(&history, &bg, &s, &historyStan, &asset, &why));
    SetupAssertNativeEqual(&s, &after);
    Require(SetupSaveProjectFile(dir, &s, &why));
    Require(SetupLoadProjectFile(dir, s.name, &saved, &why));
    SetupAssertNativeEqual(&s, &saved);
    Require(ObjectLoadSetupGeometry(dir, &saved, stan, levelscale, &reloaded, &why));
    SamePose(&moved, &reloaded, (double[3]){0,0,0});
    assert(Resolve(stan, Pad(&saved, 0)) == Resolve(stan, Pad(&s, 0)));
    /* Keep the first committed model as the starting point for each move. */
    DWORD smallsize = s.size, padcount = s.padcount, boundcount = s.boundpadcount;
    for (int i = 0; i < 200; i++)
    {
        double step[3] = { i % 2 ? -300 : 300, 0, 0 };
        SetupObjectGeometry next = {0};
        Require(EditHistoryBeginSetupEdit(&history, &s, "Move Object", &tx, &why));
        Require(ObjectTranslateSetupModel(dir, &s, stan, levelscale, &moved, 0, step, &next, &why));
        SamePose(&moved, &next, step);
        assert(Resolve(stan, Pad(&s, 0)) != STAN_TILE_NONE);
        assert(i % 2 || strcmp(Pad(&s, 0)->stanname, "p2037c"));
        Require(EditHistoryCommitEdit(&history, NULL, &s, NULL, &tx, &why));
        assert(s.size <= smallsize + 16); /* One native compaction block of alignment slack. */
        assert(s.padcount == padcount && s.boundpadcount == boundcount);
        ObjectGeometryFree(&moved); moved = next;
    }
    SetupFile rejected = {0};
    Require(SetupFileClone(&s, &rejected, &why));
    Require(EditHistoryBeginSetupEdit(&history, &s, "Move Object", &tx, &why));
    SetupObjectGeometry invalid = {0};
    assert(!ObjectTranslateSetupModel(dir, &s, stan, levelscale, &moved, 0,
                                      (double[3]){0,0,NAN}, &invalid, &why));
    assert(strstr(why, "coordinate range"));
    EditHistoryRollbackEdit(&tx, NULL, &s, NULL); EditHistoryCancelEdit(&tx);
    SetupAssertNativeEqual(&s, &rejected);
    assert(!invalid.tricount);
    /* Restore the on-disk fixture for the next independent case. */
    Require(SetupSaveProjectFile(dir, &baseline, &why));
    ObjectGeometryFree(&before); ObjectGeometryFree(&moved); ObjectGeometryFree(&reloaded);
    SetupFileFree(&s); SetupFileFree(&baseline); SetupFileFree(&after);
    SetupFileFree(&saved); SetupFileFree(&rejected); EditHistoryFree(&history);
}

static void Duplicate(StanFile *stan, BOOL bound, BOOL live, int mode)
{
    SetupFile setup = {0}, snapshot = {0}, baseline = {0}, after = {0}, saved = {0};
    SetupObjectGeometry initial = {0}, placed = {0}, pose = {0}, duplicated = {0}, reloaded = {0};
    EditHistory history = {0}; EditHistoryTransaction tx = {0}; EditHistoryAsset asset;
    BgDocument bg = {0}; StanFile historyStan = {0}; SetupPadRef ref;
    DWORD selection = (DWORD)-1;
    const double move[3] = {50,0,0}, paste[3] = {0,10,0}, drag[3] = {300,0,0};
    const double pivot[3] = {-1210.526,248.49382,1083.28711};
    Rotation rotation; RotationAxis(&rotation,1,35);
    Scaling scaling = {.factor={1.25,1.5,.75}};
    RotationAxis(&scaling.axes,0,0); memcpy(scaling.pivot,pivot,sizeof(pivot));
    Require(SetupLoadProjectFile(dir,"UsetupmoveZ",&setup,&why));
    LoadGlobalReferences(&setup);
    if (bound)
    {
        setup.objects[0].pad = 10000;
        setup.data[setup.objects[0].sourceoffset+6] = 10000 >> 8;
        setup.data[setup.objects[0].sourceoffset+7] = 10000 & 255;
    }
    Require(ObjectLoadSetupGeometry(dir,&setup,stan,levelscale,&initial,&why));
    Require(ObjectTranslateSetupModel(dir,&setup,stan,levelscale,&initial,0,move,&placed,&why));
    Require(SetupFileCompact(&setup,&why));
    Require(ObjectCopySetupModelPose(&placed,0,&pose,&why));
    Require(SetupFileClone(&setup,&snapshot,&why));
    SetupPad original = *Pad(&setup,0);
    float query[3];
    for (int axis=0;axis<3;axis++) { query[axis]=original.pos[axis]/levelscale; }
    assert(StanResolvePadTile(stan,"",query)==STAN_TILE_NONE);
    assert(StanResolvePadTile(stan,original.stanname,query)!=STAN_TILE_NONE);
    if (!live)
    {
        /* Clipboard data belongs to a different setup allocation, and remains
         * valid after the source moves or changes its own tile-name string. */
        SetupObjectGeometry changed = {0};
        Require(ObjectTranslateSetupModel(dir,&setup,stan,levelscale,&placed,0,drag,&changed,&why));
        ObjectGeometryFree(&changed);
        Require(SetupFileCompact(&setup,&why));
        assert(strcmp(Pad(&setup,0)->stanname,original.stanname));
    }
    SetupPad untouched = *Pad(&setup,0);
    Require(SetupFileClone(&setup,&baseline,&why));
    EditHistoryReset(&history,NULL,&setup,NULL);
    Require(EditHistoryBeginSetupEdit(&history,&setup,"Duplicate Object",&tx,&why));
    Require(ObjectDuplicateSetupModel(dir,&setup,live ? &setup : &snapshot,stan,levelscale,
        &pose,0,mode<2 ? (mode==0 ? paste : drag) : NULL,
        mode==2 ? &rotation : NULL,mode==2 ? pivot : NULL,mode==3 ? &scaling : NULL,
        &selection,&duplicated,&why));
    Require(SetupFileGetModelPad(&setup,selection,&ref));
    assert(setup.objects[selection].pad != setup.objects[0].pad);
    const SetupPad *sourcepad = Pad(&setup,0);
    assert(!memcmp(sourcepad->pos,untouched.pos,sizeof(untouched.pos)));
    assert(!memcmp(sourcepad->up,untouched.up,sizeof(untouched.up)));
    assert(!memcmp(sourcepad->look,untouched.look,sizeof(untouched.look)));
    /* Re-parsing clears unused bytes after the name's terminator. */
    assert(!strcmp(sourcepad->stanname,untouched.stanname) && sourcepad->deleted==untouched.deleted);
    assert(Pad(&setup,selection)->stanname[0] && Resolve(stan,Pad(&setup,selection))!=STAN_TILE_NONE);
    DWORD corner=0;
    for (DWORD i=0;i<duplicated.tricount;i++) if (duplicated.objectindices[i]==selection)
    {
        for (int j=0;j<3;j++,corner++)
        {
            const BgVertex *v=&pose.tris[corner], *actual=&duplicated.tris[i*3+j];
            double point[3]={v->x,v->y,v->z}, expected[3];
            if (mode==2) { RotationPoint(&rotation,pivot,point,expected); }
            else if (mode==3) { ScalingPoint(&scaling,point,expected); }
            else for (int axis=0;axis<3;axis++)
                { expected[axis]=point[axis]+(mode==0 ? paste[axis] : drag[axis]); }
            assert(fabs(actual->x-expected[0])<.02);
            assert(fabs(actual->y-expected[1])<.02);
            assert(fabs(actual->z-expected[2])<.02);
        }
    }
    assert(corner==pose.tricount*3);
    Require(EditHistoryCommitEdit(&history,NULL,&setup,NULL,&tx,&why));
    Require(SetupFileClone(&setup,&after,&why));
    Require(EditHistoryUndo(&history,&bg,&setup,&historyStan,&asset,&why));
    SetupAssertNativeEqual(&setup,&baseline);
    Require(EditHistoryRedo(&history,&bg,&setup,&historyStan,&asset,&why));
    SetupAssertNativeEqual(&setup,&after);
    Require(SetupSaveProjectFile(dir,&setup,&why));
    Require(SetupLoadProjectFile(dir,setup.name,&saved,&why));
    SetupAssertNativeEqual(&setup,&saved);
    assert(!strcmp(Pad(&saved,selection)->stanname,Pad(&setup,selection)->stanname));
    Require(ObjectLoadSetupGeometry(dir,&saved,stan,levelscale,&reloaded,&why));
    SamePose(&duplicated,&reloaded,(double[3]){0,0,0});
    ObjectGeometryFree(&initial); ObjectGeometryFree(&placed); ObjectGeometryFree(&pose);
    ObjectGeometryFree(&duplicated); ObjectGeometryFree(&reloaded);
    SetupFileFree(&setup); SetupFileFree(&snapshot); SetupFileFree(&baseline);
    SetupFileFree(&after); SetupFileFree(&saved); EditHistoryFree(&history);
}
static void DepotStackedRooms(StanFile *stan, BOOL bound)
{
    /* Uploaded gray-building pendant: p43e2 is the upstairs floor at Y=105,
     * but its pad is downstairs at Y=14, above room 40's floor at Y=-243. */
    const float point[3] = {141.91492f,14.330784f,879.27264f};
    const float upstairs[3] = {141.91492f,150,879.27264f};
    const double nudge[3] = {1,0,0};
    SetupFile s = {0}, saved = {0};
    SetupObjectGeometry before = {0}, after = {0}, reloaded = {0};
    SetupPadRef ref;
    DWORD tile;
    char name[16];
    double delta[3];
    tile = StanResolvePadTile(stan, "", point);
    assert(tile != STAN_TILE_NONE && stan->tiles[tile].room == 42);
    /* Runtime lookup is intentionally unchanged. New placements serialize
     * an explicit downstairs name so the game uses the same floor. */
    assert(StanResolveMovedPadName(stan, "", point, point, name));
    tile = StanResolvePadTile(stan, name, point);
    assert(tile != STAN_TILE_NONE && stan->tiles[tile].room == 40);
    assert(StanResolveMovedPadName(stan, "p43e2", upstairs, upstairs, name));
    assert(!strcmp(name, "p43e2"));
    Require(SetupLoadProjectFile(dir, "UsetupmoveZ", &s, &why));
    LoadGlobalReferences(&s);
    if (bound)
    {
        s.objects[0].pad = 10000;
        s.data[s.objects[0].sourceoffset+6] = 10000 >> 8;
        s.data[s.objects[0].sourceoffset+7] = 10000 & 255;
    }
    for (int axis = 0; axis < 3; axis++) { delta[axis] = point[axis] - Pad(&s,0)->pos[axis] / levelscale; }
    Require(SetupFileTranslateModel(&s,0,levelscale,delta,&why));
    Require(SetupFileGetModelPad(&s,0,&ref));
    Require(SetupFileSetPadStanName(&s,&ref,"p43e2",&why));
    assert(stan->tiles[Resolve(stan,Pad(&s,0))].room == 42);
    Require(ObjectLoadSetupGeometry(dir,&s,stan,levelscale,&before,&why));
    Require(ObjectTranslateSetupModel(dir,&s,stan,levelscale,&before,0,nudge,&after,&why));
    SamePose(&before,&after,nudge);
    assert(stan->tiles[Resolve(stan,Pad(&s,0))].room == 40);
    Require(SetupSaveProjectFile(dir,&s,&why));
    Require(SetupLoadProjectFile(dir,s.name,&saved,&why));
    assert(stan->tiles[Resolve(stan,Pad(&saved,0))].room == 40);
    Require(ObjectLoadSetupGeometry(dir,&saved,stan,levelscale,&reloaded,&why));
    SamePose(&after,&reloaded,(double[3]){0,0,0});
    ObjectGeometryFree(&before); ObjectGeometryFree(&after); ObjectGeometryFree(&reloaded);
    SetupFileFree(&s); SetupFileFree(&saved);
}

static void OffStan(BOOL bound, DWORD orientation)
{
    StanTile tiles[2] = {
        {.id=0x100, .sourceoffset=12, .room=1, .pointcount=3, .extreme={0,1,2},
         .points={{0,0,0,0},{0,0,100,0},{100,0,0,0}}},
        {.id=0x200, .sourceoffset=44, .room=2, .pointcount=3, .extreme={0,1,2},
         .points={{300,0,0,0},{300,0,100,0},{400,0,0,0}}}
    };
    StanFile floor = {.tiles=tiles,.tilecount=2,.levelscale=levelscale}, historyStan={0};
    SetupFile s={0}, baseline={0}, after={0}, saved={0};
    SetupObjectGeometry pose={0}, next={0}, reloaded={0};
    EditHistory history={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
    BgDocument bg={0}; SetupPadRef ref; BOOL changed;
    double delta[3], pivot[3], anchor[3];
    const double outside[3]={800,0,0}, zero[3]={0};
    Rotation rotation; RotationEuler(&rotation,(double[3]){15,35,20});
    Require(SetupLoadProjectFile(dir,"UsetupmoveZ",&s,&why));
    LoadGlobalReferences(&s);
    for (int i=0;i<2;i++)
    {
        if (bound) { s.objects[i].pad=10000; }
        s.data[s.objects[i].sourceoffset+6]=(unsigned char)(s.objects[i].pad>>8);
        s.data[s.objects[i].sourceoffset+7]=(unsigned char)s.objects[i].pad;
    }
    /* Existing bound fitting must survive; fitting bits on an ordinary pad
     * must stay inert when it is promoted. Include a non-default extra scale. */
    s.objects[0].flags |= orientation | PROPFLAG_SCALE_TO_X_BOUNDS
        | PROPFLAG_SCALE_TO_Y_BOUNDS | PROPFLAG_SCALE_TO_Z_BOUNDS;
    s.objects[0].extrascale=384;
    SetupMetaWrite32(s.data+s.objects[0].sourceoffset,0x01800003);
    SetupMetaWrite32(s.data+s.objects[0].sourceoffset+8,s.objects[0].flags);
    Require(SetupFileGetModelPad(&s,0,&ref));
    for (int a=0;a<3;a++) { delta[a]=(a==1 ? 50 : 10)-Pad(&s,0)->pos[a]/levelscale; }
    Require(SetupFileTranslatePad(&s,&ref,levelscale,delta,&changed,&why));
    Require(SetupFileRotatePad(&s,&ref,&rotation,&changed,&why));
    Require(SetupFileSetPadStanName(&s,&ref,"p1a",&why));
    SetupPad other=*Pad(&s,1);
    Require(ObjectLoadSetupGeometry(dir,&s,&floor,levelscale,&pose,&why));
    assert(pose.tricount==8);
    Require(SetupFileClone(&s,&baseline,&why));
    EditHistoryReset(&history,NULL,&s,NULL);
    Require(EditHistoryBeginSetupEdit(&history,&s,"Move Object",&tx,&why));
    Require(ObjectTranslateSetupModel(dir,&s,&floor,levelscale,&pose,0,outside,&next,&why));
    SamePose(&pose,&next,outside);
    assert(s.objects[0].flags2 & PROPFLAG2_USE_PAD_REFERENCE);
    assert(s.objects[0].extrascale==384 && s.objects[0].pad>=10000);
    assert(!memcmp(&other,Pad(&s,1),sizeof(other)) && s.objects[0].pad!=s.objects[1].pad);
    assert(Resolve(&floor,Pad(&s,0))==0);
    Require(EditHistoryCommitEdit(&history,NULL,&s,NULL,&tx,&why));
    Require(SetupFileClone(&s,&after,&why));
    Require(EditHistoryUndo(&history,&bg,&s,&historyStan,&asset,&why));
    SetupAssertNativeEqual(&s,&baseline);
    Require(EditHistoryRedo(&history,&bg,&s,&historyStan,&asset,&why));
    SetupAssertNativeEqual(&s,&after);
    ObjectGeometryFree(&pose); pose=next; memset(&next,0,sizeof(next));
    Require(SetupSaveProjectFile(dir,&s,&why));
    Require(SetupLoadProjectFile(dir,s.name,&saved,&why));
    SetupAssertNativeEqual(&s,&saved);
    Require(ObjectLoadSetupGeometry(dir,&saved,&floor,levelscale,&reloaded,&why));
    SamePose(&pose,&reloaded,zero); ObjectGeometryFree(&reloaded); SetupFileFree(&saved);
    DWORD size=s.size, pads=s.padcount, bounds=s.boundpadcount;
    for (int i=0;i<40;i++)
    {
        const double step[3]={0,0,i%2 ? -10 : 10};
        Require(ObjectTranslateSetupModel(dir,&s,&floor,levelscale,&pose,0,step,&next,&why));
        SamePose(&pose,&next,step);
        Require(SetupFileCompact(&s,&why));
        assert(s.padcount==pads && s.boundpadcount==bounds && s.size<=size+16);
        ObjectGeometryFree(&pose); pose=next; memset(&next,0,sizeof(next));
    }
    for (int a=0;a<3;a++) { pivot[a]=a==0 ? pose.tris[0].x : a==1 ? pose.tris[0].y : pose.tris[0].z; }
    RotationAxis(&rotation,0,35);
    Require(ObjectRotateSetupModel(dir,&s,&floor,levelscale,&pose,0,&rotation,pivot,&next,&why));
    ObjectGeometryFree(&pose); pose=next; memset(&next,0,sizeof(next));
    Scaling scale={.factor={1.25,1.5,.75}};
    Require(SetupFileGetModelPad(&s,0,&ref)); Require(SetupFilePadRotation(&s,&ref,&scale.axes));
    memcpy(scale.pivot,pivot,sizeof(pivot));
    Require(ObjectScaleSetupModel(dir,&s,&floor,levelscale,&pose,0,&scale,&next,&why));
    ObjectGeometryFree(&pose); pose=next; memset(&next,0,sizeof(next));
    /* Duplicate an already offset model, then verify the original stays put. */
    DWORD selection;
    Require(ObjectDuplicateSetupModel(dir,&s,&s,&floor,levelscale,&pose,0,
        (double[3]){0,10,0},NULL,NULL,NULL,&selection,&next,&why));
    SetupObjectGeometry original={0}, copy={0};
    Require(ObjectCopySetupModelPose(&next,0,&original,&why));
    Require(ObjectCopySetupModelPose(&next,selection,&copy,&why));
    for (DWORD i=0;i<original.tricount*3;i++)
    {
        assert(fabs(copy.tris[i].x-original.tris[i].x)<.02);
        assert(fabs(copy.tris[i].y-original.tris[i].y-10)<.02);
        assert(fabs(copy.tris[i].z-original.tris[i].z)<.02);
    }
    assert(s.objects[0].pad!=s.objects[selection].pad && Resolve(&floor,Pad(&s,selection))==0);
    ObjectGeometryFree(&original); ObjectGeometryFree(&copy);
    ObjectGeometryFree(&pose); pose=next; memset(&next,0,sizeof(next));
    /* Move the model's native bottom-center anchor onto a disconnected room.
     * Its Stan reference must now follow room 2 instead of remaining in 1. */
    Require(SetupFileGetModelPad(&s,0,&ref));
    const SetupBoundPad *bp=&s.boundpads[ref.index];
    for (int a=0;a<3;a++)
    {
        double side=(double)bp->pad.up[(a+1)%3]*bp->pad.look[(a+2)%3]
                   -(double)bp->pad.up[(a+2)%3]*bp->pad.look[(a+1)%3];
        anchor[a]=(bp->pad.pos[a]+side*(bp->xmin+bp->xmax)*.5
            +bp->pad.up[a]*bp->ymin+bp->pad.look[a]*(bp->zmin+bp->zmax)*.5)/levelscale;
        delta[a]=(a==0 ? 310 : a==1 ? 50 : 10)-anchor[a];
    }
    Require(ObjectTranslateSetupModel(dir,&s,&floor,levelscale,&pose,0,delta,&next,&why));
    SamePose(&pose,&next,delta);
    assert(Resolve(&floor,Pad(&s,0))==1 && Resolve(&floor,Pad(&s,selection))==0);
    Require(SetupSaveProjectFile(dir,&s,&why)); Require(SetupLoadProjectFile(dir,s.name,&saved,&why));
    SetupAssertNativeEqual(&s,&saved);
    Require(ObjectLoadSetupGeometry(dir,&saved,&floor,levelscale,&reloaded,&why));
    SamePose(&next,&reloaded,zero);
    ObjectGeometryFree(&pose); ObjectGeometryFree(&next); ObjectGeometryFree(&reloaded);
    SetupFileFree(&s); SetupFileFree(&baseline); SetupFileFree(&after); SetupFileFree(&saved);
    EditHistoryFree(&history);
}

int main(int argc, char **argv)
{
    StanFile stan = {0}; SetupFile fixture = {0};
    assert(argc == 2); dir = argv[1];
    DisconnectedMoves();
    Require(StanLoadProjectFile(dir, "Tbg_depo_all_p_stanZ", levelscale, &stan, &why));
    Require(SetupLoadProjectFile(dir, "UsetupmoveZ", &fixture, &why));
    for (int bound = 0; bound < 2; bound++) for (int named = 0; named < 2; named++)
    {
        Depot(&stan, bound, named);
        Require(SetupSaveProjectFile(dir, &fixture, &why));
    }
    for (int bound=0;bound<2;bound++) for (int live=0;live<2;live++) for (int mode=0;mode<4;mode++)
    {
        Duplicate(&stan,bound,live,mode);
        Require(SetupSaveProjectFile(dir,&fixture,&why));
    }
    for (int bound=0;bound<2;bound++)
    {
        DepotStackedRooms(&stan,bound);
        Require(SetupSaveProjectFile(dir,&fixture,&why));
    }
    const DWORD orientations[]={0,PROPFLAG_ONSIDE,PROPFLAG_UPSIDEDOWN};
    for (int bound=0;bound<2;bound++) for (int i=0;i<3;i++)
    {
        OffStan(bound,orientations[i]);
        Require(SetupSaveProjectFile(dir,&fixture,&why));
    }
    SetupFileFree(&fixture); StanFileFree(&stan);
    puts("PASS Depot elevated prop: same/linked floor, preserved height, ordinary/bound/shared pads, native save/reload, undo/redo, 800 repeated moves and invalid-move rollback.");
    puts("PASS elevated copies: clipboard snapshots and live duplication, translation/rotation/scale, normal/bound pads, unchanged source, undo/redo and native save/reload.");
    puts("PASS Depot stacked rooms: downstairs prop placement/repair, upstairs reference retained, normal/bound pads and native save/reload.");
    puts("PASS off-Stan props: independent reference/bounds, tilted/sideways/upside-down models, fitting and extra scale, copy/rotate/scale, undo/redo, compact save/reload, repeated moves and room reassignment on return.");
    return 0;
}
