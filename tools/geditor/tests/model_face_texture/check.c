#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "modelcompile.h"
#include "modeledits.h"
#include "newprops.h"
#include "texload.h"

static const char *why = "", *asset, *project, *name;
#define OK(x) do { if (!(x)) { fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why);abort(); } } while(0)
static unsigned char *Read(const char *path, DWORD *size)
{
    FILE *f=fopen(path,"rb");OK(f&&!fseek(f,0,SEEK_END));*size=ftell(f);rewind(f);
    unsigned char *data=malloc(*size);OK(data&&fread(data,1,*size,f)==*size&&!fclose(f));return data;
}
BOOL TexGetProjectImageSize(const char *dir,DWORD texture,int *w,int *h)
{ *w=texture==0x2c6?64:32;*h=texture==0x237?33:32;return texture!=0xbad; }
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
    if (!strcmp(name,"PcustomZ"))
    {
        DWORD oldsize=rom->size;rom->data=realloc(rom->data,0x200000);OK(rom->data);
        memset(rom->data+oldsize,0,0x200000-oldsize);rom->size=rom->info.size=0x200000;
        rom->info.manifestoffset=0x1000;
    }
    return TRUE;
}
void RomFree(RomFile *rom) { free(rom->data); }
BOOL RomFindFile(const RomFile *rom,const char *model,DWORD *offset,DWORD *size,const char **reason)
{ *offset=0;*size=rom->size-16;return strcmp(model,"PcustomZ") && !strcmp(model,name); }

static void CheckFaces(const ModelSource *before,const unsigned char *base,
    const DWORD *faces,DWORD count,DWORD texture)
{
    ModelSource after={0};DWORD revision,size;
    OK(ModelEditsReadSource(project,name,&after,&revision,&why));
    const unsigned char *data=ModelEditsGetData(project,name,&size,&why);OK(data);
    OK(after.count==before->count && after.haslods==before->haslods && after.root==before->root);
    for (DWORD f=0;f<after.count;f++)
    {
        BOOL selected=FALSE;
        for (DWORD j=0;j<count;j++) selected|=faces[j]==f;
        DWORD target=selected?texture:BG_TEX_ID(before->tags[f]);
        OK(BG_TEX_ID(after.tags[f])==target);
        OK(BgMaterialTextureId(&after.faces[f].material)==target);
        OK(after.materials.slots[after.materials.faces[f].slot].texture==target);
        OK(before->faces[f].closest==after.faces[f].closest && before->faces[f].farthest==after.faces[f].farthest);
        OK(before->faces[f].normalmask==after.faces[f].normalmask);
        OK(!memcmp(before->materials.faces[f].uv,after.materials.faces[f].uv,6*sizeof(float)));
        OK((before->faces[f].state.geometrymode&0x3000)==(after.faces[f].state.geometrymode&0x3000));
        if (!selected)
        {
            OK(before->tags[f]==after.tags[f] && before->flags[f]==after.flags[f]);
            BgMaterial a=before->faces[f].material,b=after.faces[f].material;
            /* Shade-only faces may inherit a different unused C0 marker from
             * the preceding textured face. Their active state must match. */
            if (target==BG_TEX_NONE) a.textureword0=a.textureword1=b.textureword0=b.textureword1=0;
            OK(BgMaterialEqual(&a,&b));
            OK(!memcmp(&before->materials.slots[before->materials.faces[f].slot],
                &after.materials.slots[after.materials.faces[f].slot],sizeof(ModelMaterialSlot)));
        }
        int w=1,h=1;if (target!=BG_TEX_NONE) OK(TexGetProjectImageSize(project,target,&w,&h));
        for (DWORD k=0;k<3;k++)
        {
            DWORD corner=f*3+k;
            const unsigned char *a=base+before->vertexoffsets[corner],*b=data+after.vertexoffsets[corner];
            OK(!memcmp(a,b,8) && !memcmp(a+12,b+12,4)); /* positions, flags, normals/colors, alpha */
            if (!selected || (before->flags[f]&BG_RENDER_ENVIRONMENT)) OK(!memcmp(a,b,16));
            else if (target!=BG_TEX_NONE)
            {
                OK(fabs(after.vertices[corner].s-round(before->materials.faces[f].uv[k*2]*w*32.0)/32.0)<.00001);
                OK(fabs(after.vertices[corner].t-round(before->materials.faces[f].uv[k*2+1]*h*32.0)/32.0)<.00001);
            }
        }
    }
    ModelFreeSource(&after);
}

int main(int argc,char **argv)
{
    OK(argc==4 || argc==5);asset=argv[1];project=argv[2];name=argv[3];
    DWORD size,revision;unsigned char *base;
    if (argc==5)
    {
        DWORD count;OK(NewPropsImport(project,name,argv[4],FALSE,&count,&why));
        const unsigned char *custom=NewPropsData(project,name,&size);OK(custom);
        base=malloc(size);OK(base);memcpy(base,custom,size);
    }
    else base=Read(asset,&size);
    ModelSource original={0};OK(ModelEditsReadSource(project,name,&original,&revision,&why));
    OK(original.count>=2);
    /* A partial slot, multiple parts/LODs and a duplicate selected face. */
    DWORD faces[3]={0,original.count-1,0};
    ModelUVChange steps[3]={{0}},noop={0};
    DWORD textures[]={0x2c6,BG_TEX_NONE,0x237};
    DWORD invalid=original.count;
    OK(!ModelEditsSetFaceTexture(project,name,revision^1,faces,3,0x2c6,&noop,&why));
    OK(!ModelEditsSetFaceTexture(project,name,revision,&invalid,1,0x2c6,&noop,&why));
    OK(!ModelEditsSetFaceTexture(project,name,revision,NULL,1,0x2c6,&noop,&why));
    OK(!ModelEditsSetFaceTexture(project,name,revision,faces,3,0xbad,&noop,&why));
    OK(!ModelEditsSetFaceTexture(project,name,revision,faces,3,BG_TEX_NONE+1,&noop,&why));
    OK(!noop.before && !noop.after);
    DWORD rev=revision,previoussize=size;
    for (int pass=0;pass<3;pass++)
    {
        OK(ModelEditsSetFaceTexture(project,name,rev,faces,3,textures[pass],&steps[pass],&why));
        OK(steps[pass].before && steps[pass].after && steps[pass].beforeSize==previoussize);
        CheckFaces(&original,base,faces,3,textures[pass]);
        rev=steps[pass].afterRevision;previoussize=steps[pass].afterSize;
        OK(ModelEditsSave(project,&why));ModelEditsReset();
        const unsigned char *saved=ModelEditsGetData(project,name,&size,&why);
        OK(saved && size==steps[pass].afterSize && !memcmp(saved,steps[pass].after,size));
        OK(ModelEditsSetFaceTexture(project,name,rev,faces,3,textures[pass],&noop,&why));
        OK(!noop.before && !ModelEditsHasUnsaved());
        unsigned char *replacement=NULL;DWORD length;
        if (argc==4)
        {
            DWORD basesize;unsigned char *rombase=Read(asset,&basesize);
            OK(ModelEditsReadReplacement(project,name,rombase,basesize,&replacement,&length,&why)==1);
            OK(length==ModelMaterialsNativeSize(saved,size) && !memcmp(replacement,saved,length));
            free(rombase);
        }
        else
        {
            RomFile rom={0};OK(RomLoad("base.z64",&rom,&why));
            OK(NewPropsExportToRom(project,&rom,&why));
            /* Native custom-prop data is exercised by the actual ROM exporter. */
            RomFree(&rom);
        }
        free(replacement);
    }
    /* Repeated reassignment reuses slots and replaces metadata/vertex streams. */
    for (int pass=0;pass<8;pass++)
    {
        ModelUVChange step={0};DWORD target=pass%2?0x237:0x2c6;
        OK(ModelEditsSetFaceTexture(project,name,rev,faces,3,target,&step,&why) && step.before);
        ModelSource current={0};OK(ModelEditsReadSource(project,name,&current,&rev,&why));
        OK(current.materials.count<=original.materials.count+1);ModelFreeSource(&current);
        ModelEditsFreeUVChange(&step);
    }
    /* The last repeated assignment returns to the exact same native revision. */
    OK(rev==steps[2].afterRevision);
    for (int pass=2;pass>=0;pass--) OK(ModelEditsRestoreUVs(project,name,&steps[pass],FALSE,&why));
    const unsigned char *restored=ModelEditsGetData(project,name,&size,&why);
    OK(size==steps[0].beforeSize && !memcmp(restored,steps[0].before,size));
    for (int pass=0;pass<3;pass++) OK(ModelEditsRestoreUVs(project,name,&steps[pass],TRUE,&why));
    CheckFaces(&original,base,faces,3,textures[2]);
    for (int pass=2;pass>=0;pass--) OK(ModelEditsRestoreUVs(project,name,&steps[pass],FALSE,&why));
    DWORD *all=malloc(original.count*sizeof(*all));OK(all);
    for (DWORD i=0;i<original.count;i++) all[i]=i;
    OK(ModelEditsSetFaceTexture(project,name,revision,all,original.count,0x2c6,&noop,&why));
    ModelSource complete={0};OK(ModelEditsReadSource(project,name,&complete,&rev,&why));
    OK(complete.materials.count==original.materials.count);
    for (DWORD i=0;i<complete.materials.count;i++) OK(!strcmp(complete.materials.slots[i].name,original.materials.slots[i].name));
    ModelFreeSource(&complete);ModelEditsFreeUVChange(&noop);free(all);
    for (int pass=0;pass<3;pass++) ModelEditsFreeUVChange(&steps[pass]);
    ModelFreeSource(&original);ModelEditsReset();free(base);
    printf("PASS %s: selected-face textures, shared slots/vertices, UVs/normals/LODs, No Texture, rejected edits, no-op/repeat edits, undo/redo, save/reopen and ROM export.\n",name);
    return 0;
}
