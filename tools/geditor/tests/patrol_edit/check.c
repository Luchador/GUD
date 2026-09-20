#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "patrolpaths.h"
#include "actionblocks.h"
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
static void Require(BOOL ok) { if (!ok) { fprintf(stderr,"%s\n",why); abort(); } }
static void Same(const SetupFile *a,const SetupFile *b)
{
    assert(a->size==b->size && a->dirty==b->dirty && a->charactercount==b->charactercount);
    assert(!memcmp(a->data,b->data,a->size));
    assert(a->actionmetasize==b->actionmetasize);
    if(a->actionmetasize)assert(!memcmp(a->actionmeta,b->actionmeta,a->actionmetasize));
    if(a->charactercount)assert(!memcmp(a->characters,b->characters,a->charactercount*sizeof(*a->characters)));
}
static void Reload(SetupFile *s)
{
    SetupFile next={0}; Require(SetupSaveProjectFile(dir,s,&why));
    Require(SetupLoadProjectFile(dir,s->name,&next,&why)); next.dirty=s->dirty; Same(s,&next);
    SetupFileFree(s); *s=next;
}
static SetupFile Fixture(void)
{
    SetupFile s={0},loaded={0}; s.size=2048; s.data=calloc(s.size,1); strcpy(s.name,"UsetuppatrolZ");
    W(s.data,40);
    const DWORD pads[]={2,0,1};
    for(int i=0;i<3;i++) { W(s.data+40+i*16,pads[i]); W(s.data+44+i*16,104+i*12); }
    W(s.data+88,0xffffffffu);
    for(int i=0;i<3;i++) { W(s.data+104+i*12,(i+1)%3); W(s.data+108+i*12,(i+2)%3); W(s.data+112+i*12,0xffffffffu); }
    W(s.data+4,144); W(s.data+144,168); W(s.data+148,172); W(s.data+168,0xffffffffu);
    W(s.data+172,0); W(s.data+176,1); W(s.data+180,2); W(s.data+184,0xffffffffu);
    W(s.data+8,200); W(s.data+200,9);
    W(s.data+12,204);
    for(int i=0;i<2;i++) {
        unsigned char *p=s.data+204+i*28;
        W(p,9); W(p+4,((7+i)<<16)|i); W(p+8,0x10000|(i ? 0x411 : 2)); W(p+12,0xffffffffu); W(p+20,0xffff);
    }
    W(s.data+260,48); W(s.data+24,264);
    for(int i=0;i<4;i++) { W(s.data+264+i*44+16,0x3f800000); W(s.data+264+i*44+32,0x3f800000); W(s.data+264+i*44+36,1840); }
    W(s.data+28,484); memcpy(s.data+1840,"test_stan",10);
    W(s.data+16,600); W(s.data+600,640); W(s.data+604,0x0580004d); /* ID5, extra flag, authored len77 */
    W(s.data+608,656); W(s.data+612,0x17010002); /* ID23 loop */
    W(s.data+640,0); W(s.data+644,1); W(s.data+648,2); W(s.data+652,0xffffffffu);
    W(s.data+656,2); W(s.data+660,0); W(s.data+664,0xffffffffu);
    W(s.data+20,700); W(s.data+700,732); W(s.data+704,0x411);
    const unsigned char idle[]={2,0,3,1,0,4}; memcpy(s.data+732,idle,sizeof(idle));
    Require(SetupSaveProjectFile(dir,&s,&why)); Require(SetupLoadProjectFile(dir,s.name,&loaded,&why));
    SetupFileFree(&s); return loaded;
}
static PatrolAssignment Request(const SetupFile *s,DWORD chr,int path)
{ return (PatrolAssignment){chr,s->characters[chr].sourceoffset,s->characters[chr].chrnum,s->characters[chr].ailistid,path}; }
static void Assign(SetupFile *s,DWORD chr,int path)
{
    SetupFile out={0}; BOOL changed; PatrolAssignment edit=Request(s,chr,path);
    Require(PatrolAssignCharacter(s,&edit,&out,&changed,&why)); assert(changed);
    SetupFileFree(s); *s=out;
}
static void Paths(const SetupFile *source)
{
    PatrolDocument d={0},reloaded={0}; SetupFile s={0}; DWORD path;
    Require(PatrolDocumentLoad(source,&d,&why)); assert(d.count==2 && d.paths[0].id==5 && d.pads[0]==2);
    Require(PatrolDocumentCompile(&d,source,&s,&why)); Same(source,&s); SetupFileFree(&s);
    Require(PatrolDocumentAdd(&d,source,&path,&why)); assert(path==2 && d.paths[path].id==0);
    assert(!PatrolDocumentCompile(&d,source,&s,&why) && !s.data); /* empty route */
    Require(PatrolPathInsert(&d,path,0,0,&why)); Require(PatrolPathInsert(&d,path,1,0,&why));
    assert(!PatrolDocumentCompile(&d,source,&s,&why)); /* same pad twice */
    Require(PatrolPathRemove(&d,path,1)); Require(PatrolPathInsert(&d,path,1,2,&why));
    Require(PatrolPathInsert(&d,path,1,1,&why)); Require(PatrolPathMove(&d,path,1,1));
    Require(PatrolPathSetLoop(&d,path,TRUE));
    Require(PatrolDocumentCompile(&d,source,&s,&why)); Reload(&s);
    Require(PatrolDocumentLoad(&s,&reloaded,&why));
    assert(reloaded.count==3 && reloaded.paths[2].count==3 && reloaded.paths[2].points[1]==2 && reloaded.paths[2].flags==1);
    assert(reloaded.paths[0].flags==0x80 && reloaded.paths[0].length==77 && reloaded.paths[0].id==5);
    assert(reloaded.paths[1].id==23 && reloaded.paths[1].length==2);
    PatrolDocumentFree(&reloaded); PatrolDocumentFree(&d);
    SetupPatrolLink *links=NULL; DWORD count;
    Require(SetupFileBuildPatrolLinks(&s,&links,&count,NULL,&why)); assert(count==3); free(links);
    DWORD size=s.size;
    for(int i=0;i<100;i++) {
        Require(PatrolDocumentLoad(&s,&d,&why));
        Require(PatrolPathSetLoop(&d,2,(i&1)!=0));
        SetupFile next={0}; Require(PatrolDocumentCompile(&d,&s,&next,&why));
        PatrolDocumentFree(&d); SetupFileFree(&s); s=next; assert(s.size==size);
    }
    /* Navigation arrays and pad indices are unchanged after pointer relocation. */
    DWORD way=R(s.data),oldway=R(source->data);
    for(int i=0;i<3;i++) {
        assert(R(s.data+way+i*16)==R(source->data+oldway+i*16));
        assert(!memcmp(s.data+R(s.data+way+i*16+4),source->data+R(source->data+oldway+i*16+4),12));
    }
    Require(PatrolDocumentLoad(&s,&d,&why)); Require(PatrolDocumentDelete(&d,&s,1,&why));
    SetupFile next={0}; Require(PatrolDocumentCompile(&d,&s,&next,&why));
    PatrolDocumentFree(&d); SetupFileFree(&s); s=next;
    Require(PatrolDocumentLoad(&s,&d,&why)); assert(d.count==2 && d.paths[0].id==5 && d.paths[1].id==0);
    PatrolDocumentFree(&d); SetupFileFree(&s);
    puts("PASS: create/insert/reorder/remove, loops, native waypoint mapping, preserved IDs/flags/navigation, preview, save/reload and 100 edits without growth.");
}
static BOOL Contains(const SetupFile *s,DWORD list,DWORD item)
{
    for (DWORD at=list;at+4<=s->size;at+=4) {
        DWORD value=R(s->data+at); if(value&0x80000000u)return FALSE; if(value==item)return TRUE;
    }
    assert(0); return FALSE;
}
/* Independently check the reciprocal links and membership required by the
 * game's init_path_table_links (invalid graphs deliberately freeze loading). */
static void Navigation(const SetupFile *s)
{
    DWORD way=R(s->data),groups=R(s->data+4),wc=0,gc=0;
    while(!(R(s->data+way+wc*16)&0x80000000u))wc++;
    while(R(s->data+groups+gc*12))gc++;
    for(DWORD i=0;i<wc;i++) {
        DWORD group=R(s->data+way+i*16+8); assert(group<gc);
        assert(Contains(s,R(s->data+groups+group*12+4),i));
        for(DWORD at=R(s->data+way+i*16+4);!(R(s->data+at)&0x80000000u);at+=4) {
            DWORD other=R(s->data+at); assert(other<wc && other!=i);
            assert(Contains(s,R(s->data+way+other*16+4),i));
            DWORD og=R(s->data+way+other*16+8);
            if(og!=group)assert(Contains(s,R(s->data+groups+group*12),og));
        }
    }
    for(DWORD i=0;i<gc;i++)for(DWORD at=R(s->data+groups+i*12);!(R(s->data+at)&0x80000000u);at+=4) {
        DWORD other=R(s->data+at); assert(other<gc && other!=i);
        assert(Contains(s,R(s->data+groups+other*12),i));
    }
}
static void NewPads(const SetupFile *source)
{
    SetupFile s={0},out={0},before={0}; PatrolDocument d={0},base={0},current={0};
    SetupPadRef pad; DWORD path;
    Require(SetupFileClone(source,&s,&why)); Require(PatrolDocumentLoad(&s,&d,&why));
    Require(PatrolDocumentLoad(&s,&base,&why)); Require(PatrolDocumentAdd(&d,&s,&path,&why));
    const double position[]={123.5,67,-89};
    Require(SetupFileAddPad(&s,0.25f,position,"test_stan",&pad,&why));
    assert(pad.index==source->padcount && !pad.bound && s.padcount==source->padcount+1);
    for(int i=0;i<3;i++)assert(s.pads[pad.index].pos[i]==(float)(position[i]*0.25));
    assert(!strcmp(s.pads[pad.index].stanname,"test_stan"));
    assert(s.pads[pad.index].up[1]==1 && s.pads[pad.index].look[2]==1);
    assert(!R(s.data+R(s.data+24)+pad.index*44+40)); /* standalone, not recyclable */
    Require(PatrolDocumentLoad(&s,&current,&why)); assert(PatrolDocumentMatches(&base,&current));
    PatrolDocumentFree(&current);
    Require(PatrolPathInsertPad(&d,&s,path,0,2,&why)); assert(d.paths[path].points[0]==0);
    Require(PatrolPathInsertPad(&d,&s,path,1,pad.index,&why));
    Require(PatrolPathInsertPad(&d,&s,path,2,0,&why));
    assert(d.waypointcount==4 && d.pads[3]==pad.index);
    DWORD count=d.waypointcount;
    Require(PatrolPathInsertPad(&d,&s,path,1,3,&why)); assert(d.waypointcount==count+1);
    Require(PatrolPathRemove(&d,path,1)); assert(d.waypointcount==count && d.pads[3]==pad.index);
    assert(!PatrolPathInsertPad(&d,&s,999,0,3,&why)); assert(d.waypointcount==count);
    assert(!PatrolPathInsertPad(&d,&s,path,0,s.padcount,&why));
    /* A modeless compile preserves edits made after the draft was opened. */
    BOOL changed; const double move[]={8,0,-4};
    Require(SetupFileTranslatePad(&s,&pad,0.25f,move,&changed,&why)); assert(changed);
    Require(PatrolDocumentCompile(&d,&s,&out,&why));
    assert(out.padcount==s.padcount && !memcmp(out.pads,s.pads,s.padcount*sizeof(*s.pads)));
    Navigation(&out); Reload(&out); Navigation(&out);
    Require(PatrolDocumentLoad(&out,&current,&why)); assert(!PatrolDocumentMatches(&base,&current));
    assert(current.pads[current.paths[path].points[1]]==pad.index);
    PatrolDocumentFree(&current); SetupFileFree(&out);
    /* Runtime also supports authored -1 group IDs assigned from group members. */
    DWORD way=R(s.data); for(int i=0;i<3;i++)W(s.data+way+i*16+8,0xffffffffu);
    Require(PatrolDocumentCompile(&d,&s,&out,&why)); Navigation(&out); SetupFileFree(&out);
    Require(SetupFileClone(&s,&before,&why));
    for(int operation=0;operation<2;operation++) {
        BOOL reached=FALSE;
        for(int step=0;step<1000 && !reached;step++) {
            SetupFile trial={0}; Require(SetupFileClone(&s,&trial,&why));
            allocations=0; failat=step;
            BOOL ok=operation ? PatrolDocumentCompile(&d,&trial,&out,&why)
                : SetupFileAddPad(&trial,0.25f,position,"test_stan",&pad,&why);
            failat=-1;
            if(operation || !ok)Same(&trial,&before);
            if(ok)reached=TRUE; else assert(!out.data);
            SetupFileFree(&trial); SetupFileFree(&out);
        }
        assert(reached);
    }
    s.pads[d.pads[3]].deleted=TRUE;
    assert(!PatrolDocumentCompile(&d,&s,&out,&why) && !out.data);
    s.pads[d.pads[3]].deleted=FALSE;
    /* Empty navigation networks can be authored entirely from ordinary pads. */
    W(s.data,0); W(s.data+4,0); W(s.data+16,0); PatrolDocumentFree(&d);
    Require(PatrolDocumentLoad(&s,&d,&why)); Require(PatrolDocumentAdd(&d,&s,&path,&why));
    Require(PatrolPathInsertPad(&d,&s,path,0,0,&why)); Require(PatrolPathInsertPad(&d,&s,path,1,1,&why));
    Require(PatrolPathSetLoop(&d,path,TRUE)); Require(PatrolDocumentCompile(&d,&s,&out,&why)); Navigation(&out);
    Reload(&out); Navigation(&out);
    DWORD size=out.size;
    for(int i=0;i<30;i++) {
        PatrolDocumentFree(&d); Require(PatrolDocumentLoad(&out,&d,&why));
        Require(PatrolPathSetLoop(&d,0,(i&1)!=0)); SetupFile next={0};
        Require(PatrolDocumentCompile(&d,&out,&next,&why)); SetupFileFree(&out); out=next; assert(out.size==size);
    }
    PatrolDocumentFree(&d); PatrolDocumentFree(&base); SetupFileFree(&s); SetupFileFree(&out); SetupFileFree(&before);
    puts("PASS: standalone pad placement, new waypoint routes/groups, empty networks, modeless merging/conflicts, save/reload, stable size and allocation failure atomicity.");
}
static void Assignments(const SetupFile *source)
{
    SetupFile s={0},before={0}; Require(SetupFileClone(source,&s,&why));
    assert(PatrolCharacterPath(&s,0)==PATROL_NONE && PatrolCharacterPath(&s,1)==PATROL_CUSTOM);
    Assign(&s,0,23); assert(PatrolCharacterPath(&s,0)==23); Reload(&s);
    unsigned short ai=s.characters[0].ailistid; assert(ai>=0x401 && ai<0x1000);
    ActionDocument actions={0}; Require(ActionDocumentLoad(&s,&actions,&why));
    assert(actions.count==2); ActionBlock *b=&actions.blocks[1];
    assert(b->instructions[0].bytes[0]==0x20 && b->instructions[0].bytes[1]==23);
    const unsigned char jump[]={5,253,0,7}; assert(!memcmp(b->instructions[1].bytes,jump,4));
    assert(b->instructions[2].bytes[0]==4); ActionDocumentFree(&actions);
    Assign(&s,1,23); assert(s.characters[1].ailistid==ai); /* shared starter */
    PatrolDocument d={0}; Require(PatrolDocumentLoad(&s,&d,&why));
    assert(!PatrolDocumentDelete(&d,&s,1,&why)); PatrolDocumentFree(&d);
    Assign(&s,0,PATROL_NONE); assert(s.characters[0].ailistid==2 && s.characters[1].ailistid==ai);
    Assign(&s,1,PATROL_NONE); Require(ActionDocumentLoad(&s,&actions,&why));
    assert(actions.count==1 && actions.blocks[0].id==0x411); ActionDocumentFree(&actions);
    Require(PatrolDocumentLoad(&s,&d,&why)); Require(PatrolDocumentDelete(&d,&s,1,&why)); PatrolDocumentFree(&d);
    DWORD size=s.size;
    for(int i=0;i<50;i++) { Assign(&s,0,5); Assign(&s,0,23); Assign(&s,0,PATROL_NONE); assert(s.size==size); }
    Require(SetupFileClone(&s,&before,&why));
    PatrolAssignment edit=Request(&s,0,99); SetupFile out={0}; BOOL changed;
    assert(!PatrolAssignCharacter(&s,&edit,&out,&changed,&why) && !changed && !out.data); Same(&s,&before);
    edit=Request(&s,0,23); edit.previous++; assert(!PatrolAssignCharacter(&s,&edit,&out,&changed,&why)); Same(&s,&before);
    edit=Request(&s,0,23); edit.sourceoffset+=4; assert(!PatrolAssignCharacter(&s,&edit,&out,&changed,&why)); Same(&s,&before);
    edit=Request(&s,0,PATROL_NONE); Require(PatrolAssignCharacter(&s,&edit,&out,&changed,&why)); assert(!changed && !out.data);
    SetupFileFree(&before); SetupFileFree(&s);
    puts("PASS: native patrol starter and alert AI, shared assignments, None, reference protection, custom AI preservation, stale requests and repeated assignment without growth.");
}
static void History(const SetupFile *source)
{
    SetupFile s={0},after={0}; Require(SetupFileClone(source,&s,&why));
    EditHistory h={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset; BgDocument bg={0}; StanFile stan={0};
    EditHistoryReset(&h,&bg,&s,&stan); Require(EditHistoryBeginSetupEdit(&h,&s,"Assign Patrol",&tx,&why));
    Assign(&s,0,5); Require(EditHistoryCommitEdit(&h,&bg,&s,&stan,&tx,&why));
    Require(SetupFileClone(&s,&after,&why)); Require(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why)); Same(&s,source);
    Require(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why)); Same(&s,&after);
    unsigned char *runtime=NULL,*compact=NULL; DWORD size,packed;
    Require(ActionSetupBuildRuntime(&s,&runtime,&size,&why));
    Require(SetupCompactNative(runtime ? runtime : s.data,size,&compact,&packed,&why));
    assert(packed==s.size && !memcmp(compact,s.data,packed)); free(runtime); free(compact);
    Require(EditHistoryBeginSetupEdit(&h,&s,"Cancel",&tx,&why)); Assign(&s,0,PATROL_NONE);
    EditHistoryRollbackEdit(&tx,&bg,&s,&stan); Same(&s,&after);
    EditHistoryFree(&h); SetupFileFree(&s); SetupFileFree(&after);
    puts("PASS: assignment undo/redo/rollback and compact native ROM-export bytecode.");
}
static void AllocationFailures(const SetupFile *source)
{
    BOOL reached=FALSE;
    for(int step=0;step<1000;step++) {
        SetupFile s={0},out={0}; Require(SetupFileClone(source,&s,&why));
        PatrolAssignment edit=Request(&s,0,23); BOOL changed;
        allocations=0; failat=step; BOOL ok=PatrolAssignCharacter(&s,&edit,&out,&changed,&why); failat=-1;
        Same(&s,source);
        if (!ok) { assert(!out.data && !changed); }
        else if(changed) { reached=TRUE; }
        SetupFileFree(&s); SetupFileFree(&out); if(reached)break;
    }
    assert(reached); reached=FALSE;
    PatrolDocument d={0}; Require(PatrolDocumentLoad(source,&d,&why)); Require(PatrolPathSetLoop(&d,0,TRUE));
    for(int step=0;step<1000;step++) {
        SetupFile s={0},out={0}; Require(SetupFileClone(source,&s,&why));
        allocations=0; failat=step; BOOL ok=PatrolDocumentCompile(&d,&s,&out,&why); failat=-1;
        Same(&s,source); if(!ok)assert(!out.data); else reached=TRUE;
        SetupFileFree(&s); SetupFileFree(&out); if(reached)break;
    }
    assert(reached); PatrolDocumentFree(&d);
    puts("PASS: allocation failure atomicity for assignment and route compilation.");
}
static void Safety(const SetupFile *source)
{
    SetupFile s={0}; PatrolDocument d={0}; Require(SetupFileClone(source,&s,&why));
    DWORD table=R(s.data+16),way=R(s.data),points=R(s.data+table);
    W(s.data+points,999); assert(!PatrolDocumentLoad(&s,&d,&why)); W(s.data+points,0);
    s.data[table+12]=5; assert(!PatrolDocumentLoad(&s,&d,&why)); s.data[table+12]=23;
    s.pads[2].deleted=TRUE; assert(!PatrolDocumentLoad(&s,&d,&why)); s.pads[2].deleted=FALSE;
    W(s.data,4); assert(!PatrolDocumentLoad(&s,&d,&why)); W(s.data,way);
    W(s.data+16,s.size-4); assert(!PatrolDocumentLoad(&s,&d,&why)); W(s.data+16,table);
    Assign(&s,0,5);
    ActionDocument a={0}; Require(ActionDocumentLoad(&s,&a,&why));
    Require(ActionDocumentSetEnabled(&a,1,FALSE,&why)); SetupFile next={0};
    Require(ActionDocumentCompile(&a,&s,&next,&why)); ActionDocumentFree(&a); SetupFileFree(&s); s=next;
    assert(PatrolCharacterPath(&s,0)==PATROL_CUSTOM);
    Require(PatrolDocumentLoad(&s,&d,&why)); assert(!PatrolDocumentDelete(&d,&s,0,&why)); PatrolDocumentFree(&d);
    unsigned short old=s.characters[0].ailistid;
    Assign(&s,0,5); assert(s.characters[0].ailistid!=old && PatrolCharacterPath(&s,0)==5);
    Assign(&s,0,PATROL_NONE);
    /* Shared vehicle paths and block references reserve IDs too. */
    RomFile rom={0}; unsigned char data[128]={0}; rom.data=data; rom.size=sizeof(data); rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,0,128,0x80000000};
    rom.info.entries[1]=(RomManifestEntry){0x4149474c,16,32,0};
    W(data+16,0x80000030); W(data+20,1); W(data+24,8); W(data+28,1);
    W(data+48,0x80000050); W(data+52,18);
    const unsigned char code[]={0xcb,23,0x20,0,5,253,4,1,4}; memcpy(data+80,code,sizeof(code));
    Require(SetupFileSetGlobalReferences(&s,&rom,&why));
    assert(SetupFileGlobalPatrolReference(&s,23) && SetupFileGlobalPatrolReference(&s,0) && SetupFileGlobalBlockReference(&s,0x401));
    Require(PatrolDocumentLoad(&s,&d,&why)); assert(!PatrolDocumentDelete(&d,&s,1,&why));
    DWORD added; Require(PatrolDocumentAdd(&d,&s,&added,&why)); assert(d.paths[added].id==1); PatrolDocumentFree(&d);
    Assign(&s,0,23); assert(s.characters[0].ailistid==0x402); SetupFileFree(&s);
    puts("PASS: malformed tables, duplicate IDs, deleted pads, disabled starters and shared guard/vehicle references.");
}
static void Stock(const char *name)
{
    SetupFile s={0},out={0}; PatrolDocument d={0};
    printf("Checking stock setup %s...\n",name);
    Require(SetupLoadProjectFile(dir,name,&s,&why)); Require(PatrolDocumentLoad(&s,&d,&why));
    Require(PatrolDocumentCompile(&d,&s,&out,&why)); Same(&s,&out); SetupFileFree(&out);
    DWORD path=(DWORD)-1;
    for(DWORD i=0;i<d.count;i++) if(d.paths[i].count>=2) { path=i; break; }
    if(path!=(DWORD)-1) {
        Require(PatrolPathSetLoop(&d,path,!(d.paths[path].flags&1)));
        Require(PatrolDocumentCompile(&d,&s,&out,&why)); Reload(&out);
        SetupPatrolLink *links=NULL; DWORD count;
        Require(SetupFileBuildPatrolLinks(&out,&links,&count,NULL,&why)); free(links);
        if(out.charactercount && !out.characters[0].deleted) {
            if(PatrolCharacterPath(&out,0)!=d.paths[path].id) { Assign(&out,0,d.paths[path].id); }
            else {
                SetupFile noop={0}; BOOL changed; PatrolAssignment edit=Request(&out,0,d.paths[path].id);
                Require(PatrolAssignCharacter(&out,&edit,&noop,&changed,&why)); assert(!changed && !noop.data);
            }
            Reload(&out);
        }
        SetupFileFree(&out);
    }
    PatrolDocumentFree(&d); SetupFileFree(&s);
    printf("PASS: stock setup %s patrol read/edit/save/reload.\n",name);
}

#include "ui.c"
int main(int argc,char **argv)
{
    setvbuf(stdout,NULL,_IONBF,0);
    assert(argc>=2); dir=argv[1]; SetupFile s=Fixture();
    Paths(&s); NewPads(&s); Modeless(&s); Assignments(&s); History(&s); AllocationFailures(&s); Safety(&s); SetupFileFree(&s);
    for(int i=2;i<argc;i++) { Stock(argv[i]); }
    return 0;
}
