#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "doorshadow.h"
#include "bghistory.h"
#include "setupmeta.h"

#define OK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s (%s)\n",__FILE__,__LINE__,#x,why); abort(); } } while (0)
static const char *why = "";
static void Put(unsigned char *p, DWORD value) { SetupMetaWrite32(p, value); }
static BgFile Fixture(void)
{
    BgFile bg = {0}; bg.size = 320; bg.data = calloc(bg.size, 1);
    strcpy(bg.name, "bg/bg_shadow.seg");
    Put(bg.data + 4, 0x0e000020);
    Put(bg.data + 56, 0x0e000074); Put(bg.data + 60, 0x0e0000b8);
    Put(bg.data + 112, 64);
    const short positions[4][3] = {{0,0,0},{100,0,0},{100,0,100},{0,0,100}};
    for (int i = 0; i < 4; i++) {
        unsigned char *v = bg.data + 116 + i * 16;
        for (int a = 0; a < 3; a++) { v[a*2] = positions[i][a] >> 8; v[a*2+1] = positions[i][a]; }
        // UVs intentionally are not affine across the full quad.
        short u = i == 2 ? 3000 : i == 1 ? 6400 : 0, t = i >= 2 ? 3200 : 0;
        v[8] = u >> 8; v[9] = u; v[10] = t >> 8; v[11] = t;
        memset(v+12,255,4);
    }
    const DWORD commands[] = {0xe7000000,0, 0xba001402,0x100000, 0xb900031d,0x0c1849d8,
        0xb7000000,5, 0xbb000001,0xffffffff, 0xc0000002,17, 0xfc26a004,0x1f1093ff,
        0x04300040,0x0e000000, 0xbf000000,0x000a14, 0xbf000000,0x00141e, 0xb8000000,0};
    Put(bg.data + 180, sizeof(commands));
    for (unsigned i=0; i<sizeof(commands)/4; i++) { Put(bg.data+184+i*4, commands[i]); }
    return bg;
}
static float Area(const DoorShadowPoint *p)
{
    return ((p[1].position[0]-p[0].position[0])*(p[2].position[2]-p[0].position[2])
        -(p[1].position[2]-p[0].position[2])*(p[2].position[0]-p[0].position[0]))/2;
}
static void Math(void)
{
    DoorShadowPoint p[6] = {{{0,0,0},{0,0},255,0},{{100,0,0},{6400,0},255,0},{{100,0,100},{3000,3200},255,0},
        {{0,0,0},{0,0},255,0},{{100,0,100},{3000,3200},255,0},{{0,0,100},{0,3200},255,0}}, out[18];
    for (unsigned dir=0; dir<4; dir++) for (int fraction=0; fraction<=100; fraction++) {
        doorShadowSplit(p,dir,fraction/100.0f,0xffffff,0x404040,out);
        float total=0,lit=0;
        for (int t=0; t<6; t++) {
            float area=Area(out+t*3); assert(area>=-.001f); total+=area;
            if (out[t*3].rgb==0xffffff) { lit+=area; }
            for (int c=0; c<3; c++) {
                DoorShadowPoint *v=out+t*3+c; float x=v->position[0],z=v->position[2];
                float u=t<3 ? 64*x-34*z : 30*x;
                assert(fabsf(v->uv[0]-u)<.003f && fabsf(v->uv[1]-32*z)<.003f);
                assert(v->alpha==255);
            }
        }
        assert(fabsf(total-10000)<.004f && fabsf(lit-100*fraction)<.004f);
    }
    doorShadowSplit(p,0,NAN,1,2,out); assert(out[0].rgb==2);
    doorShadowSplit(p,0,2,1,2,out); assert(out[0].rgb==1);
}
static void Refs(const BgDocument *bg, DWORD room, DWORD a, DWORD b, BgFaceRef refs[2])
{
    refs[0]=(BgFaceRef){.room=room,.faceid=bg->rooms[room].faces[a].id,.layer=bg->rooms[room].faces[a].layer};
    refs[1]=(BgFaceRef){.room=room,.faceid=bg->rooms[room].faces[b].id,.layer=bg->rooms[room].faces[b].layer};
}
static void CompilerDithering(void)
{
    BgFile source=Fixture();BgDocument bg={0};BgFaceRef refs[2];BgRenderState state;
    OK(BgDocumentLoad(source.data,source.size,1,&bg,&why));Refs(&bg,1,0,1,refs);
    OK(BgDocumentGetFaceRenderStates(&bg,refs,1,&state));
    for(int authored=0;authored<2;authored++)for(DWORD dither=0;dither<=0xf0;dither+=0x10) {
        unsigned char *gdl;DWORD size;BgRenderState draw;
        state.othermodehighknown=(state.othermodehighknown&~0xf0u)|(authored?0xf0u:0);
        state.othermodehigh=(state.othermodehigh&~0xf0u)|(authored?dither:0);
        OK(BgCompileDoorShadow(&bg.rooms[1].faces[0],&state,&gdl,&size,&why));
        BgRenderStateInit(&draw,FALSE);draw.othermodehigh=dither^0xf0;
        int tris=0;
        for(DWORD i=0;i<size;i+=8) {
            DWORD a=SetupMetaRead32(gdl+i),b=SetupMetaRead32(gdl+i+4);
            BgRenderStateRead(&draw,a,b);
            if((a>>24)==0xbf) {
                assert((draw.othermodehigh&0xf0u)==(authored?dither:(dither^0xf0)));
                tris++;
            }
        }
        assert(tris==6);free(gdl);
    }
    BgDocumentFree(&bg);BgFileFree(&source);
    puts("PASS compiler: inherited world dithering and explicitly authored dither modes.");
}
static void Conversion(const char *dir)
{
    BgFile source=Fixture(),compiled={0}; BgDocument bg={0},loaded={0}; SetupFile setup={0},saved={0};
    EditHistory h={0}; EditHistoryTransaction tx={0}; StanFile stan={0}; EditHistoryAsset asset;
    BgFaceRef refs[2]; DWORD shadow; DoorShadowProperties props; BOOL changed;
    RomFile rom={0}; rom.info.entrycount=1;
    rom.info.entries[0].kind=DOOR_SHADOW_MANIFEST_KIND; rom.info.entries[0].flags=DOOR_SHADOW_VERSION;
    OK(BgDocumentLoad(source.data,source.size,1,&bg,&why)); assert(bg.facecount==2);
    OK(SetupLoadProjectFile(dir,"UsetupshadowZ",&setup,&why)); assert(setup.objectcount==1);
    Refs(&bg,1,0,1,refs); EditHistoryReset(&h,&bg,&setup,&stan);
    OK(EditHistoryBeginBgSetupEdit(&h,&bg,&setup,"Create Door Shadow",&tx,&why));
    BgFaceRef invalid[2]={refs[0],refs[0]};
    assert(!DoorShadowCreate(&bg,&setup,invalid,&shadow,&why)); assert(bg.facecount==2&&setup.objectcount==1);
    OK(DoorShadowCreate(&bg,&setup,refs,&shadow,&why));
    assert(bg.facecount==0 && setup.objectcount==2 && shadow==1);
    OK(EditHistoryCommitEdit(&h,&bg,&setup,&stan,&tx,&why)); assert(bg.dirty&&setup.dirty&&!stan.dirty);
    OK(DoorShadowGet(&setup,shadow,&props)); assert(props.door==-1);
    DoorShadowEdit edit={shadow,DOOR_SHADOW_EDIT_DOOR,0};
    OK(DoorShadowSet(&setup,&edit,&changed,&why)); assert(changed);
    assert(SetupMetaRead32(setup.data+setup.objects[shadow].sourceoffset+DOOR_SHADOW_DOOR)==1);
    for (int field=DOOR_SHADOW_EDIT_DIRECTION;field<=DOOR_SHADOW_EDIT_DARK;field++) {
        edit.field=field; edit.value=field==DOOR_SHADOW_EDIT_DIRECTION?3:0x182838;
        OK(DoorShadowSet(&setup,&edit,&changed,&why));
    }
    edit.field=DOOR_SHADOW_EDIT_DIRECTION;edit.value=4;
    assert(!DoorShadowSet(&setup,&edit,&changed,&why));
    DoorShadowSetPreview(shadow,50); BgVertex vertices[18]; unsigned short tag; BgRenderFlags flags;
    OK(DoorShadowBuildPreview(&setup,shadow,1,vertices,&tag,&flags,&why)); assert((tag&BG_TEX_NONE)==17);
    char path[MAX_PATH]; snprintf(path,sizeof(path),"%s/shadow.bin",dir);
    FILE *file=fopen(path,"wb"); assert(file);
    assert(fwrite(setup.data+setup.objects[shadow].sourceoffset,1,DOOR_SHADOW_BYTES,file)==DOOR_SHADOW_BYTES);
    fclose(file);
    OK(SetupFileCompact(&setup,&why)); OK(DoorShadowGet(&setup,shadow,&props)); assert(props.door==0);
    OK(SetupSaveProjectFile(dir,&setup,&why)); OK(SetupLoadProjectFile(dir,"UsetupshadowZ",&saved,&why));
    OK(DoorShadowGet(&saved,shadow,&props)); assert(props.door==0&&props.direction==3&&props.light==0x182838);
    OK(DoorShadowValidateNative(saved.data,saved.size,&rom,&why));
    assert(!DoorShadowValidateNative(saved.data,saved.size,NULL,&why));
    OK(BgDocumentCompile(&bg,&source,&compiled,&why));
    OK(BgDocumentLoad(compiled.data,compiled.size,1,&loaded,&why)); assert(!loaded.facecount);
    EditHistoryMarkSetupSaved(&h,&setup);
    OK(EditHistoryUndo(&h,&bg,&setup,&stan,&asset,&why));
    assert(asset==EDIT_HISTORY_ASSET_BG_SETUP&&bg.facecount==2&&setup.objectcount==1&&!bg.dirty&&setup.dirty);
    OK(EditHistoryRedo(&h,&bg,&setup,&stan,&asset,&why));
    assert(bg.facecount==0&&setup.objectcount==2&&bg.dirty&&!setup.dirty);
    // Repeated deletion/recreation recycles the fixed setup command.
    DWORD size=setup.size;
    for (int i=0;i<20;i++) {
        unsigned char record[DOOR_SHADOW_BYTES];
        memcpy(record,setup.data+setup.objects[shadow].sourceoffset,sizeof(record));
        OK(SetupFileDeleteObject(&setup,shadow,&why));
        assert(!DoorShadowValidateNative(setup.data,setup.size,NULL,&why)); // tombstones need new runtime too
        OK(SetupFileAddDoorShadow(&setup,record,&shadow,&why)); assert(setup.size==size&&shadow==1);
    }
    OK(SetupFileDeleteObject(&setup,0,&why)); OK(DoorShadowGet(&setup,shadow,&props)); assert(props.door==-1);
    DoorShadowSetPreview((DWORD)-1,0); assert(DoorShadowGetPreview(&setup,shadow)==0);
    EditHistoryFree(&h); SetupFileFree(&setup); SetupFileFree(&saved);
    BgDocumentFree(&bg); BgDocumentFree(&loaded); BgFileFree(&source); BgFileFree(&compiled);
}
static void Depot(const char *dir,const char *path)
{
    FILE *f=fopen(path,"rb"); assert(f); fseek(f,0,SEEK_END); long size=ftell(f); rewind(f);
    unsigned char *data=malloc(size); assert(fread(data,1,size,f)==(size_t)size); fclose(f);
    BgDocument bg={0}; SetupFile setup={0}; DWORD count=0,shadow;
    OK(BgDocumentLoad(data,size,1,&bg,&why)); free(data);
    OK(SetupLoadProjectFile(dir,"UsetupdepotZ",&setup,&why));
    for(DWORD i=0;i<setup.objectcount;i++)if(setup.objects[i].type==PROPDEF_DOOR_SHADOW&&!setup.objects[i].deleted) {
        char path[MAX_PATH];snprintf(path,sizeof(path),"%s/saved-shadow-%lu.bin",dir,(unsigned long)i);
        FILE *out=fopen(path,"wb");assert(out);
        assert(fwrite(setup.data+setup.objects[i].sourceoffset,1,DOOR_SHADOW_BYTES,out)==DOOR_SHADOW_BYTES);
        fclose(out);
    }
    DWORD faces=bg.facecount,objects=setup.objectcount,door=(DWORD)-1;
    for (DWORD i=0;i<objects;i++) if (setup.objects[i].type==PROPDEF_DOOR&&!setup.objects[i].deleted) { door=i; break; }
    assert(door!=(DWORD)-1);
    // Adjacent triangles in each native room are typical two-triangle surfaces.
    for (DWORD r=1;r<=bg.roomcount&&!count;r++) for (DWORD a=0;a<bg.rooms[r].facecount&&!count;a++)
        for (DWORD b=a+1;b<bg.rooms[r].facecount&&!count;b++) {
            BgFaceRef refs[2]; Refs(&bg,r,a,b,refs);
            if (DoorShadowCreate(&bg,&setup,refs,&shadow,&why)) {
                DoorShadowEdit edit={shadow,DOOR_SHADOW_EDIT_DOOR,(LONG)door}; BOOL changed;
                OK(DoorShadowSet(&setup,&edit,&changed,&why));
                OK(SetupFileCompact(&setup,&why));
                DoorShadowProperties p; OK(DoorShadowGet(&setup,shadow,&p)); assert(p.door==(LONG)door);
                BgVertex vertices[18]; unsigned short tag; BgRenderFlags flags;
                DoorShadowSetPreview(shadow,37);
                OK(DoorShadowBuildPreview(&setup,shadow,1,vertices,&tag,&flags,&why));
                printf("Depot: converted faces %lu/%lu in room %lu, linked door %lu; %lu source faces, %lu objects.\n",
                    (unsigned long)refs[0].faceid,(unsigned long)refs[1].faceid,(unsigned long)r,
                    (unsigned long)door,(unsigned long)faces,(unsigned long)objects);
                count++;
            }
        }
    assert(count&&bg.facecount==faces-2);
    BgDocumentFree(&bg);SetupFileFree(&setup);
}
int main(int argc,char **argv)
{
    assert(argc>=2); Math(); CompilerDithering(); Conversion(argv[1]);
    if (argc>2) { Depot(argv[1],argv[2]); }
    puts("PASS: clipping in four directions, fixed UVs, endpoints, conversion, link, colors, save/reload, atomic undo/redo, deletion and command recycling.");
    return 0;
}
