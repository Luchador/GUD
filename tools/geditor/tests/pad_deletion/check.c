#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "actionblocks.h"
#include "setupmeta.h"
#include "bghistory.h"

/* Only setup history participates in these tests. */
BOOL BgDocumentClone(const BgDocument *a, BgDocument *b, const char **why) { abort(); }
BOOL StanFileClone(const StanFile *a, StanFile *b, const char **why) { abort(); }
void BgDocumentFree(BgDocument *a) { abort(); }
void StanFileFree(StanFile *a) { abort(); }
static const char *why = "";
static RomFile rom;
static const SetupPadRef ordinary = {1,FALSE}, bound = {1,TRUE};
static void Require(BOOL ok) { if (!ok) { fprintf(stderr,"%s\n",why); abort(); } }
#define Write SetupMetaWrite32

static SetupFile Fixture(void)
{
    SetupFile s = {0};
    s.size=2048; s.data=calloc(s.size,1); strcpy(s.name,"UsetuppaddeleteZ");
    s.padcount=3; s.pads=calloc(s.padcount,sizeof(*s.pads));
    s.boundpadcount=2; s.boundpads=calloc(s.boundpadcount,sizeof(*s.boundpads));
    Write(s.data+24,64); Write(s.data+28,256);
    Write(s.data+8,512); Write(s.data+512,9);
    Write(s.data+12,640); Write(s.data+640,48);
    for (DWORD b=0;b<2;b++) for (DWORD i=0;i<(b ? s.boundpadcount : s.padcount);i++)
    {
        DWORD at=(b ? 256 : 64)+i*(b ? 68 : 44);
        SetupPad *pad=b ? &s.boundpads[i].pad : &s.pads[i];
        pad->up[1]=pad->look[2]=1;
        Write(s.data+at+16,0x3f800000); Write(s.data+at+32,0x3f800000);
        Write(s.data+at+36,2047); /* Non-null link to an empty stan name. */
    }
    return s;
}
static void Script(SetupFile *s, const unsigned char *bytes, size_t n)
{
    Write(s->data+20,1200); Write(s->data+1200,1300); Write(s->data+1204,0x401);
    memcpy(s->data+1300,bytes,n);
}
static void Blocked(SetupFile *s, const SetupPadRef *ref, const char *message)
{
    unsigned char *before=malloc(s->size); BOOL dirty=s->dirty;
    memcpy(before,s->data,s->size);
    assert(!SetupFileDeletePad(s,ref,&rom,&why));
    if (!strstr(why,message)) { fprintf(stderr,"Expected %s, got %s\n",message,why); abort(); }
    assert(!memcmp(before,s->data,s->size) && dirty==s->dirty);
    assert(!(ref->bound ? s->boundpads[ref->index].pad.deleted : s->pads[ref->index].deleted));
    free(before);
}
static void References(void)
{
    SetupFile s;
    /* The same numeric index in the other table must remain deletable. */
    for (int door=0;door<2;door++)
    {
        s=Fixture(); s.objectcount=1; s.objects=calloc(1,sizeof(*s.objects));
        s.objects[0].type=door ? PROPDEF_DOOR : PROPDEF_PROP; s.objects[0].pad=1;
        Blocked(&s,door ? &bound : &ordinary,"setup object");
        Require(SetupFileDeletePad(&s,door ? &ordinary : &bound,&rom,&why)); SetupFileFree(&s);
    }
    s=Fixture(); s.objectcount=1; s.objects=calloc(1,sizeof(*s.objects));
    s.objects[0].type=PROPDEF_PROP; s.objects[0].pad=10001;
    Blocked(&s,&bound,"setup object"); s.objects[0].deleted=TRUE;
    Require(SetupFileDeletePad(&s,&bound,&rom,&why)); SetupFileFree(&s);
    for (DWORD flag=PROPFLAG_ASSIGNEDTOCHR;flag<=PROPFLAG_INSIDEANOTHEROBJ;flag*=2)
    {
        s=Fixture(); s.objectcount=1; s.objects=calloc(1,sizeof(*s.objects));
        s.objects[0].type=PROPDEF_PROP; s.objects[0].pad=1; s.objects[0].flags=flag;
        Require(SetupFileDeletePad(&s,&ordinary,&rom,&why)); SetupFileFree(&s);
    }
    s=Fixture(); s.charactercount=1; s.characters=calloc(1,sizeof(*s.characters)); s.characters[0].pad=1;
    Blocked(&s,&ordinary,"character"); s.characters[0].deleted=TRUE;
    Require(SetupFileDeletePad(&s,&ordinary,&rom,&why)); SetupFileFree(&s);
    for (int gun=0;gun<2;gun++)
    {
        s=Fixture(); s.objectcount=1; s.objects=calloc(1,sizeof(*s.objects));
        s.objects[0].type=gun ? PROPDEF_AUTOGUN : PROPDEF_CCTV; s.objects[0].pad=0; s.objects[0].sourceoffset=640;
        Write(s.data+640+128,gun ? 10001 : 1);
        Blocked(&s,gun ? &bound : &ordinary,"aim target"); SetupFileFree(&s);
    }
    s=Fixture(); Write(s.data,1000); Write(s.data+1000,1); Write(s.data+1016,0xffffffff);
    Blocked(&s,&ordinary,"navigation"); SetupFileFree(&s);
    const DWORD types[]={0,3,6,32,33,46};
    const DWORD fields[]={4,28,24,4,8,24}, sizes[]={12,32,40,16,20,28};
    for (DWORD i=0;i<6;i++) for (DWORD b=0;b<(i ? 2u : 1u);b++)
    {
        s=Fixture(); DWORD at=i<3 ? 512 : 640;
        Write(s.data+at,types[i]); Write(s.data+at+fields[i],b ? 10001 : 1);
        Write(s.data+at+sizes[i],i<3 ? 9 : 48);
        Blocked(&s,b ? &bound : &ordinary,i<3 ? "spawn or intro" : "objective or outro"); SetupFileFree(&s);
    }
    s=Fixture(); Write(s.data+640,0x4745442e); Write(s.data+640+24,1); Write(s.data+668,48);
    Require(SetupFileDeletePad(&s,&ordinary,&rom,&why)); SetupFileFree(&s);
    puts("PASS: placement, aim, character, navigation, intro and objective references; independent pad IDs and deleted entities.");
}
static void Scripts(void)
{
    const unsigned char move[]={0x1c,0,1,4}, boundmove[]={0x65,0,0x27,0x11,4};
    SetupFile s=Fixture(); Script(&s,move,sizeof(move)); Blocked(&s,&ordinary,"level Action Block"); SetupFileFree(&s);
    s=Fixture(); Script(&s,boundmove,sizeof(boundmove)); Blocked(&s,&bound,"level Action Block"); SetupFileFree(&s);
    for (DWORD op=0x14;op<=0x17;op++) for (DWORD flags=0;flags<=12;flags+=4)
    {
        unsigned char code[]={(unsigned char)op,0,(unsigned char)flags,0,1,0,4};
        s=Fixture(); Script(&s,code,sizeof(code));
        if (flags==8) { Blocked(&s,&ordinary,"level Action Block"); }
        else { Require(SetupFileDeletePad(&s,&ordinary,&rom,&why)); }
        SetupFileFree(&s);
    }
    memcpy(rom.data+80,move,sizeof(move));
    s=Fixture(); Blocked(&s,&ordinary,"shared Action Block"); SetupFileFree(&s); rom.data[80]=4;
    s=Fixture(); rom.info.entrycount=1; Blocked(&s,&ordinary,"no shared Action Block catalog"); rom.info.entrycount=2;
    Write(s.data,2044); Blocked(&s,&ordinary,"malformed"); Write(s.data,0);
    const unsigned char bad[]={0xfd,4}; Script(&s,bad,sizeof(bad)); Blocked(&s,&ordinary,"opcode"); SetupFileFree(&s);
    /* A manually entered reference to a tombstone must fail script validation. */
    s=Fixture(); Require(SetupFileDeletePad(&s,&ordinary,&rom,&why)); Script(&s,move,sizeof(move));
    ActionDocument doc={0}; ActionIssue *issues=NULL; DWORD count; BOOL found=FALSE;
    Require(ActionDocumentLoad(&s,&doc,&why)); Require(ActionDocumentValidate(&doc,&s,&issues,&count,&why));
    for (DWORD i=0;i<count;i++) if (issues[i].error && strstr(issues[i].text,"deleted pad")) { found=TRUE; }
    assert(found); free(issues); ActionDocumentFree(&doc); SetupFileFree(&s);
    puts("PASS: local/shared scripts, conditional pad targets, malformed data, unavailable catalogs and deleted-pad validation.");
}
static void HistoryAndSaving(const char *dir)
{
    SetupFile s=Fixture(), loaded={0}; EditHistory history={0}; EditHistoryTransaction transaction={0};
    EditHistoryAsset asset; BgDocument bg={0}; StanFile stan={0};
    unsigned char original[2048]; memcpy(original,s.data,s.size);
    EditHistoryReset(&history,&bg,&s,&stan);
    Require(EditHistoryBeginSetupEdit(&history,&s,"Delete Pad",&transaction,&why));
    Require(SetupFileDeletePad(&s,&ordinary,&rom,&why)); Require(SetupFileDeletePad(&s,&bound,&rom,&why));
    Require(EditHistoryCommitEdit(&history,&bg,&s,&stan,&transaction,&why));
    Require(SetupSaveProjectFile(dir,&s,&why)); EditHistoryMarkSetupSaved(&history,&s);
    Require(SetupLoadProjectFile(dir,s.name,&loaded,&why));
    assert(loaded.padcount==3 && loaded.boundpadcount==2);
    assert(loaded.pads[1].deleted && loaded.boundpads[1].pad.deleted);
    assert(!loaded.pads[0].deleted && !loaded.pads[2].deleted && !loaded.boundpads[0].pad.deleted);
    assert(loaded.size==sizeof(original) && !memcmp(loaded.data,s.data,s.size));
    /* Only runtime stan slots change. In particular plink/terminators, all
     * later IDs, coordinates and opaque native sections are unchanged. */
    memcpy(loaded.data+64+44+40,original+64+44+40,4);
    memcpy(loaded.data+256+68+40,original+256+68+40,4);
    assert(!memcmp(loaded.data,original,sizeof(original))); SetupFileFree(&loaded);
    Require(EditHistoryUndo(&history,&bg,&s,&stan,&asset,&why));
    assert(asset==EDIT_HISTORY_ASSET_SETUP && !s.pads[1].deleted && !s.boundpads[1].pad.deleted && s.dirty);
    assert(!memcmp(s.data,original,sizeof(original)));
    Require(EditHistoryRedo(&history,&bg,&s,&stan,&asset,&why));
    assert(s.pads[1].deleted && s.boundpads[1].pad.deleted && !s.dirty);
    assert(!SetupFileDeletePad(&s,&ordinary,&rom,&why));
    EditHistoryFree(&history); SetupFileFree(&s);
    puts("PASS: save/reload, undo/redo, dirty tracking, double deletion and byte-exact preservation outside runtime pointer slots.");
}
int main(int argc, char **argv)
{
    assert(argc==2); rom.size=128; rom.data=calloc(rom.size,1); rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,0,128,0x80000000};
    rom.info.entries[1]=(RomManifestEntry){0x4149474c,16,32,0};
    Write(rom.data+16,0x80000030); Write(rom.data+20,1); Write(rom.data+24,8); Write(rom.data+28,1);
    Write(rom.data+48,0x80000050); Write(rom.data+52,2); rom.data[80]=4;
    References(); Scripts(); HistoryAndSaving(argv[1]); free(rom.data); return 0;
}
