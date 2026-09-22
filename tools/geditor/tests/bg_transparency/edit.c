#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
BOOL SetupFileCompact(SetupFile *setup, const char **why) { (void)setup; (void)why; abort(); }
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
    BgFaceRef *ra=malloc(a->facecount*sizeof(*ra)), *rb=malloc(b->facecount*sizeof(*rb));
    BgRenderState *sa=malloc(a->facecount*sizeof(*sa)), *sb=malloc(b->facecount*sizeof(*sb));
    assert(ra && rb && sa && sb && Refs(a,ra)==Refs(b,rb));
    assert(BgDocumentGetFaceRenderStates(a,ra,a->facecount,sa));
    assert(BgDocumentGetFaceRenderStates(b,rb,b->facecount,sb));
    for (DWORD i=0; i<a->facecount; i++) {
        assert(sa[i].surfacepolicy==sb[i].surfacepolicy);
        assert(sa[i].surfacebasemode==sb[i].surfacebasemode);
    }
    free(ra); free(rb); free(sa); free(sb);
    BgDocumentRenderMeshFree(&x); BgDocumentRenderMeshFree(&y);
}
static void RoundTrip(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile compiled={0},saved={0},again={0},packed={0}; BgDocument loaded={0}; const char *why="";
    assert(BgDocumentCompile(doc,source,&compiled,&why));
    assert(BgFileValidateVertexBatches(&compiled,&why));
    char path[MAX_PATH]; snprintf(path,sizeof(path),"%s/bg",dir); CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir,&compiled,&why));
    assert(BgLoadProjectFile(dir,compiled.name,&saved,&why));
    /* Saving now packs live allocations, including their pointer locations. */
    assert(BgFileCompact(&compiled,&packed,&why));
    assert(saved.size==packed.size && !memcmp(saved.data,packed.data,saved.size));
    BgFileFree(&packed);
    assert(BgDocumentLoad(saved.data,saved.size,doc->levelscale,&loaded,&why));
    Equivalent(doc,&loaded);
    assert(BgDocumentCompile(&loaded,&saved,&again,&why));
    assert(BgFileCompact(&again,&packed,&why));
    BgFileFree(&again); again=packed; memset(&packed,0,sizeof(packed));
    /* Unused-vertex cleanup can leave padding which the next save packs out.
       Re-saving must preserve geometry/materials and must not grow the file. */
    assert(again.size <= saved.size);
    BgDocumentFree(&loaded);
    assert(BgDocumentLoad(again.data,again.size,doc->levelscale,&loaded,&why));
    Equivalent(doc,&loaded);
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
        assert(after[i].surfacepolicy==(selected?BG_SURFACE_CUTOUT:BG_SURFACE_AUTO));
        if (selected) assert(after[i].surfacebasemode==(before[i].othermode&BG_SURFACE_MODE_MASK));
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
        assert(s.surfacepolicy==BG_SURFACE_AUTO && s.surfacebasemode==0);
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
    BgRenderStateRead(&s,BG_SURFACE_MARKER,BG_SURFACE_TAG_VALUE(BG_SURFACE_CUTOUT,s.othermode));
    BgRenderStateRead(&s,0xb900031d,mode);
    assert(BgRenderSurfacePreset(&s,BG_TRANSPARENCY_AUTO,&mode) && mode==0x005049d8);
    BgRenderStateRead(&s,BG_SURFACE_MARKER,BG_SURFACE_TAG_VALUE(BG_SURFACE_AUTO,0));
    BgRenderStateRead(&s,0xb900031d,0x005049c8); /* No Z compare. */
    assert(BgRenderSurfacePreset(&s,BG_TRANSPARENCY_OPAQUE,&mode));
    assert(!(mode&0x30));
    BgRenderStateRead(&s,0xb900031d,0x005849d8); /* Additive final blender. */
    assert(!BgRenderSurfacePreset(&s,BG_TRANSPARENCY_CUTOUT,&mode));
    BgRenderStateRead(&s,0xb900031d,0x00504dd8); /* Decal. */
    assert(BgRenderSurfacePreset(&s,BG_TRANSPARENCY_CUTOUT,&mode));
    assert(mode==0x00543d58); /* Transparency changes retain coplanar depth. */
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
static void Overrides(const char *dir)
{
    BgFile source=Fixture(); BgDocument doc={0}; BgFaceRef refs[20]; BgRenderState states[20];
    BgRenderState original[20]; const char *why=""; BOOL changed;
    assert(BgDocumentLoad(source.data,source.size,1,&doc,&why));
    assert(Refs(&doc,refs)==20 && BgDocumentGetFaceRenderStates(&doc,refs,20,original));
    BgFaceRef selected[]={refs[1],refs[3],refs[6],refs[11]};
    BgFacePropertiesEdit edit={.fields=BG_FACE_PROPERTY_TRANSPARENCY,.transparency=BG_TRANSPARENCY_BLEND};
    /* Same native blend, new explicit intent: must be a real, undoable edit. */
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why) && changed);
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why) && !changed);
    assert(BgDocumentGetFaceRenderStates(&doc,refs,20,states));
    for (int i=0; i<20; i++) {
        int selected=i==1 || i==3 || i==6 || i==11;
        assert(states[i].othermode==original[i].othermode);
        assert(states[i].surfacepolicy==(selected?BG_SURFACE_BLEND:BG_SURFACE_AUTO));
    }
    RoundTrip(&doc,&source,dir);
    /* Repeated explicit choices retain the original native mode for Auto. */
    const BgTransparency choices[]={BG_TRANSPARENCY_CUTOUT,BG_TRANSPARENCY_OPAQUE,BG_TRANSPARENCY_BLEND,BG_TRANSPARENCY_AUTO};
    for (unsigned int c=0; c<sizeof(choices)/sizeof(choices[0]); c++) {
        edit.transparency=choices[c];
        assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why) && changed);
        assert(BgDocumentGetFaceRenderStates(&doc,refs,20,states));
        for (int i=0; i<20; i++) {
            int selected=i==1 || i==3 || i==6 || i==11;
            assert(states[i].surfacepolicy==(selected && choices[c]!=BG_TRANSPARENCY_AUTO ? (DWORD)choices[c]+1 : BG_SURFACE_AUTO));
            if (!selected || choices[c]==BG_TRANSPARENCY_AUTO) assert(states[i].othermode==original[i].othermode);
            else assert(states[i].surfacebasemode==(original[i].othermode&BG_SURFACE_MODE_MASK));
        }
        RoundTrip(&doc,&source,dir);
    }
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why) && !changed);
    BgDocumentFree(&doc); BgFileFree(&source);
    puts("PASS explicit BG choices: same-mode locks, repeated overrides, Auto restoration and per-face saved metadata.");
}

static void Decals(const char *dir)
{
    /* Native AA opaque/cutout/translucent render words (PASS + second cycle)
       and the corresponding no-AA opaque surface/decal from gbi.h. */
    const DWORD surfaces[]={0x0c192078,0x0c193078,0x0c1849d8,0x0c192330};
    const DWORD decals[]={0x0c192d58,0x0c193d58,0x0c184dd8,0x0c192f10};
    for (unsigned int k=0;k<4;k++) for (unsigned int cycle=0;cycle<2;cycle++)
    {
        BgRenderState state; DWORD mode;
        BgRenderStateInit(&state,FALSE);
        BgRenderStateRead(&state,0xba001402,cycle?0x100000:0);
        /* Mirror the second blender into the first for one-cycle fixtures. */
        DWORD surface=cycle?surfaces[k]:(surfaces[k]&0xffffu)|0x00500000u|(k==2?0:0x40000u);
        DWORD decal=cycle?decals[k]:(decals[k]&0xffffu)|0x00500000u|(k==2?0:0x40000u);
        BgRenderStateRead(&state,0xb900031d,surface);
        assert(BgRenderDecalPreset(&state,TRUE,&mode) && mode==decal);
        state.othermode=mode;
        assert((BgRenderStateFlags(&state)&(BG_RENDER_DECAL|BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE))
            ==(BG_RENDER_DECAL|BG_RENDER_DEPTH_TEST));
        assert(BgRenderDecalPreset(&state,FALSE,&mode) && mode==surface);
    }
    BgFile source=Fixture(); BgDocument doc={0},original={0}; BgFaceRef refs[20];
    BgRenderState before[20],after[20]; const char *why=""; BOOL changed;
    assert(BgDocumentLoad(source.data,source.size,1,&doc,&why));
    assert(Refs(&doc,refs)==20 && BgDocumentGetFaceRenderStates(&doc,refs,20,before));
    assert(BgDocumentClone(&doc,&original,&why));
    BgFaceRef selected[]={refs[1],refs[3],refs[6],refs[11]};
    BgFacePropertiesEdit edit={.fields=BG_FACE_PROPERTY_DECAL,.decal=TRUE};
    EditHistory history={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
    SetupFile setup={0}; StanFile stan={0}; EditHistoryReset(&history,&doc,&setup,&stan);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Change BG Decal",&tx,&why));
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why) && changed);
    assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&tx,&why));
    assert(BgDocumentGetFaceRenderStates(&doc,refs,20,after));
    for (int i=0;i<20;i++)
    {
        BOOL picked=i==1 || i==3 || i==6 || i==11;
        assert(after[i].othermode==(picked?0x0c184dd8:before[i].othermode));
        assert(after[i].surfacepolicy==before[i].surfacepolicy);
        assert(after[i].othermodehigh==before[i].othermodehigh);
        assert(after[i].geometrymode==before[i].geometrymode);
    }
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why) && !changed);
    RoundTrip(&doc,&source,dir);
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,&asset,&why)); Equivalent(&doc,&original);
    assert(EditHistoryRedo(&history,&doc,&setup,&stan,&asset,&why));
    /* Switching transparency, including Auto, keeps Decal enabled. */
    const BgTransparency choices[]={BG_TRANSPARENCY_OPAQUE,BG_TRANSPARENCY_CUTOUT,
        BG_TRANSPARENCY_BLEND,BG_TRANSPARENCY_AUTO};
    edit.fields=BG_FACE_PROPERTY_TRANSPARENCY;
    for (unsigned int c=0;c<4;c++)
    {
        edit.transparency=choices[c];
        assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why));
        assert(BgDocumentGetFaceRenderStates(&doc,selected,4,after));
        for (int i=0;i<4;i++)
        {
            assert(BgRenderStateFlags(&after[i])&BG_RENDER_DECAL);
            assert(BgRenderGetSurfaceTransparency(&after[i])==(c==3?BG_TRANSPARENCY_BLEND:choices[c]));
        }
        RoundTrip(&doc,&source,dir);
    }
    /* Off restores a normal surface. Auto must not revive the decal bit
       saved before an intervening transparency override. */
    edit.transparency=BG_TRANSPARENCY_OPAQUE;
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why));
    edit.fields=BG_FACE_PROPERTY_DECAL; edit.decal=FALSE;
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why));
    edit.fields=BG_FACE_PROPERTY_TRANSPARENCY; edit.transparency=BG_TRANSPARENCY_AUTO;
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why));
    Equivalent(&doc,&original);
    RoundTrip(&doc,&source,dir);
    EditHistoryFree(&history); BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS decals: native one/two-cycle modes, mixed TRI4/room/layer selection, independent transparency, save/reload, undo/redo and Off restoration.");
}

int main(int argc, char **argv)
{
    assert(argc==3); Presets(); Jungle(argv[2],argv[1]); Overrides(argv[1]); Decals(argv[1]);
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
    Put(custom->commands+20,0x0c1a4dd8); /* Decal with an unsupported custom final blender. */
    selected[4]=refs[1];
    assert(!BgDocumentSetFaceProperties(&doc,selected,5,&edit,&changed,&why) && !changed && !doc.dirty);
    assert(BgDocumentGetFaceRenderStates(&doc,refs,20,states));
    assert(BgRenderGetTransparency(&states[1])==BG_TRANSPARENCY_BLEND);
    assert(BgDocumentFindFace(&doc,&refs[1],NULL)->cullbackfaces);
    BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    printf("PASS BG surface editing: TRI4 isolation, rooms/layers, fog and RDP sync, native save/reload/export, undo/redo, no-op and %d allocation failures.\n",failures);
    return 0;
}
