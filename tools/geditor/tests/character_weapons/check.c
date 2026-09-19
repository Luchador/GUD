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
static void DuplicateCharacters(void)
{
    SetupFile source=Fixture(),s={0},before={0},after={0};
    /* Append a hat and a grenade-probability attribute after the source guard.
     * Keep the existing native stream, pads and relative tag unchanged. */
    DWORD first=R(source.data+12),end=source.characters[2].sourceoffset+28+16;
    DWORD table=source.size,extra=table+end-first;
    source.data=realloc(source.data,extra+128+12+4);assert(source.data);
    memset(source.data+table,0,extra+144-table);
    memcpy(source.data+table,source.data+first,end-first);
    W(source.data+12,table);
    W(source.data+extra,0x01000011);W(source.data+extra+4,0x00010007);
    W(source.data+extra+8,PROPFLAG_ASSIGNEDTOCHR);
    W(source.data+extra+128,18);W(source.data+extra+132,7);W(source.data+extra+136,0x12345678);
    W(source.data+extra+140,48);source.size=extra+144;
    /* Reparse after relocating the stream. */
    Require(SetupSaveProjectFile(dir,&source,&why));SetupFileFree(&source);
    Require(SetupLoadProjectFile(dir,"UsetupweaponsZ",&source,&why));
    Require(SetupFileClone(&source,&s,&why));
    Require(SetupFileClone(&source,&before,&why));
    DWORD selected=0;const DWORD original=SETUP_CHARACTER_SELECTION_BIT;
    assert(SetupFileCanDuplicateObject(&s,original));
    EditHistory h={0};EditHistoryTransaction tx={0};EditHistoryAsset asset;BgDocument bg={0};StanFile stan={0};
    EditHistoryReset(&h,&bg,&s,&stan);Require(EditHistoryBeginSetupEdit(&h,&s,"Paste Character Here",&tx,&why));
    Require(SetupFileDuplicateObject(&s,&source,original,&selected,&why));
    assert(selected==(original|3) && s.charactercount==4 && s.objectcount==source.objectcount+5);
    const SetupCharacter *chr=&s.characters[3];
    assert(chr->chrnum==10 && chr->pad!=source.characters[0].pad);
    for(int b=0;b<28;b++)if(b<4 || b>=8)
        assert(s.data[chr->sourceoffset+b]==source.data[source.characters[0].sourceoffset+b]);
    SetupCharacterWeapons view;
    Require(SetupFileGetCharacterWeapons(&s,3,&view));
    assert(view.item[0]==SETUP_WEAPON_MIXED && view.count[0]==2 && view.item[1]==26);
    for(int i=0;i<5;i++) {
        const SetupObject *a=&source.objects[i<4 ? (DWORD)i : source.objectcount-1];
        const SetupObject *b=&s.objects[source.objectcount+i];
        assert(b->pad==10 && b->sourceoffset>chr->sourceoffset);
        for(int n=0;n<(i==4 ? 128 : 136);n++)if(n!=6 && n!=7)
            assert(s.data[b->sourceoffset+n]==source.data[a->sourceoffset+n]);
    }
    DWORD attribute=s.objects[s.objectcount-1].sourceoffset+128;
    assert(R(s.data+attribute)==18 && R(s.data+attribute+4)==10 && R(s.data+attribute+8)==0x12345678);
    /* Every old command (including the tag) remains at the same command index. */
    first=R(source.data+12);
    DWORD oldbytes=source.objects[source.objectcount-1].sourceoffset+128+12-first;
    assert(!memcmp(source.data+first,s.data+R(s.data+12),oldbytes));
    Require(EditHistoryCommitEdit(&h,&bg,&s,&stan,&tx,&why));Reload(&s);
    Require(SetupFileClone(&s,&after,&why));
    Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why));Same(&s,&before);
    Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why));Same(&s,&after);
    /* A live source may alias the destination; repeated pastes own distinct pads. */
    Require(SetupFileDuplicateObject(&s,&s,selected,&selected,&why));
    assert(s.characters[4].chrnum==11 && s.characters[4].pad!=s.characters[3].pad);
    SetupFileFree(&s);SetupFileFree(&before);SetupFileFree(&after);EditHistoryFree(&h);
    for(int failure=0;failure<100;failure++) {
        Require(SetupFileClone(&source,&s,&why));selected=123;
        failat=failure;allocations=0;
        BOOL ok=SetupFileDuplicateObject(&s,&s,original,&selected,&why);
        failat=-1;
        if(!ok){assert(selected==123);Same(&s,&source);assert(s.dirty==source.dirty);}
        SetupFileFree(&s);if(ok)break;assert(failure<99);
    }
    /* Skip AI sentinel IDs; never recycle deleted character IDs. */
    source.characters[2].chrnum=247;
    W(source.data+source.characters[2].sourceoffset+4,247u<<16);
    Require(SetupFileClone(&source,&s,&why));
    Require(SetupFileDuplicateObject(&s,&source,original,&selected,&why));
    assert(s.characters[3].chrnum==256);SetupFileFree(&s);
    Require(SetupFileDeleteCharacter(&source,2,&why));
    assert(!SetupFileCanDuplicateObject(&source,original|2));
    Require(SetupFileClone(&source,&s,&why));
    Require(SetupFileDuplicateObject(&s,&source,original,&selected,&why));
    assert(s.characters[3].chrnum==256);SetupFileFree(&s);
    source.characters[2].chrnum=4999;
    W(source.data+source.characters[2].sourceoffset+4,4999u<<16);
    Require(SetupFileClone(&source,&s,&why));selected=123;
    assert(!SetupFileDuplicateObject(&s,&source,original,&selected,&why));Same(&s,&source);assert(selected==123);
    SetupFileFree(&s);SetupFileFree(&source);
    puts("PASS: character copies retain native settings, mixed/left/concealed weapons, hats and attributes; new IDs/pads, stable commands, aliasing, save/reload, undo/redo, ID limits and atomic allocation failures.");
}
static SetupCharacterHatEdit HatRequest(const SetupFile *s,DWORD index,int model)
{
    const SetupCharacter *chr=s->characters+index;
    return (SetupCharacterHatEdit){index,chr->sourceoffset,chr->chrnum,model};
}
static void HatSet(SetupFile *s,DWORD index,int model)
{
    SetupCharacterHatEdit edit=HatRequest(s,index,model);BOOL changed;
    Require(SetupFileSetCharacterHat(s,&edit,&changed,&why));assert(changed);
}
static void Hats(void)
{
    SetupFile source=Fixture(),s={0},before={0},after={0};
    DWORD first=R(source.data+12),end=source.characters[2].sourceoffset+28+16;
    DWORD table=source.size,at=table+end-first,size=at+5*128+4;
    source.data=realloc(source.data,size);assert(source.data);
    memset(source.data+table,0,size-table);memcpy(source.data+table,source.data+first,end-first);
    W(source.data+12,table);source.size=size;
    const DWORD flags2[]={PROPFLAG2_NO_LOAD_A,PROPFLAG2_NO_LOAD_SA,0xf8,0,0};
    for(int i=0;i<5;i++,at+=128) {
        W(source.data+at,0x01800011);W(source.data+at+4,((212u+i)<<16)|(i==3 ? 8 : 7));
        W(source.data+at+8,i==4 ? 0 : PROPFLAG_ASSIGNEDTOCHR);W(source.data+at+12,flags2[i]);
        W(source.data+at+0x74,777u<<16);
    }
    W(source.data+at,48);
    Require(SetupSaveProjectFile(dir,&source,&why));SetupFileFree(&source);
    Require(SetupLoadProjectFile(dir,"UsetupweaponsZ",&source,&why));
    Require(SetupFileClone(&source,&s,&why));
    SetupCharacterHat view;Require(SetupFileGetCharacterHat(&s,0,&view));
    assert(view.model==SETUP_HAT_MIXED && view.count==2);
    assert(SetupFileGetCharacterWornHat(&s,0)==s.objects+source.objectcount-5);
    HatSet(&s,0,220);Require(SetupFileGetCharacterHat(&s,0,&view));assert(view.model==220 && view.count==2);
    for(DWORD i=0;i<source.objectcount;i++) {
        const unsigned char *a=source.data+source.objects[i].sourceoffset,*b=s.data+s.objects[i].sourceoffset;
        int bytes=source.objects[i].type==PROPDEF_HAT ? 128 : 136;
        for(int n=0;n<bytes;n++) {
            if(i>=source.objectcount-5 && i<source.objectcount-3 && (n==4 || n==5))continue;
            assert(a[n]==b[n]);
        }
    }
    DWORD savedsize=s.size,savedcount=s.objectcount;
    for(int i=0;i<100;i++) {
        HatSet(&s,0,SETUP_HAT_NONE);assert(!SetupFileGetCharacterWornHat(&s,0));
        Require(SetupFileGetCharacterHat(&s,0,&view));assert(view.model==SETUP_HAT_NONE && !view.count);
        if(!i)Reload(&s);
        HatSet(&s,0,221);assert(s.size==savedsize && s.objectcount==savedcount);
        for(int h=0;h<2;h++) {
            const SetupObject *hat=s.objects+source.objectcount-5+h;
            assert(hat->flags2==flags2[h] && hat->extrascale==384);
        }
    }
    Reload(&s);
    Require(SetupFileClone(&s,&before,&why));
    SetupCharacterHatEdit edit=HatRequest(&s,0,221);BOOL changed=TRUE;
    Require(SetupFileSetCharacterHat(&s,&edit,&changed,&why));assert(!changed);Same(&s,&before);
    edit.model=999;assert(!SetupFileSetCharacterHat(&s,&edit,&changed,&why));Same(&s,&before);
    edit=HatRequest(&s,0,212);edit.sourceoffset++;assert(!SetupFileSetCharacterHat(&s,&edit,&changed,&why));Same(&s,&before);
    EditHistory h={0};EditHistoryTransaction tx={0};EditHistoryAsset asset;BgDocument bg={0};StanFile stan={0};
    EditHistoryReset(&h,&bg,&s,&stan);Require(EditHistoryBeginSetupEdit(&h,&s,"Change Hat",&tx,&why));
    HatSet(&s,2,223);assert(s.objectcount==savedcount+1);
    const SetupObject *added=s.objects+s.objectcount-1;
    assert(added->type==PROPDEF_HAT && added->pad==9 && added->extrascale==256 && added->flags==PROPFLAG_ASSIGNEDTOCHR);
    Require(EditHistoryCommitEdit(&h,&bg,&s,&stan,&tx,&why));Reload(&s);
    Require(SetupFileClone(&s,&after,&why));
    Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why));Same(&s,&before);
    Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why));Same(&s,&after);
    DWORD count;const SetupHatChoice *choices=SetupHatChoices(&count);assert(count==13);
    for(DWORD i=0;i<count;i++) {
        HatSet(&s,2,choices[i].model);
        Require(SetupFileGetCharacterHat(&s,2,&view));assert(view.model==choices[i].model);
    }
    EditHistoryFree(&h);SetupFileFree(&before);SetupFileFree(&after);SetupFileFree(&s);
    for(int mode=0;mode<3;mode++) {
        if(mode==2)HatSet(&source,0,SETUP_HAT_NONE);
        for(int failure=0;failure<100;failure++) {
            Require(SetupFileClone(&source,&s,&why));
            edit=HatRequest(&s,mode==1 ? 2 : 0,mode==0 ? SETUP_HAT_NONE : 220);changed=TRUE;
            failat=failure;allocations=0;BOOL ok=SetupFileSetCharacterHat(&s,&edit,&changed,&why);failat=-1;
            if(!ok){assert(!changed);Same(&s,&source);assert(s.dirty==source.dirty);}
            SetupFileFree(&s);if(ok)break;assert(failure<99);
        }
    }
    SetupFileFree(&source);
    puts("PASS: all 12 hats and No hat, mixed variants, native hat ownership, flags/scale, weapon and loose-hat isolation, bounded remove/re-equip cycles, no-op/stale edits, save/reload, undo/redo and atomic failures.");
}
static SetupCharacterBehaviorEdit BehaviorRequest(const SetupFile *s,DWORD index,int id)
{
    const SetupCharacter *chr=s->characters+index;
    return (SetupCharacterBehaviorEdit){index,chr->sourceoffset,chr->chrnum,chr->ailistid,id};
}
static void Behaviors(void)
{
    SetupFile s=Fixture(),before={0},after={0};BOOL changed;
    /* Existing local script assignments survive load and opening properties. */
    W(s.data+s.characters[0].sourceoffset+8,0x00010401);s.characters[0].ailistid=0x401;
    Reload(&s);Require(SetupFileClone(&s,&before,&why));
    DWORD count;const SetupBehaviorChoice *choices=SetupCharacterBehaviorChoices(&count);
    assert(count==2 && choices[0].id==2 && choices[1].id==1);
    assert(!SetupCharacterBehaviorChoiceForId(0x401));
    SetupCharacterBehaviorEdit edit=BehaviorRequest(&s,0,SETUP_BEHAVIOR_STANDARD_GUARD);
    for(int invalid=0;invalid<7;invalid++) {
        SetupCharacterBehaviorEdit bad=edit;
        switch(invalid) {
        case 0:bad.characterindex=s.charactercount;break;
        case 1:bad.sourceoffset++;break;
        case 2:bad.chrnum++;break;
        case 3:bad.previous=1;break;
        case 4:bad.ailistid=-1;break;
        case 5:bad.ailistid=0;break;
        case 6:bad.ailistid=0x401;break;
        }
        changed=TRUE;assert(!SetupFileSetCharacterBehavior(&s,&bad,&changed,&why));
        assert(!changed && s.dirty==before.dirty);Same(&s,&before);
    }
    EditHistory h={0};EditHistoryTransaction tx={0};EditHistoryAsset asset;BgDocument bg={0};StanFile stan={0};
    EditHistoryReset(&h,&bg,&s,&stan);Require(EditHistoryBeginSetupEdit(&h,&s,"Change Starting Behavior",&tx,&why));
    Require(SetupFileSetCharacterBehavior(&s,&edit,&changed,&why));assert(changed && s.dirty);
    DWORD offset=s.characters[0].sourceoffset;
    assert(s.characters[0].ailistid==2 && R(s.data+offset+8)==0x00010002 && s.size==before.size);
    for(DWORD i=0;i<s.size;i++)if(i!=offset+10 && i!=offset+11)assert(s.data[i]==before.data[i]);
    assert(!memcmp(s.objects,before.objects,s.objectcount*sizeof(*s.objects)));
    assert(!memcmp(s.characters+1,before.characters+1,(s.charactercount-1)*sizeof(*s.characters)));
    Require(EditHistoryCommitEdit(&h,&bg,&s,&stan,&tx,&why));Reload(&s);
    Require(SetupFileClone(&s,&after,&why));
    edit=BehaviorRequest(&s,0,2);Require(SetupFileSetCharacterBehavior(&s,&edit,&changed,&why));
    assert(!changed && s.dirty==after.dirty);Same(&s,&after);
    Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why));Same(&s,&before);
    Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why));Same(&s,&after);
    /* A stale cached assignment cannot silently overwrite native data. */
    W(s.data+offset+8,0x00010402);edit=BehaviorRequest(&s,0,1);
    assert(!SetupFileSetCharacterBehavior(&s,&edit,&changed,&why));assert(!changed && R(s.data+offset+8)==0x00010402);
    W(s.data+offset+8,0x00010002);
    Require(SetupFileSetCharacterBehavior(&s,&edit,&changed,&why));assert(changed && s.characters[0].ailistid==1);
    Reload(&s);assert(s.characters[0].ailistid==1);
    /* Newly placed characters default to Standard guard. Copying an existing
     * guard preserves its actual behavior, including Do nothing and custom AI. */
    const double pos[3]={10,20,30};DWORD selected;
    Require(SetupFileAddModel(&s,TRUE,1,1,pos,&selected,&why));
    DWORD index=selected&~SETUP_CHARACTER_SELECTION_BIT;
    assert(selected&SETUP_CHARACTER_SELECTION_BIT);
    assert(s.characters[index].ailistid==2 && (R(s.data+s.characters[index].sourceoffset+8)&0xffffu)==2);
    Require(SetupFileDuplicateObject(&s,&s,SETUP_CHARACTER_SELECTION_BIT,&selected,&why));
    assert(s.characters[selected&~SETUP_CHARACTER_SELECTION_BIT].ailistid==1);
    Require(SetupFileDuplicateObject(&s,&before,SETUP_CHARACTER_SELECTION_BIT,&selected,&why));
    assert(s.characters[selected&~SETUP_CHARACTER_SELECTION_BIT].ailistid==0x401);
    Require(SetupFileCompact(&s,&why)); /* Normally performed by the paste history commit. */
    Reload(&s);assert(s.characters[s.charactercount-1].ailistid==0x401);
    SetupFileFree(&s);SetupFileFree(&before);SetupFileFree(&after);EditHistoryFree(&h);
    puts("PASS: behavior edit isolation, invalid/stale requests, native save/reload, undo/redo, standard guard placement and copied idle/custom assignments.");
}
int main(int argc,char **argv)
{
    assert(argc==2);dir=argv[1];SetupFile source=Fixture();
    Variants(&source);AppendAndHistory(&source);
    AllocationFailures(&source,0,0,13);AllocationFailures(&source,2,1,25);AllocationFailures(&source,0,0,-1);
    Set(&source,0,0,-1);AllocationFailures(&source,0,0,6);
    SetupFileFree(&source);puts("PASS: allocation failures leave native bytes, caches, counts and dirty state unchanged.");
    DuplicateCharacters();Hats();Behaviors();return 0;
}
