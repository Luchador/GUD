#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef void *HWND;
typedef intptr_t LPARAM, LRESULT;
typedef uintptr_t WPARAM;
typedef unsigned int UINT;
typedef struct { int x,y; } POINT;
#define WM_APP 0x8000
#define VK_CONTROL 17
#define MB_ICONERROR 1
#define MB_ICONINFORMATION 2
#include "viewport.h"
#include "cameraframe.h"
#include "editorunits.h"
#define VIEWPORT_BOX_VERTICES 24
#define VIEWPORT_OBJECT_NONE 0xffffffffu
#define GL_TRIANGLES 4
/* Unused native rendering fields retain the production viewport structure. */
typedef int HDC,HGLRC,HCURSOR,GLuint,GLsizei,FogCurve,ViewportTexture,LARGE_INTEGER,FogCoordPointerFn;
typedef int OrbitCamera,ModelLighting,VertexColor,ViewportAimGuide,ViewportMonitors;
typedef int ViewportRoomDragPoint;
typedef float GLfloat;
typedef unsigned char GLubyte;
typedef struct { double previewposition[3]; } ViewportPad;
#include "types.inc"
#include "asset.inc"

static HWND capture;
static double parameter, eye[3]={100,150,200};
static Vertex original[9];
static int commits, copies, facecopies, moves;
static int rotations;
static BgPortal portaloriginal[3];
static ViewportRotation committedrotation;
static ViewportTranslation facecopy;
static Scaling committed;
static ViewportObjectDuplicate copied;
static int groups, drawn[4], colors[4][3], color[3];
static BgEnvironmentVertex environment;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static int GetKeyState(int key) { return 0; }
static HWND GetParent(HWND hwnd) { return hwnd; }
static HWND GetCapture(void) { return capture; }
static void SetCapture(HWND hwnd) { capture=hwnd; }
static void ReleaseCapture(void) { capture=NULL; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
static void MessageBox(HWND hwnd,const char *text,const char *title,int flags) { abort(); }
static LRESULT SendMessage(HWND hwnd,int message,int wparam,LPARAM lparam)
{
    ViewportState *s=hwnd;
    if (message==VIEWPORT_WM_TRANSFORM_PREVIEW) return TRUE;
    /* Preview must be restored before handing one transaction to the frame. */
    assert(!capture && s->dragaxis==-1 && !s->dragvertices && !s->dragmask);
    assert(!memcmp(s->scene,original,sizeof(original)));
    if(s->dragportal) { assert(!memcmp(s->portals.portals,portaloriginal,sizeof(portaloriginal))); }
    if (message==VIEWPORT_WM_SCALE_SELECTION) { committed=*(Scaling *)lparam;commits++; }
    else if (message==VIEWPORT_WM_DUPLICATE_OBJECT) { copied=*(ViewportObjectDuplicate *)lparam;copies++; }
    else if (message==VIEWPORT_WM_DUPLICATE_BG_FACES) { assert(!s->dragfaceduplicating); facecopy=*(ViewportTranslation *)lparam;facecopies++; }
    else if (message==VIEWPORT_WM_TRANSLATE_SELECTION) { moves++; }
    else if (message==VIEWPORT_WM_ROTATE_SELECTION) { committedrotation=*(ViewportRotation *)lparam;rotations++; }
    else abort();
    return TRUE;
}
static void glColor3ub(int r,int g,int b) { color[0]=r;color[1]=g;color[2]=b; }
static void glBegin(int mode) { assert(mode==GL_TRIANGLES && groups<4);memcpy(colors[groups],color,sizeof(color)); }
static void glVertex3f(float x,float y,float z) { assert(isfinite(x)&&isfinite(y)&&isfinite(z));drawn[groups]++; }
static void glEnd(void) { groups++; }
BOOL SetupFileCanDuplicateObject(const SetupFile *s,DWORD i) { return TRUE; }
BOOL SetupFileGetModelPad(const SetupFile *s,DWORD i,SetupPadRef *ref)
{ ref->index=i; ref->bound=FALSE; return TRUE; }
static BOOL groupDoorFrames;
BOOL SetupFilePadRotation(const SetupFile *s,const SetupPadRef *ref,Rotation *out)
{
    RotationAxis(out,1,ref->index==8 ? 37 : 0);
    if (groupDoorFrames)
    {
        Rotation door, yaw;
        assert(RotationBasis(&door,(double[3]){1,0,0},(double[3]){0,1,0}));
        RotationAxis(&yaw,1,ref->index==8 ? 90 : 0);
        RotationMultiply(out,&yaw,&door);
    }
    return TRUE;
}
static BOOL ViewportSelectedMarker(const ViewportState *s,SetupMarker *m) { return FALSE; }
static int ViewportSelectedPadIndex(const ViewportState *s) { return -1; }
static BOOL ViewportCornerVisible(const ViewportState *s,int corner)
{ return corner>=0 && corner<s->scenecount && (!s->hiddentris || !s->hiddentris[corner/3]); }
static BOOL ViewportBuildPickRay(HWND h,const ViewportState *s,int x,int y,ViewportPickRay *r)
{
    /* Orthographic ray fixture; camera orientation varies independently of the
     * screen motion. Production triangle intersection and handle picking run. */
    double length=sqrt(eye[0]*eye[0]+eye[1]*eye[1]+eye[2]*eye[2]);
    double horizontal=hypot(eye[0],eye[2]);
    double right[3]={eye[2]/horizontal,0,-eye[0]/horizontal};
    double up[3]={-eye[0]*eye[1]/(length*horizontal),horizontal/length,-eye[2]*eye[1]/(length*horizontal)};
    for(int k=0;k<3;k++) {
        r->origin[k]=s->gizmoposition[k]+eye[k]+(x-100)*right[k]-(y-100)*up[k];
        r->direction[k]=-eye[k]/length;
    }
    r->mindistance=.1;r->maxdistance=10000;return TRUE;
}
static double ViewportRotationParameter(const ViewportState *s,const ViewportPickRay *r,int x,int y) { return parameter; }
static double ViewportDragParameter(const ViewportState *s,const ViewportPickRay *r,int y) { return parameter; }
static double ViewportGizmoScale(const ViewportState *s) { return s->gizmovisible && s->scalevalid ? 90 : 0; }
static BOOL ViewportPreviewMarker(HWND h,ViewportState *s,double d,const Rotation *r) { abort(); }
static BOOL ViewportPadPosition(const ViewportState *s,const SetupPadRef *p,BOOL preview,double out[3]) { abort(); }
static void ViewportRefreshPortalGeometry(ViewportState *s) {}
static void ViewportRefreshStanOverlay(ViewportState *s) { abort(); }
static void ViewportBuildObjectSelectionBox(ViewportState *s);
static void ViewportUpdateGizmo(ViewportState *s) { s->hoveraxis=-1; }
static void ViewportRefreshKnifePlane(HWND h,ViewportState *s) { abort(); }
static void ViewportFinishKnifeTransform(HWND h,ViewportState *s,BOOL cancel) { abort(); }
static void ViewportCancelBoxSelection(HWND h,ViewportState *s) {}
static void ViewportSetSetupMarkers(HWND h,ViewportState *s,const SetupFile *f,float scale) { abort(); }
DWORD ViewportGetStanSelectionCount(HWND h,DWORD *t) { return 0; }
StanPointRef *ViewportGetMoveStanPoints(HWND h,DWORD *n) { abort(); }
static StanPointRef ViewportStanPointRef(const ViewportState *s,DWORD t,DWORD p) { abort(); }
static int ViewportCompareStanRefs(const void *a,const void *b) { abort(); }
static BOOL ViewportProject(const ViewportState *s,const Vertex *v,double out[2]) { return FALSE; }
static BOOL ViewportPrepareEdgeExtrusion(ViewportState *s) { abort(); }
static BOOL ViewportBeginRoomDrag(ViewportState *s) { abort(); }
static void ViewportPreviewRoomDrag(HWND h,ViewportState *s,double delta) { abort(); }
static void ViewportCancelRoomDrag(HWND h,ViewportState *s) { abort(); }
void BgRenderEnvironmentCoordinates(const BgEnvironmentVertex *v,BgRenderFlags f,const float r[3],const float u[3],float uv[2]) { environment=*v; }
static Vertex ViewportStanPointVertex(const StanPoint *p) { return (Vertex){.x=p->x,.y=p->y,.z=p->z}; }
#include "functions.inc"

static void Near(double a,double b) { assert(fabs(a-b)<.0001); }
static void Factors(const Scaling *s,double factor)
{
    assert(ScalingValid(s));
    for(int k=0;k<3;k++) Near(s->factor[k],factor);
}
static void Render(const ViewportState *s)
{
    groups=0;memset(drawn,0,sizeof(drawn));ViewportDrawGizmoHandles(s,90);
}
static void Handles(ViewportState *s)
{
    Render(s);assert(groups==4 && drawn[3]==36);
    for(int k=0;k<3;k++) assert(colors[3][k]==255);
    s->hoveraxis=3;Render(s);
    assert(colors[3][0]==255 && colors[3][1]==205 && colors[3][2]==0);
    s->hoveraxis=-1;s->dragaxis=3;Render(s);
    assert(colors[3][0]==255 && colors[3][1]==205 && colors[3][2]==0);s->dragaxis=-1;
    for(int view=0;view<4;view++) {
        eye[0]=view&1 ? -100 : 100;eye[1]=view&2 ? -150 : 150;
        assert(ViewportPickGizmo(s,s,100,100)==3);
        assert(ViewportPickGizmo(s,s,400,400)==-1);
    }
    eye[0]=100;eye[1]=150;
    /* Every original axis handle remains pickable with the center present. */
    unsigned hits=0;
    for(int y=0;y<200;y+=2)for(int x=0;x<200;x+=2) {
        int picked=ViewportPickGizmo(s,s,x,y);if(picked>=0)hits|=1u<<picked;
    }
    assert(hits==15);
    s->scalemode=FALSE;Render(s);assert(groups==3);
    assert(ViewportPickGizmo(s,s,100,100)!=3);
    s->rotationmode=TRUE;s->rotationaxes=7;Render(s);assert(groups==3);
    assert(ViewportPickGizmo(s,s,100,100)!=3);
    s->rotationmode=FALSE;s->scalemode=TRUE;
    s->flying=TRUE;assert(ViewportPickGizmo(s,s,100,100)==-1);s->flying=FALSE;
    s->gizmovisible=FALSE;assert(ViewportPickGizmo(s,s,100,100)==-1);s->gizmovisible=TRUE;
    puts("PASS: shipped axis meshes plus center cube, white/gold drawing, picking from four views and mode gating.");
}
static void Preview(ViewportState *s,double factor)
{
    Scaling live;assert(ViewportGetScaling(s,&live));Factors(&live,factor);
    for(int i=0;i<s->scenecount;i++) {
        double before[3]={original[i].x,original[i].y,original[i].z};
        double now[3]={s->scene[i].x,s->scene[i].y,s->scene[i].z};
        for(int k=0;k<3;k++) Near(now[k],s->dragmask[i] ? s->dragorigin[k]+(before[k]-s->dragorigin[k])*factor : before[k]);
    }
    double guide[3]={11,22,33};ViewportPreviewGuidePoint(s,guide);
    for(int k=0;k<3;k++) Near(guide[k],s->dragorigin[k]+((k+1)*11-s->dragorigin[k])*factor);
    if(s->selectedobject!=VIEWPORT_OBJECT_NONE) {
        float right[3]={1,0,0},up[3]={0,1,0},uv[2];
        ViewportEnvironmentCoordinates(s,0,0,right,up,uv);
        for(int k=0;k<3;k++) Near(environment.normal[k],original[0].environment.normal[k]/factor);
        ViewportEnvironmentCoordinates(s,3,0,right,up,uv);
        for(int k=0;k<3;k++) Near(environment.normal[k],original[3].environment.normal[k]);
    }
}
static void Begin(ViewportState *s,BOOL shift)
{
    assert(ViewportBeginTransform(s,s,100,100,shift));
    assert(s->dragaxis==3 && s->dragscaling && !s->dragextruding && capture==s);
    assert(s->dragduplicating==(shift && s->selectedobject!=VIEWPORT_OBJECT_NONE));
    for(int i=0;i<9;i++) {
        BOOL selected=s->selectedobject!=VIEWPORT_OBJECT_NONE ? i<3
            : (i<(s->tool==EDITOR_TOOL_EDGE_SELECT ? 2 : 3) || i==6);
        assert(s->dragmask[i]==selected);
    }
}
static void Drags(ViewportState *s)
{
    const int moves[][2]={{190,100},{100,10},{55,100},{100,145},{145,55},{145,145},{-900,100},{100,100}};
    const double factors[]={2,2,.5,.5,2,1,.01,1};
    int before=commits;
    Begin(s,FALSE);
    for(int i=0;i<8;i++){ViewportDragTransform(s,s,moves[i][0],moves[i][1]);Preview(s,factors[i]);}
    ViewportEndTransform(s,s);assert(commits==before && !capture);
    assert(!memcmp(s->scene,original,sizeof(original)));
    Begin(s,FALSE);ViewportDragTransform(s,s,190,100);Preview(s,2);
    ViewportCancelTransform(s);assert(commits==before && !capture);
    assert(!memcmp(s->scene,original,sizeof(original)));
    Begin(s,FALSE);ViewportDragTransform(s,s,190,100);ViewportEndTransform(s,s);
    assert(commits==before+1);Factors(&committed,2);
    assert(!memcmp(committed.pivot,s->gizmoposition,sizeof(committed.pivot)));
    /* Shift only duplicates an object; BG edge scaling must not extrude. */
    int copycount=copies;
    Begin(s,TRUE);ViewportDragTransform(s,s,100,145);ViewportEndTransform(s,s);
    if(s->selectedobject!=VIEWPORT_OBJECT_NONE) {
        assert(copies==copycount+1 && copied.mode==TRANSFORM_SCALE && copied.source==s->selectedobject);
        Factors(&copied.scaling,.5);
        assert(!memcmp(copied.scaling.pivot,s->gizmoposition,sizeof(copied.scaling.pivot)));
        for(int k=0;k<3;k++)assert(copied.translation.offset[k]==0);
    } else {assert(copies==copycount && commits==before+2);Factors(&committed,.5);}
    Begin(s,TRUE);ViewportEndTransform(s,s);assert(copies==copycount+(s->selectedobject!=VIEWPORT_OBJECT_NONE));
}
static void AxisDrag(ViewportState *s)
{
    for(int y=0;y<200;y++)for(int x=0;x<200;x++)if(ViewportPickGizmo(s,s,x,y)==0) {
        parameter=0;assert(ViewportBeginTransform(s,s,x,y,FALSE));
        parameter=90;ViewportDragTransform(s,s,x,y);
        Scaling scale;assert(ViewportGetScaling(s,&scale));
        Near(scale.factor[0],2);Near(scale.factor[1],1);Near(scale.factor[2],1);
        ViewportEndTransform(s,s);Near(committed.factor[0],2);Near(committed.factor[1],1);Near(committed.factor[2],1);
        return;
    }
    abort();
}
static void FaceDrags(ViewportState *s)
{
    s->tool=EDITOR_TOOL_FACE_SELECT; s->selectedobject=VIEWPORT_OBJECT_NONE;
    s->scalemode=FALSE; s->componentcount=0;
    unsigned char hidden[3]={0}; s->hiddentris=hidden;
    for (int axis=0;axis<3;axis++)
    {
        int px=-1,py=-1;
        for(int y=0;y<200 && px<0;y+=2)for(int x=0;x<200;x+=2)
            if(ViewportPickGizmo(s,s,x,y)==axis) { px=x;py=y;break; }
        assert(px>=0);
        for(int mode=0;mode<5;mode++)
        {
            /* One face, then multiple, hidden selection, cancellation and no movement. */
            s->selectedtris[1]=mode>0; hidden[1]=mode==2;
            parameter=0;
            assert(ViewportBeginTransform(s,s,px,py,TRUE));
            assert(s->dragfaceduplicating && !s->dragduplicating && !s->dragportalduplicating && capture==s);
            for(int i=0;i<9;i++)
                assert(s->dragmask[i]==(i<3 || (i<6 && mode>0 && mode!=2)));
            parameter=axis==1 ? -25 : 25;
            ViewportDragTransform(s,s,px,py);
            for(int i=0;i<9;i++) for(int a=0;a<3;a++)
            {
                double old= a==0 ? original[i].x : a==1 ? original[i].y : original[i].z;
                double now= a==0 ? s->scene[i].x : a==1 ? s->scene[i].y : s->scene[i].z;
                Near(now,old+(a==axis && s->dragmask[i] ? parameter : 0));
            }
            int before=facecopies;
            if(mode==3) { ViewportCancelTransform(s); }
            else
            {
                if(mode==4) { parameter=0; ViewportDragTransform(s,s,px,py); }
                ViewportEndTransform(s,s);
            }
            assert(facecopies==before+(mode<3));
            assert(!s->dragfaceduplicating && !capture && !memcmp(s->scene,original,sizeof(original)));
            if(mode<3) for(int a=0;a<3;a++) Near(facecopy.offset[a],a==axis ? parameter : 0);
        }
        s->selectedtris[1]=0; hidden[1]=0; parameter=0;
        assert(ViewportBeginTransform(s,s,px,py,TRUE));
        ViewportEndTransform(s,s); /* Click without any move. */
        assert(!capture && !s->dragfaceduplicating);
        parameter=0;
        assert(ViewportBeginTransform(s,s,px,py,FALSE));
        assert(!s->dragfaceduplicating && s->dragmask[6]); /* Ordinary move keeps shared-vertex behavior. */
        parameter=25; ViewportDragTransform(s,s,px,py); ViewportEndTransform(s,s);
    }
    assert(facecopies==9 && moves==3);
    s->hiddentris=NULL;
    s->dragrotation=TRUE; assert(!ViewportShouldDuplicateBgFaces(s,TRUE)); s->dragrotation=FALSE;
    s->dragscaling=TRUE; assert(!ViewportShouldDuplicateBgFaces(s,TRUE)); s->dragscaling=FALSE;
    s->dragstan=TRUE; assert(!ViewportShouldDuplicateBgFaces(s,TRUE)); s->dragstan=FALSE;
    s->dragportal=TRUE; assert(!ViewportShouldDuplicateBgFaces(s,TRUE)); s->dragportal=FALSE;
    s->tool=EDITOR_TOOL_ROOM_SELECT; assert(!ViewportShouldDuplicateBgFaces(s,TRUE));
    puts("PASS: actual face Shift-drag on XYZ, independent preview despite shared vertices, multiple/hidden faces, click/no-op/cancel, restored originals before one clone request, ordinary moves unchanged.");
}
static double GroupAxisLength(const Scaling *group, const Rotation *axes, int axis)
{
    double tip[3], scaled[3], origin[3]={0}, moved[3];
    for (int a=0;a<3;a++) tip[a]=axes->m[a][axis];
    ScalingPoint(group,origin,moved); ScalingPoint(group,tip,scaled);
    return hypot(hypot(scaled[0]-moved[0],scaled[1]-moved[1]),scaled[2]-moved[2]);
}
static void GroupDrags(ViewportState *s, BOOL doors)
{
    DWORD ids[2]={7,8}; Vertex boxes[2*VIEWPORT_BOX_VERTICES];
    groupDoorFrames=doors;
    s->tool=EDITOR_TOOL_FACE_SELECT; s->selectedobject=7;
    s->selectedobjects=ids; s->selectedobjectcount=2; s->objectselectionboxes=boxes;
    s->showobjects=TRUE; s->scalemode=TRUE; s->rotationmode=FALSE;
    RotationAxis(&s->scaleaxes,0,0);
    ViewportBuildObjectSelectionBox(s);
    double pivot[3]; assert(ViewportGroupPosition(s,pivot));
    for (int a=0;a<3;a++) s->gizmoposition[a]=pivot[a];
    for (int mode=0;mode<5;mode++)
    {
        int px=100,py=100;
        if (mode>0)
        {
            px=-1;
            for(int y=0;y<200 && px<0;y+=2)for(int x=0;x<200;x+=2)
                if(ViewportPickGizmo(s,s,x,y)==(mode-1)%3) { px=x;py=y;break; }
            assert(px>=0);
        }
        parameter=0; assert(ViewportBeginTransform(s,s,px,py,FALSE));
        assert(s->dragmodelcenters && s->dragmodelaxes && capture==s);
        for(int i=0;i<9;i++) assert(s->dragmask[i]==(i<6));
        parameter=45; ViewportDragTransform(s,s,mode==0 ? px+45 : px,py);
        Scaling group; assert(ViewportGetScaling(s,&group));
        for (int i=0;i<9;i++)
        {
            double p[3]={original[i].x,original[i].y,original[i].z}, expected[3];
            memcpy(expected,p,sizeof(p));
            if(i<6 && doors) ScalingPoint(&group,p,expected);
            else if(i<6)
            {
                int model=i/3;
                double local[3]={0}, moved[3];
                ScalingPoint(&group,s->dragmodelcenters[model],moved);
                for(int a=0;a<3;a++)for(int b=0;b<3;b++)
                    local[a]+=s->dragmodelaxes[model].m[b][a]*(p[b]-s->dragmodelcenters[model][b]);
                for(int a=0;a<3;a++)local[a]*=GroupAxisLength(&group,s->dragmodelaxes+model,a);
                RotationVector(s->dragmodelaxes+model,local,expected);
                for(int a=0;a<3;a++)expected[a]+=moved[a];
            }
            Near(s->scene[i].x,expected[0]); Near(s->scene[i].y,expected[1]); Near(s->scene[i].z,expected[2]);
            double normal[3], localnormal[3]={0}, expectednormal[3];
            for(int a=0;a<3;a++) normal[a]=original[i].environment.normal[a];
            memcpy(expectednormal,normal,sizeof(normal));
            if(i<6)
            {
                const Rotation *axes=s->dragmodelaxes+i/3;
                for(int a=0;a<3;a++)
                {
                    for(int b=0;b<3;b++) localnormal[a]+=axes->m[b][a]*normal[b];
                    localnormal[a]/=GroupAxisLength(&group,axes,a);
                }
                RotationVector(axes,localnormal,expectednormal);
            }
            float right[3]={1,0,0},up[3]={0,1,0},uv[2];
            ViewportEnvironmentCoordinates(s,i,0,right,up,uv);
            for(int a=0;a<3;a++) Near(environment.normal[a],expectednormal[a]);
        }
        int old=commits;
        if(mode==4) ViewportCancelTransform(s); else ViewportEndTransform(s,s);
        assert(commits==old+(mode!=4) && !capture && !s->dragmodelaxes && !s->dragmodelcenters);
        assert(!memcmp(s->scene,original,sizeof(original)));
    }
    /* Translation and rotation use the same multi-object membership mask. */
    int oldrotations=rotations;
    s->scalemode=FALSE; s->rotationaxes=7;
    for(int rotate=0;rotate<2;rotate++)
    {
        s->rotationmode=rotate;
        int px=-1,py=-1;
        for(int y=0;y<200 && px<0;y+=2)for(int x=0;x<200;x+=2)
            if(ViewportPickGizmo(s,s,x,y)==1) { px=x;py=y;break; }
        assert(px>=0); parameter=0;
        assert(ViewportBeginTransform(s,s,px,py,FALSE));
        parameter=30; ViewportDragTransform(s,s,px,py);
        Rotation r; RotationAxis(&r,1,30);
        for(int i=0;i<9;i++)
        {
            double p[3]={original[i].x,original[i].y,original[i].z}, expected[3];
            memcpy(expected,p,sizeof(p));
            if(i<6) { if(rotate) RotationPoint(&r,s->dragorigin,p,expected); else expected[1]+=30; }
            Near(s->scene[i].x,expected[0]); Near(s->scene[i].y,expected[1]); Near(s->scene[i].z,expected[2]);
        }
        ViewportEndTransform(s,s);
    }
    assert(rotations==oldrotations+1);
    s->selectedobjects=NULL; s->selectedobjectcount=0; s->objectselectionboxes=NULL;
    groupDoorFrames=FALSE;
    puts("PASS: group world-axis size/spacing and normal previews, door pad permutations, rotated props, XYZ/uniform handles, membership, pivot, translation/rotation, commit/cancel and drag allocation cleanup.");
}

static void PortalReflectionPreview(ViewportState *s)
{
    BgPortal portals[3]={0};
    portals[0].pointcount=4;portals[0].geometryoffset=100;
    portals[0].points[0]=(BgPortalPoint){-10,-20,30};portals[0].points[1]=(BgPortalPoint){10,-20,30};
    portals[0].points[2]=(BgPortalPoint){10,20,30};portals[0].points[3]=(BgPortalPoint){-10,20,30};
    memcpy(portaloriginal,portals,sizeof(portals));
    s->selectedobject=VIEWPORT_OBJECT_NONE;s->selectedobjectcount=0;s->componentcount=0;
    s->showportals=TRUE;s->portals=(BgPortalFile){.portals=portals,.portalcount=1};
    memset(s->portalselection,0,sizeof(s->portalselection));s->portalselection[0]=1;
    s->tool=EDITOR_TOOL_FACE_SELECT;s->rotationmode=TRUE;s->scalemode=FALSE;s->rotationaxes=7;
    DWORD count;assert(ViewportPortalSelectionPosition(s,s->gizmoposition,&count));
    int px=-1,py=-1;parameter=0;
    for(int y=0;y<200&&px<0;y++)for(int x=0;x<200;x++)
        if(ViewportPickGizmo(s,s,x,y)==2) { px=x;py=y;break; }
    assert(px>=0 && ViewportBeginTransform(s,s,px,py,FALSE));
    parameter=45;ViewportDragTransform(s,s,px,py);
    /* A repaint updates reflected textures on the whole scene. The portal
     * drag mask contains only eight entries; the scene has nine vertices. */
    assert(s->scenecount>BG_PORTAL_MAX_POINTS);
    float right[3]={1,0,0},up[3]={0,1,0},uv[2];
    for(int i=s->scenecount-1;i>=0;i--)
    {
        ViewportEnvironmentCoordinates(s,i,BG_RENDER_ENVIRONMENT,right,up,uv);
        assert(!memcmp(&environment,&original[i].environment,sizeof(environment)));
        ViewportEnvironmentCoordinates(s,i,BG_RENDER_ENVIRONMENT|BG_RENDER_ENVIRONMENT_FACE,right,up,uv);
        assert(!memcmp(&environment,&original[i].environment,sizeof(environment)));
    }
    ViewportCancelTransform(s);
    assert(!memcmp(portals,portaloriginal,sizeof(portals)));
    s->showportals=FALSE;s->portals=(BgPortalFile){0};
    puts("PASS: portal rotation repaint preserves scene reflection normals and never indexes the portal mask as scene vertices.");
}

static void PortalDrags(ViewportState *s)
{
    BgPortal portals[3]={0};
    s->selectedobject=VIEWPORT_OBJECT_NONE;s->selectedobjectcount=0;
    s->componentcount=0;s->showportals=TRUE;s->portals=(BgPortalFile){.portals=portals,.portalcount=3};
    for(int i=0;i<3;i++)
    {
        portals[i].geometryoffset=i<2?100:200;portals[i].pointcount=4;
        portals[i].points[0]=(BgPortalPoint){-10,-20,30};portals[i].points[1]=(BgPortalPoint){10,-20,30};
        portals[i].points[2]=(BgPortalPoint){10,20,30};portals[i].points[3]=(BgPortalPoint){-10,20,30};
        if(i==2)for(int p=0;p<4;p++) { portals[i].points[p].x+=100;portals[i].points[p].z+=60; }
    }
    memcpy(portaloriginal,portals,sizeof(portals));
    RotationAxis(&s->scaleaxes,0,0);
    /* Vertex, edge and face masks; multiple independent portals and shared aliases. */
    for(int tool=0;tool<3;tool++)for(int mode=0;mode<5;mode++)
    {
        s->tool=tool==0?EDITOR_TOOL_VERTEX_SELECT:tool==1?EDITOR_TOOL_EDGE_SELECT:EDITOR_TOOL_FACE_SELECT;
        s->portalselection[0]=tool==0?3:1;s->portalselection[1]=0;s->portalselection[2]=tool==2?1:0;
        DWORD count;assert(ViewportPortalSelectionPosition(s,s->gizmoposition,&count));
        s->scalemode=mode>0;s->rotationmode=mode==0;s->rotationaxes=7;
        Rotation frame;assert(ViewportGetGeometryRotation(s,&frame) && RotationValid(&frame));
        parameter=0;int axis=mode==4?3:mode==0?2:mode-1,px=-1,py=-1;
        for(int y=0;y<200&&px<0;y++)for(int x=0;x<200;x++)
            if(ViewportPickGizmo(s,s,x,y)==axis) { px=x;py=y;break; }
        assert(px>=0 && ViewportBeginTransform(s,s,px,py,TRUE));
        assert(s->dragportal && !s->dragportalduplicating && s->dragaxis==axis);
        int oldcommits=commits,oldrotations=rotations;
        if(mode==0) parameter=90;else parameter=45;
        ViewportDragTransform(s,s,px+(mode==4?45:0),py);
        double pivot[3];memcpy(pivot,s->dragorigin,sizeof(pivot));
        for(int i=0;i<3;i++)for(int p=0;p<4;p++)
        {
            BOOL selected=(i<2 || tool==2) && (tool==2 || p<2);
            BgPortalPoint a=portaloriginal[i].points[p],b=portals[i].points[p];
            double expected[3]={a.x,a.y,a.z};
            if(selected)
            {
                if(mode==0) { expected[0]=pivot[0]-(a.y-pivot[1]);expected[1]=pivot[1]+(a.x-pivot[0]); }
                else for(int k=0;k<3;k++) if(axis==3 || k==axis) { expected[k]=pivot[k]+(expected[k]-pivot[k])*1.5; }
            }
            Near(b.x,expected[0]);Near(b.y,expected[1]);Near(b.z,expected[2]);
        }
        ViewportEndTransform(s,s);
        assert(commits==oldcommits+(mode>0) && rotations==oldrotations+(mode==0));
        if(mode==0) { assert(!memcmp(committedrotation.pivot,pivot,sizeof(pivot))); }
        else for(int k=0;k<3;k++) Near(committed.factor[k],axis==3 || k==axis?1.5:1);
        assert(!memcmp(portals,portaloriginal,sizeof(portals)));
        /* A canceled gesture restores the snapshot and dispatches nothing. */
        parameter=0;assert(ViewportBeginTransform(s,s,px,py,FALSE));
        parameter=30;ViewportDragTransform(s,s,px+30,py);ViewportCancelTransform(s);
        assert(!memcmp(portals,portaloriginal,sizeof(portals)));
        assert(commits==oldcommits+(mode>0) && rotations==oldrotations+(mode==0));
    }
    s->tool=EDITOR_TOOL_VERTEX_SELECT;s->portalselection[0]=1;s->portalselection[2]=0;
    Rotation frame;assert(!ViewportGetGeometryRotation(s,&frame)); /* one corner has no orientation */
    s->showportals=FALSE;s->portals=(BgPortalFile){0};
    puts("PASS: portal vertex/edge/face rotation, XYZ/uniform scaling, group pivot, shared aliases, preview/commit restoration, Shift and cancellation.");
}

int main(void)
{
    Vertex scene[9]={0};BgDocumentVertexRef refs[9];unsigned char selected[3]={1,0,0};
    DWORD objects[3]={7,8,9};ViewportComponent edge={.corners={0,1}};
    ViewportState s={.dragaxis=-1,.hoveraxis=-1,.scalemode=TRUE,.scalevalid=TRUE,.gizmovisible=TRUE,
        .selectedobject=VIEWPORT_OBJECT_NONE,.tool=EDITOR_TOOL_FACE_SELECT,.scene=scene,.scenecount=9,
        .scenevertexrefs=refs,.selectedtris=selected,.sceneobjectindices=objects,.gizmoposition={10.25,20.5,30.75},
        .scalehandle=handle,.scalehandletris=sizeof(handle)/sizeof(*handle)/3,
        .arrow=handle,.arrowtris=sizeof(handle)/sizeof(*handle)/3,.cylinder=handle,.cylindertris=sizeof(handle)/sizeof(*handle)/3};
    double degrees[3]={0};RotationEuler(&s.scaleaxes,degrees);
    for(int i=0;i<9;i++) {
        scene[i].x=11+i;scene[i].y=17-i;scene[i].z=35+2*i;
        scene[i].environment.normal[0]=.2;scene[i].environment.normal[1]=.4;scene[i].environment.normal[2]=.8;
        refs[i]=(BgDocumentVertexRef){.room=1,.index=i};
    }
    scene[6]=scene[0];refs[6]=refs[0]; /* Same native vertex on another face follows. */
    scene[7]=scene[0]; /* A coincident but separate native vertex does not. */
    memcpy(original,scene,sizeof(original));edge.refs[0]=refs[0];edge.refs[1]=refs[1];
    Handles(&s);
    for(int mode=0;mode<3;mode++) {
        s.tool=mode==1 ? EDITOR_TOOL_EDGE_SELECT : EDITOR_TOOL_FACE_SELECT;
        s.components=&edge;s.componentcount=mode==1;
        s.selectedobject=mode==2 ? 7 : VIEWPORT_OBJECT_NONE;
        for(int rotated=0;rotated<2;rotated++) {
            degrees[0]=rotated*19;degrees[1]=rotated*31;degrees[2]=rotated*47;
            RotationEuler(&s.scaleaxes,degrees);Drags(&s);
        }
    }
    AxisDrag(&s);
    FaceDrags(&s);
    GroupDrags(&s,FALSE);
    GroupDrags(&s,TRUE);
    PortalDrags(&s);
    PortalReflectionPreview(&s);
    puts("PASS: object/face/edge masks, rotated axes, screen directions, snapshot preview, proportional XYZ factors, guides/normals, no-op, clamp, cancellation, one commit and Shift duplication; axis scaling unchanged.");
    return 0;
}
