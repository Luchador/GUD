/* Real OpenGL regression: an opaque, slope-offset fill must not hide its
 * own perimeter. Draw through a surfaceless EGL compatibility context. */
#include <assert.h>
#include <string.h>
#define WINGDIAPI extern
#define APIENTRY
#include <GL/gl.h>
#include "stanload.h"
#include "bgrender.h"
#include "types.inc"
#include "state.inc"
static BOOL ViewportStanTileHidden(const ViewportState *state, DWORD tile) { return FALSE; }
#include "draw.inc"

void Render(float distance, float angle, int opacity, int fill, int wall,
            int extrusion, unsigned char *pixels)
{
    static const int corners[6]={0,1,2,0,2,3};
    StanTile tile={.pointcount=4};
    StanPoint preview[6]; StanEdgeRef edge={0};
    const float points[4][3]={{-1.2f,0,-1.2f},{1.2f,0,-1.2f},{1.2f,0,1.2f},{-1.2f,0,1.2f}};
    Vertex vertices[6]={0},edges[8]={0};
    for(int p=0;p<4;p++)
    {
        tile.points[p]=(StanPoint){points[p][0],points[p][1],points[p][2],0};
        for(int end=0;end<2;end++)
        {
            int i=(p+end)%4;
            edges[p*2+end]=(Vertex){.x=points[i][0],.y=points[i][1],.z=points[i][2],
                .r=255,.g=255,.b=255,.a=255};
        }
    }
    for(int p=0;p<6;p++)
    {
        int i=corners[p];
        vertices[p]=(Vertex){.x=points[i][0],.y=points[i][1],.z=points[i][2],.g=128,.a=255};
        preview[p]=tile.points[i];
    }
    ViewportState state={.showstan=TRUE,.stanopacity=opacity,.stan={.tiles=&tile,.tilecount=1,.levelscale=1},
        .stanfill=vertices,.stanfillcount=fill?6:0,.stanedges=edges,.stanedgecount=8,
        .extrudecount=1,.stanextrudeedges=&edge,.stanextrudepreview=preview};
    glViewport(0,0,256,256);
    glDepthMask(GL_TRUE); glDepthFunc(GL_LESS); glDepthRange(0,1); glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND); glDisable(GL_CULL_FACE); glDisable(GL_LINE_SMOOTH); glDisable(GL_DITHER);
    glDisable(GL_POLYGON_OFFSET_FILL); glDisable(GL_POLYGON_OFFSET_LINE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glClearColor(0,0,0,1); glClearDepth(1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glFrustum(-.57735,.57735,-.57735,.57735,1,10000);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    if(wall)
    {
        glColor3ub(0,0,128); glBegin(GL_QUADS);
        glVertex3f(-2,-2,-1.05f); glVertex3f(2,-2,-1.05f);
        glVertex3f(2,2,-1.05f); glVertex3f(-2,2,-1.05f); glEnd();
    }
    glTranslatef(0,0,-distance); glRotatef(angle,1,0,0);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    if(extrusion)
    {
        state.dragextruding=state.dragstan=state.extrudepreviewvalid=TRUE;
        ViewportDrawStanExtrusion(&state);
    }
    else { ViewportDrawStanOverlay(&state); }
    glDisableClientState(GL_VERTEX_ARRAY); glDisableClientState(GL_COLOR_ARRAY);
    glReadPixels(0,0,256,256,GL_RGB,GL_UNSIGNED_BYTE,pixels);
    assert(glGetError()==GL_NO_ERROR);
}
