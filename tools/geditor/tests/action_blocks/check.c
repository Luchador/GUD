#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "actionblocks.h"
#include "setupmeta.h"
#include "bghistory.h"
unsigned char *TestReadResource(const char *,const char *,DWORD *,const char **);
/* This suite exercises setup history; other asset variants are not involved. */
BOOL BgDocumentClone(const BgDocument *a,BgDocument *b,const char **why) { abort(); }
BOOL StanFileClone(const StanFile *a,StanFile *b,const char **why) { abort(); }
void BgDocumentFree(BgDocument *a) { abort(); }
void StanFileFree(StanFile *a) { abort(); }
static const char *why="";
static DWORD scripts, instructions, sharedscripts;
static void RequireAt(BOOL ok, int line) { if (!ok) { fprintf(stderr,"line %d: %s\n",line,why); abort(); } }
#define Require(ok) RequireAt(ok,__LINE__)
static void NativeEqual(const SetupFile *a,const SetupFile *b)
{ assert(a->size==b->size && !memcmp(a->data,b->data,a->size)); }
static void ScriptsEqual(const ActionDocument *a,const ActionDocument *b)
{
    DWORD count=0;
    for (DWORD i=0;i<a->count;i++) if (!a->blocks[i].global)
    {
        assert(count<b->count); const ActionBlock *x=&a->blocks[i],*y=&b->blocks[count++];
        assert(x->disabled==y->disabled && x->id==y->id && x->count==y->count && !strcmp(x->name,y->name));
        for (DWORD j=0;j<x->count;j++)
        {
            assert(x->instructions[j].size==y->instructions[j].size);
            assert(!memcmp(x->instructions[j].bytes,y->instructions[j].bytes,x->instructions[j].size));
            assert(!strcmp(x->instructions[j].name,y->instructions[j].name));
            assert(!strcmp(x->instructions[j].note,y->instructions[j].note));
            assert(ActionFindInstruction(x,x->instructions[j].target)==ActionFindInstruction(y,y->instructions[j].target));
        }
    }
    assert(count==b->count);
}
static SetupFile Fixture(const unsigned char *bytes,DWORD n)
{
    SetupFile s={0}; s.size=256+n; s.data=calloc(s.size,1); assert(s.data);
    strcpy(s.name,"UsetupactionsZ"); SetupMetaWrite32(s.data+20,40);
    SetupMetaWrite32(s.data+40,256); SetupMetaWrite32(s.data+44,0x401);
    SetupMetaWrite32(s.data+24,112); SetupMetaWrite32(s.data+28,156);
    memcpy(s.data+256,bytes,n); return s;
}
static void Branches(void)
{
    /* Same label number occurs twice. First and next jumps reach different rows. */
    unsigned char bytes[]={2,7,3,0,7,2,7,3,1,7,4};
    SetupFile s=Fixture(bytes,sizeof(bytes)),out={0}; ActionDocument d={0},copy={0},loaded={0};
    Require(ActionDocumentLoad(&s,&d,&why)); assert(d.blocks[0].count==7);
    ActionBlock *b=&d.blocks[0];
    assert(b->instructions[2].target==b->instructions[3].uid);
    assert(b->instructions[5].target==b->instructions[0].uid);
    Require(ActionDocumentCompile(&d,&s,&out,&why)); NativeEqual(&s,&out); SetupFileFree(&out);
    Require(ActionDocumentClone(&d,&copy,&why));
    assert(!ActionBlockMove(&d,0,2,1,&why)); ScriptsEqual(&d,&copy);
    assert(!ActionBlockDelete(&d,0,3,&why));
    DWORD values[8]={7};
    assert(!ActionInstructionSet(&d,0,2,values,b->instructions[0].uid,"","","",&why)); ScriptsEqual(&d,&copy);
    Require(ActionInstructionSet(&d,0,3,values,0,"Keep chasing","A second occurrence of label 07","",&why));
    Require(ActionDocumentCompile(&d,&s,&out,&why)); NativeEqual(&s,&out); assert(out.actionmetasize);
    Require(ActionDocumentLoad(&out,&loaded,&why)); ScriptsEqual(&d,&loaded); ActionDocumentFree(&loaded); SetupFileFree(&out);
    Require(ActionBlockMove(&d,0,1,1,&why));
    Require(ActionBlockInsert(&d,0,6,0xad,&why));
    Require(ActionInstructionSet(&d,0,6,values,0,"","Note survives editing","hello\nworld",&why));
    Require(ActionDocumentCompile(&d,&s,&out,&why));
    Require(ActionDocumentLoad(&out,&loaded,&why)); ScriptsEqual(&d,&loaded);
    assert(!memcmp(s.data+24,out.data+24,s.size-24)); /* Only header table pointer changed in original prefix. */
    ActionDocumentFree(&loaded); SetupFileFree(&out); ActionDocumentFree(&copy); ActionDocumentFree(&d); SetupFileFree(&s);
}
static void Malformed(void)
{
    const unsigned char cases[][5]={{0xfd,4},{0xad,'a','b','c','d'},{5,253,4,1,3},{0,99,4},{2,0,1,0,4}};
    for (unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++)
    {
        SetupFile s=Fixture(cases[i],5),out={0}; ActionDocument d={0}; ActionIssue *issues=NULL; DWORD count=0;
        if (i<3) { assert(!ActionDocumentLoad(&s,&d,&why)); }
        else
        {
            Require(ActionDocumentLoad(&s,&d,&why)); Require(ActionDocumentValidate(&d,&s,&issues,&count,&why)); assert(count);
            if (i==3)
            {
                assert(!issues[0].error); Require(ActionDocumentCompile(&d,&s,&out,&why)); NativeEqual(&s,&out); SetupFileFree(&out);
                DWORD copy; Require(ActionDocumentAddBlock(&d,0,FALSE,&copy,&why));
                assert(!ActionDocumentCompile(&d,&s,&out,&why));
            }
            else { assert(!issues[0].error); Require(ActionDocumentCompile(&d,&s,&out,&why)); }
        }
        free(issues); ActionDocumentFree(&d); SetupFileFree(&s); SetupFileFree(&out);
    }
    unsigned char end[]={4}; SetupFile s=Fixture(end,1); ActionDocument d={0};
    SetupMetaWrite32(s.data+20,s.size-1); assert(!ActionDocumentLoad(&s,&d,&why)); SetupFileFree(&s);
}
static void OffsetLimit(void)
{
    unsigned char *bytes=malloc(65537); assert(bytes); memset(bytes,'x',65537);
    bytes[0]=0xad; bytes[65534]=0; bytes[65535]=4;
    SetupFile s=Fixture(bytes,65536); ActionDocument d={0};
    Require(ActionDocumentLoad(&s,&d,&why)); assert(d.blocks[0].sourcesize==65536);
    assert(!ActionBlockInsert(&d,0,1,3,&why));
    bytes[65534]='x'; bytes[65535]=0; bytes[65536]=4;
    DWORD values[8]={0}; assert(!ActionInstructionSet(&d,0,0,values,0,"","",(char *)bytes+1,&why));
    assert(d.blocks[0].instructions[0].size==65535);
    ActionDocumentFree(&d); SetupFileFree(&s); s=Fixture(bytes,65537);
    assert(!ActionDocumentLoad(&s,&d,&why)); SetupFileFree(&s); free(bytes);
}
static void Parameters(void)
{
#include "layouts.inc"
    for (unsigned op=0;op<ACTION_OPCODE_COUNT;op++) for (unsigned p=0;p<g_ActionOpcodes[op].paramcount;p++)
    {
        const ActionParam *param=&g_ActionOpcodes[op].params[p]; DWORD max=param->width==4 ? 0xffffffff : (1u<<(8*param->width))-1;
        DWORD cases[]={0,1,max/2,max};
        for (unsigned i=0;i<4;i++)
        {
            char s[64]; DWORD result=0; snprintf(s,sizeof(s),"%.12g",ActionDisplayValue(param,cases[i]));
            assert(ActionParseValue(param,s,&result)); assert(result==cases[i]);
        }
        DWORD result; assert(!ActionParseValue(param,"nan",&result)); assert(!ActionParseValue(param,"-1",&result));
        assert(!ActionParseValue(param,"2oops",&result)); assert(!ActionParseValue(param,"",&result));
    }
}
static void Persistence(const char *dir)
{
    unsigned char bytes[]={2,0,3,1,0,4}; SetupFile s=Fixture(bytes,sizeof(bytes)),out={0},saved={0},reloaded={0};
    ActionDocument d={0},after={0}; DWORD index,values[8]={0};
    s.charactercount=1; s.characters=calloc(1,sizeof(*s.characters)); assert(s.characters);
    s.characters[0].sourceoffset=80; s.characters[0].ailistid=0x401; s.data[83]=9;
    s.data[90]=4; s.data[91]=1; SetupMetaWrite32(s.data+12,80); s.data[111]=0x30;
    Require(ActionDocumentLoad(&s,&d,&why));
    assert(!ActionDocumentDeleteBlock(&d,&s,0,&why));
    Require(ActionDocumentAddBlock(&d,0,FALSE,&index,&why)); assert(d.blocks[index].id==0x402);
    Require(ActionDocumentAssign(&d,0,index,&why));
    Require(ActionInstructionSet(&d,index,0,values,0,"Look for Bond","My note","",&why));
    Require(ActionDocumentDeleteBlock(&d,&s,0,&why));
    Require(ActionDocumentCompile(&d,&s,&out,&why)); assert(out.characters[0].ailistid==0x402 && out.data[91]==2);
    Require(SetupSaveProjectFile(dir,&out,&why)); Require(SetupLoadProjectFile(dir,out.name,&saved,&why));
    unsigned char *packed; DWORD packedsize;
    Require(SetupCompactNative(out.data,out.size,&packed,&packedsize,&why));
    assert(saved.size==packedsize && !memcmp(saved.data,packed,packedsize)); free(packed);
    assert(saved.actionmetasize==out.actionmetasize && !memcmp(saved.actionmeta,out.actionmeta,out.actionmetasize));
    Require(ActionDocumentLoad(&saved,&after,&why)); ScriptsEqual(&d,&after); ActionDocumentFree(&after);
    /* A failed atomic rename must leave the previous native bytes AND notes. */
    out.data[250]=99; test_fail_move=1; assert(!SetupSaveProjectFile(dir,&out,&why)); out.data[250]=0;
    Require(SetupLoadProjectFile(dir,out.name,&reloaded,&why)); NativeEqual(&saved,&reloaded);
    assert(!memcmp(saved.actionmeta,reloaded.actionmeta,saved.actionmetasize)); SetupFileFree(&reloaded);
    char path[512]; snprintf(path,sizeof(path),"%s/setup/%s.set",dir,out.name);
    DWORD n; unsigned char *native=TestReadResource(path,out.name,&n,&why); assert(native && n==saved.size && !memcmp(native,saved.data,n)); free(native);
    /* Genuine project history deep-copies the separate metadata allocation. */
    EditHistory history={0}; EditHistoryTransaction transaction={0}; BgDocument bg={0}; StanFile stan={0}; EditHistoryAsset asset;
    EditHistoryReset(&history,&bg,&s,&stan); Require(EditHistoryBeginSetupEdit(&history,&s,"Action Blocks",&transaction,&why));
    SetupFileFree(&s); Require(SetupFileClone(&out,&s,&why));
    Require(EditHistoryCommitEdit(&history,&bg,&s,&stan,&transaction,&why));
    Require(EditHistoryUndo(&history,&bg,&s,&stan,&asset,&why)); assert(!s.actionmetasize && s.characters[0].ailistid==0x401);
    Require(EditHistoryRedo(&history,&bg,&s,&stan,&asset,&why)); assert(s.actionmetasize && s.characters[0].ailistid==0x402);
    EditHistoryFree(&history);
    /* Model and spawn placement rebuild the setup; notes must remain owned. */
    double position[3]={100,200,300}; DWORD selection;
    Require(SetupFileAddModel(&saved,FALSE,1,1.0f,position,&selection,&why));
    assert(saved.actionmetasize==out.actionmetasize && !memcmp(saved.actionmeta,out.actionmeta,out.actionmetasize));
    SetupMarkerRef marker; Require(SetupFilePlaceSpawn(&saved,1.0f,position,&marker,&why));
    assert(saved.actionmetasize==out.actionmetasize && !memcmp(saved.actionmeta,out.actionmeta,out.actionmetasize));
    ActionDocumentFree(&d); SetupFileFree(&s); SetupFileFree(&out); SetupFileFree(&saved);
}
static void Disabled(const char *dir)
{
    const unsigned char code[]={2,0,3,1,0,4};
    SetupFile source=Fixture(code,sizeof(code)),edited={0},saved={0},again={0};
    ActionDocument d={0},loaded={0},copy={0}; DWORD size,packedsize;
    unsigned char *runtime=NULL,*packed=NULL; char path[512],title[160];
    /* Three table occurrences share one script. The third even shares the
     * first ID: automatic level threads must remain individually addressable. */
    for (DWORD i=0;i<3;i++)
    {
        SetupMetaWrite32(source.data+40+i*8,256);
        SetupMetaWrite32(source.data+44+i*8,i==1 ? 0x1003 : 0x1002);
    }
    Require(ActionDocumentLoad(&source,&d,&why)); assert(d.count==3);
    Require(ActionDocumentSetEnabled(&d,0,TRUE,&why)); assert(!d.changed);
    assert(!ActionDocumentSetEnabled(&d,99,FALSE,&why));
    strcpy(d.blocks[0].name,"Guard spawner"); strcpy(d.blocks[0].instructions[0].note,"Keep this original loop");
    Require(ActionDocumentSetEnabled(&d,0,FALSE,&why));
    Require(ActionDocumentSetEnabled(&d,2,FALSE,&why));
    assert(d.changed && !d.blocks[0].changed && !d.blocks[2].changed);
    ActionBlockTitle(&d.blocks[0],title,sizeof(title)); assert(strstr(title,"[Disabled]"));
    Require(ActionDocumentClone(&d,&copy,&why)); ScriptsEqual(&d,&copy);
    DWORD duplicate; Require(ActionDocumentAddBlock(&copy,0,TRUE,&duplicate,&why));
    assert(copy.blocks[duplicate].disabled); ActionDocumentFree(&copy);
    Require(ActionDocumentCompile(&d,&source,&edited,&why)); NativeEqual(&source,&edited);
    assert(!memcmp(edited.actionmeta,"AIN2",4));
    Require(SetupSaveProjectFile(dir,&edited,&why));
    Require(SetupLoadProjectFile(dir,edited.name,&saved,&why));
    Require(ActionDocumentLoad(&saved,&loaded,&why)); ScriptsEqual(&d,&loaded);
    ActionDocumentFree(&loaded);
    Require(ActionSetupBuildRuntime(&saved,&runtime,&size,&why)); assert(runtime);
    DWORD table=SetupMetaRead32(runtime+20),stub=SetupMetaRead32(runtime+table);
    assert(runtime[stub]==4 && SetupMetaRead32(runtime+table+16)==stub);
    assert(!memcmp(runtime+SetupMetaRead32(runtime+table+8),code,sizeof(code)));
    Require(SetupCompactNative(runtime,size,&packed,&packedsize,&why));
    SetupFile game={0}; game.data=packed; game.size=packedsize;
    Require(ActionDocumentLoad(&game,&loaded,&why)); assert(loaded.count==3);
    for (DWORD i=0;i<3;i++)
    {
        assert(loaded.blocks[i].id==d.blocks[i].id);
        assert(loaded.blocks[i].count==(i==1 ? 4u : 1u));
        if (i!=1) { assert(loaded.blocks[i].instructions[0].bytes[0]==4); }
    }
    ActionDocumentFree(&loaded); free(packed);
    /* Exercise the actual ROM resource reader, including metadata stripping.
     * Reading/exporting must not persist the disabled stub into the project. */
    snprintf(path,sizeof(path),"%s/setup/%s.set",dir,saved.name);
    unsigned char *exported=TestReadResource(path,saved.name,&packedsize,&why);
    assert(exported && packedsize==size && !memcmp(exported,runtime,size));
    free(exported); free(runtime); runtime=NULL;
    Require(SetupLoadProjectFile(dir,saved.name,&again,&why)); NativeEqual(&saved,&again);
    assert(saved.actionmetasize==again.actionmetasize && !memcmp(saved.actionmeta,again.actionmeta,saved.actionmetasize));
    SetupFileFree(&again);
    /* Existing setup undo/redo must restore the switches together with notes. */
    EditHistory history={0}; EditHistoryTransaction transaction={0};
    BgDocument bg={0}; StanFile stan={0}; EditHistoryAsset asset;
    EditHistoryReset(&history,&bg,&source,&stan);
    Require(EditHistoryBeginSetupEdit(&history,&source,"Action Blocks",&transaction,&why));
    SetupFileFree(&source); Require(SetupFileClone(&saved,&source,&why));
    Require(EditHistoryCommitEdit(&history,&bg,&source,&stan,&transaction,&why));
    Require(EditHistoryUndo(&history,&bg,&source,&stan,&asset,&why));
    Require(ActionDocumentLoad(&source,&loaded,&why)); assert(!loaded.blocks[0].disabled);
    ActionDocumentFree(&loaded);
    Require(EditHistoryRedo(&history,&bg,&source,&stan,&asset,&why));
    Require(ActionDocumentLoad(&source,&loaded,&why)); ScriptsEqual(&d,&loaded);
    ActionDocumentFree(&loaded); EditHistoryFree(&history);
    /* Repeated toggles never append bytecode or native tables to the project. */
    ActionDocumentFree(&d); Require(ActionDocumentLoad(&saved,&d,&why));
    for (int i=0;i<100;i++)
    {
        Require(ActionDocumentSetEnabled(&d,0,i&1,&why));
        Require(ActionDocumentCompile(&d,&saved,&again,&why)); NativeEqual(&saved,&again);
        SetupFileFree(&again);
    }
    Require(ActionDocumentSetEnabled(&d,2,TRUE,&why));
    Require(ActionDocumentCompile(&d,&saved,&again,&why)); NativeEqual(&saved,&again);
    assert(!memcmp(again.actionmeta,"AIN1",4));
    Require(ActionSetupBuildRuntime(&again,&runtime,&size,&why)); assert(!runtime && size==saved.size);
    Require(ActionDocumentLoad(&again,&loaded,&why)); ScriptsEqual(&d,&loaded);
    ActionDocumentFree(&loaded); SetupFileFree(&again);
    /* Unknown flags and damaged metadata fail export instead of silently
     * enabling a supposedly disabled script. */
    saved.actionmeta[19]=2;
    assert(!ActionSetupBuildRuntime(&saved,&runtime,&size,&why) && !runtime);
    saved.actionmeta[19]=1;
    Require(ActionSetupBuildRuntime(&saved,&runtime,&size,&why)); free(runtime);
    ActionDocumentFree(&d); SetupFileFree(&source); SetupFileFree(&edited); SetupFileFree(&saved);
    puts("Disabled blocks: aliases/duplicate IDs, native preservation, save/reload, undo/redo, export/compaction, exact re-enable and malformed metadata passed.");
}
static void Vehicle(void)
{
    unsigned char bytes[]={3,1,0,2,0,4}; SetupFile s=Fixture(bytes,sizeof(bytes)); ActionDocument d={0};
    s.objectcount=1; s.objects=calloc(1,sizeof(*s.objects)); s.objects[0].sourceoffset=80; s.objects[0].type=39;
    SetupMetaWrite32(s.data+80+0x80,0x401); Require(ActionDocumentLoad(&s,&d,&why));
    assert(!ActionDocumentDeleteBlock(&d,&s,0,&why)); assert(strstr(why,"vehicle"));
    ActionDocumentFree(&d); SetupFileFree(&s);
}
static void Globals(void)
{
    ActionDocument d={0}; RomFile r={0}; const unsigned char code[]={2,0,3,1,0,4}; DWORD index;
    r.size=128; r.data=calloc(r.size,1); r.info.entrycount=2;
    r.info.entries[0]=(RomManifestEntry){0x434d4150,0,128,0x80000000};
    r.info.entries[1]=(RomManifestEntry){0x4149474c,16,32,0};
    SetupMetaWrite32(r.data+16,0x80000030); SetupMetaWrite32(r.data+20,1); SetupMetaWrite32(r.data+24,8); SetupMetaWrite32(r.data+28,1);
    SetupMetaWrite32(r.data+48,0x80000050); SetupMetaWrite32(r.data+52,2); memcpy(r.data+80,code,sizeof(code));
    d.nextuid=1; Require(ActionDocumentLoadGlobals(&d,&r,&why)); assert(d.count==1 && d.blocks[0].global && d.blocks[0].id==2);
    assert(!ActionDocumentSetEnabled(&d,0,FALSE,&why));
    assert(!ActionBlockDelete(&d,0,1,&why)); Require(ActionDocumentAddBlock(&d,0,FALSE,&index,&why)); assert(!d.blocks[index].global);
    ActionDocumentFree(&d); r.data[28]=99; assert(!ActionDocumentLoadGlobals(&d,&r,&why)); assert(d.count==0);
    ActionDocumentFree(&d); free(r.data);
}
static void Stock(const char *dir,const char *name)
{
    SetupFile s={0},out={0}; ActionDocument d={0},reloaded={0}; ActionIssue *issues=NULL; DWORD count;
    if (!SetupLoadProjectFile(dir,name,&s,&why)) { fprintf(stderr,"%s: %s\n",name,why); abort(); }
    Require(ActionDocumentLoad(&s,&d,&why));
    Require(ActionDocumentValidate(&d,&s,&issues,&count,&why));
    for (DWORD i=0;i<count;i++) if (issues[i].error)
    { fprintf(stderr,"%s block %04lX row %lu: %s\n",name,(unsigned long)d.blocks[issues[i].block].id,(unsigned long)issues[i].instruction,issues[i].text); }
    free(issues); Require(ActionDocumentCompile(&d,&s,&out,&why)); NativeEqual(&s,&out); SetupFileFree(&out);
    for (DWORD b=0;b<d.count;b++)
    {
        scripts++; instructions+=d.blocks[b].count;
        for (DWORD i=0;i<d.blocks[b].count;i++)
        {
            DWORD values[8]={0}; const ActionInstruction *ins=&d.blocks[b].instructions[i]; const ActionOpcode *op=&g_ActionOpcodes[ins->bytes[0]];
            for (int p=0;p<op->paramcount;p++) { values[p]=ActionReadValue(ins,p); }
            Require(ActionInstructionSet(&d,b,i,values,ins->target,ins->name,ins->note,ins->bytes[0]==0xad ? (char *)ins->bytes+1 : "",&why));
        }
    }
    assert(!d.changed); Require(ActionDocumentCompile(&d,&s,&out,&why)); NativeEqual(&s,&out); SetupFileFree(&out);
    /* Table occurrences, including duplicate IDs, own distinct project names. */
    for (DWORD b=0;b<d.count;b++)
    {
        snprintf(d.blocks[b].name,sizeof(d.blocks[b].name),"Stock entry %lu",(unsigned long)b);
        snprintf(d.blocks[b].instructions[0].note,ACTION_NOTE_SIZE,"Script %lu note",(unsigned long)b);
    }
    Require(ActionDocumentCompile(&d,&s,&out,&why)); NativeEqual(&s,&out);
    Require(ActionDocumentLoad(&out,&reloaded,&why)); ScriptsEqual(&d,&reloaded);
    ActionDocumentFree(&reloaded); SetupFileFree(&out);
    /* Exercise selected spawn scripts in a real saved setup as well. Every
     * other script and ID must survive the runtime compactor unchanged. */
    DWORD disabled=0;
    for (DWORD b=0;b<d.count;b++) for (DWORD i=0;i<d.blocks[b].count;i++)
    {
        unsigned int op=d.blocks[b].instructions[i].bytes[0];
        if (op==0xbd || op==0xbe)
        {
            Require(ActionDocumentSetEnabled(&d,b,FALSE,&why)); disabled++;
            printf("%s: disable spawn block %04lX\n",name,(unsigned long)d.blocks[b].id); break;
        }
    }
    if (disabled)
    {
        unsigned char *runtime,*packed; DWORD size,packedsize; SetupFile game={0};
        Require(ActionDocumentCompile(&d,&s,&out,&why)); NativeEqual(&s,&out);
        Require(ActionSetupBuildRuntime(&out,&runtime,&size,&why)); assert(runtime);
        Require(SetupCompactNative(runtime,size,&packed,&packedsize,&why)); free(runtime);
        game.data=packed; game.size=packedsize;
        Require(ActionDocumentLoad(&game,&reloaded,&why)); assert(reloaded.count==d.count);
        for (DWORD b=0;b<d.count;b++)
        {
            const ActionBlock *before=&d.blocks[b],*after=&reloaded.blocks[b];
            assert(before->id==after->id);
            if (before->disabled) { assert(after->count==1 && after->instructions[0].bytes[0]==4); }
            else
            {
                assert(before->count==after->count);
                for (DWORD i=0;i<before->count;i++)
                { assert(before->instructions[i].size==after->instructions[i].size);
                  assert(!memcmp(before->instructions[i].bytes,after->instructions[i].bytes,before->instructions[i].size)); }
            }
        }
        free(packed);
    }
    ActionDocumentFree(&reloaded); ActionDocumentFree(&d); SetupFileFree(&s); SetupFileFree(&out);
}
static void StockGlobals(const char *dir)
{
    char path[512]; snprintf(path,sizeof(path),"%s/shared.bin",dir);
    FILE *f=fopen(path,"rb"); if (!f) { return; }
    unsigned char header[8]; assert(fread(header,1,8,f)==8);
    DWORD catalog=SetupMetaRead32(header); RomFile r={0}; r.size=SetupMetaRead32(header+4);
    r.data=malloc(r.size); assert(r.data && fread(r.data,1,r.size,f)==r.size); fclose(f);
    r.info.entrycount=2; r.info.entries[0]=(RomManifestEntry){0x434d4150,0,r.size,0x80000000};
    r.info.entries[1]=(RomManifestEntry){0x4149474c,catalog,catalog+16,0};
    ActionDocument d={0},loaded={0}; d.nextuid=1; Require(ActionDocumentLoadGlobals(&d,&r,&why));
    sharedscripts=d.count; assert(sharedscripts==18);
    for (DWORD i=0;i<sharedscripts;i++)
    {
        DWORD index; Require(ActionDocumentAddBlock(&d,i,FALSE,&index,&why));
        assert(d.blocks[index].count==d.blocks[i].count);
        for (DWORD j=0;j<d.blocks[i].count;j++)
        { assert(d.blocks[index].instructions[j].size==d.blocks[i].instructions[j].size);
          assert(!memcmp(d.blocks[index].instructions[j].bytes,d.blocks[i].instructions[j].bytes,d.blocks[i].instructions[j].size)); }
    }
    const unsigned char end[]={4}; SetupFile s=Fixture(end,1),out={0}; SetupMetaWrite32(s.data+20,0);
    Require(ActionDocumentCompile(&d,&s,&out,&why)); Require(ActionDocumentLoad(&out,&loaded,&why)); ScriptsEqual(&d,&loaded);
    ActionDocumentFree(&loaded); ActionDocumentFree(&d); SetupFileFree(&s); SetupFileFree(&out); free(r.data);
}
int main(int argc,char **argv)
{
    assert(argc>=2); Parameters(); Branches(); Malformed(); OffsetLimit();
    Persistence(argv[1]); Disabled(argv[1]); Vehicle(); Globals(); StockGlobals(argv[1]);
    for (int i=2;i<argc;i++) { Stock(argv[1],argv[i]); }
    printf("Action Blocks: safety, metadata, atomic save, history, export and %lu stock scripts / %lu instructions and %lu shared scripts passed.\n",(unsigned long)scripts,(unsigned long)instructions,(unsigned long)sharedscripts);
    return 0;
}
