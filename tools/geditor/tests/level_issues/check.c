#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "levelissues.h"
#include "../../../../src/propconstants.h"

static const char *why;
static void Require(BOOL ok) { if (!ok) { fprintf(stderr, "%s\n", why); abort(); } }
static void Put(unsigned char *p, DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static StanFile Collision(void)
{
    unsigned char b[76]={0}; StanFile s={0};
    Put(b+4,0x0e00000c); Put(b+12,0x00010001);
    b[18]=0x30; b[19]=0x12; b[33]=100; b[37]=100; memcpy(b+52,"unstric",8);
    Require(StanLoadNative(b,sizeof(b),.25f,&s,&why)); return s;
}
static DWORD Count(const LevelIssueReport *r, LevelIssueKind kind)
{ DWORD n=0;for(DWORD i=0;i<r->count;i++)n+=r->items[i].kind==kind;return n; }
static LevelIssue Find(const LevelIssueReport *r, LevelIssueKind kind, DWORD index, BOOL bound)
{
    for(DWORD i=0;i<r->count;i++) if(r->items[i].kind==kind && r->items[i].pad.index==index && r->items[i].pad.bound==bound)
        return r->items[i];
    assert(!"Issue missing"); return (LevelIssue){0};
}
static void Diagnostics(void)
{
    StanFile stan=Collision(), none={0};
    BgDocumentRoom room={0}; BgDocument bg={.roomcount=1,.rooms=&room};
    SetupPad pads[4]={0}; SetupBoundPad bounds[1]={0}; SetupObject objects[2]={0}; SetupCharacter guards[2]={0};
    SetupFile setup={.pads=pads,.padcount=3,.boundpads=bounds,.boundpadcount=1,.objects=objects,.objectcount=2};
    for(int i=0;i<4;i++) { pads[i].pos[0]=i ? 1000 : 10;pads[i].pos[1]=20;pads[i].pos[2]=10;strcpy(pads[i].stanname,"old"); }
    pads[2].deleted=TRUE;bounds[0].pad=pads[1];
    objects[0].type=PROPDEF_PROP;objects[0].pad=10000;
    objects[1].type=PROPDEF_COLLECTABLE;objects[1].pad=1;objects[1].flags=PROPFLAG_ASSIGNEDTOCHR;
    LevelIssueReport r={0}; LevelIssueLocation location; SetupPad snapshot[4]; memcpy(snapshot,pads,sizeof(pads));
    unsigned char raw[76];memcpy(raw,stan.data,sizeof(raw));
    Require(LevelIssuesBuild(&bg,&setup,&stan,.25f,&r,&why));
    assert(r.count==2 && r.warnings==2 && !r.errors);
    LevelIssue ordinary=Find(&r,LEVEL_ISSUE_UNRESOLVED_PAD,1,FALSE);
    LevelIssue bound=Find(&r,LEVEL_ISSUE_UNRESOLVED_PAD,0,TRUE);
    assert(ordinary.target==LEVEL_ISSUE_PAD && bound.target==LEVEL_ISSUE_MODEL && bound.index==0);
    assert(!memcmp(snapshot,pads,sizeof(pads)) && !memcmp(raw,stan.data,sizeof(raw)) && !setup.dirty && !stan.dirty && !bg.dirty);
    Require(LevelIssueLocate(&ordinary,&bg,&setup,&stan,.25f,&location));
    assert(location.min[0]==3975 && location.max[0]==4025);
    Require(LevelIssueLocate(&bound,&bg,&setup,&stan,.25f,&location));
    objects[0].deleted=TRUE; assert(!LevelIssueLocate(&bound,&bg,&setup,&stan,.25f,&location));objects[0].deleted=FALSE;
    objects[0].pad=0; assert(!LevelIssueLocate(&bound,&bg,&setup,&stan,.25f,&location));objects[0].pad=10000;
    pads[1].deleted=TRUE; assert(!LevelIssueLocate(&ordinary,&bg,&setup,&stan,.25f,&location));pads[1].deleted=FALSE;
    LevelIssuesFree(&r);
    /* Doors use direct bound-pad IDs. Multiple owners select the pad itself. */
    objects[1].type=PROPDEF_DOOR;objects[1].flags=0;objects[1].pad=0;
    Require(LevelIssuesBuild(&bg,&setup,&stan,.25f,&r,&why));
    bound=Find(&r,LEVEL_ISSUE_UNRESOLVED_PAD,0,TRUE);assert(bound.target==LEVEL_ISSUE_PAD);LevelIssuesFree(&r);
    setup.objectcount=0;setup.characters=guards;setup.charactercount=1;guards[0].pad=1;guards[0].chrnum=42;
    Require(LevelIssuesBuild(&bg,&setup,&stan,.25f,&r,&why));
    ordinary=Find(&r,LEVEL_ISSUE_UNRESOLVED_PAD,1,FALSE);
    assert(ordinary.target==LEVEL_ISSUE_MODEL && ordinary.index==SETUP_CHARACTER_SELECTION_BIT);
    Require(LevelIssueLocate(&ordinary,&bg,&setup,&stan,.25f,&location));LevelIssuesFree(&r);
    setup.charactercount=2;guards[1]=guards[0];
    Require(LevelIssuesBuild(&bg,&setup,&stan,.25f,&r,&why));assert(Count(&r,LEVEL_ISSUE_CHARACTER_ID)==1);LevelIssuesFree(&r);
    guards[1].deleted=TRUE;
    pads[3].pos[0]=NAN;setup.padcount=4;
    Require(LevelIssuesBuild(&bg,&setup,&none,.25f,&r,&why));
    assert(Count(&r,LEVEL_ISSUE_MISSING_STAN)==1 && !Count(&r,LEVEL_ISSUE_UNRESOLVED_PAD));
    LevelIssue invalid=Find(&r,LEVEL_ISSUE_PAD_POSITION,3,FALSE);
    assert(invalid.target==LEVEL_ISSUE_NOWHERE && invalid.error && !LevelIssueLocate(&invalid,&bg,&setup,&stan,.25f,&location));
    assert(!Count(&r,LEVEL_ISSUE_CHARACTER_ID));LevelIssuesFree(&r);setup.padcount=3;
    assert(!LevelIssuesBuild(&bg,&setup,&stan,0,&r,&why) && !r.items);
    /* Broken connectivity remains navigable even if STAN preparation fails. */
    stan.tiles[0].room=2;stan.data[15]=2;stan.tiles[0].points[0].link=0x1234;stan.data[26]=0x12;stan.data[27]=0x34;
    BgPortal portal={0};portal.connectedroom1=1;portal.connectedroom2=1;portal.pointcount=3;portal.points[1].x=100;portal.points[2].z=100;
    bg.portals.portals=&portal;bg.portals.portalcount=1;
    unsigned char bytes[64]={0};setup.data=bytes;setup.size=sizeof(bytes);Put(bytes,0xfffffff0);
    Require(LevelIssuesBuild(&bg,&setup,&stan,.25f,&r,&why));
    assert(Count(&r,LEVEL_ISSUE_STAN_ROOM)==1 && Count(&r,LEVEL_ISSUE_STAN_LINK)==1);
    assert(Count(&r,LEVEL_ISSUE_PATROL)==1 && Count(&r,LEVEL_ISSUE_PORTAL)==1);
    for(DWORD i=0;i<r.count;i++) if(r.items[i].target==LEVEL_ISSUE_STAN || r.items[i].target==LEVEL_ISSUE_PORTAL_FACE)
    { Require(LevelIssueLocate(&r.items[i],&bg,&setup,&stan,.25f,&location));assert(location.max[0]>=100); }
    LevelIssuesFree(&r);
    memset(bytes,0,sizeof(bytes));Put(bytes+16,40);Put(bytes+40,56);bytes[44]=17;Put(bytes+56,0xffffffff);
    Require(LevelIssuesBuild(&bg,&setup,&none,.25f,&r,&why));
    assert(Count(&r,LEVEL_ISSUE_PATROL)==1);
    for(DWORD i=0;i<r.count;i++)if(r.items[i].kind==LEVEL_ISSUE_PATROL)assert(!r.items[i].error && strstr(r.items[i].subject,"17"));
    LevelIssuesFree(&r);StanFileFree(&stan);
    puts("PASS: unresolved/repairable/deleted pads, owner selection, invalid coordinates, broken STAN/portals/patrols, duplicate guards and read-only scans.");
}

static unsigned char *Read(const char *dir,const char *name,DWORD *size)
{
    char path[1024];snprintf(path,sizeof(path),"%s/%s",dir,name);FILE *f=fopen(path,"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));*size=ftell(f);rewind(f);unsigned char *b=malloc(*size);assert(b&&fread(b,1,*size,f)==*size);fclose(f);return b;
}
static void Depot(const char *dir)
{
    SetupFile setup={0};StanFile stan={0};BgDocument bg={0};LevelIssueReport r={0};DWORD size;unsigned char *b;
    const float scale=.21847887337207794f;
    Require(SetupLoadProjectFile(dir,"UsetupdepoZ",&setup,&why));
    b=Read(dir,"depot.stan",&size);Require(StanLoadNative(b,size,scale,&stan,&why));free(b);
    b=Read(dir,"depot.seg",&size);Require(BgDocumentLoad(b,size,scale,&bg,&why));free(b);
    Require(LevelIssuesBuild(&bg,&setup,&stan,scale,&r,&why));
    assert(Count(&r,LEVEL_ISSUE_UNRESOLVED_PAD)==3);
    Find(&r,LEVEL_ISSUE_UNRESOLVED_PAD,21,FALSE);Find(&r,LEVEL_ISSUE_UNRESOLVED_PAD,145,FALSE);
    LevelIssue lamp=Find(&r,LEVEL_ISSUE_UNRESOLVED_PAD,82,TRUE);
    assert(lamp.target==LEVEL_ISSUE_MODEL && setup.objects[lamp.index].modelid==513);
    for(DWORD i=0;i<r.count;i++)
    {
        printf("Depot: %s: %s\n",r.items[i].subject,r.items[i].description);
        if(r.items[i].target!=LEVEL_ISSUE_NOWHERE) { LevelIssueLocation location;Require(LevelIssueLocate(&r.items[i],&bg,&setup,&stan,scale,&location)); }
    }
    printf("PASS: actual Depot has %u errors / %u warnings; unresolved pads 21, 145, bound 82; custom lamp selected.\n",r.errors,r.warnings);
    LevelIssuesFree(&r);BgDocumentFree(&bg);StanFileFree(&stan);SetupFileFree(&setup);
}

#include "ui.c"
int main(int argc,char **argv)
{ Diagnostics(); Window(); Navigation();if(argc==2)Depot(argv[1]);return 0; }
