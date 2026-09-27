#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "stanload.h"
#include "bgrender.h"
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef int GLsizei;
#include "types.inc"
typedef struct ViewportState {
    BOOL showstan, dragextruding, dragstan, extrudepreviewvalid;
    int stanopacity;
    unsigned statisticsfont;
    StanFile stan;
    Vertex *stanfill, *stanedges;
    GLsizei stanfillcount, stanedgecount;
    StanDiscontinuity *standiscontinuities;
    DWORD standiscontinuitycount;
    DWORD extrudecount;
    StanEdgeRef *stanextrudeedges;
    StanPoint *stanextrudepreview;
} ViewportState;
enum { GL_FALSE, GL_TRUE, GL_BLEND, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
    GL_CURRENT_BIT=8, GL_ENABLE_BIT=16, GL_COLOR_BUFFER_BIT=32, GL_DEPTH_BUFFER_BIT=64,
    GL_LINE_BIT=128, GL_POLYGON_BIT=256, GL_CLIENT_VERTEX_ARRAY_BIT=512,
    GL_COLOR_ARRAY, GL_TEXTURE_2D, GL_ALPHA_TEST, GL_CULL_FACE, GL_LEQUAL, GL_LESS,
    GL_POLYGON_OFFSET_FILL, GL_FRONT_AND_BACK, GL_FILL, GL_TRIANGLES, GL_LINES, GL_FLOAT, GL_UNSIGNED_BYTE,
    GL_LIST_BIT=1024, GL_LIGHTING, GL_FOG, GL_DEPTH_TEST, GL_POLYGON_OFFSET_LINE, GL_LINE, GL_POLYGON };
typedef struct RenderState { BOOL blend, write, offset, offsetline, line, edgeflag; int depth; double far; float factor, units; } RenderState;
static RenderState gl, stack, draws[8];
static int drawcount, modes[8], alpha;
static void glEnable(int flag) { if(flag==GL_BLEND)gl.blend=TRUE; if(flag==GL_POLYGON_OFFSET_FILL)gl.offset=TRUE; if(flag==GL_POLYGON_OFFSET_LINE)gl.offsetline=TRUE; }
static void glDisable(int flag) { if(flag==GL_BLEND)gl.blend=FALSE; if(flag==GL_POLYGON_OFFSET_FILL)gl.offset=FALSE; if(flag==GL_POLYGON_OFFSET_LINE)gl.offsetline=FALSE; }
static void glBlendFunc(int a,int b) { assert(a==GL_SRC_ALPHA && b==GL_ONE_MINUS_SRC_ALPHA); }
static void glDepthMask(int value) { gl.write=value; }
static void glDepthFunc(int value) { gl.depth=value; }
static void glDepthRange(double near,double far) { assert(near==0);gl.far=far; }
static void glPolygonOffset(float factor,float units) { assert(factor<0 && units<0);gl.factor=factor;gl.units=units; }
static void glPolygonMode(int a,int b) { assert(a==GL_FRONT_AND_BACK);gl.line=b==GL_LINE; }
static void glLineWidth(float value) {}
static void glVertexPointer(int a,int b,size_t stride,const void *p) { assert(stride==(gl.line?2:1)*sizeof(Vertex) && p); }
static void glColorPointer(int a,int b,size_t stride,const void *p) { assert(stride==(gl.line?2:1)*sizeof(Vertex) && p); }
static void glPushAttrib(int flags) { stack=gl; }
static void glPopAttrib(void) { gl=stack; }
static void glPushClientAttrib(int flags) {}
static void glPopClientAttrib(void) {}
static void glDisableClientState(int flag) {}
static int color[3],vertices,boundaries;
static void glColor4ub(int r,int g,int b,int a) { alpha=a;color[0]=r;color[1]=g;color[2]=b; }
static void glEdgeFlag(int value) { gl.edgeflag=value; }
static void glVertex3f(float x,float y,float z) { vertices++;boundaries+=gl.edgeflag; }
static void glBegin(int mode) { assert(drawcount<8);modes[drawcount]=mode;draws[drawcount++]=gl; }
static void glEnd(void) {}
static void glDrawArrays(int mode,int first,int count) { assert(first==0 && count>0);glBegin(mode); }
static int labels;
static char letters[8];
static float position[3], centers[8][3];
static BOOL hidden;
static BOOL ViewportStanTileHidden(const ViewportState *s,DWORD tile) { return hidden; }
static void glListBase(unsigned font) { assert(font); }
static void glRasterPos3fv(const float *p) { memcpy(position,p,sizeof(position)); }
static void glBitmap(int w,int h,float x,float y,float dx,float dy,const void *data)
{ assert(!w && !h && !data && dx<0 && dy<0); }
static void glCallLists(int n,int type,const void *text)
{
    assert(n==1 && type==GL_UNSIGNED_BYTE && labels<8);
    assert(!gl.blend && !gl.write && gl.depth==GL_LEQUAL && gl.far<1);
    letters[labels]=*(const char *)text;memcpy(centers[labels++],position,sizeof(position));
}
#include "draw.inc"

int main(void)
{
    Vertex fill[6]={0},edges[8]={0};StanTile tile={.pointcount=4};StanEdgeRef ref={0};StanPoint preview[6]={0};
    ViewportState s={.showstan=TRUE,.stan={.tiles=&tile,.tilecount=1,.levelscale=.25f},.stanfill=fill,.stanedges=edges,
        .stanfillcount=6,.stanedgecount=8,.extrudecount=1,.stanextrudeedges=&ref,.stanextrudepreview=preview};
    const int opacity[]={0,44,99,100};
    for(int extrude=0;extrude<2;extrude++)for(int i=0;i<4;i++)
    {
        s.stanopacity=opacity[i];s.dragextruding=s.dragstan=s.extrudepreviewvalid=extrude;
        gl=(RenderState){.write=TRUE,.depth=GL_LESS,.far=1};drawcount=0;
        ViewportDrawStanOverlay(&s);
        assert(drawcount==(opacity[i] ? extrude?4:2 : 0));
        for(int d=0;d<drawcount;d++)
        {
            BOOL opaque=opacity[i]==100,fillpass=!draws[d].line;
            assert(draws[d].blend==!opaque && draws[d].write==(opaque && fillpass));
            assert(draws[d].depth==GL_LEQUAL);
            if(fillpass)assert(draws[d].offset);
            else
            {
                assert(modes[d]==GL_POLYGON || modes[d]==GL_TRIANGLES); /* GL_LINES ignores polygon offset. */
                assert(draws[d].offsetline && draws[d].factor<draws[d-1].factor && draws[d].units<draws[d-1].units);
            }
        }
        if(extrude && opacity[i])assert(alpha==opacity[i]*255/100);
        assert(!gl.blend && gl.write && gl.depth==GL_LESS && gl.far==1 && !gl.offset && !gl.offsetline && !gl.line);
    }
    /* Vertical quad: screen-facing labels use all four corners, not XZ alone. */
    tile.pointcount=4;
    tile.points[0]=(StanPoint){10,0,20,0};tile.points[1]=(StanPoint){10,12,20,0};
    tile.points[2]=(StanPoint){10,12,40,0};tile.points[3]=(StanPoint){10,0,40,0};
    s.statisticsfont=1;
    const float scales[]={.25f,.5f,1.0f,2.0f};
    for(unsigned scale=0;scale<sizeof(scales)/sizeof(*scales);scale++)
    for(int type=0;type<6;type++)for(int i=0;i<4;i++)for(int hide=0;hide<2;hide++)
    {
        tile.special=type;s.stanopacity=opacity[i];hidden=hide;labels=0;s.stan.levelscale=scales[scale];
        gl=(RenderState){.write=TRUE,.depth=GL_LESS,.far=1};
        ViewportDrawStanTypeLabels(&s);
        int expected=!hide && opacity[i] && (type==STAN_TYPE_LADDER || type==STAN_TYPE_FORCED_CROUCH) ? 2:0;
        assert(labels==expected);
        for(int j=0;j<labels;j++)
        {
            assert(letters[j]==(type==STAN_TYPE_LADDER?'L':'C'));
            assert(centers[j][0]==10 && centers[j][1]==6+2/scales[scale] && centers[j][2]==30);
        }
        assert(!gl.blend && gl.write && gl.depth==GL_LESS && gl.far==1);
    }
    hidden=FALSE;tile.special=STAN_TYPE_LADDER;labels=0;s.showstan=FALSE;
    ViewportDrawStanTypeLabels(&s);assert(!labels);
    s.showstan=TRUE;s.statisticsfont=0;ViewportDrawStanTypeLabels(&s);assert(!labels);
    StanDiscontinuity gap={.tiles={0,0}};
    s.standiscontinuities=&gap;s.standiscontinuitycount=1;
    for(int hide=0;hide<2;hide++)for(int show=0;show<2;show++)for(int i=0;i<4;i++)
    {
        hidden=hide;s.showstan=show;s.stanopacity=opacity[i];vertices=boundaries=drawcount=0;
        gl=(RenderState){.write=TRUE,.depth=GL_LESS,.far=1,.edgeflag=TRUE};
        ViewportDrawStanDiscontinuities(&s);
        assert(vertices==(!hide && show && opacity[i]?6:0) && boundaries==vertices/3);
        if(vertices)
        {
            assert(drawcount==1 && modes[0]==GL_TRIANGLES && draws[0].line && draws[0].offsetline);
            assert(!draws[0].blend && !draws[0].write && draws[0].depth==GL_LEQUAL && draws[0].factor<-2);
            assert(color[0]==255 && !color[1] && !color[2] && alpha==255);
        }
        assert(gl.edgeflag && !gl.blend && gl.write && gl.depth==GL_LESS && gl.far==1 && !gl.line && !gl.offsetline);
    }
    puts("PASS: stan opacity/depth, slope-biased perimeters and extrusion previews, L/C lift in native units across level scales, hidden/normal/unknown suppression and restored GL state.");
    return 0;
}
