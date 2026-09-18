#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "setupload.h"
#include "setupmeta.h"
#include "bghistory.h"
#define W SetupMetaWrite32
#define R SetupMetaRead32
static const char *why, *dir;
static int failat=-1, allocations;
void *__real_malloc(size_t n); void *__real_calloc(size_t n,size_t z); void *__real_realloc(void *p,size_t n);
static int Fail(void) { return failat>=0 && allocations++==failat; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t z) { return Fail() ? NULL : __real_calloc(n,z); }
void *__wrap_realloc(void *p,size_t n) { return Fail() ? NULL : __real_realloc(p,n); }
void BgDocumentFree(BgDocument *d) { memset(d,0,sizeof(*d)); }
void StanFileFree(StanFile *d) { memset(d,0,sizeof(*d)); }
static void Require(BOOL ok) { if (!ok) { fprintf(stderr,"%s\n",why);abort(); } }
static SetupCharacterWeaponEdit Request(const SetupFile *s,DWORD index,int hand,int item)
{
    const SetupCharacter *chr=&s->characters[index];
    return (SetupCharacterWeaponEdit){index,chr->sourceoffset,chr->chrnum,hand,item};
}
static void Same(const SetupFile *a,const SetupFile *b)
{
    assert(a->size==b->size && a->objectcount==b->objectcount && a->charactercount==b->charactercount);
    assert(!memcmp(a->data,b->data,a->size));
    assert(!memcmp(a->objects,b->objects,a->objectcount*sizeof(*a->objects)));
    assert(!memcmp(a->characters,b->characters,a->charactercount*sizeof(*a->characters)));
    assert(a->actionmetasize==b->actionmetasize);
    if(a->actionmetasize)assert(!memcmp(a->actionmeta,b->actionmeta,a->actionmetasize));
}
static void Reload(SetupFile *s)
{
    SetupFile loaded={0};Require(SetupSaveProjectFile(dir,s,&why));
    Require(SetupLoadProjectFile(dir,s->name,&loaded,&why));Same(s,&loaded);
    SetupFileFree(s);*s=loaded;
}
static void Guard(unsigned char *p,int id)
{ W(p,9);W(p+4,(DWORD)id<<16);W(p+8,0x10001);W(p+12,0xffffffff);W(p+20,0xffff); }
static void Weapon(unsigned char *p,int id,int item,DWORD flags,DWORD flags2)
{
    const SetupWeaponChoice *choice=SetupWeaponChoiceForItem(item);assert(choice);
    W(p,0x01800008);W(p+4,((DWORD)choice->model<<16)|id);
    W(p+8,PROPFLAG_ASSIGNEDTOCHR|flags);W(p+12,flags2);
    W(p+0x74,1000u<<16);W(p+128,((DWORD)item<<24)|0xffffff);
}
static SetupFile Fixture(void)
{
    SetupFile s={0},loaded={0};s.size=2048;s.data=calloc(1,s.size);strcpy(s.name,"UsetupweaponsZ");
    DWORD at=40;W(s.data+12,at);
    Guard(s.data+at,7);at+=28;
    Weapon(s.data+at,7,4,PROPFLAG_NO_AMMO,PROPFLAG2_NO_LOAD_SA);at+=136;
    Weapon(s.data+at,7,6,0,PROPFLAG2_NO_LOAD_A);at+=136;
    Weapon(s.data+at,7,26,PROPFLAG_WEAPON_LEFTHANDED,0);at+=136;
    Weapon(s.data+at,7,8,PROPFLAG_CONCEAL_GUN,0);at+=136;
    Weapon(s.data+at,7,10,0,0xf8);at+=136; /* unrelated deleted weapon */
    Guard(s.data+at,8);at+=28;
    Weapon(s.data+at,8,22,0,0);at+=136;
    Weapon(s.data+at,9,4,0,0);at+=136; /* before its owner: not initial equipment */
    Guard(s.data+at,9);at+=28;
    W(s.data+at,22);W(s.data+at+4,(3u<<16)|(unsigned short)-9);at+=16; /* relative tag */
    W(s.data+at,48);at+=4;
    W(s.data+24,at);W(s.data+at+16,0x3f800000);W(s.data+at+32,0x3f800000);W(s.data+at+36,2044);
    W(s.data+28,at+88);
    memcpy(s.data+2044,"pad",4);
    Require(SetupSaveProjectFile(dir,&s,&why));Require(SetupLoadProjectFile(dir,s.name,&loaded,&why));
    SetupFileFree(&s);return loaded;
}
static void Set(SetupFile *s,int chr,int hand,int item)
{
    SetupCharacterWeaponEdit edit=Request(s,chr,hand,item);BOOL changed;
    Require(SetupFileSetCharacterWeapon(s,&edit,&changed,&why));assert(changed);
}
static void Variants(const SetupFile *source)
{
    SetupFile s={0};Require(SetupFileClone(source,&s,&why));
    SetupCharacterWeapons view;const SetupObject *held[2];
    Require(SetupFileGetCharacterWeapons(&s,0,&view));
    assert(view.item[0]==SETUP_WEAPON_MIXED && view.count[0]==2 && view.item[1]==26);
    SetupFileGetCharacterHeldWeapons(&s,0,held);assert(held[0]==s.objects && held[1]==s.objects+2);
    Set(&s,0,0,13);Require(SetupFileGetCharacterWeapons(&s,0,&view));assert(view.item[0]==13 && view.count[0]==2);
    for(int i=0;i<2;i++) {
        assert(s.objects[i].modelid==SetupWeaponChoiceForItem(13)->model);
        assert(s.objects[i].flags==source->objects[i].flags && s.objects[i].flags2==source->objects[i].flags2);
        assert(s.objects[i].extrascale==source->objects[i].extrascale);
        const unsigned char *a=source->data+source->objects[i].sourceoffset,*b=s.data+s.objects[i].sourceoffset;
        for(int n=0;n<136;n++)if(n!=4 && n!=5 && n!=128)assert(a[n]==b[n]);
    }
    for(DWORD i=2;i<s.objectcount;i++)assert(!memcmp(s.data+s.objects[i].sourceoffset,source->data+source->objects[i].sourceoffset,136));
    DWORD size=s.size,count=s.objectcount;
    for(int cycle=0;cycle<100;cycle++) {
        Set(&s,0,0,-1);Require(SetupFileGetCharacterWeapons(&s,0,&view));assert(view.item[0]==-1 && view.item[1]==26);
        for(int weapon=0;weapon<2;weapon++) {
            const unsigned char *record=s.data+s.objects[weapon].sourceoffset;
            /* Tagged skipped records must never look like live prop pointers. */
            assert(!R(record+16) && !R(record+20));
            for(int difficulty=0;difficulty<4;difficulty++)assert(R(record+12)&(1u<<(difficulty+4)));
        }
        SetupFileGetCharacterHeldWeapons(&s,0,held);assert(!held[0] && held[1]);
        if(cycle==0)Reload(&s);
        Set(&s,0,0,4);
        assert(s.size==size && s.objectcount==count && s.objects[4].deleted);
        for(int i=0;i<2;i++)assert(s.objects[i].flags2==source->objects[i].flags2);
    }
    Reload(&s);
    Set(&s,0,1,-1);Require(SetupFileGetCharacterWeapons(&s,0,&view));assert(view.item[0]==4 && view.item[1]==-1);
    assert(!memcmp(s.data+s.objects[3].sourceoffset,source->data+source->objects[3].sourceoffset,136));
    SetupFile before={0};Require(SetupFileClone(&s,&before,&why));
    SetupCharacterWeaponEdit edit=Request(&s,0,1,-1);BOOL changed=TRUE;
    Require(SetupFileSetCharacterWeapon(&s,&edit,&changed,&why));assert(!changed);Same(&s,&before);
    edit.hand=2;assert(!SetupFileSetCharacterWeapon(&s,&edit,&changed,&why));Same(&s,&before);
    edit=Request(&s,0,0,255);assert(!SetupFileSetCharacterWeapon(&s,&edit,&changed,&why));Same(&s,&before);
    edit=Request(&s,0,0,6);edit.sourceoffset++;assert(!SetupFileSetCharacterWeapon(&s,&edit,&changed,&why));Same(&s,&before);
    SetupFileFree(&before);SetupFileFree(&s);
    puts("PASS: right/left ownership, mixed variants, item/model pairing, preserved flags/scale, concealed/deleted/other-owner isolation, no-op, stale edits and 100 clear/re-equip cycles without growth.");
}
static void AppendAndHistory(const SetupFile *source)
{
    SetupFile s={0},after={0};Require(SetupFileClone(source,&s,&why));
    EditHistory h={0};EditHistoryTransaction tx={0};EditHistoryAsset asset;BgDocument bg={0};StanFile stan={0};
    EditHistoryReset(&h,&bg,&s,&stan);Require(EditHistoryBeginSetupEdit(&h,&s,"Character Weapon",&tx,&why));
    SetupCharacterWeaponEdit edit=Request(&s,2,1,25);BOOL changed;
    Require(SetupFileSetCharacterWeapon(&s,&edit,&changed,&why));assert(changed && s.objectcount==source->objectcount+1);
    DWORD first=R(source->data+12),next=R(s.data+12),last=source->characters[2].sourceoffset+28+16;
    assert(!memcmp(source->data+first,s.data+next,last-first)); /* all prior command indices/relative tags stable */
    SetupObject *obj=&s.objects[s.objectcount-1];
    assert(obj->pad==9 && obj->flags==(PROPFLAG_ASSIGNEDTOCHR|PROPFLAG_WEAPON_LEFTHANDED));
    assert(obj->sourceoffset>s.characters[2].sourceoffset && obj->extrascale==256);
    assert(R(s.data+obj->sourceoffset+128)==((25u<<24)|0xffffff));
    SetupCharacterWeapons view;Require(SetupFileGetCharacterWeapons(&s,2,&view));assert(view.item[0]==-1 && view.item[1]==25);
    Require(EditHistoryCommitEdit(&h,&bg,&s,&stan,&tx,&why));Reload(&s);
    Require(SetupFileClone(&s,&after,&why));
    Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why));Same(&s,source);
    Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why));Same(&s,&after);
    Require(EditHistoryBeginSetupEdit(&h,&s,"Cancel",&tx,&why));Set(&s,2,1,-1);
    EditHistoryRollbackEdit(&tx,&bg,&s,&stan);Same(&s,&after);
    DWORD size=s.size,count=s.objectcount;
    for(int i=0;i<50;i++){Set(&s,2,1,-1);Set(&s,2,1,22);assert(s.size==size && s.objectcount==count);}
    /* All named choices encode the real item and prop IDs, including None. */
    DWORD choices;const SetupWeaponChoice *list=SetupWeaponChoices(&choices);
    for(DWORD i=1;i<choices;i++) {
        if(view.item[1]!=list[i].item)Set(&s,2,1,list[i].item);
        Require(SetupFileGetCharacterWeapons(&s,2,&view));assert(view.item[1]==list[i].item);
        assert(s.objects[count-1].modelid==list[i].model);
    }
    EditHistoryFree(&h);SetupFileFree(&s);SetupFileFree(&after);
    puts("PASS: new weapon appended after owner, relative command references stable, save/reload, native compacted bytes, undo/redo/rollback, all catalog choices and bounded append/clear cycles.");
}
static void AllocationFailures(const SetupFile *source,int chr,int hand,int item)
{
    for(int failure=0;failure<100;failure++) {
        SetupFile s={0};Require(SetupFileClone(source,&s,&why));BOOL changed=TRUE;
        SetupCharacterWeaponEdit edit=Request(&s,chr,hand,item);
        failat=failure;allocations=0;
        BOOL ok=SetupFileSetCharacterWeapon(&s,&edit,&changed,&why);
        failat=-1;
        if(!ok){assert(!changed);Same(&s,source);assert(s.dirty==source->dirty);}
        SetupFileFree(&s);
        if(ok)return;
    }
    abort();
}
int main(int argc,char **argv)
{
    assert(argc==2);dir=argv[1];SetupFile source=Fixture();
    Variants(&source);AppendAndHistory(&source);
    AllocationFailures(&source,0,0,13);AllocationFailures(&source,2,1,25);AllocationFailures(&source,0,0,-1);
    Set(&source,0,0,-1);AllocationFailures(&source,0,0,6);
    SetupFileFree(&source);puts("PASS: allocation failures leave native bytes, caches, counts and dirty state unchanged.");return 0;
}
