#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include "modeluv.h"
#include "newprops.h"
#include "gltf.h"
#define WM_APP 0x8000
#define MB_ICONERROR 16
#define UVCANVAS_PI 3.14159265358979323846
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
#include "types.inc"
static const char *why = "", *asset, *project, *name = "PuvtestZ";
#define OK(x) do { if (!(x)) { fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why);abort(); } } while(0)
static UVCanvasState state;
static ModelSource source;
static DWORD revision;
static BgFaceRef refs[128];
static DWORD refcount;
static ModelUVChange last;
static int commits;
static unsigned char *Read(const char *path, DWORD *size)
{
    FILE *f=fopen(path,"rb");OK(f&&!fseek(f,0,SEEK_END));*size=ftell(f);rewind(f);
    unsigned char *data=malloc(*size);OK(data&&fread(data,1,*size,f)==*size&&!fclose(f));return data;
}
BOOL TexGetProjectImageSize(const char *dir,DWORD texture,int *w,int *h)
{ *w=texture==0x2c6?64:32;*h=texture==0x237?33:32;return TRUE; }
BOOL TexLoadProjectImage(const char *dir,DWORD texture,TexPixel *pixels,int *w,int *h)
{ TexGetProjectImageSize(dir,texture,w,h);memset(pixels,255,(size_t)*w**h*sizeof(*pixels));return TRUE; }
BOOL TexEncodePng(const TexPixel *pixels,int w,int h,unsigned char **data,DWORD *size)
{ *size=8;*data=calloc(8,1);return *data!=NULL; }
BOOL RomLoad(const char *path,RomFile *rom,const char **reason)
{
    memset(rom,0,sizeof(*rom));rom->data=Read(asset,&rom->size);
    DWORD at=rom->size;rom->data=realloc(rom->data,at+16);OK(rom->data);rom->size+=16;
    memset(rom->data+at,0,16);rom->data[at+3]=CUSTOM_PROP_CONFIG_VERSION;
    rom->info.entrycount=2;
    rom->info.entries[0]=(RomManifestEntry){.kind=CUSTOM_PROP_MANIFEST_KIND,.romstart=at,.romend=at+16,.flags=CUSTOM_PROP_CONFIG_VERSION};
    rom->info.entries[1]=(RomManifestEntry){.kind=CUSTOM_PROP_DATA_KIND,.flags=CUSTOM_PROP_CONFIG_VERSION};
    return TRUE;
}
void RomFree(RomFile *rom) { free(rom->data); }
BOOL RomFindFile(const RomFile *rom,const char *model,DWORD *offset,DWORD *size,const char **reason)
{ *offset=0;*size=rom->size-16;return !strcmp(model,"PuvtestZ"); }
static UVCanvasState *UVCanvasGetState(HWND hwnd) { return hwnd; }
static HWND GetParent(HWND hwnd) { return (HWND)1; }
static HWND GetCapture(void) { return NULL; }
static void ReleaseCapture(void) {}
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
static void UVCanvasEndPan(HWND hwnd,UVCanvasState *s) { s->panning=FALSE; }
static void MessageBox(HWND hwnd,const char *text,const char *title,int flags) { fprintf(stderr,"%s\n",text);abort(); }
static void Refresh(void);
static LRESULT SendMessage(HWND hwnd,unsigned message,int wparam,LPARAM lparam)
{
    if (message==UVCANVAS_WM_SELECTION_CHANGED) return 0;
    OK(message==UVCANVAS_WM_COMMIT||message==UVCANVAS_WM_COMMIT_FACES);
    /* The model owner rebuilds a fresh authored snapshot, as the real dialog
     * does. Committing then replaces the canvas during its synchronous call. */
    UVCanvasTriangle *triangles=NULL;ModelUVEdit *edits=NULL;DWORD count=0;
    OK(ModelUVBuild(&source,refs,refcount,project,&triangles,&why));
    OK(ModelUVConvert(triangles,refcount,message==UVCANVAS_WM_COMMIT?(void *)lparam:NULL,
        message==UVCANVAS_WM_COMMIT_FACES?(void *)lparam:NULL,&edits,&count,&why));
    ModelEditsFreeUVChange(&last);
    OK(ModelEditsSetUVs(project,name,revision,edits,count,&last,&why));
    free(edits);free(triangles);commits++;Refresh();return TRUE;
}
#include "canvas.inc"
static void Refresh(void)
{
    ModelFreeSource(&source);OK(ModelEditsReadSource(project,name,&source,&revision,&why));
    UVCanvasTriangle *triangles=NULL;OK(ModelUVBuild(&source,refs,refcount,project,&triangles,&why));
    OK(UVCanvasSetTriangles(&state,triangles,refcount));
}
static void AllNodes(void) { for(int i=0;i<state.nodecount;i++)state.nodes[i].selected=TRUE; }
static void SameGeometry(const ModelSource *before,const unsigned char *base)
{
    DWORD size;const unsigned char *data=ModelEditsGetData(project,name,&size,&why);OK(data);
    OK(source.count==before->count&&source.materials.count==before->materials.count);
    OK(!memcmp(source.materials.slots,before->materials.slots,(size_t)source.materials.count*sizeof(*source.materials.slots)));
    for(DWORD f=0;f<source.count;f++)
    {
        OK(source.flags[f]==before->flags[f]&&source.tags[f]==before->tags[f]);
        OK(source.faces[f].list==before->faces[f].list&&source.faces[f].normalmask==before->faces[f].normalmask);
        BOOL selected=FALSE;for(DWORD r=0;r<refcount;r++)selected|=refs[r].faceid==f+1;
        for(DWORD k=0;k<3;k++)
        {
            DWORD id=f*3+k;const unsigned char *a=base+before->vertexoffsets[id],*b=data+source.vertexoffsets[id];
            OK(!memcmp(a,b,8)&&!memcmp(a+12,b+12,4)); /* Native XYZ, flags, RGB/normal, alpha. */
            if(!selected)
            { OK(!memcmp(a,b,16));OK(!memcmp(before->materials.faces[f].uv+k*2,source.materials.faces[f].uv+k*2,2*sizeof(float))); }
        }
    }
}
static void Saved(void)
{
    DWORD expected=revision;OK(ModelEditsSave(project,&why));OK(!ModelEditsHasUnsaved());ModelEditsReset();Refresh();OK(revision==expected);
    DWORD basesize;unsigned char *base=Read(asset,&basesize),*replacement=NULL;DWORD size;
    OK(ModelEditsReadReplacement(project,name,base,basesize,&replacement,&size,&why)==1);
    ModelSource saved={0};OK(ModelReadSource(replacement,size,&saved,&why));OK(saved.count==source.count);
    for(DWORD f=0;f<source.count;f++)for(DWORD k=0;k<3;k++)
    {
        const BgVertex *a=&source.vertices[f*3+k],*b=&saved.vertices[f*3+k];
        OK(a->s==b->s&&a->t==b->t);
    }
    ModelFreeSource(&saved);free(base);free(replacement);
}
int main(int argc,char **argv)
{
    OK(argc>=4);asset=argv[1];project=argv[2];
    ModelSource original={0};DWORD originalrevision,basesize;unsigned char *base=Read(asset,&basesize);
    OK(ModelEditsReadSource(project,name,&original,&originalrevision,&why));
    refcount=1;refs[0]=(BgFaceRef){.room=1,.faceid=1};
    if(!strcmp(argv[3],"reflection"))
    {
        UVCanvasTriangle *triangles=NULL;
        OK(!ModelUVBuild(&original,refs,refcount,project,&triangles,&why)&&!triangles);
        ModelUVEdit edit={.corner=0,.uv={.5,.25}};
        OK(!ModelEditsSetUVs(project,name,originalrevision,&edit,1,NULL,&why)&&!ModelEditsHasUnsaved());
        puts("PASS: generated reflection UVs rejected without changes.");goto done;
    }
    Refresh();OK(UVCanvasHasFaces(&state));
    AllNodes();state.nodes[0].selected=TRUE;
    for(int i=1;i<state.nodecount;i++)state.nodes[i].selected=FALSE;
    double target=state.nodes[0].source.s/(32.0*state.nodes[0].width)+.25;
    OK(UVCanvasSetCoordinate(&state,0,target,&why));OK(last.before&&commits==1);SameGeometry(&original,base);
    DWORD editedrevision=revision;
    OK(!ModelEditsSetUVs(project,name,originalrevision,NULL,0,NULL,&why));
    OK(ModelEditsRestoreUVs(project,name,&last,FALSE,&why));Refresh();OK(revision==originalrevision);
    OK(ModelEditsRestoreUVs(project,name,&last,TRUE,&why));Refresh();OK(revision==editedrevision);
    Saved();
    ModelUVEdit edit={.corner=0};memcpy(edit.uv,source.materials.faces[0].uv,sizeof(edit.uv));
    ModelUVChange noop={0};OK(ModelEditsSetUVs(project,name,revision,&edit,1,&noop,&why)&&!noop.before&&!ModelEditsHasUnsaved());
    edit.uv[0]=NAN;OK(!ModelEditsSetUVs(project,name,revision,&edit,1,NULL,&why)&&!ModelEditsHasUnsaved());
    edit.uv[0]=10000;OK(!ModelEditsSetUVs(project,name,revision,&edit,1,NULL,&why)&&!ModelEditsHasUnsaved());
    edit.corner=source.count*3;OK(!ModelEditsSetUVs(project,name,revision,&edit,1,NULL,&why)&&!ModelEditsHasUnsaved());
    /* Real planar tools, rotation and scale feed the model owner, retaining
     * all other native attributes and unselected faces after each operation. */
    for(int p=UV_PROJECTION_X;p<UV_PROJECTION_COUNT;p++)
    {
        OK(UVCanvasProjectFaces(&state,p,0,&why));SameGeometry(&original,base);
        OK(UVCanvasProjectFaces(&state,p,4,&why));SameGeometry(&original,base);
    }
    AllNodes();UVCanvasSetTransformMode(&state,TRANSFORM_ROTATE);
    double values[2]={90,0};OK(UVCanvasApplyTransform(&state,values,&why));SameGeometry(&original,base);
    AllNodes();UVCanvasSetTransformMode(&state,TRANSFORM_SCALE);values[0]=.5;values[1]=2;
    OK(UVCanvasApplyTransform(&state,values,&why));SameGeometry(&original,base);
    /* Once the seam exists, repeated moves update the same allocation. */
    DWORD stableSize;OK(ModelEditsGetData(project,name,&stableSize,&why));
    for(int i=0;i<16;i++)
    {
        AllNodes();UVCanvasSetTransformMode(&state,TRANSFORM_MOVE);
        OK(UVCanvasSetCoordinate(&state,0,.125+(i%2)*.25,&why));
        DWORD size;OK(ModelEditsGetData(project,name,&size,&why)&&size==stableSize);
    }
    Saved();
    /* UV history coexists with the native paint API in chronological order. */
    if(!original.faces[0].normalmask)
    {
        unsigned char rgba[4]={37,81,142,source.vertices[0].a};ModelVertexPaint paint;
        OK(ModelEditsSetVertexColor(project,name,revision,0,rgba,&paint,&why));
        OK(!ModelEditsRestoreUVs(project,name,&last,FALSE,&why));
        OK(ModelEditsRestoreVertexColor(project,name,&paint,FALSE,&why));
        OK(ModelEditsRestoreUVs(project,name,&last,FALSE,&why));
        OK(ModelEditsRestoreUVs(project,name,&last,TRUE,&why));
        OK(ModelEditsRestoreVertexColor(project,name,&paint,TRUE,&why));Refresh();
    }
    DWORD slot=source.materials.faces[0].slot;
    OK(ModelEditsSetMaterial(project,name,revision,slot,BG_TEX_NONE,&why));Refresh();
    AllNodes();OK(UVCanvasSetCoordinate(&state,0,.625,&why));OK(source.materials.faces[0].uv[0]==.625f);
    Saved();
    OK(ModelEditsSetMaterial(project,name,revision,slot,0x2c6,&why));Refresh();
    OK(source.vertices[0].s==40&&source.materials.faces[0].uv[0]==.625f);Saved();
    /* The same UV editor path must work for newly added prop models. */
    if(argc==5)
    {
        DWORD faces;name="PuvcustomZ";OK(NewPropsImport(project,name,argv[4],FALSE,&faces,&why));OK(faces==16);
        refcount=faces;for(DWORD f=0;f<faces;f++)refs[f]=(BgFaceRef){.room=1,.faceid=f+1};
        Refresh();OK(ModelEditsSetMaterial(project,name,revision,0,0x2c6,&why));Refresh();
        OK(UVCanvasProjectCylinder(&state,2,16,&why));OK(last.before);
        DWORD uvrevision=revision;OK(ModelEditsRestoreUVs(project,name,&last,FALSE,&why));Refresh();
        OK(ModelEditsRestoreUVs(project,name,&last,TRUE,&why));Refresh();OK(revision==uvrevision);
        OK(ModelEditsSave(project,&why));ModelEditsReset();Refresh();OK(revision==uvrevision);
        puts("PASS: custom prop cylindrical unwrap, seam splits, undo/redo and save/reopen.");
    }
    printf("PASS: %s: native attributes/unselected faces, projections, transforms, undo/redo, repeat size, untextured UVs, save/reopen and ROM replacement.\n",argv[3]);
done:
    ModelEditsFreeUVChange(&last);ModelFreeSource(&source);ModelFreeSource(&original);ModelEditsReset();free(base);
    free(state.nodes);free(state.triangles);return 0;
}
