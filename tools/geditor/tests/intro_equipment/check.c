#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include "setupload.h"
#include "setupmeta.h"
#include "bghistory.h"
#include "actionblocks.h"
static char g_RomExportError[512];
#include "export.inc"
#define R SetupMetaRead32
#define W SetupMetaWrite32
static const char *why = "", *dir;
static int failat = -1, allocations;
void *__real_malloc(size_t); void *__real_calloc(size_t,size_t); void *__real_realloc(void *,size_t);
static BOOL Fail(void) { return failat >= 0 && allocations++ == failat; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t z) { return Fail() ? NULL : __real_calloc(n,z); }
void *__wrap_realloc(void *p,size_t n) { return Fail() ? NULL : __real_realloc(p,n); }
void BgDocumentFree(BgDocument *d) { memset(d,0,sizeof(*d)); }
void StanFileFree(StanFile *s) { memset(s,0,sizeof(*s)); }
static void Require(BOOL ok) { if (!ok) { fprintf(stderr,"%s\n",why); abort(); } }
static SetupIntroEntry *Entries(const SetupFile *s, DWORD expected)
{ SetupIntroEntry *entries; DWORD count; Require(SetupFileGetIntroEquipment(s,&entries,&count,&why)); assert(count==expected); return entries; }
static void Same(const SetupFile *a,const SetupFile *b)
{
    assert(a->size==b->size && !memcmp(a->data,b->data,a->size));
    assert(a->objectcount==b->objectcount && a->charactercount==b->charactercount);
    assert(a->padcount==b->padcount && a->boundpadcount==b->boundpadcount);
    if(a->objectcount)assert(!memcmp(a->objects,b->objects,a->objectcount*sizeof(*a->objects)));
    if(a->charactercount)assert(!memcmp(a->characters,b->characters,a->charactercount*sizeof(*a->characters)));
    assert(a->actionmetasize==b->actionmetasize);
    if(a->actionmetasize)assert(!memcmp(a->actionmeta,b->actionmeta,a->actionmetasize));
}
static void Apply(SetupFile *s, SetupIntroAction action, SetupIntroEntry entry)
{ SetupIntroEdit edit={action,entry}; BOOL changed; Require(SetupFileEditIntroEquipment(s,&edit,&changed,&why)); assert(changed && s->dirty); }
static void Persist(SetupFile *s)
{
    SetupFile loaded={0}; Require(SetupSaveProjectFile(dir,s,&why));
    Require(SetupLoadProjectFile(dir,s->name,&loaded,&why)); Same(s,&loaded); SetupFileFree(&loaded);
}
static void Edits(const SetupFile *source)
{
    SetupFile s={0}, before={0}; Require(SetupFileClone(source,&s,&why));
    SetupIntroEntry *e=Entries(&s,4);
    assert(e[0].command==1 && e[0].value[0]==4 && e[0].value[1]==-1);
    assert(e[1].command==2 && e[1].value[0]==AMMO_9MM && e[1].value[1]==100);
    assert(e[2].command==7 && e[3].command==8);
    e[0].value[0]=13; e[0].value[1]=13; Apply(&s,SETUP_INTRO_UPDATE,e[0]);
    e[1].value[1]=250; Apply(&s,SETUP_INTRO_UPDATE,e[1]);
    Apply(&s,SETUP_INTRO_UP,e[2]); free(e);
    e=Entries(&s,4); assert(e[0].value[0]==8 && e[2].value[0]==13 && e[2].value[1]==13);
    Apply(&s,SETUP_INTRO_REMOVE,e[0]); free(e);
    e=Entries(&s,3); assert(e[0].command==1 && e[0].type==SETUP_INTRO_AMMO && e[1].command==6); free(e);
    /* Gadgets and dual pairs do not depend on placed-character model choices. */
    Apply(&s,SETUP_INTRO_ADD,(SetupIntroEntry){0,SETUP_INTRO_WEAPON,{40,-1}});
    Apply(&s,SETUP_INTRO_ADD,(SetupIntroEntry){0,SETUP_INTRO_AMMO,{AMMO_CAMERA,1}});
    Persist(&s);
    Require(SetupFileClone(&s,&before,&why));
    e=Entries(&s,5); SetupIntroEdit noop={SETUP_INTRO_UPDATE,e[0]}; BOOL changed=TRUE;
    Require(SetupFileEditIntroEquipment(&s,&noop,&changed,&why)); assert(!changed); Same(&s,&before);
    noop.action=SETUP_INTRO_UP; Require(SetupFileEditIntroEquipment(&s,&noop,&changed,&why)); assert(!changed); free(e);
    /* No abandoned-table accumulation after hundreds of add/remove cycles. */
    DWORD maximum=s.size;
    for(int i=0;i<120;i++)
    {
        Apply(&s,SETUP_INTRO_ADD,(SetupIntroEntry){0,SETUP_INTRO_WEAPON,{5,5}});
        e=Entries(&s,6); Apply(&s,SETUP_INTRO_REMOVE,e[5]); free(e);
        assert(s.size<=maximum+32);
    }
    /* No UI-sized row limit. */
    for(int i=0;i<100;i++)Apply(&s,SETUP_INTRO_ADD,(SetupIntroEntry){0,SETUP_INTRO_WEAPON,{4,-1}});
    e=Entries(&s,105); free(e);
    while(1)
    {
        DWORD count; Require(SetupFileGetIntroEquipment(&s,&e,&count,&why));
        if(count==5){free(e);break;} Apply(&s,SETUP_INTRO_REMOVE,e[count-1]); free(e);
    }
    Persist(&s);
    /* Preserve editor metadata across native edits and project reload. */
    s.actionmeta=malloc(4); memcpy(s.actionmeta,"test",4); s.actionmetasize=4;
    Apply(&s,SETUP_INTRO_ADD,(SetupIntroEntry){0,SETUP_INTRO_AMMO,{AMMO_TANK,2147483647}});
    Persist(&s); assert(s.actionmetasize==4 && !memcmp(s.actionmeta,"test",4));
    free(s.actionmeta); s.actionmeta=NULL; s.actionmetasize=0;
    strcpy(s.name,"Usetupintro_finalZ"); Persist(&s);
    char path[1024]; snprintf(path,sizeof(path),"%s/setup/%s.set",dir,s.name);
    DWORD size;unsigned char *runtime=RomExportReadResource(path,s.name,&size,&why);Require(runtime!=NULL);
    assert(size==s.size && !memcmp(runtime,s.data,size));free(runtime);
    SetupFileFree(&s); SetupFileFree(&before);
    puts("PASS: normal-play equipment, dual weapons/gadgets, ordering, removal, quantities, no-ops, 100+ rows, bounded size, project reload and ROM resource export.");
}
static void Failures(const SetupFile *source)
{
    for(int action=SETUP_INTRO_ADD;action<=SETUP_INTRO_DOWN;action++)
    {
        BOOL completed=FALSE;
        for(int fault=0;fault<80 && !completed;fault++)
        {
            SetupFile s={0}; Require(SetupFileClone(source,&s,&why));
            SetupIntroEdit edit={(SetupIntroAction)action,{action==SETUP_INTRO_UP?7:1,SETUP_INTRO_WEAPON,{13,13}}};
            BOOL changed=TRUE; allocations=0; failat=fault;
            completed=SetupFileEditIntroEquipment(&s,&edit,&changed,&why); failat=-1;
            if(!completed){assert(!changed);Same(&s,source);assert(s.dirty==source->dirty);} else assert(changed);
            SetupFileFree(&s);
        }
        assert(completed);
    }
    SetupIntroEdit invalid[]={
        {SETUP_INTRO_ADD,{0,SETUP_INTRO_WEAPON,{-1,-1}}},
        {SETUP_INTRO_ADD,{0,SETUP_INTRO_WEAPON,{999,-1}}},
        {SETUP_INTRO_ADD,{0,SETUP_INTRO_WEAPON,{4,-2}}},
        {SETUP_INTRO_ADD,{0,SETUP_INTRO_AMMO,{AMMOTYPE_MAX,10}}},
        {SETUP_INTRO_ADD,{0,SETUP_INTRO_AMMO,{AMMO_9MM,-1}}},
        {SETUP_INTRO_REMOVE,{4,SETUP_INTRO_WEAPON,{5,5}}}, /* demo */
        {SETUP_INTRO_REMOVE,{2,SETUP_INTRO_WEAPON,{4,-1}}}, /* wrong type */
        {SETUP_INTRO_REMOVE,{999,SETUP_INTRO_WEAPON,{4,-1}}}};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++)
    { SetupFile s={0};BOOL changed;Require(SetupFileClone(source,&s,&why));assert(!SetupFileEditIntroEquipment(&s,&invalid[i],&changed,&why));assert(!changed);Same(&s,source);SetupFileFree(&s); }
    for(int bad=0;bad<4;bad++)
    {
        SetupFile s={0};Require(SetupFileClone(source,&s,&why)); DWORD start=R(s.data+8),count;SetupIntroEntry *e=(void *)1;
        if(bad==0)W(s.data+8,s.size+4);
        if(bad==1)W(s.data+start,10);
        if(bad==2)W(s.data+8,start+1);
        if(bad==3)s.size=start+8;
        assert(!SetupFileGetIntroEquipment(&s,&e,&count,&why) && !e && !count);SetupFileFree(&s);
    }
    SetupFile empty={0};Require(SetupFileClone(source,&empty,&why));W(empty.data+8,0);strcpy(empty.name,"UsetupemptyZ");
    SetupIntroEntry *e=Entries(&empty,0);free(e);
    Apply(&empty,SETUP_INTRO_ADD,(SetupIntroEntry){0,SETUP_INTRO_WEAPON,{0,-1}});
    e=Entries(&empty,1);Apply(&empty,SETUP_INTRO_REMOVE,e[0]);free(e);e=Entries(&empty,0);free(e);
    Persist(&empty);SetupFileFree(&empty);
    puts("PASS: invalid IDs/quantities/stale and demo rows, malformed tables, empty intro lists and allocation-failure atomicity.");
}
static void History(const SetupFile *source)
{
    SetupFile s={0},after={0};EditHistory h={0};EditHistoryTransaction tx={0};EditHistoryAsset asset;BgDocument bg={0};StanFile stan={0};
    Require(SetupFileClone(source,&s,&why));EditHistoryReset(&h,&bg,&s,&stan);
    Require(EditHistoryBeginSetupEdit(&h,&s,"Intro",&tx,&why));
    Apply(&s,SETUP_INTRO_ADD,(SetupIntroEntry){0,SETUP_INTRO_WEAPON,{5,5}});
    Require(EditHistoryCommitEdit(&h,&bg,&s,&stan,&tx,&why));assert(s.dirty);
    Require(SetupFileClone(&s,&after,&why));
    Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why));assert(asset==EDIT_HISTORY_ASSET_SETUP && !s.dirty);Same(&s,source);
    Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why));assert(s.dirty);Same(&s,&after);
    Require(EditHistoryBeginSetupEdit(&h,&s,"Cancelled",&tx,&why));Apply(&s,SETUP_INTRO_ADD,(SetupIntroEntry){0,SETUP_INTRO_AMMO,{AMMO_9MM,10}});
    EditHistoryRollbackEdit(&tx,&bg,&s,&stan);Same(&s,&after);
    EditHistoryFree(&h);SetupFileFree(&after);SetupFileFree(&s);
    puts("PASS: shared setup undo/redo, dirty state and rollback.");
}
#include "quantity.inc"
int main(int argc,char **argv)
{
    setvbuf(stdout,NULL,_IONBF,0);
    assert(argc==2);dir=argv[1];SetupFile source={0};Require(SetupLoadProjectFile(dir,"UsetupintroZ",&source,&why));
    Edits(&source);Failures(&source);History(&source);SetupFileFree(&source);
    LONG q;assert(LevelManagerParseQuantity("0",&q)&&q==0);assert(LevelManagerParseQuantity("2147483647",&q)&&q==2147483647);
    assert(!LevelManagerParseQuantity("2147483648",&q));assert(!LevelManagerParseQuantity("999999999999999999999999999",&q));
    assert(!LevelManagerParseQuantity("",&q));assert(!LevelManagerParseQuantity("-1",&q));assert(!LevelManagerParseQuantity("1.5",&q));assert(!LevelManagerParseQuantity("1x",&q));
    return 0;
}
