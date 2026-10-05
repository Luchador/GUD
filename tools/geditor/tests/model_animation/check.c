#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modelanimation.h"
#include "modelcompile.h"
#include "idleposes.h"

static const char *root,*work,*why="";
static BOOL packed;
#define OK(x) do { if (!(x)) { fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why);abort(); } } while(0)
static unsigned char *Read(const char *path,DWORD *size)
{
    FILE *f=fopen(path,"rb");OK(f && !fseek(f,0,SEEK_END));long length=ftell(f);OK(length>=0);rewind(f);
    unsigned char *data=malloc(length);OK(data && fread(data,1,length,f)==(size_t)length && !fclose(f));
    *size=(DWORD)length;return data;
}
BOOL RomLoad(const char *path,RomFile *rom,const char **reason)
{
    char file[1024];DWORD headers,frames;
    snprintf(file,sizeof(file),"%s/%sheaders.bin",work,packed?"packed-":"");
    unsigned char *h=Read(file,&headers);
    snprintf(file,sizeof(file),"%s/%sframes.bin",work,packed?"packed-":"");
    unsigned char *f=Read(file,&frames);
    memset(rom,0,sizeof(*rom));rom->size=headers+frames;rom->data=malloc(rom->size);OK(rom->data);
    memcpy(rom->data,h,headers);memcpy(rom->data+headers,f,frames);free(h);free(f);
    rom->info.entrycount=2;
    rom->info.entries[0]=(RomManifestEntry){0x414e4944,0,headers,0};
    rom->info.entries[1]=(RomManifestEntry){0x414e4946,headers,headers+frames,0};
    return TRUE;
}
void RomFree(RomFile *rom) { free(rom->data); }
BOOL ModelEditsCopyNative(const char *project,const char *name,unsigned char **data,DWORD *size,const char **reason)
{
    char path[1024];snprintf(path,sizeof(path),"%s/assets/obseg/%s/%s.bin",root,name[0]=='C'?"chr":"prop",name);
    *data=Read(path,size);return TRUE;
}

static void CheckModel(const char *name,DWORD clips)
{
    ModelAnimationPreview p={0},compressed={0};ModelSource source={0};
    packed=FALSE;OK(ModelAnimationOpen(&p,"project",name,&why));OK(p.count==clips);
    packed=TRUE;OK(ModelAnimationOpen(&compressed,"project",name,&why));OK(compressed.count==p.count);
    OK(ModelReadSource(p.model,p.modelsize,&source,&why));
    BgVertex *original=malloc(source.count*3*sizeof(*original));OK(original);
    memcpy(original,source.vertices,source.count*3*sizeof(*original));
    for (DWORD c=0;c<p.count;c++)
    {
        const ModelAnimationClip *clip=&p.clips[c];
        double frames[]={0,(clip->frames-1)*0.5,clip->frames-1.0};
        for (unsigned sample=0;sample<3;sample++)
        {
            unsigned short a[45]={0},b[45]={0};float ha,hb;
            OK(ModelAnimationReadFrame(&p,clip,frames[sample],a,&ha));
            OK(ModelAnimationReadFrame(&compressed,&compressed.clips[c],frames[sample],b,&hb));
            OK(!memcmp(a,b,sizeof(a)) && ha==hb);
            if (!strcmp(clip->name,"idle_unarmed") && sample==0) OK(!memcmp(a,g_EditorPose_idle_unarmed,sizeof(a)));
            BgVertex *posed=ModelAnimationPose(&p,c,frames[sample],source.count,&why);OK(posed);
            for (DWORD i=0;i<source.count*3;i++)
                OK(isfinite(posed[i].x) && isfinite(posed[i].y) && isfinite(posed[i].z));
            free(posed);
        }
    }
    OK(!memcmp(original,source.vertices,source.count*3*sizeof(*original)));
    /* Edited topology keeps the native rig and source-order associations. */
    DWORD face=0,size;unsigned char *edited=NULL;
    OK(ModelCompileDeleteFaces(p.model,p.modelsize,&source,&face,1,&edited,&size,&why));
    ModelSource changed={0};OK(ModelReadSource(edited,size,&changed,&why));OK(changed.count==source.count-1);
    free(p.model);p.model=edited;p.modelsize=size;
    BgVertex *posed=ModelAnimationPose(&p,0,0.5,changed.count,&why);OK(posed);free(posed);
    OK(!ModelAnimationPose(&p,0,0,changed.count+1,&why));
    unsigned short angles[45];float height;
    ModelAnimationClip bad=p.clips[0];bad.width=17;OK(!ModelAnimationReadFrame(&p,&bad,0,angles,&height));
    bad=p.clips[0];bad.dataoffset=p.framesize;OK(!ModelAnimationReadFrame(&p,&bad,0,angles,&height));
    bad=p.clips[0];bad.header=p.headersize;OK(!ModelAnimationReadFrame(&p,&bad,0,angles,&height));
    OK(!ModelAnimationReadFrame(&p,&p.clips[0],NAN,angles,&height));
    free(original);ModelFreeSource(&source);ModelFreeSource(&changed);
    ModelAnimationClose(&p);ModelAnimationClose(&compressed);
    printf("PASS %s: %lu clips, packed/raw equality, poses, edited topology, malformed frames.\n",name,(unsigned long)clips);
}
int main(int argc,char **argv)
{
    OK(argc==3);root=argv[1];work=argv[2];
    CheckModel("CtrevguardZ",170);
    CheckModel("PhelicopterZ",2);
    CheckModel("PplaneZ",1);
    ModelAnimationPreview p={0};
    OK(ModelAnimationOpen(&p,"project","Pdesk1Z",&why) && !p.count);ModelAnimationClose(&p);
    OK(ModelAnimationOpen(&p,"project","GwppkZ",&why) && !p.count);ModelAnimationClose(&p);
    OK(ModelAnimationOpen(&p,"project","Cnew_staticZ",&why) && !p.count);ModelAnimationClose(&p);
    puts("PASS unsupported/static models remain editable without animation controls.");
}
