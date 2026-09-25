#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <mema.h>
#include <string.h>
#include "bgtransparency.h"
#include "doorshadowformat.h"
#include "doorshadow.h"
#include "bg.h"
#include "bgonecycle.h"
#include "bgroomtrans.h"
#include "dyn.h"
#include "environment.h"
#include "loadobjectmodel.h"
#include "line_tri_intersect.h"
#include "tex.h"
#include "renderconfig.h"
#include "rendercache.h"
#include "matrixmath.h"
#include "player.h"

/* Recenter before increasing precision: a shadow far from its room origin
 * should have the same precision as an equally sized shadow near it. */
#define DOOR_SHADOW_VERTEX_SCALE 64.0f

typedef struct DoorShadowMapping {
    s32 axis[2];
    f32 weight[2][2];
} DoorShadowMapping;

typedef struct DoorShadowRuntime {
    u8 *record;
    DoorRecord *door;
    Gfx *gdl;
    Gfx *oneCycleGdl;
    s32 size, next;
    s16 hitTexture;
    DoorShadowPoint source[6];
    f32 center[3], vertexScale[3];
    DoorShadowMapping mapping[2];
} DoorShadowRuntime;
static DoorShadowRuntime g_DoorShadows[DOOR_SHADOW_MAX];
static s16 g_DoorShadowRooms[MAXROOMCOUNT];
static s32 g_DoorShadowCount;

static u32 doorShadowWord(u8 *p,s32 offset) { return *(u32 *)(p+offset); }
static s16 doorShadowHitTexture(u8 *record)
{
    Gfx *gdl=(Gfx *)(record+DOOR_SHADOW_GDL);
    s32 count=doorShadowWord(record,DOOR_SHADOW_GDL_SIZE)/sizeof(Gfx),i;
    s16 texture=-1;
    /* Read the compact material before texture expansion. This keeps hits
     * independent of rendering, room-cache eviction and allocation failure.
     * Detail materials also load the base image last, just like normal BG. */
    for(i=0;i<count;i++) {
        u32 op=gdl[i].words.w0>>24;
        if(op==G_VTX||op==(u8)G_ENDDL)break;
        if(op==G_NOOP&&!BG_EDITOR_IS_MARKER(gdl[i].words.w0,gdl[i].words.w1))
            texture=gdl[i].words.w1&0xfff;
    }
    return texture;
}
static void doorShadowPrepareGeometry(DoorShadowRuntime *s)
{
    s32 a,v,t;
    for(a=0;a<3;a++) {
        f32 low=s->source[0].position[a],high=low,scale=1;
        for(v=1;v<6;v++) {
            if(s->source[v].position[a]<low)low=s->source[v].position[a];
            if(s->source[v].position[a]>high)high=s->source[v].position[a];
        }
        /* An integer center preserves the original corners exactly, including
         * the full asymmetric s16 range. Use powers of two for exact inverses. */
        s->center[a]=(s32)((low+high)*0.5f);
        low-=s->center[a];high-=s->center[a];
        while(scale<DOOR_SHADOW_VERTEX_SCALE&&low*scale*2>=-32768&&high*scale*2<=32767)scale*=2;
        s->vertexScale[a]=scale;
    }
    for(t=0;t<2;t++) {
        DoorShadowMapping *m=&s->mapping[t];
        DoorShadowPoint *p=s->source+t*3;
        f32 u[3],v[3],best=0;
        for(a=0;a<3;a++) {u[a]=p[1].position[a]-p[0].position[a];v[a]=p[2].position[a]-p[0].position[a];}
        memset(m,0,sizeof(*m));
        /* Project onto the plane with the largest determinant, so vertical
         * and sloping surfaces work as well as horizontal floors. */
        for(a=0;a<3;a++) {
            s32 b=(a+1)%3;
            f32 det=u[a]*v[b]-u[b]*v[a],magnitude=det<0?-det:det;
            if(magnitude>best) {
                best=magnitude;m->axis[0]=a;m->axis[1]=b;
                m->weight[0][0]=v[b]/det;m->weight[0][1]=-v[a]/det;
                m->weight[1][0]=-u[b]/det;m->weight[1][1]=u[a]/det;
            }
        }
    }
}
void doorShadowReset(void)
{
    s32 i;
    g_DoorShadowCount=0;
    for(i=0;i<MAXROOMCOUNT;i++)g_DoorShadowRooms[i]=-1;
}
s32 doorShadowHasRoom(s32 room)
{
    return g_DoorShadowCount&&room>0&&room<MAXROOMCOUNT&&g_DoorShadowRooms[room]>=0;
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
                s->gdl=NULL;s->oneCycleGdl=NULL;s->size=0;s->next=g_DoorShadowRooms[room];
                s->hitTexture=doorShadowHitTexture(p);
                for(v=0;v<6;v++) {
                    u8 *vertex=p+DOOR_SHADOW_VERTICES+v*16;
                    for(a=0;a<3;a++)s->source[v].position[a]=*(s16 *)(vertex+a*2);
                    s->source[v].uv[0]=*(s16 *)(vertex+8);s->source[v].uv[1]=*(s16 *)(vertex+10);
                    s->source[v].alpha=vertex[15];s->source[v].rgb=0;
                }
                doorShadowPrepareGeometry(s);
                g_DoorShadowRooms[room]=g_DoorShadowCount++;
            }
        }
        cmd+=sizepropdef(cmd);
    }
    for(i=1;i<g_MaxNumRooms;i++)doorShadowExpandRoomBounds(i);
}
bool doorShadowTestHit(s32 room,coord3d *from,coord3d *to,coord3d *dir,HitThing *hit)
{
    s32 i,t,found=FALSE;
    f32 bestdist=0;
    if(!g_DoorShadowCount||room<=0||room>=MAXROOMCOUNT)return FALSE;
    for(i=g_DoorShadowRooms[room];i>=0;i=g_DoorShadows[i].next) {
        DoorShadowRuntime *s=&g_DoorShadows[i];
        Vertex *vertices=(Vertex *)(s->record+DOOR_SHADOW_VERTICES);
        coord3d *origin=(coord3d *)(s->record+DOOR_SHADOW_ORIGIN);
        /* Preserve the normal BG exception for Archives light-shaft images. */
        if(s->hitTexture==0x4fd)continue;
        for(t=0;t<2;t++) {
            HitThing candidate;
            Vertex *v=vertices+t*3;
            f32 dx,dy,dz,distance;
            /* Collision uses both original triangles, never the animated
             * light/dark split or the renderer's rescaled vertex buffer.
             * Use the saved origin even if later edits recenter the BG room. */
            if(!intersectRayTriangle(v,v+1,v+2,origin,from,to,dir,&candidate))continue;
            dx=candidate.hitpos.x-from->x;dy=candidate.hitpos.y-from->y;dz=candidate.hitpos.z-from->z;
            distance=dx*dx+dy*dy+dz*dz;
            if(!found||distance<bestdist) {
                found=TRUE;bestdist=distance;
                candidate.vtx0=v;candidate.vtx1=v+1;candidate.vtx2=v+2;
                candidate.texturenum=s->hitTexture;
                /* Shadows cannot contain breakable light fixtures and have
                 * no static BG triangle command or resident tile binding. */
                candidate.tricmd=NULL;candidate.unk28=0;
                candidate.tileformat=-1;candidate.tilesize=-1;
                *hit=candidate;
            }
        }
    }
    return found;
}
void doorShadowFreeRoom(s32 room)
{
    s32 i;
    if(!g_DoorShadowCount||room<=0||room>=MAXROOMCOUNT)return;
    for(i=g_DoorShadowRooms[room];i>=0;i=g_DoorShadows[i].next) {
        DoorShadowRuntime *s=&g_DoorShadows[i];
        if(s->oneCycleGdl){renderCacheFree(s->oneCycleGdl);s->oneCycleGdl=NULL;renderInvalidateDisplayListCache();}
        if(s->gdl){memaFree(s->gdl,s->size);s->gdl=NULL;s->size=0;renderInvalidateDisplayListCache();}
    }
}
/* The shared cache owns these allocations and releases them after the RSP
 * queue drains. Drop references before that memory can be reused. */
void doorShadowClearRenderCaches(void)
{
    s32 i;
    for(i=0;i<g_DoorShadowCount;i++)g_DoorShadows[i].oneCycleGdl=NULL;
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
    /* Older editor templates wrote all of OtherMode H here, accidentally
     * replacing world colour/alpha dithering with zero (MAGICSQ/PATTERN).
     * Repair the generated header in RAM so saved shadows need no rebuild.
     * Later authored commands still apply normally. */
    if(bytes>=5*sizeof(Gfx)&&input[4].words.w0==0xba000020u) {
        input[4].words.w0=0xba000818u;
        input[4].words.w1&=0xffffff00u;
    }
    size=texLoadFromGdl(input,bytes,memory,NULL);
    if(size<=0||size>capacity){memaFree(memory,allocation);return;}
    lut=doorShadowWord(s->record,DOOR_SHADOW_LAYER)
        ? (envGetCurrent()->FogEnabled?CCRMLUT_SECONDARY_ADDFOG:CCRMLUT_SECONDARY)
        : (envGetCurrent()->FogEnabled?CCRMLUT_PRIMARY_ADDFOG:CCRMLUT_PRIMARY);
    bgApplyDynamicCCRMLUT(memory,(Gfx *)((u8 *)memory+size),lut);
    s->size=(size+15)&~15;s->gdl=memory;
    memaRealloc((s32)memory,allocation,s->size);
    /* Use the same converter as the owning BG pass. Otherwise AA-Off rooms
     * sample the base texture while their shadows keep blending distant mips. */
    if(renderCacheIsEnabled()) {
        s32 layer=doorShadowWord(s->record,DOOR_SHADOW_LAYER);
        s32 alternateSize=layer?bgBuildCutoutGdl(memory,size,NULL,0)
            :bgBuildOneCycleGdl(memory,size,NULL,0);
        if(alternateSize>0) {
            Gfx *alternate=renderCacheAlloc(alternateSize);
            if(alternate) {
                if((layer?bgBuildCutoutGdl(memory,size,alternate,alternateSize)
                        :bgBuildOneCycleGdl(memory,size,alternate,alternateSize))>0)
                    s->oneCycleGdl=alternate;
                else renderCacheFree(alternate);
            }
        }
    }
}
static s16 doorShadowRound(f32 value) { return (s16)(value<0?value-0.5f:value+0.5f); }
static s16 doorShadowRoundClamped(f32 value,f32 low,f32 high)
{
    if(value<low)value=low;
    if(value>high)value=high;
    return doorShadowRound(value);
}
static void doorShadowWriteVertex(DoorShadowRuntime *s,s32 index,DoorShadowPoint *point,Vtx *vertex)
{
    DoorShadowMapping *m=&s->mapping[index/9];
    DoorShadowPoint *p=s->source+(index/9)*3;
    f32 position[3],delta[2],w1,w2;
    s32 a;
    for(a=0;a<3;a++) {
        vertex->v.ob[a]=doorShadowRound((point->position[a]-s->center[a])*s->vertexScale[a]);
        position[a]=vertex->v.ob[a]/s->vertexScale[a]+s->center[a];
    }
    for(a=0;a<2;a++)delta[a]=position[m->axis[a]]-p[0].position[m->axis[a]];
    w1=delta[0]*m->weight[0][0]+delta[1]*m->weight[0][1];
    w2=delta[0]*m->weight[1][0]+delta[1]*m->weight[1][1];
    /* Evaluate UVs at the positions actually sent to the RSP. Independent
     * rounding of an unsnapped UV and a snapped position makes textures swim.
     * Each original triangle keeps its own mapping, including diagonal seams. */
    for(a=0;a<2;a++)vertex->v.tc[a]=doorShadowRoundClamped(p[0].uv[a]
        +w1*(p[1].uv[a]-p[0].uv[a])+w2*(p[2].uv[a]-p[0].uv[a]),-32768,32767);
    vertex->v.flag=0;
    vertex->v.cn[0]=point->rgb>>16;vertex->v.cn[1]=point->rgb>>8;vertex->v.cn[2]=point->rgb;
    vertex->v.cn[3]=doorShadowRoundClamped(p[0].alpha
        +w1*(p[1].alpha-p[0].alpha)+w2*(p[2].alpha-p[0].alpha),0,255);
}
Gfx *doorShadowRenderRoom(Gfx *gdl,s32 room,s32 layer,bool oneCycle)
{
    s32 i,drew=FALSE;
    if(!g_DoorShadowCount||room<=0||room>=MAXROOMCOUNT)return gdl;
    for(i=g_DoorShadowRooms[room];i>=0;i=g_DoorShadows[i].next) {
        DoorShadowRuntime *s=&g_DoorShadows[i];
        DoorShadowPoint points[DOOR_SHADOW_OUTPUT_VERTICES];
        Vtx *vertices;
        Mtx *matrix;
        Mtxf transform;
        f32 opening=0;
        s32 v,a;
        if(doorShadowWord(s->record,DOOR_SHADOW_LAYER)!=layer)continue;
        if(!s->gdl)doorShadowLoad(s);
        if(!s->gdl||dynGetFreeVertexBytes()<DOOR_SHADOW_OUTPUT_VERTICES*sizeof(Vtx)+sizeof(Mtx))continue;
        if(s->door&&s->door->prop&&s->door->maxFrac>0)opening=s->door->openPosition/s->door->maxFrac;
        doorShadowSplit(s->source,doorShadowWord(s->record,DOOR_SHADOW_DIRECTION),opening,
            doorShadowWord(s->record,DOOR_SHADOW_LIGHT),doorShadowWord(s->record,DOOR_SHADOW_DARK),points);
        vertices=dynAllocateVertices(DOOR_SHADOW_OUTPUT_VERTICES);
        for(v=0;v<DOOR_SHADOW_OUTPUT_VERTICES;v++)doorShadowWriteVertex(s,v,&points[v],&vertices[v]);
        matrix=dynAllocateMatrix();
        matrix_4x4_set_identity(&transform);
        for(a=0;a<3;a++) {
            transform.m[a][a]=g_LevelInverseScale/s->vertexScale[a];
            transform.m[3][a]=(s->center[a]+*(f32 *)(s->record+DOOR_SHADOW_ORIGIN+a*4))
                *g_LevelInverseScale-g_CurrentPlayer->current_model_pos.f[a];
        }
        matrix_4x4_f32_to_s32(transform.m,matrix->m);
        gSPMatrix(gdl++,OS_K0_TO_PHYSICAL(matrix),G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
        gSPSegment(gdl++,SPSEGMENT_BG_VTX,OS_K0_TO_PHYSICAL(vertices));
        if(oneCycle&&s->oneCycleGdl) {
            if(layer) {
                gDPPipeSync(gdl++);
                gDPSetAlphaCompare(gdl++,G_AC_NONE);
                gDPSetBlendColor(gdl++,0,0,0,BG_CUTOUT_THRESHOLD);
            }
            gSPDisplayList(gdl++,OS_K0_TO_PHYSICAL(s->oneCycleGdl));
        } else {
            gSPDisplayList(gdl++,OS_K0_TO_PHYSICAL(s->gdl));
        }
        drew=TRUE;
    }
    if(drew) {
        gdl=applyRoomMatrixToDisplayList(gdl,room);
        gSPSegment(gdl++,SPSEGMENT_BG_VTX,OS_K0_TO_PHYSICAL(g_BgRoomInfo[room].vertices));
    }
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
