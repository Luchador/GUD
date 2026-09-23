#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
{ *w=32;*h=texture>=0x7db && texture<=0x7e8?48:32;return TRUE; }
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
{ *offset=0;*size=rom->size-16;return !strcmp(model,name); }

static void CheckEdit(const ModelSource *before,const unsigned char *base,ModelLod lod,BOOL shared)
{
    ModelSource after={0};DWORD revision,size;
    OK(ModelEditsReadSource(project,name,&after,&revision,&why));
    const unsigned char *data=ModelEditsGetData(project,name,&size,&why);OK(data);
    OK(after.count==before->count && after.haslods==before->haslods);
    unsigned char *used=calloc(after.materials.count,1);OK(used);
    DWORD clearslot=0xffffffffu;
    for (DWORD face=0;face<before->count;face++)
    {
        BOOL clear=ModelSourceFaceInLod(before,face,lod) && (shared || !before->haslods || lod==MODEL_LOD_ALL
            || !(before->faces[face].closest && before->faces[face].farthest));
        DWORD slot=after.materials.faces[face].slot;OK(slot<after.materials.count);used[slot]=1;
        OK(before->faces[face].closest==after.faces[face].closest);
        OK(before->faces[face].farthest==after.faces[face].farthest);
        OK(before->faces[face].normalmask==after.faces[face].normalmask);
        OK(!memcmp(before->materials.faces[face].uv,after.materials.faces[face].uv,sizeof(after.materials.faces[face].uv)));
        if (clear)
        {
            OK(BG_TEX_ID(after.tags[face])==BG_TEX_NONE);
            OK(BgMaterialTextureId(&after.faces[face].material)==BG_TEX_NONE);
            OK(after.materials.slots[slot].texture==BG_TEX_NONE);
            if (clearslot==0xffffffffu) clearslot=slot;
            OK(clearslot==slot);
        }
        else
        {
            OK(before->tags[face]==after.tags[face] && before->flags[face]==after.flags[face]);
            OK(BgMaterialEqual(&before->faces[face].material,&after.faces[face].material));
            OK(!memcmp(&before->materials.slots[before->materials.faces[face].slot],
                &after.materials.slots[slot],sizeof(ModelMaterialSlot)));
        }
        for (DWORD k=0;k<3;k++)
        {
            DWORD corner=face*3+k;
            OK(!memcmp(&before->vertices[corner].x,&after.vertices[corner].x,3*sizeof(float)));
            OK(!memcmp(base+before->vertexoffsets[corner],data+after.vertexoffsets[corner],16));
        }
    }
    for (DWORD slot=0;slot<after.materials.count;slot++) OK(used[slot]);
    /* Fully untextured native lists must not retain any texture-load markers. */
    for (DWORD list=0;list<after.listcount;list++)
    {
        BOOL any=FALSE,textured=FALSE;
        for (DWORD face=0;face<after.count;face++) if (after.faces[face].list==list)
        { any=TRUE;textured|=BG_TEX_ID(after.tags[face])!=BG_TEX_NONE; }
        if (any && !textured)
            for (DWORD pc=after.lists[list].offset;pc<after.lists[list].end;pc+=8) OK(data[pc]!=0xc0);
    }
    free(used);ModelFreeSource(&after);
}

int main(int argc,char **argv)
{
    OK(argc==4);asset=argv[1];project=argv[2];name=argv[3];
    DWORD basesize,revision,changed;unsigned char *base=Read(asset,&basesize);
    ModelSource original={0};OK(ModelEditsReadSource(project,name,&original,&revision,&why));
    DWORD high=0,low=0,shared=0;
    for (DWORD face=0;face<original.count;face++)
    {
        high+=ModelSourceFaceInLod(&original,face,MODEL_LOD_HIGH);
        low+=ModelSourceFaceInLod(&original,face,MODEL_LOD_LOW);
        shared+=original.faces[face].closest && original.faces[face].farthest;
    }
    if (name[0]=='C') OK(original.haslods && high==319 && low==174 && shared==58 && original.count==435);
    else OK(!original.haslods && high==original.count && low==high);
    OK(!ModelSourceFaceInLod(&original,original.count,MODEL_LOD_LOW));
    printf("%s: high %lu, low %lu, shared %lu\n",name,(unsigned long)high,(unsigned long)low,(unsigned long)shared);
    for (int include=0;include<2;include++)
    {
        ModelUVChange step={0};
        OK(ModelEditsMakeUntextured(project,name,revision,MODEL_LOD_LOW,include,&step,&changed,&why));
        OK(step.before && step.after && changed==low-(!include && original.haslods?shared:0));
        CheckEdit(&original,base,MODEL_LOD_LOW,include);
        OK(!ModelEditsMakeUntextured(project,name,revision,MODEL_LOD_LOW,include,NULL,NULL,&why));
        OK(ModelEditsRestoreUVs(project,name,&step,FALSE,&why));
        DWORD size;const unsigned char *data=ModelEditsGetData(project,name,&size,&why);
        OK(data && size==basesize && !memcmp(data,base,size));
        OK(ModelEditsRestoreUVs(project,name,&step,TRUE,&why));
        CheckEdit(&original,base,MODEL_LOD_LOW,include);
        OK(ModelEditsSave(project,&why));ModelEditsReset();
        unsigned char *replacement=NULL;DWORD length;
        OK(ModelEditsReadReplacement(project,name,base,basesize,&replacement,&length,&why)==1);
        ModelSource exported={0};OK(ModelReadSource(replacement,length,&exported,&why));
        OK(exported.count==original.count && exported.materials.count==0);
        for (DWORD f=0;f<exported.count;f++)
            if (ModelSourceFaceInLod(&original,f,MODEL_LOD_LOW)
                && (include || !original.haslods || !original.faces[f].closest)) OK(BG_TEX_ID(exported.tags[f])==BG_TEX_NONE);
        free(replacement);ModelFreeSource(&exported);
        CheckEdit(&original,base,MODEL_LOD_LOW,include);
        ModelUVChange noop={0};
        OK(ModelEditsMakeUntextured(project,name,step.afterRevision,MODEL_LOD_LOW,include,&noop,&changed,&why));
        OK(!changed && !noop.before && !ModelEditsHasUnsaved());
        OK(ModelEditsRestoreUVs(project,name,&step,FALSE,&why));
        ModelEditsFreeUVChange(&step);
    }
    ModelUVChange all={0};
    OK(ModelEditsMakeUntextured(project,name,revision,MODEL_LOD_ALL,FALSE,&all,&changed,&why));
    OK(changed==original.count);CheckEdit(&original,base,MODEL_LOD_ALL,FALSE);
    ModelSource final={0};DWORD finalrevision;
    OK(ModelEditsReadSource(project,name,&final,&finalrevision,&why));OK(final.materials.count==1);
    OK(!ModelEditsMakeUntextured(project,name,finalrevision,(ModelLod)99,TRUE,NULL,NULL,&why));
    ModelFreeSource(&final);ModelFreeSource(&original);ModelEditsFreeUVChange(&all);free(base);ModelEditsReset();
    puts("PASS: LOD isolation, shared faces, geometry/colors/UVs, material cleanup, undo/redo, save and native ROM replacement.");
    return 0;
}
