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
static void Require(BOOL ok) { if (!ok) { fprintf(stderr,"%s\n",why); abort(); } }
static void NativeEqual(const SetupFile *a,const SetupFile *b)
{ assert(a->size==b->size && !memcmp(a->data,b->data,a->size)); }
static void ScriptsEqual(const ActionDocument *a,const ActionDocument *b)
{
    DWORD count=0;
    for (DWORD i=0;i<a->count;i++) if (!a->blocks[i].global)
    {
        assert(count<b->count); const ActionBlock *x=&a->blocks[i],*y=&b->blocks[count++];
        assert(x->id==y->id && x->count==y->count && !strcmp(x->name,y->name));
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
    Require(SetupSaveProjectFile(dir,&out,&why)); Require(SetupLoadProjectFile(dir,out.name,&saved,&why)); NativeEqual(&out,&saved);
    assert(saved.actionmetasize==out.actionmetasize && !memcmp(saved.actionmeta,out.actionmeta,out.actionmetasize));
    Require(ActionDocumentLoad(&saved,&after,&why)); ScriptsEqual(&d,&after); ActionDocumentFree(&after);
    /* A failed atomic rename must leave the previous native bytes AND notes. */
    out.data[250]=99; test_fail_move=1; assert(!SetupSaveProjectFile(dir,&out,&why)); out.data[250]=0;
    Require(SetupLoadProjectFile(dir,out.name,&reloaded,&why)); NativeEqual(&saved,&reloaded);
    assert(!memcmp(saved.actionmeta,reloaded.actionmeta,saved.actionmetasize)); SetupFileFree(&reloaded);
    char path[512]; snprintf(path,sizeof(path),"%s/setup/%s.set",dir,out.name);
    DWORD n; unsigned char *native=TestReadResource(path,out.name,&n,&why); assert(native && n==out.size && !memcmp(native,out.data,n)); free(native);
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
    assert(!ActionBlockDelete(&d,0,1,&why)); Require(ActionDocumentAddBlock(&d,0,FALSE,&index,&why)); assert(!d.blocks[index].global);
    ActionDocumentFree(&d); r.data[28]=99; assert(!ActionDocumentLoadGlobals(&d,&r,&why)); assert(d.count==0);
    ActionDocumentFree(&d); free(r.data);
}
static void Stock(const char *dir,const char *name)
{
    SetupFile s={0},out={0}; ActionDocument d={0},reloaded={0}; ActionIssue *issues=NULL; DWORD count;
    Require(SetupLoadProjectFile(dir,name,&s,&why)); Require(ActionDocumentLoad(&s,&d,&why));
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
    Persistence(argv[1]); Vehicle(); Globals(); StockGlobals(argv[1]);
    for (int i=2;i<argc;i++) { Stock(argv[1],argv[i]); }
    printf("Action Blocks: safety, metadata, atomic save, history, export and %lu stock scripts / %lu instructions and %lu shared scripts passed.\n",(unsigned long)scripts,(unsigned long)instructions,(unsigned long)sharedscripts);
    return 0;
}
