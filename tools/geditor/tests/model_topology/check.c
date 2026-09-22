#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modelcompile.h"
#include "modeledits.h"
#include "texload.h"
static const char *why="",*asset;
#define OK(x) do { if(!(x)) {fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why);abort();} } while(0)
static DWORD Get(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static short Get16(const unsigned char *p) { return (short)((p[0]<<8)|p[1]); }
static unsigned char *Read(const char *path,DWORD *size)
{ FILE *f=fopen(path,"rb");OK(f&&!fseek(f,0,SEEK_END));*size=ftell(f);rewind(f);unsigned char *p=malloc(*size);OK(p&&fread(p,1,*size,f)==*size&&!fclose(f));return p; }
BOOL TexGetProjectImageSize(const char *dir,DWORD id,int *w,int *h)
{ *w=id==0x2c6?64:32;*h=id==0x2c6?64:33;return TRUE; }
BOOL TexLoadProjectImage(const char *dir,DWORD id,TexPixel *out,int *w,int *h)
{ TexGetProjectImageSize(dir,id,w,h);memset(out,255,(size_t)*w**h*sizeof(*out));return TRUE; }
BOOL TexEncodePng(const TexPixel *p,int w,int h,unsigned char **data,DWORD *size)
{ *size=8;*data=calloc(8,1);return *data!=NULL; }
BOOL RomLoad(const char *path,RomFile *rom,const char **reason)
{ memset(rom,0,sizeof(*rom));rom->data=Read(asset,&rom->size);return TRUE; }
void RomFree(RomFile *rom) { free(rom->data); }
BOOL RomFindFile(const RomFile *rom,const char *name,DWORD *offset,DWORD *size,const char **reason)
{ *offset=0;*size=rom->size;return TRUE; }
static void Collision(const unsigned char *data,const ModelSource *s)
{
    for(DWORD l=0;l<s->listcount;l++)
    {
        const ModelSourceList *p=&s->lists[l];if(!p->pointusagepointer)continue;
        DWORD count=Get16(data+p->vertexpointer+4),nc=Get16(data+p->vertexpointer+6);
        DWORD points=Get(data+p->vertexpointer+8)&0xffffffu,links=Get(data+p->pointusagepointer)&0xffffffu;
        unsigned char *seen=calloc(count,1);OK(seen);
        for(DWORD i=0;i<nc;i++)
        {
            int v=Get16(data+points+i*16+6),steps=0;
            OK(!Get(data+points+i*16+8)&&Get16(data+points+i*16+12)==-1);
            while(v>=0)
            {
                OK((DWORD)v<count&&!seen[v]&&steps++<(int)count);seen[v]=1;
                OK(!memcmp(data+points+i*16,data+p->vertexbase+v*16,6));v=Get16(data+links+v*2);
            }
        }
        for(DWORD i=0;i<count;i++) { OK(seen[i]); }
        free(seen);
    }
}
static void Compare(const ModelSource *s,const GltfModelImport *in)
{
    OK(s->count==in->count&&s->materials.facecount==in->count);
    unsigned char *seen=calloc(in->count,1);OK(seen);
    for(DWORD f=0;f<s->count;f++)
    {
        DWORD i;for(i=0;i<in->count;i++)
        {
            if(seen[i]||s->materials.faces[f].slot!=in->materials.faces[i].slot)continue;
            BOOL same=TRUE;for(DWORD k=0;k<3;k++)
            {
                const BgVertex *a=&s->vertices[f*3+k],*b=&in->vertices[i*3+k];
                same&=fabs(a->x-round(b->x))<.01&&fabs(a->y-round(b->y))<.01&&fabs(a->z-round(b->z))<.01;
                same&=a->r==b->r&&a->g==b->g&&a->b==b->b&&a->a==b->a;
                same&=s->materials.faces[f].uv[k*2]==b->s&&s->materials.faces[f].uv[k*2+1]==b->t;
            }
            if(same)break;
        }
        OK(i<in->count);seen[i]=1;
    }
    free(seen);
}
int main(int argc,char **argv)
{
    OK(argc==5);asset=argv[1];DWORD size;unsigned char *data=Read(asset,&size);ModelSource original={0},source={0};
    GltfModelImport imported={0};
    if(!strcmp(argv[4],"reject"))
    {
        DWORD before,after;OK(!ModelEditsImport(argv[3],"PsevdoormetslideZ",argv[2],&before,&after,&why));
        OK(!ModelEditsHasUnsaved());printf("PASS: rejected unsafe topology: %s\n",why);goto done;
    }
    OK(ModelReadSource(data,size,&original,&why));OK(ModelMaterialsEnsure(&original,argv[3],&why));
    if(!strcmp(argv[4],"export"))
    { OK(GltfWriteEditableModel(argv[2],argv[3],&original,ModelDataHash(data,size),&why));goto done; }
    OK(GltfReadModelImport(argv[2],ModelDataHash(data,size),&imported,&why));
    DWORD before,after;OK(ModelEditsImport(argv[3],"PsevdoormetslideZ",argv[2],&before,&after,&why));
    OK(before==40&&after==imported.count);DWORD revision;
    OK(ModelEditsReadSource(argv[3],"PsevdoormetslideZ",&source,&revision,&why));Compare(&source,&imported);
    for(DWORD f=0;f<source.count;f++)
    {
        DWORD match=0;while(match<original.count&&original.materials.faces[match].slot!=source.materials.faces[f].slot)match++;
        OK(match<original.count&&source.flags[f]==original.flags[match]);
        OK(source.faces[f].state.othermode==original.faces[match].state.othermode);
    }
    OK(source.materials.count==2&&source.materials.slots[0].texture==0x2c6&&source.materials.slots[1].texture==0x237);
    ModelFreeSource(&source);OK(ModelEditsSave(argv[3],&why));OK(!ModelEditsHasUnsaved());ModelEditsReset();
    OK(ModelEditsReadSource(argv[3],"PsevdoormetslideZ",&source,&revision,&why));Compare(&source,&imported);ModelFreeSource(&source);
    unsigned char *replacement=NULL;DWORD length;
    OK(ModelEditsReadReplacement(argv[3],"PsevdoormetslideZ",data,size,&replacement,&length,&why)==1);
    OK(ModelReadSource(replacement,length,&source,&why));OK(source.count==imported.count);Collision(replacement,&source);
    for(DWORD l=0;l<source.listcount;l++) { OK(!(source.lists[l].offset&7)); }
    /* Model tree, bounding boxes and node identity survive; only mesh table pointers/counts change. */
    DWORD record=original.lists[0].pointer;OK(!memcmp(data,replacement,original.lists[0].vertexbase));
    OK(!memcmp(data+record+24,replacement+record+24,8));
    DWORD loads=0,vertices=0;for(DWORD l=0;l<source.listcount;l++)for(DWORD pc=source.lists[l].offset;pc<source.lists[l].end;pc+=8)
        if(replacement[pc]==4) {loads++;vertices+=(replacement[pc+1]>>4)+1;}
    printf("PASS: %lu -> %lu faces, native %lu -> %lu bytes, %lu vertex loads / %lu vertices; attributes, material slots, collision chains, save/reopen and ROM replacement.\n",
        (unsigned long)before,(unsigned long)after,(unsigned long)size,(unsigned long)length,(unsigned long)loads,(unsigned long)vertices);
    if(imported.count==8) { OK(loads<=5&&vertices<=24&&length<=size); }
    ModelFreeSource(&source);free(replacement);
    char exportpath[MAX_PATH];snprintf(exportpath,sizeof(exportpath),"%s/current.gltf",argv[3]);
    OK(ModelEditsExport(argv[3],"PsevdoormetslideZ",exportpath,&why));
    OK(ModelEditsImport(argv[3],"PsevdoormetslideZ",exportpath,&before,&after,&why)&&before==after&&!ModelEditsHasUnsaved());
    puts("PASS: rebuilt export can be reimported without a new edit or byte growth.");
    /* Re-run retopology against its own output, as when a DCC drops corner IDs. */
    for(int pass=0;pass<3;pass++)
    {
        DWORD nativeSize;const unsigned char *native=ModelEditsGetData(argv[3],"PsevdoormetslideZ",&nativeSize,&why);OK(native);
        OK(ModelEditsReadSource(argv[3],"PsevdoormetslideZ",&source,&revision,&why));
        GltfModelImport next={0};ModelMaterials ordered={0};unsigned char *compiled=NULL;DWORD compiledSize;
        OK(GltfReadModelImport(exportpath,ModelDataHash(native,nativeSize),&next,&why));
        for(DWORD i=0;i<next.count;i++)next.tags[i]=(next.tags[i]&~BG_TEX_ID_MASK)|source.materials.slots[next.materials.faces[i].slot].texture;
        OK(ModelCompileRetopology(native,nativeSize,&source,&next,argv[3],&ordered,&compiled,&compiledSize,&why));
        if(compiledSize!=ModelMaterialsNativeSize(native,nativeSize)||memcmp(compiled,native,compiledSize<ModelMaterialsNativeSize(native,nativeSize)?compiledSize:ModelMaterialsNativeSize(native,nativeSize)))
        {
            fprintf(stderr,"Repeat native bytes: %u -> %u\n",ModelMaterialsNativeSize(native,nativeSize),compiledSize);
            int shown=0;for(DWORD at=0;at<compiledSize&&at<ModelMaterialsNativeSize(native,nativeSize)&&shown<24;at++)
                if(compiled[at]!=native[at]) { fprintf(stderr," %x: %02x -> %02x\n",at,native[at],compiled[at]);shown++; }
        }
        OK(compiledSize==ModelMaterialsNativeSize(native,nativeSize)&&!memcmp(compiled,native,compiledSize));
        ModelMaterialsFree(&ordered);GltfFreeModelImport(&next);ModelFreeSource(&source);free(compiled);
    }
    puts("PASS: repeated retopology is byte-stable.");
done:
    ModelEditsReset();GltfFreeModelImport(&imported);ModelFreeSource(&original);free(data);return 0;
}
