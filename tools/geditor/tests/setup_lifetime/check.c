#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "actionblocks.h"
#include "setupmeta.h"
#include "bghistory.h"

BOOL BgDocumentClone(const BgDocument *a, BgDocument *b, const char **why) { abort(); }
BOOL StanFileClone(const StanFile *a, StanFile *b, const char **why) { abort(); }
void BgDocumentFree(BgDocument *a) { abort(); }
void StanFileFree(StanFile *a) { abort(); }
#define W SetupMetaWrite32
#define R SetupMetaRead32
static const char *why = "", *dir;
static RomFile rom;
static BgDocument bg;
static StanFile stan;
static DWORD commits;
static void Require(BOOL ok) { if (!ok) { fprintf(stderr,"%s\n",why); abort(); } }
static SetupFile Fixture(void)
{
    SetupFile s = {0}, loaded = {0};
    s.size = 2048; s.data = calloc(s.size,1); strcpy(s.name,"UsetuplifetimeZ");
    W(s.data+24,64); W(s.data+28,256); W(s.data+8,512); W(s.data+12,640);
    W(s.data+640,48); W(s.data+512,0); W(s.data+516,0); W(s.data+524,9);
    for (DWORD i=0; i<3; i++)
    {
        DWORD at=64+i*44;
        W(s.data+at+16,0x3f800000); W(s.data+at+32,0x3f800000); W(s.data+at+36,2047);
    }
    Require(SetupSaveProjectFile(dir,&s,&why));
    Require(SetupLoadProjectFile(dir,s.name,&loaded,&why)); SetupFileFree(&s);
    Require(SetupFileSetGlobalReferences(&loaded,&rom,&why)); return loaded;
}
static void Begin(EditHistory *h, SetupFile *s, EditHistoryTransaction *t)
{ Require(EditHistoryBeginSetupEdit(h,s,"Stress edit",t,&why)); }
static void Commit(EditHistory *h, SetupFile *s, EditHistoryTransaction *t)
{ Require(EditHistoryCommitEdit(h,NULL,s,NULL,t,&why)); commits++; }
static void Reopen(SetupFile *s)
{
    SetupFile loaded={0}; DWORD size=s->size;
    Require(SetupSaveProjectFile(dir,s,&why));
    assert(s->size==size); /* Save never mutates the live document. */
    Require(SetupLoadProjectFile(dir,s->name,&loaded,&why));
    assert(loaded.size==size && !memcmp(loaded.data,s->data,size));
    assert(loaded.padcount==s->padcount && loaded.objectcount==s->objectcount
        && loaded.charactercount==s->charactercount && loaded.boundpadcount==s->boundpadcount);
    Require(SetupFileSetGlobalReferences(&loaded,&rom,&why));
    SetupFileFree(s); *s=loaded;
}
static void Movement(void)
{
    SetupFile s=Fixture(); EditHistory h={0}; EditHistoryTransaction t={0};
    const double pos[3]={10,20,30}, step[3]={1,0,-1}, bounds[6]={-10,10,-20,20,-30,30};
    DWORD objects[2], bytes, pads; EditHistoryAsset asset;
    for (DWORD i=0;i<2;i++) { Require(SetupFileAddModel(&s,FALSE,1,1,pos,&objects[i],&why)); }
    Require(SetupFileCompact(&s,&why)); EditHistoryReset(&h,NULL,&s,NULL);
    pads=s.padcount; bytes=s.size;
    for (DWORD i=0;i<2000;i++)
    {
        DWORD id=objects[i%2]; Begin(&h,&s,&t);
        Require(SetupFileTranslateModel(&s,id,1,step,&why)); Commit(&h,&s,&t);
        assert(s.padcount==pads && s.size==bytes);
        if (i%100==0)
        {
            float x=s.pads[s.objects[id].pad].pos[0];
            Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why));
            assert(s.pads[s.objects[id].pad].pos[0]==x-1);
            Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why));
            assert(s.pads[s.objects[id].pad].pos[0]==x);
            Reopen(&s);
        }
    }
    for (DWORD i=0;i<1000;i++)
    {
        Begin(&h,&s,&t); Require(SetupFileSetModelBounds(&s,objects[i%2],1,bounds,&why)); Commit(&h,&s,&t);
        assert(s.boundpadcount<=2);
        if (i==3) { bytes=s.size; }
        if (i>3) { assert(s.size==bytes); }
    }
    assert(h.undocount==EDIT_HISTORY_LIMIT);
    EditHistoryFree(&h); SetupFileFree(&s);
    puts("PASS: 3,000 alternating moves/scales; constant pad counts/bytes, undo/redo and reopen.");
}
static void Churn(void)
{
    SetupFile s=Fixture(); EditHistory h={0}; EditHistoryTransaction t={0};
    const double pos[3]={10,20,30}, facing[3]={0,0,1}; DWORD id, size=0, pads=0, bound=0;
    EditHistoryReset(&h,NULL,&s,NULL);
    for (DWORD i=0;i<2000;i++)
    {
        Begin(&h,&s,&t);
        switch (i%4)
        {
        case 0: Require(SetupFileAddModel(&s,FALSE,1,1,pos,&id,&why)); break;
        case 1: Require(SetupFileAddModel(&s,TRUE,1,1,pos,&id,&why)); break;
        case 2: Require(SetupFileAddDroneGun(&s,1,1,pos,facing,&id,&why)); break;
        default: Require(SetupFileAddArmor(&s,1,1,pos,&id,&why)); break;
        }
        Commit(&h,&s,&t);
        Begin(&h,&s,&t);
        if (id&SETUP_CHARACTER_SELECTION_BIT)
        { Require(SetupFileDeleteCharacter(&s,id&~SETUP_CHARACTER_SELECTION_BIT,&why)); }
        else { Require(SetupFileDeleteObject(&s,id,&why)); }
        Commit(&h,&s,&t);
        if (i==19) { size=s.size; pads=s.padcount; bound=s.boundpadcount; }
        if (i>19) { assert(s.size==size && s.padcount==pads && s.boundpadcount==bound); }
        if (i%100==99) { Reopen(&s); }
    }
    assert(s.objectcount==3 && s.charactercount==1);
    EditHistoryFree(&h); SetupFileFree(&s);
    puts("PASS: 2,000 add/delete pairs recycle props, guards and aim pads without cumulative growth.");
}
static void CamerasAndSpawns(void)
{
    SetupFile s=Fixture(); EditHistory h={0}; EditHistoryTransaction t={0};
    const double pos[3]={10,20,30}, facing[3]={0,0,1}; SetupMarkerRef ref; DWORD size=0,pads=0;
    Require(SetupFilePlaceCamera(&s,SETUP_MARKER_INTRO,1,pos,facing,&ref,&why));
    Require(SetupFilePlaceCamera(&s,SETUP_MARKER_OUTRO,1,pos,facing,&ref,&why));
    Require(SetupFileCompact(&s,&why)); EditHistoryReset(&h,NULL,&s,NULL);
    for (DWORD i=0;i<1000;i++)
    {
        Begin(&h,&s,&t); Require(SetupFilePlaceSpawn(&s,1,pos,&ref,&why)); Commit(&h,&s,&t);
        Begin(&h,&s,&t); Require(SetupFilePlaceCamera(&s,SETUP_MARKER_INTRO,1,pos,facing,&ref,&why)); Commit(&h,&s,&t);
        Begin(&h,&s,&t); Require(SetupFileDeleteIntroCamera(&s,&ref,&why)); Commit(&h,&s,&t);
        Begin(&h,&s,&t); Require(SetupFilePlaceCamera(&s,SETUP_MARKER_OUTRO,1,pos,facing,&ref,&why)); Commit(&h,&s,&t);
        if (i==9) { size=s.size; pads=s.padcount; }
        if (i>9) { assert(s.size==size && s.padcount==pads); }
        if (i%100==99) { Reopen(&s); }
    }
    EditHistoryFree(&h); SetupFileFree(&s);
    puts("PASS: 4,000 spawn/camera edits, including repeated intro add/delete and outro replacement.");
}
static void Scripts(void)
{
    SetupFile s=Fixture(); EditHistory h={0}; EditHistoryTransaction t={0}; DWORD size=0;
    EditHistoryReset(&h,NULL,&s,NULL);
    for (DWORD i=0;i<1000;i++)
    {
        ActionDocument doc={0}; SetupFile result={0}; DWORD block=0;
        Require(ActionDocumentLoad(&s,&doc,&why));
        if (!doc.count) { Require(ActionDocumentAddBlock(&doc,ACTION_MISSING_TARGET,TRUE,&block,&why)); }
        strcpy(doc.blocks[block].name,"Persistent script name");
        strcpy(doc.blocks[block].instructions[0].note,"Keep this note through compaction");
        /* Change native bytecode as well as recompiling: alternate the loop
         * label and its matching branch, retaining the same instruction IDs. */
        doc.blocks[block].instructions[0].bytes[1]=i%2;
        doc.blocks[block].instructions[2].bytes[1]=i%2;
        doc.blocks[block].changed=TRUE;
        Require(ActionDocumentCompile(&doc,&s,&result,&why)); ActionDocumentFree(&doc);
        Begin(&h,&s,&t); SetupFileFree(&s); s=result; Commit(&h,&s,&t);
        if (i==9) { size=s.size; }
        if (i>9) { assert(s.size==size); }
        if (i%100==99)
        {
            Reopen(&s); Require(ActionDocumentLoad(&s,&doc,&why));
            assert(!strcmp(doc.blocks[0].name,"Persistent script name"));
            assert(!strcmp(doc.blocks[0].instructions[0].note,"Keep this note through compaction"));
            assert(doc.blocks[0].instructions[0].bytes[1]==i%2
                && doc.blocks[0].instructions[2].bytes[1]==i%2);
            ActionDocumentFree(&doc);
        }
    }
    EditHistoryFree(&h); SetupFileFree(&s);
    puts("PASS: 1,000 script recompiles, compact metadata saves and reopen.");
}
static DWORD Append(SetupFile *s, DWORD bytes)
{
    DWORD at=(s->size+3)&~3u, size=at+bytes;
    s->data=realloc(s->data,size); assert(s->data);
    memset(s->data+s->size,0,size-s->size); s->size=size; return at;
}
static DWORD Command(SetupFile *s, DWORD bytes)
{
    DWORD old=R(s->data+12), end=old;
    /* These fixtures contain only the 32-word prop and seven-word guard. */
    while (s->data[end+3]!=48) { end+=s->data[end+3]==9 ? 28 : 128; }
    DWORD at=Append(s,end-old+bytes+4), record=at+end-old;
    memcpy(s->data+at,s->data+old,end-old); W(s->data+12,at); W(s->data+record+bytes,48);
    for (DWORD i=0;i<s->objectcount;i++) { s->objects[i].sourceoffset+=at-old; }
    for (DWORD i=0;i<s->charactercount;i++) { s->characters[i].sourceoffset+=at-old; }
    return record;
}
static void Protection(void)
{
    const double pos[3]={10,20,30}, step[3]={1,0,0};
    for (DWORD kind=0;kind<7;kind++)
    {
        SetupFile s=Fixture(); DWORD id,at,other;
        Require(SetupFileAddModel(&s,FALSE,1,1,pos,&id,&why));
        s.objects[id].pad=1; s.data[s.objects[id].sourceoffset+7]=1;
        switch (kind)
        {
        case 0: /* navigation */
            at=Append(&s,32); W(s.data,at); W(s.data+at,1); W(s.data+at+16,0xffffffff); break;
        case 1: /* spawn */ W(s.data+R(s.data+8)+4,1); break;
        case 2: /* local Action Block */
            at=Append(&s,20); W(s.data+20,at); W(s.data+at,at+16); W(s.data+at+4,0x401);
            memcpy(s.data+at+16,(unsigned char[]){0x1c,0,1,4},4); break;
        case 3: /* shared Action Block */
            memcpy(rom.data+80,(unsigned char[]){0x1c,0,1,4},4);
            Require(SetupFileSetGlobalReferences(&s,&rom,&why)); rom.data[80]=4; break;
        case 4: /* another placement */
            Require(SetupFileAddModel(&s,FALSE,1,1,pos,&other,&why));
            s.objects[other].pad=1; s.data[s.objects[other].sourceoffset+7]=1; break;
        case 5: /* guard preset (distinct from its placement) */
            Require(SetupFileAddModel(&s,TRUE,1,1,pos,&other,&why));
            at=s.characters[other&~SETUP_CHARACTER_SELECTION_BIT].sourceoffset;
            s.data[at+12]=0; s.data[at+13]=1; break;
        default: at=Command(&s,16); W(s.data+at,PROPDEF_OBJECTIVE_ENTER_ROOM); W(s.data+at+4,1); break;
        }
        float before=s.pads[1].pos[0];
        Require(SetupFileTranslateModel(&s,id,1,step,&why));
        assert(s.objects[id].pad!=1 && s.pads[1].pos[0]==before);
        DWORD count=s.padcount; Require(SetupFileCompact(&s,&why));
        Require(SetupFileTranslateModel(&s,id,1,step,&why)); assert(s.padcount==count);
        SetupFileFree(&s);
    }
    /* A tag still targets its original tombstone, never a replacement prop. */
    SetupFile s=Fixture(); DWORD id,newid;
    Require(SetupFileAddModel(&s,FALSE,1,1,pos,&id,&why));
    DWORD at=Command(&s,16); W(s.data+at,PROPDEF_TAG); W(s.data+at+4,0x0001ffff);
    Require(SetupFileDeleteObject(&s,id,&why)); Require(SetupFileAddModel(&s,FALSE,1,1,pos,&newid,&why));
    assert(newid!=id && s.objects[id].deleted); SetupFileFree(&s);
    /* A script-referenced deleted character ID is likewise reserved. */
    s=Fixture(); Require(SetupFileAddModel(&s,TRUE,1,1,pos,&id,&why));
    at=Append(&s,21); W(s.data+20,at); W(s.data+at,at+16); W(s.data+at+4,0x401);
    memcpy(s.data+at+16,(unsigned char[]){5,0,0,1,4},5);
    Require(SetupFileDeleteCharacter(&s,id&~SETUP_CHARACTER_SELECTION_BIT,&why));
    Require(SetupFileAddModel(&s,TRUE,1,1,pos,&newid,&why));
    assert(newid!=id && s.characters[newid&~SETUP_CHARACTER_SELECTION_BIT].chrnum==1);
    SetupFileFree(&s);
    puts("PASS: shared placements, navigation, spawn, guard presets, objectives, local/shared AI, tags and character IDs remain protected.");
}
static void Failure(void)
{
    SetupFile s=Fixture(), original={0}, saved={0}; EditHistory h={0}; EditHistoryTransaction t={0};
    Require(SetupFileClone(&s,&original,&why)); EditHistoryReset(&h,&bg,&s,&stan);
    Begin(&h,&s,&t); W(s.data,s.size+16); DWORD size=s.size; unsigned char *data=s.data;
    assert(!EditHistoryCommitEdit(&h,&bg,&s,&stan,&t,&why));
    assert(t.active && !h.undocount && h.nextrevision==2 && s.size==size && s.data==data);
    assert(!SetupSaveProjectFile(dir,&s,&why));
    Require(SetupLoadProjectFile(dir,s.name,&saved,&why));
    assert(saved.size==original.size && !memcmp(saved.data,original.data,saved.size));
    EditHistoryRollbackEdit(&t,&bg,&s,&stan);
    assert(s.size==original.size && !memcmp(s.data,original.data,s.size));
    SetupFileFree(&saved); SetupFileFree(&original); EditHistoryFree(&h); SetupFileFree(&s);
    puts("PASS: failed compaction preserves history and the previous save; rollback restores the live document.");
}
static void CharacterTargets(void)
{
    const double pos[3]={10,20,30};
    for (DWORD op=0x14;op<=0x17;op++) for (DWORD mode=0;mode<8;mode++)
    {
        SetupFile s=Fixture(); DWORD id,next;
        Require(SetupFileAddModel(&s,TRUE,1,1,pos,&id,&why));
        DWORD target=mode&4 ? 10000 : 0;
        unsigned char script[]={op,0,mode&2 ? 12 : 4,target>>8,target&255,1,4};
        if (mode&1)
        {
            memcpy(rom.data+80,script,sizeof(script));
            Require(SetupFileSetGlobalReferences(&s,&rom,&why)); rom.data[80]=4;
        }
        else
        {
            DWORD at=Append(&s,16+sizeof(script)); W(s.data+20,at);
            W(s.data+at,at+16); W(s.data+at+4,0x401);
            memcpy(s.data+at+16,script,sizeof(script));
        }
        Require(SetupFileDeleteCharacter(&s,id&~SETUP_CHARACTER_SELECTION_BIT,&why));
        Require(SetupFileAddModel(&s,TRUE,1,1,pos,&next,&why));
        assert(next!=id && s.characters[id&~SETUP_CHARACTER_SELECTION_BIT].deleted);
        SetupFileFree(&s);
    }
    puts("PASS: local/shared aim and facing targets reserve character IDs and clone IDs, including mixed mode bits.");
}
static void CommandReferences(void)
{
    const double pos[3]={10,20,30};
    const DWORD types[]={PROPDEF_LINK,PROPDEF_SWITCH,PROPDEF_LOCK_DOOR,PROPDEF_SAFE_ITEM,
        PROPDEF_RENAME,PROPDEF_DOOR,PROPDEF_MONITOR,PROPDEF_PROP};
    const DWORD sizes[]={12,16,16,20,40,256,256,128};
    for (DWORD i=0;i<sizeof(types)/sizeof(*types);i++)
    {
        SetupFile s=Fixture(); DWORD id,next,at;
        Require(SetupFileAddModel(&s,FALSE,1,1,pos,&id,&why));
        at=Command(&s,sizes[i]); W(s.data+at,types[i]);
        if (types[i]==PROPDEF_PROP)
        { W(s.data+at+4,0x1ffff); W(s.data+at+8,PROPFLAG_INSIDEANOTHEROBJ); }
        else { W(s.data+at+(types[i]==PROPDEF_DOOR ? 128 : types[i]==PROPDEF_MONITOR ? 244 : 4),0xffffffff); }
        Require(SetupFileDeleteObject(&s,id,&why));
        Require(SetupFileAddModel(&s,FALSE,1,1,pos,&next,&why));
        assert(next!=id && s.objects[id].deleted);
        SetupFileFree(&s);
    }
    puts("PASS: relative links, switches, locks, safes, renames, linked doors, monitor owners and embedded props retain their targets.");
}
static void SelectionAndCatalogs(void)
{
    const double pos[3]={10,20,30},step[3]={1,0,0};
    SetupFile s=Fixture(),snapshot={0}; EditHistory h={0}; EditHistoryTransaction t={0};
    EditHistoryAsset asset; DWORD selection,next,offset;
    Require(SetupFileAddModel(&s,FALSE,1,1,pos,&selection,&why));
    Require(SetupFileCompact(&s,&why)); EditHistoryReset(&h,&bg,&s,&stan);
    Require(EditHistorySetSelection(&h,&selection,sizeof(selection),FALSE,&why));
    Begin(&h,&s,&t);
    Require(SetupFileAddModel(&s,TRUE,1,1,pos,&next,&why));
    offset=s.objects[selection].sourceoffset; Commit(&h,&s,&t);
    assert(offset!=s.objects[selection].sourceoffset && *(DWORD *)h.selection==selection);
    Require(EditHistorySetSelection(&h,&next,sizeof(next),FALSE,&why));
    EditHistoryMarkSetupSaved(&h,&s); assert(!s.dirty);
    Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why));
    assert(s.dirty && *(DWORD *)h.selection==selection && !s.charactercount);
    Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why));
    assert(!s.dirty && *(DWORD *)h.selection==next && s.charactercount==1);
    Require(SetupFileTranslateModel(&s,selection,1,step,&why));
    Require(SetupFileTranslateModel(&s,next,1,step,&why)); /* Both relocated offsets work. */
    Require(SetupFileClone(&s,&snapshot,&why));
    RomFile oldrom=rom; oldrom.info.entrycount=1; /* CMAP only, no AIGL. */
    Require(SetupFileSetGlobalReferences(&s,&oldrom,&why));
    assert(!s.globalrefs && snapshot.globalrefs);
    DWORD pads=s.padcount;
    Require(SetupFileTranslateModel(&s,selection,1,step,&why));
    assert(s.padcount==pads+1); /* Unknown shared references must not be guessed. */
    Require(SetupFileTranslateModel(&snapshot,selection,1,step,&why));
    assert(snapshot.padcount==pads); /* Shared snapshot cache remains valid. */
    RomFile badrom=rom; badrom.info.entries[1].romstart=rom.size;
    assert(!SetupFileSetGlobalReferences(&snapshot,&badrom,&why) && snapshot.globalrefs);
    SetupFileFree(&snapshot); EditHistoryFree(&h); SetupFileFree(&s);
    puts("PASS: selection, dirty revisions and relocated model offsets survive undo/redo; missing/invalid catalogs preserve safe cache ownership.");
}
static void Depot(const char *path)
{
    SetupFile s={0}; EditHistory h={0}; EditHistoryTransaction t={0}; EditHistoryAsset asset;
    const double pos[3]={352,-242,950},step[3]={1,0,-1}; DWORD ids[2],size,pads,original;
    Require(RomLoad(path,&rom,&why)); Require(SetupExtractAll(&rom,dir,&why)!=0);
    Require(SetupLoadProjectFile(dir,"UsetupdepoZ",&s,&why));
    original=s.size; Require(SetupFileSetGlobalReferences(&s,&rom,&why)); assert(s.globalrefs);
    for (DWORD i=0;i<2;i++) { Require(SetupFileAddModel(&s,FALSE,1,1,pos,&ids[i],&why)); }
    Require(SetupFileCompact(&s,&why)); size=s.size; pads=s.padcount;
    EditHistoryReset(&h,&bg,&s,&stan);
    for (DWORD i=0;i<2000;i++)
    {
        DWORD id=ids[i%2]; Begin(&h,&s,&t);
        Require(SetupFileTranslateModel(&s,id,1,step,&why)); Commit(&h,&s,&t);
        assert(s.size==size && s.padcount==pads);
        if (i%100==99)
        {
            Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why));
            Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why)); Reopen(&s);
        }
    }
    for (DWORD i=0;i<1000;i++)
    {
        DWORD id; Begin(&h,&s,&t);
        Require(SetupFileAddModel(&s,FALSE,1,1,pos,&id,&why)); Commit(&h,&s,&t);
        Begin(&h,&s,&t); Require(SetupFileDeleteObject(&s,id,&why)); Commit(&h,&s,&t);
        if (i==9) { size=s.size; pads=s.padcount; }
        if (i>9) { assert(s.size==size && s.padcount==pads); }
    }
    Reopen(&s);
    printf("PASS: supplied Depot, %u compact bytes initially; 4,000 edits stabilize at %u bytes / %u pads.\n",original,s.size,s.padcount);
    EditHistoryFree(&h); SetupFileFree(&s); RomFree(&rom);
}
int main(int argc, char **argv)
{
    setbuf(stdout,NULL); assert(argc==2 || argc==3); dir=argv[1]; rom.size=128; rom.data=calloc(rom.size,1); rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,0,128,0x80000000};
    rom.info.entries[1]=(RomManifestEntry){0x4149474c,16,32,0};
    W(rom.data+16,0x80000030); W(rom.data+20,1); W(rom.data+24,8); W(rom.data+28,1);
    W(rom.data+48,0x80000050); W(rom.data+52,2); rom.data[80]=4;
    Movement(); Churn(); CamerasAndSpawns(); Scripts(); Protection(); Failure();
    CharacterTargets(); CommandReferences(); SelectionAndCatalogs(); free(rom.data); memset(&rom,0,sizeof(rom));
    if (argc==3) { Depot(argv[2]); }
    printf("PASS: %u committed edits under ASan/UBSan.\n",commits); return 0;
}
