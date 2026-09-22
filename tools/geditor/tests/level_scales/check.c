#include <assert.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include "levelscale.h"
static const char *why="";
#define OK(x) do { if (!(x)) { fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why);abort(); } } while(0)
static void Put(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void Mesh(const BgDocument *bg,float x)
{
    BgDocumentRenderMesh mesh={0};OK(BgDocumentBuildRenderMesh(bg,&mesh,&why));
    OK(mesh.facecount==1&&mesh.vertices[0].x==x);BgDocumentRenderMeshFree(&mesh);
}
static void Synthetic(void)
{
    BgDocument bg={.roomcount=1,.facecount=1,.levelscale=1}, faces={0};
    BgPortalFile portals={0};EditHistory history={0};SetupFile setup={0};StanFile stan={0};
    EditHistoryTransaction transaction={0};EditHistoryAsset asset;
    bg.rooms=calloc(2,sizeof(*bg.rooms));OK(bg.rooms);
    BgDocumentRoom *room=&bg.rooms[1];room->origin[0]=10;room->vertexcount=3;room->facecount=1;
    room->vertices=calloc(3,sizeof(*room->vertices));room->faces=calloc(1,sizeof(*room->faces));OK(room->vertices&&room->faces);
    room->layers[0].groupcount=1;room->layers[0].groups=calloc(1,sizeof(*room->layers[0].groups));OK(room->layers[0].groups);
    for(int i=0;i<3;i++) { room->vertices[i]=(BgDocumentVertex){.id=i+1,.room=1,.x=10+i,.z=i==2?10:0,.a=255}; }
    room->faces[0]=(BgDocumentFace){.id=1,.room=1,.vertexindices={0,1,2}};
    bg.portals.portalcount=1;bg.portals.portals=calloc(1,sizeof(BgPortal));OK(bg.portals.portals);
    BgPortal *portal=bg.portals.portals;portal->pointcount=3;portal->geometryoffset=0x100;
    for(int i=0;i<3;i++) { portal->points[i]=portal->nativepoints[i]=(BgPortalPoint){10+i,20,30}; }
    unsigned char data[76]={0};Put(data+4,0x0e00000c);Put(data+12,0x00010001);data[18]=0x30;data[19]=0x12;
    data[21]=10;data[32]=0;data[33]=100;data[36]=0;data[37]=100;memcpy(data+52,"unstric",8);
    OK(StanLoadNative(data,sizeof(data),1,&stan,&why));stan.tiles[0].editorid=927;
    OK(BgDocumentClone(&bg,&faces,&why));portals.portalcount=1;portals.portals=malloc(sizeof(BgPortal));OK(portals.portals);*portals.portals=*portal;
    EditHistoryReset(&history,&bg,&setup,&stan);int selection=927;
    OK(EditHistorySetSelection(&history,&selection,sizeof(selection),FALSE,&why));
    OK(EditHistoryBeginBgEdit(&history,&bg,"Vertex edit",&transaction,&why));
    bg.rooms[1].vertices[0].x=30;
    OK(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&transaction,&why));
    OK(EditHistoryBeginStanEdit(&history,&stan,"Stan edit",&transaction,&why));
    stan.data[21]=20;stan.tiles[0].points[0].x=20;
    OK(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&transaction,&why));
    OK(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why)&&asset==EDIT_HISTORY_ASSET_STAN);
    ULONGLONG revision=history.currentstaterevision;BgDocumentVertex authored[3];
    memcpy(authored,bg.rooms[1].vertices,sizeof(authored));
    OK(LevelScaleApply(&bg,&stan,&history,&faces,&portals,.25f,&why));
    OK(history.undocount==1&&history.redocount==1&&history.currentstaterevision==revision);
    OK(bg.dirty&&!stan.dirty&&*(int *)history.selection==927);
    OK(!memcmp(authored,bg.rooms[1].vertices,sizeof(authored))&&!memcmp(data,stan.data,sizeof(data)));
    OK(portal->points[0].x==40&&portal->nativepoints[0].x==10&&portal->geometryoffset==0x100);
    OK(stan.tiles[0].points[0].x==40&&stan.tiles[0].editorid==927&&stan.levelscale==.25f);
    OK(faces.levelscale==.25f&&faces.rooms[1].vertices[0].x==10&&portals.portals[0].points[0].x==40);
    Mesh(&bg,160);
    OK(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why));Mesh(&bg,80);OK(bg.levelscale==.25f&&!bg.dirty);
    OK(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why));Mesh(&bg,160);
    OK(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why));
    OK(stan.levelscale==.25f&&stan.tiles[0].points[0].x==80&&stan.tiles[0].editorid==927&&stan.dirty);
    for(int i=0;i<30;i++)
    {
        OK(LevelScaleApply(&bg,&stan,&history,&faces,&portals,.333333343f,&why));
        OK(LevelScaleApply(&bg,&stan,&history,&faces,&portals,1,&why));
        OK(stan.tiles[0].points[0].x==20&&bg.portals.portals[0].points[0].x==10);
        OK(!memcmp(authored,bg.rooms[1].vertices,sizeof(authored)));Mesh(&bg,40);
    }
    /* A bad inactive snapshot must fail before touching the live documents. */
    history.undoentries[0].bgdocument.portals.portals[0].nativepoints[0].x=FLT_MAX;
    OK(!LevelScaleApply(&bg,&stan,&history,&faces,&portals,.25f,&why));
    OK(bg.levelscale==1&&stan.levelscale==1&&faces.levelscale==1&&bg.portals.portals[0].points[0].x==10);
    const float invalid[]={0,-1,NAN,INFINITY,1e-40f};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++)
    { OK(!LevelScaleApply(&bg,&stan,&history,&faces,&portals,invalid[i],&why)&&bg.levelscale==1); }
    EditHistoryFree(&history);BgDocumentFree(&bg);BgDocumentFree(&faces);BgPortalFileFree(&portals);StanFileFree(&stan);
    puts("PASS: scale preview, exact native vertices/portals/STAN, IDs, dirty revisions, selection, clipboard, undo/redo, drift-free repeated edits and atomic rejection.");
}
static void RealBackground(const char *path)
{
    FILE *f=fopen(path,"rb");OK(f);OK(!fseek(f,0,SEEK_END));long size=ftell(f);rewind(f);
    unsigned char *data=malloc(size);OK(data&&fread(data,1,size,f)==(size_t)size);fclose(f);
    BgDocument bg={0},faces={0};BgPortalFile portals={0};StanFile stan={0};EditHistory history={0};
    BgDocumentRenderMesh before={0},after={0};
    OK(BgDocumentLoad(data,size,1,&bg,&why));OK(BgDocumentBuildRenderMesh(&bg,&before,&why));
    OK(LevelScaleApply(&bg,&stan,&history,&faces,&portals,.5f,&why));
    OK(BgDocumentBuildRenderMesh(&bg,&after,&why));OK(after.facecount==before.facecount);
    for(DWORD v=0;v<after.facecount*3;v++)
    { OK(after.vertices[v].x==2*before.vertices[v].x&&after.vertices[v].y==2*before.vertices[v].y&&after.vertices[v].z==2*before.vertices[v].z); }
    BgDocumentRenderMeshFree(&after);OK(LevelScaleApply(&bg,&stan,&history,&faces,&portals,1,&why));
    OK(BgDocumentBuildRenderMesh(&bg,&after,&why));
    OK(!memcmp(before.vertices,after.vertices,before.facecount*3*sizeof(*before.vertices)));
    printf("PASS: real BG scale preview and exact round trip, %lu faces.\n",(unsigned long)before.facecount);
    BgDocumentRenderMeshFree(&before);BgDocumentRenderMeshFree(&after);BgDocumentFree(&bg);free(data);
}
int main(int argc,char **argv) { Synthetic();if(argc>1)RealBackground(argv[1]);return 0; }
