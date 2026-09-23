#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
BOOL SetupFileCompact(SetupFile *setup, const char **why) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
#include "fixture.inc"
#include "comparison.inc"

static void PreviewNative(void)
{
    const DWORD modes[]={0x0c192078u,0x0c182078u,0x0c192d58u,0x0c184dd8u,
        0x0c1849d8u,0x0c193078u,0x0c192048u,0x0c182048u};
    BgMaterial material; BgRenderState state;
    BgMaterialInit(&material);
    for(unsigned int i=0;i<sizeof(modes)/sizeof(*modes);i++) {
        BgRenderStateInit(&state,FALSE);
        BgRenderStateRead(&state,0xba001402u,0x100000u);
        BgRenderStateRead(&state,0xb900031du,modes[i]);
        assert(BgRenderUsesFog(&state,&material));
        BgRenderStateRead(&state,0xb900030du,0x3078u);
        assert(BgRenderUsesFog(&state,&material));
        BgRenderStateRead(&state,0xb6000000u,0x10000u);
        assert(!BgRenderUsesFog(&state,&material));
    }
    BgRenderStateInit(&state,FALSE);
    BgRenderStateRead(&state,0xba001402u,0);
    BgRenderStateRead(&state,0xb900031du,0x00502078u);
    assert(!BgRenderUsesFog(&state,&material));
    material.fog=BG_FOG_ON;
    assert(BgRenderUsesFog(&state,&material) && BgRenderSupportsFog(&state,&material));
    /* An unrecognized RGB equation cannot be silently rewritten for fog. */
    material.combineword0^=1;
    assert(!BgRenderSupportsFog(&state,&material));
    material.combineword0^=1;
    material.combineword1^=1;
    assert(!BgRenderSupportsFog(&state,&material));
    material.alphasource=BG_ALPHA_CONSTANT;
    assert(BgRenderSupportsFog(&state,&material));
    BgMaterialSetTexture(&material,1508);
    assert(!BgRenderSupportsFog(&state,&material));
    material.fog=BG_FOG_AUTO;
    assert(BgRenderSupportsFog(&state,&material));
}

static void RoundTrip(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile compiled={0}, saved={0}, next={0}, packed={0}; BgDocument loaded={0}; const char *why="";
    assert(BgDocumentCompile(doc,source,&compiled,&why));
    assert(BgFileValidateVertexBatches(&compiled,&why));
    char path[MAX_PATH]; snprintf(path,sizeof(path),"%s/bg",dir); CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir,&compiled,&why));
    assert(BgLoadProjectFile(dir,compiled.name,&saved,&why));
    for (int iteration=0; iteration<20; iteration++) {
        assert(BgDocumentLoad(saved.data,saved.size,doc->levelscale,&loaded,&why));
        EquivalentDocuments(doc,&loaded);
        assert(BgDocumentCompile(&loaded,&saved,&next,&why));
        assert(BgFileCompact(&next,&packed,&why));
        assert(packed.size<=saved.size);
        BgFileFree(&next); BgFileFree(&saved); saved=packed; memset(&packed,0,sizeof(packed));
        BgDocumentFree(&loaded);
    }
    BgFileFree(&compiled); BgFileFree(&saved);
}

static void Synthetic(const char *dir)
{
    BgFile source=Fixture(); BgDocument doc={0}, before={0}; const char *why=""; BOOL changed;
    BgFaceRef refs[20], selected[4]; BgRenderState states[20], original[20];
    EditHistory history={0}; EditHistoryTransaction tx={0}; SetupFile setup={0}; StanFile stan={0};
    assert(BgDocumentLoad(source.data,source.size,1,&doc,&why)); assert(Refs(&doc,refs)==20);
    assert(BgDocumentClone(&doc,&before,&why));
    assert(BgDocumentGetFaceRenderStates(&doc,refs,20,original));
    selected[0]=refs[1]; selected[1]=refs[3]; selected[2]=refs[9]; selected[3]=refs[15];
    BgFacePropertiesEdit edit={.fields=BG_FACE_PROPERTY_FOG,.fog=BG_FOG_OFF};
    EditHistoryReset(&history,&doc,NULL,NULL);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Fog",&tx,&why));
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why)&&changed);
    assert(EditHistoryCommitEdit(&history,&doc,NULL,NULL,&tx,&why));
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,NULL,&why)); EquivalentDocuments(&before,&doc);
    assert(EditHistoryRedo(&history,&doc,&setup,&stan,NULL,&why));
    for (DWORD policy=0;policy<=BG_FOG_OFF;policy++) {
        edit.fog=policy;
        assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why));
        assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why)&&!changed);
        assert(BgDocumentGetFaceRenderStates(&doc,refs,20,states));
        BgDocumentRenderMesh mesh={0}; assert(BgDocumentBuildRenderMesh(&doc,&mesh,&why));
        for(DWORD i=0;i<20;i++) {
            BOOL chosen=i==1||i==3||i==9||i==15;
            const BgDocumentFace *f=BgDocumentFindFace(&doc,&refs[i],NULL);
            BgMaterial expected=BgDocumentFindFace(&before,&refs[i],NULL)->material;
            expected.fog=chosen?policy:BG_FOG_AUTO;
            assert(BgMaterialEqual(&f->material,&expected));
            assert(states[i].othermode==original[i].othermode && states[i].othermodehigh==original[i].othermodehigh);
            assert(states[i].environmentword1==original[i].environmentword1 && states[i].geometrymode==original[i].geometrymode);
            assert(!!(mesh.renderflags[i]&BG_RENDER_NO_FOG)==(chosen&&policy==BG_FOG_OFF));
        }
        BgDocumentRenderMeshFree(&mesh); RoundTrip(&doc,&source,dir);
    }
    edit.fog=BG_FOG_ON;
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why));
    edit.fields=BG_FACE_PROPERTY_ALPHA_SOURCE; edit.alphasource=BG_ALPHA_VERTEX;
    assert(!BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why)&&!changed);
    edit.fields|=BG_FACE_PROPERTY_FOG; edit.fog=BG_FOG_OFF;
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why)&&changed);
    edit.fields=BG_FACE_PROPERTY_FOG; edit.fog=BG_FOG_ON;
    assert(!BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why)&&!changed);
    edit.fields|=BG_FACE_PROPERTY_ALPHA_SOURCE|BG_FACE_PROPERTY_OPACITY;
    edit.alphasource=BG_ALPHA_CONSTANT; edit.opacity=99;
    assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why)&&changed);
    {
        BgDocument clip={0}; BgFaceRef *pasted; DWORD n; const double offset[3]={0,10,0};
        assert(BgDocumentCopyFaces(&doc,selected,4,&clip,&why));
        assert(BgDocumentPasteFaces(&doc,&clip,offset,&pasted,&n,&why)&&n==4);
        for(DWORD i=0;i<n;i++) assert(BgDocumentFindFace(&doc,&pasted[i],NULL)->material.fog==BG_FOG_ON);
        RoundTrip(&doc,&source,dir); free(pasted); BgDocumentFree(&clip);
    }
    edit.fields=BG_FACE_PROPERTY_FOG; edit.fog=3;
    assert(!BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why)&&!changed);
    for (int iteration=0;iteration<100;iteration++) {
        edit.fog=iteration%3;
        assert(BgDocumentSetFaceProperties(&doc,selected,4,&edit,&changed,&why));
    }
    RoundTrip(&doc,&source,dir);
    EditHistoryFree(&history); BgDocumentFree(&before); BgDocumentFree(&doc); BgFileFree(&source);
    puts("PASS fog: mixed rooms/layers, preview, unchanged alpha/RGB/transparency, conflicts, history, copy/paste and stable saves.");
}

static void Runway(const char *path,const char *dir,BOOL attached)
{
    BgFile source={0}; BgDocument doc={0}, original={0}; const char *why=""; BOOL changed;
    FILE *fp=fopen(path,"rb"); assert(fp); fseek(fp,0,SEEK_END); source.size=ftell(fp); rewind(fp);
    source.data=malloc(source.size); assert(source.data&&fread(source.data,1,source.size,fp)==source.size); fclose(fp);
    strcpy(source.name,"bg/fog_runway.seg");
    assert(BgDocumentLoad(source.data,source.size,1,&doc,&why));
    assert(BgDocumentClone(&doc,&original,&why));
    BgFaceRef *refs=malloc(doc.facecount*sizeof(*refs)); BgRenderState *states=malloc(doc.facecount*sizeof(*states));
    assert(refs&&states&&Refs(&doc,refs)==doc.facecount);
    assert(BgDocumentGetFaceRenderStates(&doc,refs,doc.facecount,states));
    BgFaceRef selected[3]; DWORD n=0;
    for(DWORD i=0;i<doc.facecount&&n<3;i++) {
        const BgDocumentFace *f=BgDocumentFindFace(&doc,&refs[i],NULL);
        BOOL choose=attached?(f->id>=1609&&f->id<=1611):
            (f->textureid==0x46a && !(states[i].othermodehigh&0x300000u));
        if(!choose)continue;
        assert(!BgRenderUsesFog(&states[i],&f->material));
        assert(f->material.combineword0==0xfc127e24u && f->material.combineword1==0xfffff9fcu);
        selected[n++]=refs[i];
    }
    assert(n==3);
    BgFacePropertiesEdit edit={.fields=BG_FACE_PROPERTY_FOG,.fog=BG_FOG_ON};
    assert(BgDocumentSetFaceProperties(&doc,selected,n,&edit,&changed,&why)&&changed);
    BgDocumentRenderMesh mesh={0}; assert(BgDocumentBuildRenderMesh(&doc,&mesh,&why));
    DWORD fogged=0;
    for(DWORD i=0;i<mesh.facecount;i++) {
        const BgDocumentFace *f=BgDocumentFindFace(&doc,&mesh.facerefs[i],NULL);
        if(f->material.fog==BG_FOG_ON) {assert(!(mesh.renderflags[i]&BG_RENDER_NO_FOG));fogged++;}
    }
    assert(fogged==3); BgDocumentRenderMeshFree(&mesh);
    RoundTrip(&doc,&source,dir);
    edit.fog=BG_FOG_AUTO;
    assert(BgDocumentSetFaceProperties(&doc,selected,n,&edit,&changed,&why)&&changed);
    EquivalentDocuments(&doc,&original);
    printf("PASS Runway%s: faces %u/%u/%u gain fog, retain authored one-cycle materials, and restore Auto exactly.\n",attached?" attachment":"",selected[0].faceid,selected[1].faceid,selected[2].faceid);
    free(refs);free(states);BgDocumentFree(&original);BgDocumentFree(&doc);BgFileFree(&source);
}
int main(int argc,char **argv)
{
    assert(argc>=3); PreviewNative(); Synthetic(argv[1]); Runway(argv[2],argv[1],FALSE);
    for(int i=3;i<argc;i++) Runway(argv[i],argv[1],TRUE);
    return 0;
}
