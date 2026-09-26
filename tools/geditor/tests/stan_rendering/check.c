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
    StanFile stan;
    Vertex *stanfill, *stanedges;
    GLsizei stanfillcount, stanedgecount;
    DWORD extrudecount;
    StanEdgeRef *stanextrudeedges;
    StanPoint *stanextrudepreview;
} ViewportState;
enum { GL_FALSE, GL_TRUE, GL_BLEND, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
    GL_CURRENT_BIT=8, GL_ENABLE_BIT=16, GL_COLOR_BUFFER_BIT=32, GL_DEPTH_BUFFER_BIT=64,
    GL_LINE_BIT=128, GL_POLYGON_BIT=256, GL_CLIENT_VERTEX_ARRAY_BIT=512,
    GL_COLOR_ARRAY, GL_TEXTURE_2D, GL_ALPHA_TEST, GL_CULL_FACE, GL_LEQUAL, GL_LESS,
    GL_POLYGON_OFFSET_FILL, GL_FRONT_AND_BACK, GL_FILL, GL_TRIANGLES, GL_LINES, GL_FLOAT, GL_UNSIGNED_BYTE };
typedef struct RenderState { BOOL blend, write, offset; int depth; double far; } RenderState;
static RenderState gl, stack, draws[8];
static int drawcount, modes[8], alpha;
static void glEnable(int flag) { if(flag==GL_BLEND)gl.blend=TRUE; if(flag==GL_POLYGON_OFFSET_FILL)gl.offset=TRUE; }
static void glDisable(int flag) { if(flag==GL_BLEND)gl.blend=FALSE; if(flag==GL_POLYGON_OFFSET_FILL)gl.offset=FALSE; }
static void glBlendFunc(int a,int b) { assert(a==GL_SRC_ALPHA && b==GL_ONE_MINUS_SRC_ALPHA); }
static void glDepthMask(int value) { gl.write=value; }
static void glDepthFunc(int value) { gl.depth=value; }
static void glDepthRange(double near,double far) { assert(near==0);gl.far=far; }
static void glPolygonOffset(float factor,float units) { assert(factor<0 && units<0); }
static void glPolygonMode(int a,int b) {}
static void glLineWidth(float value) {}
static void glVertexPointer(int a,int b,size_t stride,const void *p) { assert(stride==sizeof(Vertex) && p); }
static void glColorPointer(int a,int b,size_t stride,const void *p) { assert(stride==sizeof(Vertex) && p); }
static void glPushAttrib(int flags) { stack=gl; }
static void glPopAttrib(void) { gl=stack; }
static void glPushClientAttrib(int flags) {}
static void glPopClientAttrib(void) {}
static void glDisableClientState(int flag) {}
static void glColor4ub(int r,int g,int b,int a) { alpha=a; }
static void glVertex3f(float x,float y,float z) {}
static void glBegin(int mode) { assert(drawcount<8);modes[drawcount]=mode;draws[drawcount++]=gl; }
static void glEnd(void) {}
static void glDrawArrays(int mode,int first,int count) { assert(first==0 && count>0);glBegin(mode); }
#include "draw.inc"

int main(void)
{
    Vertex fill[6]={0},edges[8]={0};StanTile tile={0};StanEdgeRef ref={0};StanPoint preview[6]={0};
    ViewportState s={.showstan=TRUE,.stan={.tiles=&tile,.tilecount=1},.stanfill=fill,.stanedges=edges,
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
            BOOL opaque=opacity[i]==100,fillpass=modes[d]==GL_TRIANGLES;
            assert(draws[d].blend==!opaque && draws[d].write==(opaque && fillpass));
            assert(draws[d].depth==GL_LEQUAL);
            if(fillpass)assert(draws[d].offset);
            else if(opaque)assert(draws[d].far<1); /* outlines survive their own opaque fill */
        }
        if(extrude && opacity[i])assert(alpha==opacity[i]*255/100);
        assert(!gl.blend && gl.write && gl.depth==GL_LESS && gl.far==1 && !gl.offset);
    }
    puts("PASS: 0% hidden, partial stan blending, 100% opaque depth writes, outline bias, extrusion previews and restored GL state.");
    return 0;
}
