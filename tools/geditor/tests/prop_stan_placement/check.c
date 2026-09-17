#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
                                      (double[3]){0,0,100000}, &invalid, &why));
    assert(strstr(why, "Stan tile"));
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
    SetupFileFree(&fixture); StanFileFree(&stan);
    puts("PASS Depot elevated prop: same/linked floor, preserved height, ordinary/bound/shared pads, native save/reload, undo/redo, 800 repeated moves and invalid-move rollback.");
    puts("PASS elevated copies: clipboard snapshots and live duplication, translation/rotation/scale, normal/bound pads, unchanged source, undo/redo and native save/reload.");
    return 0;
}
