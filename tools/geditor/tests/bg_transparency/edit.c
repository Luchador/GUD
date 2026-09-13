#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
/* Exercise every allocation failure in the edit, including its native group
   rebuild. The live document must stay intact even after earlier rooms succeeded. */
static int allocations = -1;
static BOOL Fail(void) { if (!allocations) { return TRUE; } if (allocations > 0) { allocations--; } return FALSE; }
static void *TestMalloc(size_t n) { return Fail() ? NULL : malloc(n); }
static void *TestCalloc(size_t n, size_t s) { return Fail() ? NULL : calloc(n, s); }
static void *TestRealloc(void *p, size_t n) { return Fail() ? NULL : realloc(p, n); }
#define malloc TestMalloc
#define calloc TestCalloc
#define realloc TestRealloc
#include "bgdocument.c"
#undef malloc
#undef calloc
#undef realloc

static void Put(unsigned char *p, DWORD v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static BgFile Fixture(void)
{
    BgFile bg={0}; bg.data=calloc(4096,1); assert(bg.data);
    strcpy(bg.name,"bg/bg_transparency_test.seg");
    Put(bg.data+4,0x0e000020);
    DWORD next=160;
    for (DWORD room=1; room<=2; room++)
    {
        Put(bg.data+next,48); next+=4;
        Put(bg.data+32+room*24,0x0e000000|next);
        bg.data[next+17]=10; bg.data[next+35]=10;
        for (int v=0; v<3; v++) { memset(bg.data+next+v*16+12,255,4); }
        next+=48;
        for (DWORD layer=0; layer<2; layer++)
        {
            /* Four faces in one native TRI4, then an inherited-state face in
               another group, followed by a state-only tail group. */
            const DWORD commands[]={0xe7000000,0, 0xba001402,0x00100000,
                0xb900031d,0x0c1849d8, 0xb9000002,0, 0xb7000000,0x2005,
                0xbb000001,0xffffffff, 0xc0000002,17, 0xfc26a004,0x1f1093ff,
                0x04200030,0, 0xb1002222,0x10101010,
                0xfb000000,0xffffffff, 0xbf000000,0xa14,
                0xe7000000,0, 0xfa000000,0xffffffff, 0xb8000000,0};
            Put(bg.data+next,sizeof(commands)); next+=4;
            Put(bg.data+36+room*24+layer*4,0x0e000000|next);
            for (DWORD i=0; i<sizeof(commands)/4; i++) { Put(bg.data+next+i*4,commands[i]); }
            next+=sizeof(commands);
        }
    }
    bg.size=next; return bg;
}
static DWORD Refs(const BgDocument *doc, BgFaceRef *refs)
{
    DWORD n=0;
    for (DWORD r=1; r<=doc->roomcount; r++)
    for (DWORD f=0; f<doc->rooms[r].facecount; f++)
    { const BgDocumentFace *p=&doc->rooms[r].faces[f]; refs[n++]=(BgFaceRef){.room=r,.faceid=p->id,.layer=p->layer}; }
    return n;
}
static void Equivalent(const BgDocument *a, const BgDocument *b)
{
    BgDocumentRenderMesh x={0}, y={0}; const char *why="";
    assert(BgDocumentBuildRenderMesh(a,&x,&why) && BgDocumentBuildRenderMesh(b,&y,&why));
    assert(x.facecount==y.facecount);
    assert(!memcmp(x.tags,y.tags,x.facecount*sizeof(*x.tags)));
    assert(!memcmp(x.renderflags,y.renderflags,x.facecount*sizeof(*x.renderflags)));
    assert(!memcmp(x.vertices,y.vertices,x.facecount*3*sizeof(*x.vertices)));
    BgDocumentRenderMeshFree(&x); BgDocumentRenderMeshFree(&y);
}
static void RoundTrip(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile compiled={0},saved={0},again={0}; BgDocument loaded={0}; const char *why="";
    assert(BgDocumentCompile(doc,source,&compiled,&why));
    assert(BgFileValidateVertexBatches(&compiled,&why));
    char path[MAX_PATH]; snprintf(path,sizeof(path),"%s/bg",dir); CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir,&compiled,&why));
    assert(BgLoadProjectFile(dir,compiled.name,&saved,&why));
    assert(saved.size==compiled.size && !memcmp(saved.data,compiled.data,saved.size));
    assert(BgDocumentLoad(saved.data,saved.size,doc->levelscale,&loaded,&why));
    Equivalent(doc,&loaded);
    assert(BgDocumentCompile(&loaded,&saved,&again,&why));
    /* Saving again must not accumulate commands or change the native data. */
    assert(again.size==saved.size && !memcmp(again.data,saved.data,saved.size));
    BgDocumentFree(&loaded); BgFileFree(&compiled); BgFileFree(&saved); BgFileFree(&again);
}
static void CheckModes(const BgDocument *doc, const BgFaceRef *refs, const BgRenderState *before, DWORD count)
{
    BgRenderState after[20]; assert(count<=20);
    assert(BgDocumentGetFaceRenderStates(doc,refs,count,after));
    for (DWORD i=0; i<count; i++)
    {
        BOOL selected=i==1 || i==3 || i==6 || i==11;
        assert(BgRenderGetTransparency(&after[i])==(selected?BG_TRANSPARENCY_CUTOUT:BG_TRANSPARENCY_BLEND));
        if (!selected) { assert(after[i].othermode==before[i].othermode); }
        assert(after[i].othermodehigh==before[i].othermodehigh);
        assert(after[i].geometrymode==before[i].geometrymode);
        assert((after[i].othermode&0xCCCC0007u)==(before[i].othermode&0xCCCC0007u));
    }
    for (DWORD r=1; r<=doc->roomcount; r++)
    for (int layer=0; layer<2; layer++)
    {
        BgRenderState s; BOOL synced=FALSE;
        BgRenderStateInit(&s,layer);
        const BgDocumentLayerData *data=&doc->rooms[r].layers[layer];
        for (DWORD g=0; g<data->groupcount; g++)
        {
            const BgDocumentDrawGroup *group=&data->groups[g];
            for (DWORD k=0; k<group->commandsize; k+=8)
            {
                DWORD w0=BgDocumentRead32(group->commands+k), w1=BgDocumentRead32(group->commands+k+4);
                if ((w0>>24)==0xe7) { synced=TRUE; }
                if ((w0>>24)==0xb9) { assert(synced); }
                /* Simulate the game's fog LUT replacing PASS in the original
                   full render command. Partial surface edits must retain it. */
                if (w0==0xb900031d && w1==0x0c1849d8) { w1=0xc81849d8; }
                BgRenderStateRead(&s,w0,w1);
            }
            for (DWORD f=0; f<doc->rooms[r].facecount; f++)
            {
                const BgDocumentFace *face=&doc->rooms[r].faces[f];
                if (face->layer==layer && face->drawgroup==g)
                { synced=FALSE; assert((s.othermode&0xCCCC0000u)==0xC8080000u); }
            }
        }
        assert(s.othermode==0xc81849d8); /* Caller state restored, including fog. */
    }
}
static void Presets(void)
{
    BgRenderState s; DWORD mode;
    BgRenderStateInit(&s,FALSE);
    BgRenderStateRead(&s,0xb900031d,0x005049d8);
    assert(!BgRenderSurfacePreset(&s,BG_TRANSPARENCY_CUTOUT,&mode)); /* Inherited cycle. */
    BgRenderStateRead(&s,0xba001402,0);
    assert(BgRenderSurfacePreset(&s,BG_TRANSPARENCY_CUTOUT,&mode));
    assert(mode==0x00543078); /* One-cycle final blender, other cycle retained. */
    BgRenderStateRead(&s,0xb900031d,0x005049c8); /* No Z compare. */
    assert(BgRenderSurfacePreset(&s,BG_TRANSPARENCY_OPAQUE,&mode));
    assert(!(mode&0x30));
    BgRenderStateRead(&s,0xb900031d,0x005849d8); /* Additive final blender. */
    assert(!BgRenderSurfacePreset(&s,BG_TRANSPARENCY_CUTOUT,&mode));
    BgRenderStateRead(&s,0xb900031d,0x00504dd8); /* Decal. */
    assert(!BgRenderSurfacePreset(&s,BG_TRANSPARENCY_CUTOUT,&mode));
    BgRenderStateRead(&s,0xb900031d,0x005049d8);
    BgRenderStateRead(&s,0xba001402,0x00200000); /* Copy cycle. */
    assert(!BgRenderSurfacePreset(&s,BG_TRANSPARENCY_CUTOUT,&mode));
    BgRenderStateRead(&s,0xba001402,0);
    BgRenderStateRead(&s,0xb9000002,1); /* Alpha compare. */
    assert(!BgRenderSurfacePreset(&s,BG_TRANSPARENCY_OPAQUE,&mode));
}
static void Jungle(const char *filename, const char *dir)
{
    BgFile source={0}; BgDocument doc={0}; const char *why="";
    FILE *file=fopen(filename,"rb"); assert(file);
    fseek(file,0,SEEK_END); source.size=(DWORD)ftell(file); rewind(file);
    source.data=malloc(source.size); assert(source.data);
    assert(fread(source.data,1,source.size,file)==source.size); fclose(file);
    strcpy(source.name,"bg/bg_jungle_transparency_test.seg");
    assert(BgDocumentLoad(source.data,source.size,1,&doc,&why));
    BgFaceRef *refs=malloc(doc.facecount*sizeof(*refs)), *selected=malloc(doc.facecount*sizeof(*selected));
    BgRenderState *before=malloc(doc.facecount*sizeof(*before)), *after=malloc(doc.facecount*sizeof(*after));
    unsigned char *edited=calloc(doc.facecount,1);
    assert(refs && selected && before && after && edited);
    assert(Refs(&doc,refs)==doc.facecount);
    assert(BgDocumentGetFaceRenderStates(&doc,refs,doc.facecount,before));
    DWORD count=0, mode;
    for (DWORD i=0; i<doc.facecount; i++)
    {
        if (BgRenderGetTransparency(&before[i])==BG_TRANSPARENCY_BLEND
            && BgRenderSurfacePreset(&before[i],BG_TRANSPARENCY_CUTOUT,&mode))
        { selected[count++]=refs[i]; edited[i]=1; }
    }
    assert(count>100);
    BgFacePropertiesEdit edit={.fields=BG_FACE_PROPERTY_TRANSPARENCY,.transparency=BG_TRANSPARENCY_CUTOUT};
    BOOL changed;
    assert(BgDocumentSetFaceProperties(&doc,selected,count,&edit,&changed,&why) && changed);
    assert(BgDocumentGetFaceRenderStates(&doc,refs,doc.facecount,after));
    for (DWORD i=0; i<doc.facecount; i++)
    {
        if (edited[i]) { assert(BgRenderGetTransparency(&after[i])==BG_TRANSPARENCY_CUTOUT); }
        else { assert(after[i].othermode==before[i].othermode); }
        assert(after[i].othermodehigh==before[i].othermodehigh && after[i].geometrymode==before[i].geometrymode);
    }
    RoundTrip(&doc,&source,dir);
    printf("PASS Jungle native data: %u translucent faces converted to cutout; %u total faces preserved through save/reload.\n",count,doc.facecount);
    free(refs); free(selected); free(before); free(after); free(edited); BgDocumentFree(&doc); BgFileFree(&source);
}
int main(int argc, char **argv)
{
    assert(argc==3); Presets(); Jungle(argv[2],argv[1]);
    BgFile source=Fixture(); BgDocument doc={0},original={0}; BgFaceRef refs[20]; BgRenderState states[20];
    const char *why=""; BOOL changed=FALSE;
    assert(BgDocumentLoad(source.data,source.size,1,&doc,&why));
    assert(Refs(&doc,refs)==20);
    assert(BgDocumentGetFaceRenderStates(&doc,refs,20,states));
    assert(BgDocumentClone(&doc,&original,&why));
    BgFaceRef selected[]={refs[1],refs[3],refs[6],refs[11],refs[1]};
    BgFacePropertiesEdit edit={.fields=BG_FACE_PROPERTY_TRANSPARENCY,.transparency=BG_TRANSPARENCY_CUTOUT};
    int failures=0;
    for (int limit=0; limit<300; limit++)
    {
        allocations=limit;
        BOOL ok=BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why);
        allocations=-1;
        if (ok) { assert(changed); break; }
        failures++;
        assert(!changed && !doc.dirty && why[0]);
        Equivalent(&doc,&original);
    }
    assert(failures>20 && failures<300); CheckModes(&doc,refs,states,20);
    RoundTrip(&doc,&source,argv[1]);
    assert(BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && !changed);
    /* Restore original, then test the real editor history transaction. */
    BgDocumentFree(&doc); assert(BgDocumentClone(&original,&doc,&why));
    EditHistory history={0}; EditHistoryTransaction transaction={0}; EditHistoryAsset asset;
    SetupFile setup={0}; StanFile stan={0}; EditHistoryReset(&history,&doc,&setup,&stan);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Change BG Transparency",&transaction,&why));
    assert(BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && changed);
    assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&transaction,&why));
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,&asset,&why)); Equivalent(&doc,&original);
    assert(EditHistoryRedo(&history,&doc,&setup,&stan,&asset,&why)); CheckModes(&doc,refs,states,20);
    /* Stale references and mixed unsupported state reject all fields atomically. */
    selected[4].faceid=99999;
    edit.fields|=BG_FACE_PROPERTY_CULL; edit.cullbackfaces=FALSE;
    assert(!BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && !changed);
    CheckModes(&doc,refs,states,20);
    EditHistoryFree(&history); BgDocumentFree(&doc);
    assert(BgDocumentClone(&original,&doc,&why));
    /* Unsupported state in a second room rejects the first room's edit too. */
    BgDocumentDrawGroup *custom=&doc.rooms[2].layers[0].groups[0];
    Put(custom->commands+20,0x0c184dd8); /* Explicit translucent decal. */
    selected[4]=refs[1];
    assert(!BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && !changed && !doc.dirty);
    assert(BgDocumentGetFaceRenderStates(&doc,refs,20,states));
    assert(BgRenderGetTransparency(&states[1])==BG_TRANSPARENCY_BLEND);
    assert(BgDocumentFindFace(&doc,&refs[1],NULL)->cullbackfaces);
    BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    printf("PASS BG surface editing: TRI4 isolation, rooms/layers, fog and RDP sync, native save/reload/export, undo/redo, no-op and %d allocation failures.\n",failures);
    return 0;
}
