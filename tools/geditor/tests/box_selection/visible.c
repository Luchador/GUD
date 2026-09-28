#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define WINGDIAPI extern
#define APIENTRY
#include <GL/gl.h>
#include "bgdocument.h"
#include "stanload.h"
#include "orbitcamera.h"
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
typedef struct { int left,top,right,bottom; } RECT;
#include "types.inc"
typedef struct ViewportState {
    int width,height,scenecount,batchcount;
    float posx,posy,posz,yaw,pitch;
    double selectionfar;
    BOOL orbit,cullbackfaces,showobjects,showbgprimary,showbgsecondary;
    OrbitCamera orbitcamera;
    Vertex *scene; SceneBatch *batches; ViewportTexture *texturecache;
    unsigned char *hiddentris;
    ViewportRenderMode rendermode;
    StanFile stan; DWORD *stanhiddenids,stanhiddencount;
    BgPortalFile portals;
} ViewportState;
static int tint;
static int ViewportGlassAlpha(const ViewportState *s,const SceneBatch *b) { return b->object ? tint : 0; }
static void ViewportUpdateEnvironmentMapping(ViewportState *s) {} /* Fixtures have authored UVs. */
#include "logic.inc"

static void Quad(Vertex *v,float z,float half)
{
    const float xy[6][2]={{-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1}};
    for(int i=0;i<6;i++) { v[i]=(Vertex){.x=xy[i][0]*half,.y=xy[i][1]*half,.z=z,.a=255,.s=(xy[i][0]+1)/2,.t=(xy[i][1]+1)/2}; }
}
static void Check(ViewportState *s,RECT box,ViewportBoxFaceKind kind,unsigned int expected,size_t n)
{
    unsigned char hits[32];assert(n<=sizeof(hits));memset(hits,1,n);int count=999;
    assert(ViewportFilterBoxFacesGL(s,&box,kind,hits,n,&count));
    unsigned int got=0;int wanted=0;for(size_t i=0;i<n;i++){if(hits[i])got|=1u<<i;if(expected&(1u<<i))wanted++;}
    if(got!=expected)fprintf(stderr,"Visibility: got %x, expected %x\n",got,expected);
    assert(got==expected && count==wanted && glGetError()==GL_NO_ERROR);
}
void CheckVisibleFaces(void)
{
    Vertex vertices[18]={0};SceneBatch batches[3]={0};unsigned char hidden[6]={0};
    Quad(vertices,-100,40);Quad(vertices+6,-200,80);Quad(vertices+12,-100,40);
    for(int i=0;i<3;i++) batches[i]=(SceneBatch){.first=i*6,.count=6,.cullbackfaces=TRUE,.monitor=-1,
        .renderflags=BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE};
    ViewportTexture cache[1]={0};
    ViewportState s={.width=200,.height=200,.scene=vertices,.scenecount=12,.batches=batches,.batchcount=2,
        .hiddentris=hidden,.showbgprimary=TRUE,.showbgsecondary=TRUE,.showobjects=TRUE,.cullbackfaces=TRUE,.texturecache=cache};
    RECT all={0,0,199,199},left={40,40,60,60};
    Check(&s,all,VIEWPORT_BOX_BG,3,4); /* Far faces are fully covered. */
    for(int i=0;i<6;i++) vertices[i].x-=30;
    Check(&s,all,VIEWPORT_BOX_BG,15,4); /* Exposed portions of both planes count. */
    Check(&s,left,VIEWPORT_BOX_BG,2,4); /* Far faces visible elsewhere, but not in this box. */
    Quad(vertices,-100,40);
    hidden[0]=hidden[1]=1;Check(&s,all,VIEWPORT_BOX_BG,12,4);hidden[0]=hidden[1]=0;
    for(int i=0;i<6;i+=3){Vertex v=vertices[i];vertices[i]=vertices[i+2];vertices[i+2]=v;}
    Check(&s,all,VIEWPORT_BOX_BG,12,4);s.cullbackfaces=FALSE;Check(&s,all,VIEWPORT_BOX_BG,3,4);s.cullbackfaces=TRUE;
    Quad(vertices,-100,40);
    batches[0].secondary=TRUE;s.showbgsecondary=FALSE;Check(&s,all,VIEWPORT_BOX_BG,12,4);
    batches[0].secondary=FALSE;s.showbgsecondary=TRUE;
    batches[0].object=TRUE;Check(&s,all,VIEWPORT_BOX_BG,0,4);s.showobjects=FALSE;Check(&s,all,VIEWPORT_BOX_BG,12,4);
    s.showobjects=TRUE;batches[0].object=FALSE;
    GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    unsigned char rgba[16]={17,83,251,0, 211,39,61,255, 64,243,9,0, 244,11,129,255};
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);batches[0].gltex=texture;
    batches[0].renderflags|=BG_RENDER_ALPHA_TEST;Check(&s,all,VIEWPORT_BOX_BG,15,4);
    for(int i=3;i<16;i+=4)rgba[i]=0;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    Check(&s,all,VIEWPORT_BOX_BG,12,4);
    s.rendermode=VIEWPORT_RENDER_UNTEXTURED;Check(&s,all,VIEWPORT_BOX_BG,3,4);
    s.rendermode=VIEWPORT_RENDER_WIREFRAME;Check(&s,all,VIEWPORT_BOX_BG,3,4);s.rendermode=VIEWPORT_RENDER_NORMAL;
    batches[0].object=TRUE;tint=255;Check(&s,all,VIEWPORT_BOX_BG,0,4);tint=0;batches[0].object=FALSE;
    for(int i=3;i<16;i+=4)rgba[i]=255;
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);Check(&s,all,VIEWPORT_BOX_BG,3,4);
    s.batchcount=3;s.scenecount=18;batches[2].renderflags=BG_RENDER_DEPTH_TEST|BG_RENDER_DECAL;batches[2].secondary=TRUE;
    Check(&s,all,VIEWPORT_BOX_BG,48,6); /* Last decal wins without replacing supporting depth. */
    batches[2].secondary=FALSE;s.batchcount=2;s.scenecount=12;batches[0].gltex=0;
    batches[0].renderflags=BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE;
    for(int i=0;i<6;i++) vertices[i].z=-1;
    Check(&s,all,VIEWPORT_BOX_BG,12,4); /* Clipped near plane must not occlude. */
    Quad(vertices,-100,40);
    glViewport(7,9,90,80);glEnable(GL_SCISSOR_TEST);glScissor(11,12,13,14);
    glEnable(GL_DITHER);glEnable(GL_BLEND);glPixelStorei(GL_PACK_ROW_LENGTH,7);glPixelStorei(GL_PACK_SKIP_ROWS,3);
    glMatrixMode(GL_TEXTURE);Check(&s,all,VIEWPORT_BOX_BG,3,4);
    GLint n[4];glGetIntegerv(GL_VIEWPORT,n);assert(n[0]==7 && n[1]==9 && n[2]==90 && n[3]==80);
    glGetIntegerv(GL_SCISSOR_BOX,n);assert(n[0]==11 && n[1]==12 && n[2]==13 && n[3]==14);
    glGetIntegerv(GL_PACK_ROW_LENGTH,n);assert(n[0]==7);glGetIntegerv(GL_PACK_SKIP_ROWS,n);assert(n[0]==3);
    glGetIntegerv(GL_MATRIX_MODE,n);assert(n[0]==GL_TEXTURE && glIsEnabled(GL_DITHER) && glIsEnabled(GL_BLEND));
    glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);glDisable(GL_SCISSOR_TEST);
    puts("PASS: real GL visible face IDs, local partial visibility, backfaces, hidden/layer/object occluders, cutout alpha, glass tint, wireframe/untextured, decals, near clipping and state restoration.");
    /* A colored texture must not corrupt any byte of a larger face ID. */
    const size_t capacity=0x010104;Vertex *many=calloc(capacity*3,sizeof(*many));unsigned char *hits=calloc(capacity,1);assert(many&&hits);
    s.scene=many;s.hiddentris=NULL;s.batchcount=1;s.scenecount=capacity*3;
    batches[0].first=(capacity-2)*3;batches[0].gltex=texture;Quad(many+batches[0].first,-100,40);
    hits[capacity-2]=hits[capacity-1]=1;int count=0;
    assert(ViewportFilterBoxFacesGL(&s,&all,VIEWPORT_BOX_BG,hits,capacity,&count) && count==2 && hits[capacity-2] && hits[capacity-1]);
    free(many);free(hits);glDeleteTextures(1,&texture);s.scene=vertices;s.hiddentris=hidden;s.scenecount=12;s.batchcount=2;
    batches[0].first=0;batches[0].gltex=0;
    StanTile tiles[2]={0};s.stan=(StanFile){.tiles=tiles,.tilecount=2};
    BgPortal portals[3]={0};s.portals=(BgPortalFile){.portals=portals,.portalcount=3};
    for(int t=0;t<2;t++) {
        tiles[t].pointcount=4;tiles[t].editorid=t+1;
        for(int p=0;p<4;p++) tiles[t].points[p]=(StanPoint){.x=p==1||p==2?40:-40,.y=p>=2?40:-40,.z=t?-150:-100};
    }
    Check(&s,all,VIEWPORT_BOX_STAN,1,2); /* Coplanar floor tile wins, rear tile is hidden. */
    for(int t=0;t<3;t++) {
        portals[t].pointcount=4;portals[t].geometryoffset=t<2?5:8;
        for(int p=0;p<4;p++) portals[t].points[p]=(BgPortalPoint){p==1||p==2?40:-40,p>=2?40:-40,t==2?-150:-90};
    }
    Check(&s,all,VIEWPORT_BOX_PORTAL,3,3); /* Visible aliases selected, occluded portal ignored. */
    puts("PASS: 24-bit IDs, stan occlusion with coplanar floor bias, portal visibility and aliased room links.");
}
