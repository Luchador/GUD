#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <mema.h>
#include <string.h>
#include "doorshadowformat.h"
#include "doorshadow.h"
#include "bg.h"
#include "dyn.h"
#include "environment.h"
#include "loadobjectmodel.h"
#include "tex.h"
#include "renderconfig.h"
#include "rendercache.h"

typedef struct DoorShadowRuntime {
    u8 *record;
    DoorRecord *door;
    Gfx *gdl;
    s32 size, next;
    DoorShadowPoint source[6];
} DoorShadowRuntime;
static DoorShadowRuntime g_DoorShadows[DOOR_SHADOW_MAX];
static s16 g_DoorShadowRooms[MAXROOMCOUNT];
static s32 g_DoorShadowCount;

static u32 doorShadowWord(u8 *p,s32 offset) { return *(u32 *)(p+offset); }
void doorShadowReset(void)
{
    s32 i;
    g_DoorShadowCount=0;
    for(i=0;i<MAXROOMCOUNT;i++)g_DoorShadowRooms[i]=-1;
}
void doorShadowExpandRoomBounds(s32 room)
{
    s32 i,v,a;
    if(!g_DoorShadowCount||room<=0||room>=MAXROOMCOUNT)return;
    for(i=g_DoorShadowRooms[room];i>=0;i=g_DoorShadows[i].next)
        for(v=0;v<6;v++)for(a=0;a<3;a++) {
            f32 p=g_DoorShadows[i].source[v].position[a]+*(f32 *)(g_DoorShadows[i].record+DOOR_SHADOW_ORIGIN+a*4);
            if(p<g_BgRoomInfo[room].minbounds.f[a])g_BgRoomInfo[room].minbounds.f[a]=p;
            if(p>g_BgRoomInfo[room].maxbounds.f[a])g_BgRoomInfo[room].maxbounds.f[a]=p;
        }
}
void doorShadowInit(PropDefHeaderRecord *commands)
{
    PropDefHeaderRecord *cmd=commands;
    s32 i;
    doorShadowReset();
    while(cmd&&cmd->type!=PROPDEF_END) {
        if(cmd->type==PROPDEF_DOOR_SHADOW&&g_DoorShadowCount<DOOR_SHADOW_MAX) {
            u8 *p=(u8 *)cmd;
            s32 room=doorShadowWord(p,DOOR_SHADOW_ROOM);
            u32 bytes=doorShadowWord(p,DOOR_SHADOW_GDL_SIZE);
            if((doorShadowWord(p,12)&0xf8u)!=0xf8u&&room>0&&room<g_MaxNumRooms
                &&doorShadowWord(p,DOOR_SHADOW_FORMAT)==DOOR_SHADOW_VERSION
                &&bytes&&bytes<=DOOR_SHADOW_GDL_CAPACITY&&!(bytes&7)
                &&doorShadowWord(p,DOOR_SHADOW_DIRECTION)<=3&&doorShadowWord(p,DOOR_SHADOW_LAYER)<=1) {
                DoorShadowRuntime *s=&g_DoorShadows[g_DoorShadowCount];
                s32 v,a;
                ObjectRecord *door=setupGetPtrToCommandByIndex((s32)doorShadowWord(p,DOOR_SHADOW_DOOR));
                s->record=p;s->door=door&&door->type==PROPDEF_DOOR?(DoorRecord *)door:NULL;
                s->gdl=NULL;s->size=0;s->next=g_DoorShadowRooms[room];
                for(v=0;v<6;v++) {
                    u8 *vertex=p+DOOR_SHADOW_VERTICES+v*16;
                    for(a=0;a<3;a++)s->source[v].position[a]=*(s16 *)(vertex+a*2);
                    s->source[v].uv[0]=*(s16 *)(vertex+8);s->source[v].uv[1]=*(s16 *)(vertex+10);
                    s->source[v].alpha=vertex[15];s->source[v].rgb=0;
                }
                g_DoorShadowRooms[room]=g_DoorShadowCount++;
            }
        }
        cmd+=sizepropdef(cmd);
    }
    for(i=1;i<g_MaxNumRooms;i++)doorShadowExpandRoomBounds(i);
}
void doorShadowFreeRoom(s32 room)
{
    s32 i;
    if(!g_DoorShadowCount||room<=0||room>=MAXROOMCOUNT)return;
    for(i=g_DoorShadowRooms[room];i>=0;i=g_DoorShadows[i].next) {
        DoorShadowRuntime *s=&g_DoorShadows[i];
        if(s->gdl){memaFree(s->gdl,s->size);s->gdl=NULL;s->size=0;renderInvalidateDisplayListCache();}
    }
}
static void doorShadowLoad(DoorShadowRuntime *s)
{
    u32 bytes=doorShadowWord(s->record,DOOR_SHADOW_GDL_SIZE);
    /* The generated template contains at most one ordinary texture request.
     * Reserve expansion space plus an aligned input tail, then shrink once. */
    s32 capacity=(bytes+2048+15)&~15;
    s32 allocation=capacity+((bytes+15)&~15),size;
    Gfx *memory=memaAlloc(allocation);
    Gfx *input;
    enum CCRMLUT lut;
    if(!memory){renderCacheRequestReclaim();return;}
    input=(Gfx *)((u8 *)memory+capacity);
    memcpy(input,s->record+DOOR_SHADOW_GDL,bytes);
    size=texLoadFromGdl(input,bytes,memory,NULL);
    if(size<=0||size>capacity){memaFree(memory,allocation);return;}
    lut=doorShadowWord(s->record,DOOR_SHADOW_LAYER)
        ? (envGetCurrent()->FogEnabled?CCRMLUT_SECONDARY_ADDFOG:CCRMLUT_SECONDARY)
        : (envGetCurrent()->FogEnabled?CCRMLUT_PRIMARY_ADDFOG:CCRMLUT_PRIMARY);
    bgApplyDynamicCCRMLUT(memory,(Gfx *)((u8 *)memory+size),lut);
    s->size=(size+15)&~15;s->gdl=memory;
    memaRealloc((s32)memory,allocation,s->size);
}
static s16 doorShadowRound(f32 value) { return (s16)(value<0?value-0.5f:value+0.5f); }
Gfx *doorShadowRenderRoom(Gfx *gdl,s32 room,s32 layer)
{
    s32 i,drew=FALSE;
    if(!g_DoorShadowCount||room<=0||room>=MAXROOMCOUNT)return gdl;
    for(i=g_DoorShadowRooms[room];i>=0;i=g_DoorShadows[i].next) {
        DoorShadowRuntime *s=&g_DoorShadows[i];
        DoorShadowPoint points[DOOR_SHADOW_OUTPUT_VERTICES];
        Vtx *vertices;
        f32 opening=0;
        s32 v,a;
        if(doorShadowWord(s->record,DOOR_SHADOW_LAYER)!=layer)continue;
        if(!s->gdl)doorShadowLoad(s);
        if(!s->gdl||dynGetFreeVertexBytes()<DOOR_SHADOW_OUTPUT_VERTICES*sizeof(Vtx))continue;
        if(s->door&&s->door->prop&&s->door->maxFrac>0)opening=s->door->openPosition/s->door->maxFrac;
        doorShadowSplit(s->source,doorShadowWord(s->record,DOOR_SHADOW_DIRECTION),opening,
            doorShadowWord(s->record,DOOR_SHADOW_LIGHT),doorShadowWord(s->record,DOOR_SHADOW_DARK),points);
        vertices=dynAllocateVertices(DOOR_SHADOW_OUTPUT_VERTICES);
        for(v=0;v<DOOR_SHADOW_OUTPUT_VERTICES;v++) {
            for(a=0;a<3;a++)vertices[v].v.ob[a]=doorShadowRound(points[v].position[a]
                +*(f32 *)(s->record+DOOR_SHADOW_ORIGIN+a*4)
                -ptr_bgdata_room_fileposition_list[room].pos.f[a]);
            vertices[v].v.flag=0;
            vertices[v].v.tc[0]=doorShadowRound(points[v].uv[0]);vertices[v].v.tc[1]=doorShadowRound(points[v].uv[1]);
            vertices[v].v.cn[0]=points[v].rgb>>16;vertices[v].v.cn[1]=points[v].rgb>>8;
            vertices[v].v.cn[2]=points[v].rgb;vertices[v].v.cn[3]=doorShadowRound(points[v].alpha);
        }
        gSPSegment(gdl++,SPSEGMENT_BG_VTX,OS_K0_TO_PHYSICAL(vertices));
        gSPDisplayList(gdl++,OS_K0_TO_PHYSICAL(s->gdl));drew=TRUE;
    }
    if(drew) { gSPSegment(gdl++,SPSEGMENT_BG_VTX,OS_K0_TO_PHYSICAL(g_BgRoomInfo[room].vertices)); }
    return gdl;
}

s32 doorShadowHasSecondary(s32 room)
{
    s32 i;
    if(!g_DoorShadowCount||room<=0||room>=MAXROOMCOUNT)return FALSE;
    for(i=g_DoorShadowRooms[room];i>=0;i=g_DoorShadows[i].next)
        if(doorShadowWord(g_DoorShadows[i].record,DOOR_SHADOW_LAYER))return TRUE;
    return FALSE;
}
