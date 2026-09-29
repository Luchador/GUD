#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <src/propconstants.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"
#include "setup_compare.h"
#include "bghistory.h"
#include "modelload.h"
#include "objectload.h"
#include "doorshadow.h"

void Fixtures(const char *root);
void FreeFixtures(void);
static const char *why;
#define Require(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, why); abort(); } } while (0)
static DWORD Read(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static void Near(double a, double b) { assert(fabs(a-b) < .002); }
static int Model(const char *wanted)
{
    const char *name;
    for (int i=0;ModelGetPropDefinition(i,&name,NULL);i++) if (!strcmp(wanted,name)) return i;
    abort();
}
static void Link(const SetupFile *s, DWORD item, LONG body, LONG door)
{
    LONG b,d; Require(SetupFileGetSafeLink(s,item,&b,&d,&why)); assert(b==body && d==door);
    if (body<0) return;
    LONG ic=SetupFileObjectCommand(s,item),bc=SetupFileObjectCommand(s,body),dc=SetupFileObjectCommand(s,door);
    assert(ic!=(LONG)item); /* Fixture has tags, guards, and outro commands. */
    DWORD at=Read(s->data+12); int found=0;
    for (LONG command=0;s->data[at+3]!=PROPDEF_END;command++)
    {
        if (s->data[at+3]==PROPDEF_SAFE_ITEM && command+(LONG)Read(s->data+at+4)==ic)
        {
            assert(command+(LONG)Read(s->data+at+8)==bc && command+(LONG)Read(s->data+at+12)==dc);
            assert(Read(s->data+at+16)==0); found++;
        }
        at+=SetupObjectWordCount(s->data[at+3])*4;
    }
    assert(found==1);
}
static void RoundTrip(const char *dir, const SetupFile *s, DWORD item, LONG body, LONG door)
{
    SetupFile loaded={0};
    Require(SetupSaveProjectFile(dir,s,&why)); Require(SetupLoadProjectFile(dir,s->name,&loaded,&why));
    SetupAssertNativeEqual(s,&loaded); Link(&loaded,item,body,door); SetupFileFree(&loaded);
}
static void Geometry(const char *dir,const SetupFile *s,DWORD body,DWORD door,float scale,const double pos[3])
{
    SetupObjectGeometry mesh={0}; Require(ObjectLoadSetupGeometry(dir,s,NULL,scale,&mesh,&why));
    const SetupBoundPad *p=&s->boundpads[s->objects[body].pad-10000];
    double lo[2][3]={{1e9,1e9,1e9},{1e9,1e9,1e9}}, hi[2][3]={{-1e9,-1e9,-1e9},{-1e9,-1e9,-1e9}};
    unsigned seen=0;
    for(DWORD t=0;t<mesh.tricount;t++)
    {
        int which=mesh.objectindices[t]==body?0:mesh.objectindices[t]==door?1:-1;
        if(which<0)continue;
        seen|=1u<<which;
        for(int c=0;c<3;c++)
        {
            BgVertex *v=&mesh.tris[t*3+c];double x=v->x-pos[0],y=v->y-pos[1],z=v->z-pos[2];
            double local[3]={x*p->pad.look[2]-z*p->pad.look[0],y,x*p->pad.look[0]+z*p->pad.look[2]};
            for(int a=0;a<3;a++){if(local[a]<lo[which][a])lo[which][a]=local[a];if(local[a]>hi[which][a])hi[which][a]=local[a];}
        }
    }
    assert(seen==3); Near(lo[0][1],0); Near(hi[0][1],100);
    Near(lo[0][0],-50); Near(hi[0][0],50);
    Near(lo[1][1],500.0/57); Near(hi[1][1],5200.0/57);
    Near(lo[1][0],-2350.0/57); Near(hi[1][0],2350.0/57);
    /* Main panel sits inside the frame; handle projects toward the viewer. */
    assert(lo[1][2]>0 && lo[1][2]<hi[0][2] && hi[1][2]>hi[0][2]);
    ObjectGeometryFree(&mesh);
}
static void Place(const char *dir,const SetupFile *source,float scale,const double facing[3],int bodymodel,int doormodel)
{
    SetupFile s={0},before={0};DWORD body,door,item,b2,d2;BOOL changed;
    EditHistory history={0};EditHistoryTransaction tx;EditHistoryAsset asset;
    BgDocument bg={0};StanFile stan={0};double pos[3]={300,-20,-400};
    Require(SetupFileClone(source,&s,&why));
    EditHistoryReset(&history,&bg,&s,&stan);
    Require(EditHistoryBeginSetupEdit(&history,&s,"Add Safe",&tx,&why));
    Require(SetupFileAddSafe(&s,bodymodel,doormodel,scale,pos,facing,&body,&door,&why));
    assert(s.objectcount==source->objectcount+2 && s.boundpadcount==source->boundpadcount+2 && s.padcount==source->padcount);
    assert(s.objects[body].type==PROPDEF_SAFE && s.objects[body].modelid==bodymodel);
    assert(s.objects[door].type==PROPDEF_DOOR && s.objects[door].modelid==doormodel);
    assert(s.objects[body].pad-10000 != s.objects[door].pad);
    const SetupPad *bp=&s.boundpads[s.objects[body].pad-10000].pad,*dp=&s.boundpads[s.objects[door].pad].pad;
    for(int a=0;a<3;a++){Near(bp->pos[a]/scale,pos[a]);Near(dp->pos[a]/scale,pos[a]);}
    SetupObjectProperties props;Require(SetupFileGetObjectProperties(&s,door,&props,&why));
    assert(props.door.type==DOORTYPE_SWINGING && props.door.flags==DOORFLAG_FLIP && props.door.travel==90 && props.door.clearance==1000);
    assert(!props.keyflags && props.door.closeframes==0xfffffff && (props.object.flags&PROPFLAG_NO_PORTAL_CLOSE));
    Geometry(dir,&s,body,door,scale,pos);
    Require(EditHistoryCommitEdit(&history,&bg,&s,&stan,&tx,&why));
    Require(EditHistoryUndo(&history,&bg,&s,&stan,&asset,&why));SetupAssertNativeEqual(source,&s);
    Require(EditHistoryRedo(&history,&bg,&s,&stan,&asset,&why));
    Require(SetupFileAddModel(&s,FALSE,bodymodel,scale,pos,&item,&why));
    Require(SetupFileClone(&s,&before,&why));
    assert(!SetupFileSetSafeLink(&s,door,body,door,&changed,&why));
    assert(!SetupFileSetSafeLink(&s,item,body,-1,&changed,&why));
    assert(!SetupFileSetSafeLink(&s,item,door,body,&changed,&why));
    SetupAssertNativeEqual(&before,&s);SetupFileFree(&before);
    EditHistoryReset(&history,&bg,&s,&stan);
    Require(EditHistoryBeginSetupEdit(&history,&s,"Link Safe Item",&tx,&why));
    Require(SetupFileSetSafeLink(&s,item,body,door,&changed,&why));assert(changed);Link(&s,item,body,door);
    Require(EditHistoryCommitEdit(&history,&bg,&s,&stan,&tx,&why));
    RoundTrip(dir,&s,item,body,door);
    Require(EditHistoryUndo(&history,&bg,&s,&stan,&asset,&why));Link(&s,item,-1,-1);
    Require(EditHistoryRedo(&history,&bg,&s,&stan,&asset,&why));Link(&s,item,body,door);
    Require(SetupFileSetSafeLink(&s,item,body,door,&changed,&why));assert(!changed);
    Require(SetupFileSetSafeLink(&s,item,-1,-1,&changed,&why));assert(changed);Link(&s,item,-1,-1);
    Require(SetupFileAddSafe(&s,bodymodel,doormodel,scale,pos,facing,&b2,&d2,&why));
    DWORD size=s.size;
    for(int i=0;i<8;i++)
    {
        /* Reuse a link earlier in the stream: positive safe/door offsets. */
        Require(SetupFileSetSafeLink(&s,item,b2,d2,&changed,&why));Link(&s,item,b2,d2);
        RoundTrip(dir,&s,item,b2,d2);assert(s.size<=size);size=s.size;
        Require(SetupFileSetSafeLink(&s,item,-1,-1,&changed,&why));Link(&s,item,-1,-1);assert(s.size==size);
    }
    DWORD anotheritem;
    Require(SetupFileAddModel(&s,FALSE,bodymodel,scale,pos,&anotheritem,&why));
    Require(SetupFileSetSafeLink(&s,anotheritem,b2,d2,&changed,&why));
    Require(SetupFileSetSafeLink(&s,item,body,door,&changed,&why));
    Link(&s,anotheritem,b2,d2);
    Require(SetupFileDeleteObject(&s,body,&why));
    LONG b,d;assert(!SetupFileGetSafeLink(&s,item,&b,&d,&why));
    Require(SetupFileAddSafe(&s,bodymodel,doormodel,scale,pos,facing,&b2,&d2,&why));assert(b2!=body);
    Require(SetupFileSetSafeLink(&s,item,-1,-1,&changed,&why));RoundTrip(dir,&s,item,-1,-1);
    EditHistoryFree(&history);SetupFileFree(&s);
}
/* Unmodified game function: verifies the exact threshold, unrelated pickups,
 * multiple links and missing doors without pretending to run an emulator. */
typedef int bool;
typedef struct ObjectRecord { DWORD flags2; } ObjectRecord;
typedef struct { void *prop; float openPosition; } DoorRecord;
typedef struct SafeObjectRecord { ObjectRecord *item; DoorRecord *door; struct SafeObjectRecord *next; } SafeObjectRecord;
static SafeObjectRecord *g_LevelLoadPropSafeItem;
#include "pickup.inc"
static void Pickup(void)
{
    ObjectRecord item={PROPFLAG2_LINKEDTOSAFE},unrelated={0};DoorRecord door={(void*)1,0};
    SafeObjectRecord link={&item,&door,NULL};g_LevelLoadPropSafeItem=&link;
    assert(!objCanPickupFromSafe(&item) && objCanPickupFromSafe(&unrelated));
    door.openPosition=.5f;assert(!objCanPickupFromSafe(&item));
    door.openPosition=1;assert(objCanPickupFromSafe(&item));
    door.openPosition=90;assert(objCanPickupFromSafe(&item));
    DoorRecord second={(void*)1,0};SafeObjectRecord another={&item,&second,NULL};link.next=&another;
    assert(!objCanPickupFromSafe(&item));second.openPosition=90;assert(objCanPickupFromSafe(&item));
    link.next=NULL;door.prop=NULL;assert(objCanPickupFromSafe(&item));
    g_LevelLoadPropSafeItem=NULL;assert(objCanPickupFromSafe(&item));
}
int main(int argc,char **argv)
{
    assert(argc==3);Fixtures(argv[2]);SetupFile source={0},s={0};
    Require(SetupLoadProjectFile(argv[1],"UsetupsafeZ",&source,&why));
    int bodymodel=Model(SETUP_DEFAULT_SAFE_MODEL),doormodel=Model(SETUP_DEFAULT_SAFE_DOOR_MODEL);
    const float scales[]={.15019713f,.53931433f,1.20648f,1};
    const double facing[][3]={{0,0,-1},{1,.5,1},{-1,0,0},{0,-1,0}};
    for(int scale=0;scale<4;scale++)for(int f=0;f<4;f++)Place(argv[1],&source,scales[scale],facing[f],bodymodel,doormodel);
    Require(SetupFileClone(&source,&s,&why));DWORD b,d;const double pos[3]={0,0,0},bad[3]={NAN,0,0};
    assert(!SetupFileAddSafe(&s,bodymodel,-1,1,pos,facing[0],&b,&d,&why));
    assert(!SetupFileAddSafe(&s,bodymodel,doormodel,0,pos,facing[0],&b,&d,&why));
    assert(!SetupFileAddSafe(&s,bodymodel,doormodel,1,pos,bad,&b,&d,&why));
    assert(!SetupFileAddSafe(&s,bodymodel,doormodel,1,NULL,facing[0],&b,&d,&why));
    assert(!SetupFileAddSafe(&s,bodymodel,doormodel,1,pos,facing[0],NULL,&d,&why));
    SetupAssertNativeEqual(&s,&source);Pickup();
    puts("PASS: safe pair atomic placement, real PsafeZ/PsafedoorZ alignment at four orientations/scales, unlocked stock swing defaults, and undo/redo.");
    puts("PASS: native command-relative safe links, add/change/remove/reuse, persistence, history, deleted targets, invalid edits, and game's closed/open pickup gate.");
    SetupFileFree(&s);SetupFileFree(&source);FreeFixtures();return 0;
}
