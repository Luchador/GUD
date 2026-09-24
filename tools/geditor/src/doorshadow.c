#include "doorshadow.h"
#include "setupmeta.h"
#include <src/propconstants.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static DWORD previewIndex = (DWORD)-1;
static int previewPercent;
static short Read16(const unsigned char *p) { return (short)((unsigned)p[0]*256+p[1]); }
static void Write16(unsigned char *p, short v) { p[0]=(unsigned short)v>>8;p[1]=(unsigned char)v; }
static float ReadFloat(const unsigned char *p) { union { DWORD u;float f; } v;v.u=SetupMetaRead32(p);return v.f; }
static void WriteFloat(unsigned char *p,float f) { union { DWORD u;float f; } v;v.f=f;SetupMetaWrite32(p,v.u); }
static const unsigned char *Record(const SetupFile *s,DWORD i)
{
    if (!s || i>=s->objectcount || s->objects[i].type!=PROPDEF_DOOR_SHADOW || s->objects[i].deleted
        || s->objects[i].sourceoffset>s->size || s->size-s->objects[i].sourceoffset<DOOR_SHADOW_BYTES) return NULL;
    return s->data+s->objects[i].sourceoffset;
}
static void Points(const unsigned char *p,DoorShadowPoint points[6])
{
    for(int i=0;i<6;i++) {
        const unsigned char *v=p+DOOR_SHADOW_VERTICES+i*16;
        for(int a=0;a<3;a++)points[i].position[a]=Read16(v+a*2);
        points[i].uv[0]=Read16(v+8);points[i].uv[1]=Read16(v+10);points[i].alpha=v[15];points[i].rgb=0;
    }
}
static BOOL AxisValid(const DoorShadowPoint p[6],DWORD direction)
{
    int axis=direction<2?0:2;
    float low=p[0].position[axis],high=low;
    for(int i=1;i<6;i++){low=fminf(low,p[i].position[axis]);high=fmaxf(high,p[i].position[axis]);}
    if (!(high>low))return FALSE;
    for(int i=0;i<6;i++)if(p[i].position[axis]!=low&&p[i].position[axis]!=high)return FALSE;
    return TRUE;
}
BOOL DoorShadowGet(const SetupFile *s,DWORD i,DoorShadowProperties *out)
{
    const unsigned char *p=Record(s,i);if(!p)return FALSE;
    out->room=SetupMetaRead32(p+DOOR_SHADOW_ROOM);out->layer=SetupMetaRead32(p+DOOR_SHADOW_LAYER);
    out->direction=SetupMetaRead32(p+DOOR_SHADOW_DIRECTION);
    out->light=SetupMetaRead32(p+DOOR_SHADOW_LIGHT);out->dark=SetupMetaRead32(p+DOOR_SHADOW_DARK);
    out->door=SetupFileCommandObject(s,(LONG)SetupMetaRead32(p+DOOR_SHADOW_DOOR));
    if(out->door>=0&&(s->objects[out->door].type!=PROPDEF_DOOR||s->objects[out->door].deleted))out->door=-1;
    return TRUE;
}
BOOL DoorShadowSet(SetupFile *s,const DoorShadowEdit *edit,BOOL *changed,const char **why)
{
    const unsigned char *p=edit?Record(s,edit->objectindex):NULL;DWORD offset,value;
    *changed=FALSE;*why="The Door Shadow is no longer available.";if(!p)return FALSE;
    value=(DWORD)edit->value;
    switch(edit->field) {
    case DOOR_SHADOW_EDIT_DOOR:
        offset=DOOR_SHADOW_DOOR;
        if(edit->value==-1)break;
        *why="Pick a live door in the viewport.";
        if(value>=s->objectcount||s->objects[value].type!=PROPDEF_DOOR||s->objects[value].deleted)return FALSE;
        value=(DWORD)SetupFileObjectCommand(s,value);if(value==(DWORD)-1)return FALSE;break;
    case DOOR_SHADOW_EDIT_DIRECTION: {
        DoorShadowPoint points[6];Points(p,points);offset=DOOR_SHADOW_DIRECTION;
        *why="The quadrilateral must have opposite edges aligned with the selected X or Z direction.";
        if(value>3||!AxisValid(points,value))return FALSE;
        break;
    }
    case DOOR_SHADOW_EDIT_LIGHT: case DOOR_SHADOW_EDIT_DARK:
        offset=edit->field==DOOR_SHADOW_EDIT_LIGHT?DOOR_SHADOW_LIGHT:DOOR_SHADOW_DARK;
        *why="RGB components must be from 0 to 255.";if(value>0xffffffu)return FALSE;break;
    default:return FALSE;
    }
    *why="";if(SetupMetaRead32(p+offset)==value)return TRUE;
    SetupMetaWrite32(s->data+s->objects[edit->objectindex].sourceoffset+offset,value);
    s->dirty=TRUE;*changed=TRUE;return TRUE;
}
void DoorShadowSetPreview(DWORD index,int percent) { previewIndex=index;previewPercent=percent<0?0:percent>100?100:percent; }
int DoorShadowGetPreview(const SetupFile *s,DWORD index)
{
    DoorShadowProperties p;
    if(previewIndex==index)return previewPercent;
    return DoorShadowGet(s,index,&p)&&p.door>=0&&(s->objects[p.door].flags&PROPFLAG_STARTOPEN)?100:0;
}
BOOL DoorShadowBuildPreview(const SetupFile *s,DWORD index,float scale,BgVertex vertices[18],
    unsigned short *tag,BgRenderFlags *flags,const char **why)
{
    const unsigned char *p=Record(s,index);DoorShadowProperties props;
    DoorShadowPoint source[6],out[18];BgRenderState state;BgMaterial material;BgRenderAlpha alpha;
    *why="Invalid Door Shadow geometry.";if(!p||!(scale>0)||!DoorShadowGet(s,index,&props))return FALSE;
    DWORD bytes=SetupMetaRead32(p+DOOR_SHADOW_GDL_SIZE);
    if(bytes>DOOR_SHADOW_GDL_CAPACITY||(bytes&7))return FALSE;
    BgRenderStateInit(&state,props.layer!=0);BgMaterialInit(&material);
    for(DWORD pc=0;pc<bytes;pc+=8) {
        DWORD a=SetupMetaRead32(p+DOOR_SHADOW_GDL+pc),b=SetupMetaRead32(p+DOOR_SHADOW_GDL+pc+4);
        if((a>>24)==4)break;
        BgRenderStateRead(&state,a,b);BgMaterialReadCommand(&material,a,b);
    }
    Points(p,source);doorShadowSplit(source,props.direction,DoorShadowGetPreview(s,index)/100.0f,props.light,props.dark,out);
    *tag=BgMaterialTextureId(&material)|(props.layer?BG_TRI_SECONDARY:0);
    if(state.geometrymode&0x2000u)*tag|=BG_TRI_CULL_BACK;
    *flags=BgRenderStateFlags(&state)|BgRenderMaterialWrap(&material)|BgRenderModelCulling(&state);
    alpha=BgRenderGetAlpha(&state,&material);
    if(!alpha.texture)*flags|=BG_RENDER_IGNORE_TEXTURE_ALPHA;
    if(!BgRenderUsesFog(&state,&material))*flags|=BG_RENDER_NO_FOG;
    for(int i=0;i<18;i++) {
        BgVertex *v=&vertices[i];ZeroMemory(v,sizeof(*v));
        v->x=(out[i].position[0]+ReadFloat(p+DOOR_SHADOW_ORIGIN))/scale;
        v->y=(out[i].position[1]+ReadFloat(p+DOOR_SHADOW_ORIGIN+4))/scale;
        v->z=(out[i].position[2]+ReadFloat(p+DOOR_SHADOW_ORIGIN+8))/scale;
        v->s=out[i].uv[0]/32;v->t=out[i].uv[1]/32;
        v->r=out[i].rgb>>16;v->g=out[i].rgb>>8;v->b=out[i].rgb;
        v->a=BgRenderVertexAlpha(alpha,(unsigned char)roundf(out[i].alpha));
    }
    *why="";return TRUE;
}

/* Position identity, rather than vertex IDs, allows UV/color seams on the
 * shared diagonal. Consistent winding plus convexity excludes bow ties. */
static BOOL Quad(const BgDocumentRoom *room,const BgDocumentFace *a,const BgDocumentFace *b)
{
    double points[4][3],normal[3]={0};int ids[2][3],count=0,edges[4][2],n=0;
    for(int t=0;t<2;t++)for(int c=0;c<3;c++) {
        const BgDocumentVertex *v=&room->vertices[(t?b:a)->vertexindices[c]];
        int i;for(i=0;i<count;i++)if(points[i][0]==v->x&&points[i][1]==v->y&&points[i][2]==v->z)break;
        if(i==count){if(count==4)return FALSE;points[count][0]=v->x;points[count][1]=v->y;points[count++][2]=v->z;}
        ids[t][c]=i;
    }
    if(count!=4)return FALSE;
    for(int t=0;t<2;t++)for(int c=0;c<3;c++) {
        int from=ids[t][c],to=ids[t][(c+1)%3],shared=0;
        if(from==to)return FALSE;
        for(int d=0;d<3;d++)if(ids[1-t][d]==to&&ids[1-t][(d+1)%3]==from)shared=1;
        if(!shared){if(n==4)return FALSE;edges[n][0]=from;edges[n++][1]=to;}
    }
    if(n!=4)return FALSE;
    int order[4]={edges[0][0],edges[0][1],-1,-1};
    for(int i=1;i<4;i++) {
        int next=-1;for(int e=0;e<4;e++)if(edges[e][0]==order[i])next=edges[e][1];
        if(i==3){if(next!=order[0])return FALSE;}else{if(next<0)return FALSE;order[i+1]=next;}
    }
    for(int i=0;i<4;i++) {
        double u[3],v[3],cross[3],length=0,dot=0;
        for(int k=0;k<3;k++){u[k]=points[order[(i+1)%4]][k]-points[order[i]][k];v[k]=points[order[(i+2)%4]][k]-points[order[(i+1)%4]][k];}
        for(int k=0;k<3;k++){cross[k]=u[(k+1)%3]*v[(k+2)%3]-u[(k+2)%3]*v[(k+1)%3];length+=cross[k]*cross[k];}
        if(!(length>0))return FALSE;
        length=sqrt(length);
        for(int k=0;k<3;k++){cross[k]/=length;if(!i)normal[k]=cross[k];dot+=normal[k]*cross[k];}
        if(dot<.999999)return FALSE;
    }
    return TRUE;
}
static BOOL SameState(const BgRenderState *a, const BgRenderState *b)
{
    /* Do not compare padding between the byte alpha fields and DWORD words. */
#define SAME(field) (a->field == b->field)
    return SAME(othermode) && SAME(othermodehigh) && SAME(fogothermode)
        && SAME(othermodeknown) && SAME(othermodehighknown) && SAME(zbuffer)
        && SAME(geometryknown) && SAME(geometrymode) && SAME(environmentalpha)
        && SAME(primitivealpha) && SAME(environmentword1) && SAME(primitiveword0)
        && SAME(primitiveword1) && SAME(surfacepolicy) && SAME(surfacebasemode);
#undef SAME
}

BOOL DoorShadowCreate(BgDocument *bg,SetupFile *s,const BgFaceRef refs[2],DWORD *selection,const char **why)
{
    const BgDocumentRoom *room,*other;const BgDocumentFace *a=BgDocumentFindFace(bg,&refs[0],&room),*b=BgDocumentFindFace(bg,&refs[1],&other);
    BgRenderState states[2];unsigned char record[DOOR_SHADOW_BYTES]={0},*gdl=NULL;DWORD bytes;
    BgDocument copybg={0};SetupFile copysetup={0};BOOL ok=FALSE;DWORD deleted;
    *why="Select two adjacent, consistently wound triangles forming a planar convex quadrilateral in one room and layer.";
    if(!a||!b||a==b||room!=other||a->layer!=b->layer||!Quad(room,a,b))return FALSE;
    *why="Both triangles must use the same static material, culling, colors/opacity pipeline and render state.";
    if(!BgMaterialEqual(&a->material,&b->material)||a->cullbackfaces!=b->cullbackfaces
        ||!BgDocumentGetFaceRenderStates(bg,refs,2,states)||!SameState(states,states+1))return FALSE;
    BgMaterial check=a->material;check.environment=BG_ENV_SPHERICAL;
    if((a->textureid!=BG_TEX_NONE&&!BgMaterialEnvironmentImageSupported(&check))
        ||BG_ENV_GENERATED(a->material.environment)||(states[0].geometrymode&0xe0000u))return FALSE;
    for(DWORD g=0;g<=(a->drawgroup>b->drawgroup?a->drawgroup:b->drawgroup);g++) {
        const BgDocumentDrawGroup *group=&room->layers[a->layer].groups[g];
        for(DWORD pc=0;pc<group->commandsize;pc+=8) {
            unsigned op=group->commands[pc];
            if(op!=0xba&&op!=0xb9&&op!=0xb6&&op!=0xb7&&op!=0xe7&&op!=0xe8&&op!=0xfa&&op!=0xfb&&op!=0xfc&&op!=0xbb&&op!=0xc0)return FALSE;
        }
    }
    SetupMetaWrite32(record,(256u<<16)|0x2000u|PROPDEF_DOOR_SHADOW);
    SetupMetaWrite32(record+4,0xffffffffu);
    for(int k=0;k<3;k++)WriteFloat(record+DOOR_SHADOW_ORIGIN+k*4,room->origin[k]);
    SetupMetaWrite32(record+DOOR_SHADOW_ROOM,a->room);SetupMetaWrite32(record+DOOR_SHADOW_LAYER,a->layer);
    SetupMetaWrite32(record+DOOR_SHADOW_DOOR,0xffffffffu);
    SetupMetaWrite32(record+DOOR_SHADOW_LIGHT,0xffffffu);SetupMetaWrite32(record+DOOR_SHADOW_DARK,0x404040u);
    SetupMetaWrite32(record+DOOR_SHADOW_FORMAT,DOOR_SHADOW_VERSION);
    for(int t=0;t<2;t++)for(int c=0;c<3;c++) {
        const BgDocumentVertex *v=&room->vertices[(t?b:a)->vertexindices[c]];
        unsigned char *p=record+DOOR_SHADOW_VERTICES+(t*3+c)*16;
        Write16(p,v->x);Write16(p+2,v->y);Write16(p+4,v->z);Write16(p+6,v->flag);
        Write16(p+8,v->s);Write16(p+10,v->t);p[12]=v->r;p[13]=v->g;p[14]=v->b;p[15]=v->a;
    }
    DoorShadowPoint points[6];Points(record,points);
    DWORD direction=AxisValid(points,DOOR_SHADOW_POS_X)?DOOR_SHADOW_POS_X:DOOR_SHADOW_POS_Z;
    *why="The quadrilateral needs opposite edges aligned with X or Z.";
    if(!AxisValid(points,direction))return FALSE;
    SetupMetaWrite32(record+DOOR_SHADOW_DIRECTION,direction);
    if(!BgCompileDoorShadow(a,states,&gdl,&bytes,why))return FALSE;
    if(bytes>DOOR_SHADOW_GDL_CAPACITY){free(gdl);*why="The source material is too complex for a Door Shadow.";return FALSE;}
    memcpy(record+DOOR_SHADOW_GDL,gdl,bytes);free(gdl);SetupMetaWrite32(record+DOOR_SHADOW_GDL_SIZE,bytes);
    if(!BgDocumentClone(bg,&copybg,why)||!SetupFileClone(s,&copysetup,why))goto done;
    if(!SetupFileAddDoorShadow(&copysetup,record,selection,why)||!BgDocumentDeleteFaces(&copybg,refs,2,&deleted,why))goto done;
    BgDocumentFree(bg);*bg=copybg;ZeroMemory(&copybg,sizeof(copybg));
    SetupFileFree(s);*s=copysetup;ZeroMemory(&copysetup,sizeof(copysetup));ok=TRUE;
    DoorShadowSetPreview((DWORD)-1,0);
done:
    BgDocumentFree(&copybg);SetupFileFree(&copysetup);return ok;
}
