#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modelcompile.h"
#include "texload.h"
static const char *why="";
#define OK(x) do { if(!(x)) {fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why);abort();} } while(0)
static DWORD U(const unsigned char *p) {return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3];}
static int H(const unsigned char *p) {return (short)(p[0]*256+p[1]);}
BOOL TexGetProjectImageSize(const char *dir,DWORD id,int *w,int *h) {*w=*h=32;return TRUE;}

int main(int argc,char **argv)
{
    OK(argc==3);
    FILE *f=fopen(argv[1],"rb");OK(f && !fseek(f,0,SEEK_END));
    DWORD size=ftell(f);rewind(f);unsigned char *data=malloc(size);OK(data && fread(data,1,size,f)==size);fclose(f);
    ModelSource before={0},after={0};unsigned char *out=NULL,*again=NULL;DWORD length=0,againlength=0;
    OK(ModelReadSource(data,size,&before,&why) && ModelMaterialsEnsure(&before,"",&why));
    OK(ModelCompileBodyBloodVertices(data,size,&before,&out,&length,&why));
    OK(out && ModelReadSource(out,length,&after,&why));
    OK(before.count==after.count && before.listcount==after.listcount);
    OK(before.materials.count==after.materials.count);
    OK(!memcmp(before.materials.slots,after.materials.slots,before.materials.count*sizeof(*before.materials.slots)));
    OK(!memcmp(before.materials.faces,after.materials.faces,before.count*sizeof(*before.materials.faces)));
    for(DWORD i=0;i<before.count*3;i++) {
        OK(before.vertexmatrices[i]==after.vertexmatrices[i]);
        OK(!memcmp(data+before.vertexoffsets[i],out+after.vertexoffsets[i],16));
        if(i%3==0) OK(before.tags[i/3]==after.tags[i/3] && before.flags[i/3]==after.flags[i/3]);
        DWORD owner=0;
        for(DWORD l=0;l<after.listcount;l++) {
            const ModelSourceList *p=&after.lists[l];
            DWORD n=H(out+p->vertexpointer+4),at=after.vertexoffsets[i];
            owner+=at>=p->vertexbase && at-p->vertexbase<n*16;
        }
        OK(owner==1); /* Every drawn corner can reach one instance blood buffer. */
    }
    for(DWORD l=0;l<after.listcount;l++) {
        const ModelSourceList *p=&after.lists[l];
        DWORD n=H(out+p->vertexpointer+4),nc=H(out+p->vertexpointer+6);
        DWORD points=U(out+p->vertexpointer+8)&0xffffff,links=U(out+p->pointusagepointer)&0xffffff;
        unsigned char *seen=calloc(n,1);OK(seen);
        for(DWORD j=0;j<nc;j++) {
            int index=H(out+points+j*16+6);
            while(index>=0) {
                OK((DWORD)index<n && !seen[index]);seen[index]=1;
                OK(!memcmp(out+points+j*16,out+p->vertexbase+index*16,6));
                index=H(out+links+index*2);
            }
        }
        for(DWORD j=0;j<n;j++) OK(seen[j]);
        free(seen);
    }
    OK(ModelCompileBodyBloodVertices(out,length,&after,&again,&againlength,&why));
    OK(!again && !againlength); /* Repeated export is stable. */
    f=fopen(argv[2],"wb");OK(f && fwrite(out,1,length,f)==length && !fclose(f));
    printf("PASS: %s: %lu faces; all vertices blood-addressable; exact joints/positions/UV/colors/materials; stable repeat export\n",argv[1],(unsigned long)after.count);
    ModelFreeSource(&before);ModelFreeSource(&after);free(data);free(out);return 0;
}
