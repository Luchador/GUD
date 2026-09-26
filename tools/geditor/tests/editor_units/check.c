#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "editorunits.h"
#include "bgdocument.h"
#include "bgrender.h"
#include "stanload.h"
#include "setupload.h"
#include "edittool.h"

typedef float GLfloat;
typedef unsigned char GLubyte;
typedef void *HWND;
#define VIEWPORT_OBJECT_NONE ((DWORD)-1)
#include "types.inc"
typedef struct ViewportState {
    EditorTool tool;
    Vertex *scene;
    BgDocumentVertexRef *scenevertexrefs;
    ViewportComponent *components;
    ViewportStanComponent *stancomponents;
    int componentcount, scenecount, stancomponentcount, dragaxis;
    BOOL dragrotation, dragscaling, dragstan, dragextruding, extrudepreviewvalid;
    BOOL knifeactive, knifepreview, showobjects;
    struct { double position[3]; } knifeplane;
    double dragdelta, extrudeoffset[3];
    unsigned char *dragmask, *selectedtris;
    DWORD selectedobject, *sceneobjectindices;
    StanFile stan;
} ViewportState;
static const ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static BOOL ViewportRoomPosition(const ViewportState *s, double p[3]) { return FALSE; }
static int ViewportObjectCount(const ViewportState *s) { return s && s->selectedobject != VIEWPORT_OBJECT_NONE; }
static BOOL ViewportSelectedMarker(const ViewportState *s, SetupMarker *marker) { return FALSE; }
static BOOL ViewportPortalSelectionPosition(const ViewportState *s, double p[3], DWORD *count) { return FALSE; }
static BOOL ViewportPadSelectionPosition(const ViewportState *s, double p[3], BOOL preview) { return FALSE; }
static BOOL ViewportStanSelectionPosition(const ViewportState *s, BOOL gizmo, double p[3], DWORD *count) { return FALSE; }
static BOOL ViewportCornerVisible(const ViewportState *s, int corner) { return corner >= 0 && corner < s->scenecount; }
#include "logic.inc"

static void Close(double a, double b) { assert(fabs(a-b) < 1e-9); }

int main(void)
{
    const float scales[] = {
#include "scales.inc"
    };
    BgDocumentVertex vertices[3] = {{.x=614,.y=10,.z=556},{.x=615,.y=10,.z=556},{.x=614,.y=13,.z=556}};
    BgDocumentRoom rooms[2] = {[1]={.vertices=vertices,.vertexcount=3}};
    BgDocument bg = {.rooms=rooms,.roomcount=1};
    Vertex scene[3];
    BgDocumentVertexRef refs[3] = {{1,0},{1,1},{1,2}};
    ViewportComponent components[2] = {{{{1,0},{1,1}},{0,1}},{{{1,1},{1,2}},{1,2}}};
    unsigned char mask[3] = {1,1,1}, selected[1] = {1};
    ViewportState state = {.scene=scene,.scenevertexrefs=refs,.components=components,.componentcount=1,
        .scenecount=3,.dragaxis=-1,.selectedobject=VIEWPORT_OBJECT_NONE,.selectedtris=selected};
    for (unsigned s=0;s<sizeof(scales)/sizeof(*scales);s++)
    {
        bg.levelscale = scales[s];
        double factor = EditorUnitsFactor(EDITOR_UNITS_NATIVE, bg.levelscale), p[3]; DWORD count;
        Close(EditorUnitsFactor(EDITOR_UNITS_WORLD, bg.levelscale), 1);
        for (int i=0;i<3;i++)
        {
            float point[3]; BgDocumentGetWorldPosition(&bg,&rooms[1],&vertices[i],point);
            scene[i]=(Vertex){.x=point[0],.y=point[1],.z=point[2]};
        }
        BgDocumentVertex saved[3]; memcpy(saved,vertices,sizeof(saved));
        state.tool=EDITOR_TOOL_VERTEX_SELECT; state.componentcount=1;
        for (int toggle=0;toggle<20;toggle++)
        {
            assert(ViewportGetEditorSelectionPosition(&state,&bg,p,&count)&&count==1);
            const double expected[3]={614,10,556};
            for(int a=0;a<3;a++)Close(p[a]*factor,expected[a]);
            assert(ViewportGetSelectionPosition(&state,p,&count)); Close(p[0],scene[0].x);
        }
        assert(!memcmp(saved,vertices,sizeof(saved)) && !bg.dirty);
        state.componentcount=2;
        assert(ViewportGetEditorSelectionPosition(&state,&bg,p,&count)&&count==2); Close(p[0]*factor,614.5);
        state.tool=EDITOR_TOOL_EDGE_SELECT; state.componentcount=1;
        assert(ViewportGetEditorSelectionPosition(&state,&bg,p,&count)); Close(p[0]*factor,614.5);
        state.tool=EDITOR_TOOL_FACE_SELECT;
        assert(ViewportGetEditorSelectionPosition(&state,&bg,p,&count)); Close(p[0]*factor,614+1.0/3); Close(p[1]*factor,11);
        state.tool=EDITOR_TOOL_VERTEX_SELECT;
        rooms[1].origin[0]=.125f;
        assert(ViewportGetEditorSelectionPosition(&state,&bg,p,&count)); Close(p[0]*factor,614.125);
        rooms[1].origin[0]=0;
        state.dragaxis=0; state.dragmask=mask; state.dragdelta=EditorUnitsSnap(2.7/factor,factor);
        assert(ViewportGetEditorSelectionPosition(&state,&bg,p,&count)); Close(p[0]*factor,617);
        state.dragaxis=-1; state.dragmask=NULL; state.dragdelta=0;
        state.dragextruding=state.extrudepreviewvalid=TRUE; state.extrudeoffset[1]=2/factor;
        assert(ViewportGetEditorSelectionPosition(&state,&bg,p,&count)); Close(p[1]*factor,12);
        state.dragextruding=state.extrudepreviewvalid=FALSE;
        assert(ViewportGetEditorSelectionPosition(&state,&bg,p,&count));
        /* The same numeric-entry conversion as the frame, through real native
         * translation: only edited X moves, and setting it again is a no-op. */
        double offset[3]={(615-p[0]*factor)/factor,0,0}, applied[3]; DWORD moved; const char *why;
        assert(BgDocumentTranslateVertices(&bg,refs,1,offset,applied,&moved,&why)&&moved==1);
        assert(vertices[0].x==615 && vertices[0].y==10 && vertices[0].z==556);
        assert(ViewportGetEditorSelectionPosition(&state,&bg,p,&count)); offset[0]=(615-p[0]*factor)/factor;
        assert(BgDocumentTranslateVertices(&bg,refs,1,offset,applied,&moved,&why)&&moved==0);
        vertices[0].x=614;
        Close(EditorUnitsSnap(-1.6/factor,factor)*factor,-2);
        Close(EditorUnitsSnap(1.6,1),2);
    }
    unsigned char raw[24]={0};
    const short xyz[2][3]={{614,10,-556},{615,10,-555}};
    for(int p=0;p<2;p++)for(int a=0;a<3;a++)
    { raw[8+p*8+a*2]=(unsigned short)xyz[p][a]>>8; raw[9+p*8+a*2]=(unsigned short)xyz[p][a]; }
    StanTile tile={.pointcount=2}; ViewportStanComponent componentsStan[1]={{{{0,0},{0,1}}}};
    state.stan=(StanFile){.data=raw,.size=sizeof(raw),.tiles=&tile,.tilecount=1,.levelscale=.49886572f};
    state.stancomponents=componentsStan; state.stancomponentcount=1;
    double p[3]; state.tool=EDITOR_TOOL_VERTEX_SELECT;
    assert(ViewportEditorStanPosition(&state,p)); Close(p[0]*state.stan.levelscale,614); Close(p[2]*state.stan.levelscale,-556);
    state.tool=EDITOR_TOOL_EDGE_SELECT;
    assert(ViewportEditorStanPosition(&state,p)); Close(p[0]*state.stan.levelscale,614.5);
    Close(EditorUnitsFactor(EDITOR_UNITS_NATIVE,0),1); Close(EditorUnitsFactor(EDITOR_UNITS_NATIVE,NAN),1);
    printf("PASS: %zu native level scales, exact BG/STAN vertices, fractional origins/centers, toggles without mutation, typed moves/no-ops and drag/extrusion steps.\n",sizeof(scales)/sizeof(*scales));
    return 0;
}
