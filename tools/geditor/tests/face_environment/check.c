#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bgdocument.h"
#include "bghistory.h"
BOOL SetupFileCompact(SetupFile *s,const char **w) { abort(); }
void SetupFileFree(SetupFile *s) { abort(); }
void StanFileFree(StanFile *s) { abort(); }
#include "fixture.inc"
#include "comparison.inc"
#include "viewport.c"
static DWORD Read(const unsigned char *p) { return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static void Write(FILE *f,DWORD n) { unsigned char b[4];Put(b,n);assert(fwrite(b,1,4,f)==4); }

static void Export(const BgDocument *doc,const BgFile *file,const char *path)
{
    FILE *fp=fopen(path,"ab"); assert(fp);
    DWORD table=Read(file->data+4)&0xffffffu;
    for(DWORD r=1;r<=doc->roomcount;r++) {
        DWORD v=Read(file->data+table+r*24)&0xffffffu, vs=Read(file->data+v-4);
        for(DWORD l=0;l<2;l++) {
            DWORD p=Read(file->data+table+r*24+4+l*4)&0xffffffu;
            if(!p)continue;
            DWORD size=Read(file->data+p-4), mapped=0;
            for(DWORD i=0;i<doc->rooms[r].facecount;i++)
                if(doc->rooms[r].faces[i].layer==l&&BG_ENV_GENERATED(doc->rooms[r].faces[i].material.environment))mapped++;
            Write(fp,l); Write(fp,mapped); Write(fp,vs); Write(fp,size);
            assert(fwrite(file->data+v,1,vs,fp)==vs);
            assert(fwrite(file->data+p,1,size,fp)==size);
        }
    }
    fclose(fp);
}

static void Check(BgFile *source,const char *path)
{
    BgDocument doc={0}, original={0}, loaded={0};const char *why=""; BOOL changed;
    assert(BgDocumentLoad(source->data,source->size,1,&doc,&why));
    assert(BgDocumentClone(&doc,&original,&why));
    BgFaceRef *refs=malloc(doc.facecount*sizeof(*refs));assert(refs&&Refs(&doc,refs)==doc.facecount);
    BgFaceRef chosen[4];DWORD n=0;
    BgRenderState *states=malloc(doc.facecount*sizeof(*states));assert(states);
    assert(BgDocumentGetFaceRenderStates(&doc,refs,doc.facecount,states));
    /* Choose ordinary static textured faces in both passes and rooms. */
    for(DWORD pass=0;pass<2;pass++)for(DWORD i=0;i<doc.facecount && n<(pass+1)*2;i++) {
        const BgDocumentFace *f=BgDocumentFindFace(&doc,&refs[i],NULL);
        if(f->layer!=pass||f->textureid==BG_TEX_NONE)continue;
        BgFacePropertiesEdit e={.fields=BG_FACE_PROPERTY_ENVIRONMENT,.environment=pass?BG_ENV_LINEAR:BG_ENV_SPHERICAL};
        if(BgDocumentSetFaceProperties(&doc,&refs[i],1,&e,&changed,&why)&&changed)chosen[n++]=refs[i];
    }
    assert(n==4);
    BgDocumentRenderMesh mesh={0};assert(BgDocumentBuildRenderMesh(&doc,&mesh,&why));
    DWORD mapped=0;
    for(DWORD i=0;i<mesh.facecount;i++) {
        const BgDocumentRoom *room;const BgDocumentFace *f=BgDocumentFindFace(&doc,&mesh.facerefs[i],&room);
        if(!BG_ENV_GENERATED(f->material.environment))continue;
        mapped++;assert(mesh.renderflags[i]&BG_RENDER_ENVIRONMENT);
        assert(!!(mesh.renderflags[i]&BG_RENDER_ENVIRONMENT_LINEAR)==(f->material.environment==BG_ENV_LINEAR));
        for(int c=0;c<3;c++) {
            BgVertex *v=&mesh.vertices[i*3+c];assert(v->r==255&&v->g==255&&v->b==255);
            assert(v->environment.scale[0]==-1&&v->environment.scale[1]==-1);
            BgEnvironmentVertex e=v->environment;e.scale[0]=e.scale[1]=1;
            float right[3]={1,0,0},up[3]={0,1,0},uv[2];
            BgRenderEnvironmentCoordinates(&e,mesh.renderflags[i],right,up,uv);
            assert(uv[0]>=0&&uv[0]<=1&&uv[1]>=0&&uv[1]<=1);
        }
    }
    assert(mapped==4);BgDocumentRenderMeshFree(&mesh);
    /* Undo/redo retains material metadata. */
    EditHistory h={0};EditHistoryTransaction tx={0};SetupFile setup={0};StanFile stan={0};
    EditHistoryReset(&h,&doc,NULL,NULL);
    BgFacePropertiesEdit edit={.fields=BG_FACE_PROPERTY_ENVIRONMENT,.environment=BG_ENV_OFF};
    assert(EditHistoryBeginBgEdit(&h,&doc,"Mapping",&tx,&why));
    assert(BgDocumentSetFaceProperties(&doc,chosen,n,&edit,&changed,&why)&&changed);
    assert(EditHistoryCommitEdit(&h,&doc,NULL,NULL,&tx,&why));
    assert(EditHistoryUndo(&h,&doc,&setup,&stan,NULL,&why));
    assert(EditHistoryRedo(&h,&doc,&setup,&stan,NULL,&why));
    assert(EditHistoryUndo(&h,&doc,&setup,&stan,NULL,&why));EditHistoryFree(&h);
    BgFile saved={0},next={0},packed={0};assert(BgDocumentCompile(&doc,source,&saved,&why));
    DWORD limit=saved.size;
    for(int i=0;i<20;i++) {
        assert(BgFileValidateVertexBatches(&saved,&why));
        assert(BgDocumentLoad(saved.data,saved.size,1,&loaded,&why));EquivalentDocuments(&doc,&loaded);
        assert(BgDocumentCompile(&loaded,&saved,&next,&why));BgDocumentFree(&loaded);
        assert(BgFileCompact(&next,&packed,&why));BgFileFree(&next);next=packed;memset(&packed,0,sizeof(packed));
        BgFileFree(&saved);saved=next;memset(&next,0,sizeof(next));
        assert(saved.size<=limit);limit=saved.size;
    }
    /* Run the same cleanup passes used by ROM export. */
    assert(BgFileRemoveUnusedVertices(&saved,&next,&why));if(next.data){BgFileFree(&saved);saved=next;memset(&next,0,sizeof(next));}
    assert(BgFileOptimize(&saved,&next,&why));if(next.data){BgFileFree(&saved);saved=next;memset(&next,0,sizeof(next));}
    assert(BgFileCompact(&saved,&next,&why));BgFileFree(&saved);saved=next;memset(&next,0,sizeof(next));
    assert(BgDocumentLoad(saved.data,saved.size,1,&loaded,&why));EquivalentDocuments(&doc,&loaded);BgDocumentFree(&loaded);
    Export(&doc,&saved,path);
    edit.environment=BG_ENV_AUTO;assert(BgDocumentSetFaceProperties(&doc,chosen,n,&edit,&changed,&why)&&changed);
    EquivalentDocuments(&original,&doc);
    edit.environment=4;assert(!BgDocumentSetFaceProperties(&doc,chosen,n,&edit,&changed,&why)&&!changed);
    free(refs);free(states);BgDocumentFree(&doc);BgDocumentFree(&original);BgFileFree(&saved);
    puts("PASS environment: both passes, preview, undo/redo, exact Auto, 20 saves and ROM export cleanup.");
}

static void Policies(const char *path)
{
    BgFile source=Fixture(); const char *why="";BOOL changed;
    for(DWORD mode=BG_ENV_OFF;mode<=BG_ENV_LINEAR;mode++) {
        BgDocument doc={0},before={0},clip={0};BgFile saved={0};BgFaceRef refs[20];
        assert(BgDocumentLoad(source.data,source.size,1,&doc,&why));assert(Refs(&doc,refs)==20);
        BgFaceRef selected[2]={refs[1],refs[7]};
        BgFacePropertiesEdit edit={.fields=BG_FACE_PROPERTY_ENVIRONMENT|BG_FACE_PROPERTY_FOG
            |BG_FACE_PROPERTY_ALPHA_SOURCE|BG_FACE_PROPERTY_OPACITY,
            .environment=mode,.fog=mode==BG_ENV_LINEAR?BG_FOG_ON:BG_FOG_OFF,
            .alphasource=BG_ALPHA_CONSTANT,.opacity=99};
        assert(BgDocumentSetFaceProperties(&doc,selected,2,&edit,&changed,&why)&&changed);
        assert(BgDocumentSetFaceProperties(&doc,selected,2,&edit,&changed,&why)&&!changed);
        assert(BgDocumentCopyFaces(&doc,selected,2,&clip,&why));
        BgFaceRef *pasted=NULL;DWORD count=0;const double offset[3]={20,30,40};
        assert(BgDocumentPasteFaces(&doc,&clip,offset,&pasted,&count,&why)&&count==2);
        for(DWORD i=0;i<count;i++)assert(BgDocumentFindFace(&doc,&pasted[i],NULL)->material.environment==mode);
        assert(BgDocumentCompile(&doc,&source,&saved,&why));Export(&doc,&saved,path);BgFileFree(&saved);
        if(BG_ENV_GENERATED(mode)) {
            assert(BgDocumentClone(&doc,&before,&why));
            assert(!BgDocumentSetFaceTexture(&doc,selected,2,1508,&changed,&why)&&!changed);
            EquivalentDocuments(&before,&doc);BgDocumentFree(&before);
            assert(BgDocumentSetFaceTexture(&doc,&selected[1],1,BG_TEX_NONE,&changed,&why)&&changed);
            assert(BgDocumentFindFace(&doc,&selected[1],NULL)->material.environment==BG_ENV_AUTO);
            assert(BgDocumentClone(&doc,&before,&why));
            edit.fields=BG_FACE_PROPERTY_ENVIRONMENT;edit.environment=mode==BG_ENV_LINEAR?BG_ENV_SPHERICAL:BG_ENV_LINEAR;
            assert(!BgDocumentSetFaceProperties(&doc,selected,2,&edit,&changed,&why)&&!changed);
            EquivalentDocuments(&before,&doc);BgDocumentFree(&before);
        }
        free(pasted);BgDocumentFree(&clip);BgDocumentFree(&doc);
    }
    BgFileFree(&source);
    puts("PASS environment policies: Off/Spherical/Linear, fog/opacity coexistence, copy/paste and atomic texture validation.");
}
int main(int argc,char **argv)
{
    assert(argc==3);PreviewEdits();Policies(argv[1]);BgFile fixture=Fixture();Check(&fixture,argv[1]);BgFileFree(&fixture);
    FILE *f=fopen(argv[2],"rb");assert(f);fseek(f,0,SEEK_END);fixture.size=ftell(f);rewind(f);
    fixture.data=malloc(fixture.size);assert(fixture.data&&fread(fixture.data,1,fixture.size,f)==fixture.size);fclose(f);
    Check(&fixture,argv[1]);BgFileFree(&fixture);return 0;
}
