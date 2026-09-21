#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupmeta.h"
#include "bghistory.h"
#include "setupstan.h"
#include "occluders.h"

BOOL BgDocumentClone(const BgDocument *a, BgDocument *b, const char **why) { abort(); }
BOOL StanFileClone(const StanFile *a, StanFile *b, const char **why) { abort(); }
void BgDocumentFree(BgDocument *a) { abort(); }
void StanFileFree(StanFile *a) { abort(); }
static const char *why;
static void Require(BOOL ok) { if (!ok) { fprintf(stderr,"%s\n",why); abort(); } }
#define Write SetupMetaWrite32

int main(int argc, char **argv)
{
    assert(argc == 2);
    SetupFile setup = {0}, loaded = {0}; RomFile rom = {0};
    SetupPadRef ref, other;
    EditHistory history = {0}; EditHistoryTransaction transaction = {0};
    EditHistoryAsset asset; BgDocument bg = {0}; StanFile stan = {0};
    double point[3] = {50,20,30}, offset[3] = {17,83,-21};
    BOOL changed;
    setup.size = 160; setup.data = calloc(setup.size,1); strcpy(setup.name,"UsetupoccluderZ");
    Write(setup.data+24,48); Write(setup.data+28,92);
    Write(setup.data+8,40); Write(setup.data+40,9);
    Write(setup.data+12,44); Write(setup.data+44,48);
    /* Empty shared-script catalog so safe deleted-pad recycling is enabled. */
    rom.size = 128; rom.data = calloc(rom.size,1); rom.info.entrycount = 3;
    rom.info.entries[0] = (RomManifestEntry){0x434d4150,0,128,0x80000000};
    rom.info.entries[1] = (RomManifestEntry){0x4149474c,16,32,0};
    rom.info.entries[2] = (RomManifestEntry){OCCLUDER_MANIFEST_KIND,0,0,OCCLUDER_VERSION};
    Write(rom.data+16,0x80000030); Write(rom.data+20,1); Write(rom.data+24,8); Write(rom.data+28,1);
    Write(rom.data+48,0x80000050); Write(rom.data+52,2); rom.data[80] = 4;
    Require(SetupFileSetGlobalReferences(&setup,&rom,&why));
    EditHistoryReset(&history,&bg,&setup,&stan);
    Require(EditHistoryBeginSetupEdit(&history,&setup,"Add Occluder",&transaction,&why));
    Require(SetupFileAddOccluder(&setup,.25f,point,&ref,&why));
    assert(ref.bound && ref.index == 0 && setup.padcount == 0 && setup.boundpadcount == 1);
    assert(setup.boundpads[0].pad.occluder && setup.boundpads[0].xmax == 12.5f);
    Require(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&transaction,&why));
    Require(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why)); assert(!setup.boundpadcount);
    Require(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why)); assert(setup.boundpads[0].pad.occluder);
    Require(SetupFileTranslatePad(&setup,&ref,.25f,offset,&changed,&why)); assert(changed);
    Rotation rotation; RotationAxis(&rotation,1,37);
    Require(SetupFileRotatePad(&setup,&ref,&rotation,&changed,&why)); assert(changed);
    Scaling scale = {0}; RotationEuler(&scale.axes,(double[]){0,0,0});
    scale.factor[0] = 3; scale.factor[1] = 2; scale.factor[2] = 4;
    Require(SetupFileScalePad(&setup,&ref,.25f,&scale,&why));
    assert(setup.boundpads[0].pad.occluder && setup.boundpads[0].xmax == 37.5f);
    assert(setup.boundpads[0].pad.pos[1] == 25.75f);
    Require(SetupSaveProjectFile(argv[1],&setup,&why));
    Require(SetupLoadProjectFile(argv[1],setup.name,&loaded,&why));
    assert(loaded.boundpads[0].pad.occluder && loaded.boundpads[0].pad.pos[1] == 25.75f);
    assert(loaded.boundpads[0].xmax == 37.5f);
    Require(OccludersValidateNative(loaded.data,loaded.size,&rom,&why));
    rom.info.entrycount = 2;
    assert(!OccludersValidateNative(loaded.data,loaded.size,&rom,&why) && strstr(why,"Rebase"));
    rom.info.entrycount = 3;
    /* Save/export refresh must leave a floating occluder ungrounded and not
     * report it as an unresolved navigation pad. */
    StanTile tile = {0}; unsigned char *packed; DWORD size; SetupStanRefresh stats;
    stan.tiles = &tile; stan.tilecount = 1; stan.levelscale = .25f;
    Require(SetupRefreshPadStanNative(loaded.data,loaded.size,&stan,&packed,&size,&stats,&why));
    assert(stats.unresolved == 0 && stats.updated == 0);
    assert(size == loaded.size && !memcmp(packed,loaded.data,size)); free(packed);
    stan.tiles = NULL; stan.tilecount = 0;
    Require(SetupFileAddOccluder(&setup,.25f,point,&other,&why)); assert(other.index == 1);
    Require(SetupFileDeletePad(&setup,&ref,&rom,&why)); assert(!setup.boundpads[0].pad.occluder);
    Require(SetupFileAddOccluder(&setup,.25f,point,&other,&why)); assert(other.index == 0);
    DWORD stable = setup.size;
    for (int i = 0; i < 40; i++)
    {
        Require(SetupFileDeletePad(&setup,&other,&rom,&why));
        Require(SetupFileAddOccluder(&setup,.25f,point,&other,&why));
        assert(other.index == 0 && setup.size == stable);
    }
    for (int i = 2; i < OCCLUDER_MAX; i++) { Require(SetupFileAddOccluder(&setup,1,point,&other,&why)); }
    stable = setup.size;
    assert(!SetupFileAddOccluder(&setup,1,point,&other,&why) && setup.size == stable);
    Require(OccludersValidateNative(setup.data,setup.size,&rom,&why));
    DWORD at = SetupMetaRead32(setup.data+28);
    Write(setup.data+at+48,SetupMetaRead32(setup.data+at+44)); /* collapse X */
    assert(!OccludersValidateNative(setup.data,setup.size,&rom,&why) && strstr(why,"collapsed"));
    SetupFileFree(&loaded); SetupFileFree(&setup); EditHistoryFree(&history); free(rom.data);
    puts("PASS: creation, transforms, undo/redo, save/reload, refresh/export markers, capability checks, deletion/reuse without growth, limits and bad bounds.");
    return 0;
}
