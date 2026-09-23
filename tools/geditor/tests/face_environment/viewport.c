/* Run production live-paint/drag preview functions with only window drawing
 * stubbed. Camera-dependent coordinates and triangle normals are real. */
#include "rotation.h"
#include "scaling.h"
typedef void *HWND;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef unsigned int GLuint;
typedef int GLsizei;
#define VIEWPORT_OBJECT_NONE 0xffffffffu
#define VIEWPORT_UNIFORM_SCALE_AXIS 3
#include "viewport_types.inc"
typedef struct { BgFaceRef face; unsigned int corner; } ViewportBgVertexHit;
typedef struct {
    Vertex *scene;
    VertexColor *scenecolors;
    BgFaceRef *scenefacerefs;
    SceneBatch *batches;
    unsigned char *dragmask, *selectedtris;
    int scenecount, batchcount, dragaxis;
    BOOL dragrotation, dragscaling, dragstan, dragpad;
    DWORD selectedobject;
    double dragdelta;
    Rotation scaleaxes;
} ViewportState;
static ViewportState *ViewportGetState(HWND h) { return h; }
static void ViewportRedraw(HWND h) {}
static void ViewportSetTriangleColor(ViewportState *s,int i,BOOL selected) {}
#include "viewport_functions.inc"

static void PreviewEdits(void)
{
    BgFile file=Fixture(); BgDocument doc={0}; const char *why="";
    assert(BgDocumentLoad(file.data,file.size,1,&doc,&why));
    BgDocumentRoom *room=&doc.rooms[1]; BgDocumentFace *face=&room->faces[0];
    face->material.environment=BG_ENV_SPHERICAL;
    /* An XY triangle faces +Z. */
    for(int c=0;c<3;c++) {
        BgDocumentVertex *v=&room->vertices[face->vertexindices[c]];
        v->x=c==1?10:0; v->y=c==2?10:0; v->z=0;
    }
    assert(BgDocumentEnvironmentNormal(room,face)==127);
    BgDocumentRenderMesh mesh={0}; assert(BgDocumentBuildRenderMesh(&doc,&mesh,&why));
    Vertex scene[3]={0}; VertexColor colors[3]={0}; unsigned char mask[3]={0,0,1};
    SceneBatch batch={.first=0,.count=3,.renderflags=mesh.renderflags[0]};
    ViewportState s={.scene=scene,.scenecolors=colors,.scenefacerefs=mesh.facerefs,
        .batches=&batch,.scenecount=3,.batchcount=1,.dragaxis=-1,.selectedobject=VIEWPORT_OBJECT_NONE};
    for(int c=0;c<3;c++) {
        scene[c].x=mesh.vertices[c].x; scene[c].y=mesh.vertices[c].y; scene[c].z=mesh.vertices[c].z;
        scene[c].environment=mesh.vertices[c].environment;
        scene[c].environment.scale[0]=scene[c].environment.scale[1]=1;
    }
    float right[3]={1,0,0},up[3]={0,1,0},uv[2];
    ViewportEnvironmentCoordinates(&s,0,batch.renderflags,right,up,uv);
    assert(fabs(uv[0]-.5)<1e-6 && fabs(uv[1]-.5)<1e-6);
    /* Paint must preserve the generated normal, regardless of RGB bytes. */
    BgDocumentVertex *painted=&room->vertices[face->vertexindices[0]];
    painted->r=255;painted->g=12;painted->b=34;
    ViewportBgVertexHit hit={.face=mesh.facerefs[0],.corner=0};
    ViewportRefreshBgVertexColor(&s,&doc,&hit);
    assert(colors[0].r==255&&colors[0].g==255&&colors[0].b==255);
    assert(scene[0].environment.normal[0]==0&&scene[0].environment.normal[2]==1);
    /* Moving only corner 2 tilts the entire plane toward -Y, including the
     * two unselected corners. Test both coordinate projections independently. */
    s.dragmask=mask;s.dragaxis=2;scene[2].z=10;
    for(int c=0;c<3;c++) {
        ViewportEnvironmentCoordinates(&s,c,batch.renderflags,right,up,uv);
        assert(fabs(uv[0]-.5)<1e-6 && fabs(uv[1]-(1-1/sqrt(2))*.5)<1e-6);
        ViewportEnvironmentCoordinates(&s,c,batch.renderflags|BG_RENDER_ENVIRONMENT_LINEAR,right,up,uv);
        assert(fabs(uv[0]-.5)<1e-6 && fabs(uv[1]-.75)<1e-6);
    }
    /* A camera turned toward +Z samples the endpoint of the same normal. */
    s.dragaxis=-1;right[0]=0;right[2]=1;
    ViewportEnvironmentCoordinates(&s,0,batch.renderflags,right,up,uv);assert(fabs(uv[0]-1)<1e-6);
    ViewportEnvironmentCoordinates(&s,0,batch.renderflags|BG_RENDER_ENVIRONMENT_LINEAR,right,up,uv);assert(fabs(uv[0])<1e-6);
    BgDocumentRenderMeshFree(&mesh);BgDocumentFree(&doc);BgFileFree(&file);
    puts("PASS environment preview: numeric projections, camera rotation, vertex paint and live corner deformation.");
}
