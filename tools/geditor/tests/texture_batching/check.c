#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

static const char *why;
#define OK(x) do { if (!(x)) { fprintf(stderr,"%s:%d %s: %s\n",__FILE__,__LINE__,#x,why?why:"");abort(); } } while (0)
static DWORD Get(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Put(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void Cmd(unsigned char *p,DWORD a,DWORD b) { Put(p,a);Put(p+4,b); }
static DWORD Textures(const unsigned char *p,DWORD size)
{
    DWORD n=0;
    for (DWORD i=0;i<size;i+=8) if (p[i]==0xc0 && !BG_EDITOR_IS_MARKER(Get(p+i),Get(p+i+4))) n++;
    return n;
}
static BgFile Fixture(DWORD mode,BOOL barrier,BOOL untextured)
{
    BgFile bg={0}; bg.size=4096;bg.data=calloc(bg.size,1);OK(bg.data);
    Put(bg.data+4,32);Put(bg.data+56,0x0e000100);Put(bg.data+60,0x0e000200);Put(bg.data+64,0x0e000600);
    Put(bg.data+252,8*16);
    for (DWORD i=0;i<8;i++)
    {
        unsigned char *v=bg.data+256+i*16;
        v[1]=i*20;v[3]=(i%2)*20;v[9]=i*3;v[11]=i*5;
        v[12]=100+i;v[13]=200;v[14]=150;v[15]=255;
    }
    for (DWORD layer=0;layer<2;layer++)
    {
        DWORD start=layer?0x600:0x200,pc=start;
        Cmd(bg.data+pc,0xba001402,0x100000);pc+=8;
        Cmd(bg.data+pc,0xb900031d,layer?0x005049d8:mode);pc+=8;
        for (DWORD f=0;f<6;f++)
        {
            static const DWORD images[]={0x31,0x32,0x31,0x33,0x32,0x31};
            Cmd(bg.data+pc,0xe7000000,0);pc+=8;
            if (barrier && f==3) { Cmd(bg.data+pc,0xfa000000,0xff8080ff);pc+=8; }
            if (untextured && f==3) { Cmd(bg.data+pc,0xbb000000,0xffffffff);pc+=8; }
            else
            {
                Cmd(bg.data+pc,0xbb000001,0xffffffff);pc+=8;
                Cmd(bg.data+pc,0xc0000002,images[f]);pc+=8;
            }
            Cmd(bg.data+pc,0xfc121824,0xff33ffff);pc+=8;
            Cmd(bg.data+pc,0x04200030,0x0e000000+f*16);pc+=8;
            Cmd(bg.data+pc,0xbf000000,0x00000a14);pc+=8;
        }
        Cmd(bg.data+pc,0xb8000000,0);pc+=8;Put(bg.data+start-4,pc-start);
    }
    return bg;
}

/* Compare decoded triangles by geometry/material/state, allowing only primary
 * order changes. Group numbers and parser-assigned face IDs are not geometry. */
typedef struct FaceKey {
    unsigned char vertices[3][16];
    DWORD material[8],state[7];
    DWORD layer,cull;
} FaceKey;
static FaceKey Key(const BgDocument *doc,DWORD room,DWORD f)
{
    const BgDocumentFace *face=&doc->rooms[room].faces[f];
    BgFaceRef ref={face->id,(unsigned short)room,face->layer,0};
    BgRenderState state;FaceKey key={0};
    OK(BgDocumentGetFaceRenderStates(doc,&ref,1,&state));
    for (DWORD c=0;c<3;c++)
    {
        const BgDocumentVertex *v=&doc->rooms[room].vertices[face->vertexindices[c]];
        unsigned char *p=key.vertices[c];
        p[0]=(unsigned short)v->x>>8;p[1]=v->x;p[2]=(unsigned short)v->y>>8;p[3]=v->y;
        p[4]=(unsigned short)v->z>>8;p[5]=v->z;p[6]=v->flag>>8;p[7]=v->flag;
        p[8]=(unsigned short)v->s>>8;p[9]=v->s;p[10]=(unsigned short)v->t>>8;p[11]=v->t;
        p[12]=v->r;p[13]=v->g;p[14]=v->b;p[15]=v->a;
    }
    memcpy(key.material,&face->material,sizeof(key.material));
    key.state[0]=state.othermode;key.state[1]=state.othermodehigh;key.state[2]=state.environmentalpha;
    key.state[3]=state.primitiveword0;key.state[4]=state.primitiveword1;
    key.state[5]=state.geometrymode&~0x2000u;key.state[6]=state.surfacepolicy;
    key.layer=face->layer;key.cull=face->cullbackfaces;return key;
}
static int Compare(const void *a,const void *b) { return memcmp(a,b,sizeof(FaceKey)); }
static void EquivalentDocuments(const BgDocument *x,const BgDocument *y)
{
    OK(x->roomcount==y->roomcount && x->facecount==y->facecount);
    for (DWORD r=1;r<=x->roomcount;r++)
    {
        DWORD count=x->rooms[r].facecount;OK(count==y->rooms[r].facecount);
        OK(!memcmp(x->rooms[r].origin,y->rooms[r].origin,sizeof(x->rooms[r].origin)));
        FaceKey *left=calloc(count?count:1,sizeof(*left)),*right=calloc(count?count:1,sizeof(*right));OK(left&&right);
        for (DWORD f=0;f<count;f++)
        {
            left[f]=Key(x,r,f);right[f]=Key(y,r,f);
            if (left[f].layer || (left[f].state[0]&0x5c00u) || left[f].state[6]>=BG_SURFACE_CUTOUT)
                OK(!memcmp(&left[f],&right[f],sizeof(*left)));
        }
        qsort(left,count,sizeof(*left),Compare);qsort(right,count,sizeof(*right),Compare);
        OK(!memcmp(left,right,count*sizeof(*left)));free(left);free(right);
    }
}
static void Equivalent(const BgFile *a,const BgFile *b)
{
    BgDocument x={0},y={0};OK(BgDocumentLoad(a->data,a->size,1,&x,&why));OK(BgDocumentLoad(b->data,b->size,1,&y,&why));
    EquivalentDocuments(&x,&y);
    BgDocumentFree(&x);BgDocumentFree(&y);
}
static void Synthetic(void)
{
    for (DWORD kind=0;kind<6;kind++)
    {
        DWORD modes[]={0x00552078,0x005049d8,0x00553078,0x00504dd8,0x00552078,0x00552078};
        BgFile source=Fixture(modes[kind],kind==4,kind==5),out={0},again={0},compiled={0};BgDocument doc={0};
        OK(BgFileValidateVertexBatches(&source,&why));
        unsigned char *snapshot=malloc(source.size);OK(snapshot);memcpy(snapshot,source.data,source.size);
        OK(BgFileBatchOpaque(&source,&out,&why));OK(!memcmp(snapshot,source.data,source.size));
        if (!kind)
        {
            OK(out.data&&out.size==source.size);
            OK(Textures(out.data+0x200,Get(out.data+0x1fc))==3);
            OK(!memcmp(out.data+0x600,source.data+0x600,Get(source.data+0x5fc)));
            OK(!memcmp(out.data+256,source.data+256,8*16));
            printf("BG fixture: texture selections 6 -> 3; primary bytes %u -> %u; vertices/secondary unchanged.\n",
                Get(source.data+0x1fc),Get(out.data+0x1fc));
        }
        if (kind>=1 && kind<=3) OK(!out.data);
        if (out.data)
        {
            Equivalent(&source,&out);OK(BgFileValidateVertexBatches(&out,&why));
            OK(BgFileBatchOpaque(&out,&again,&why));OK(!again.data);
        }
        OK(BgDocumentLoad(source.data,source.size,1,&doc,&why));
        OK(BgDocumentCompile(&doc,&source,&compiled,&why));Equivalent(&source,&compiled);
        OK(!memcmp(snapshot,source.data,source.size));
        BgDocumentFree(&doc);BgFileFree(&source);BgFileFree(&out);BgFileFree(&compiled);free(snapshot);
    }
    puts("PASS: opaque sorting, native geometry/UV/RGBA/material state, secondary order, blend/cutout/decal barriers, constant-color boundaries, untextured inheritance, bullet vertex batches and idempotence.");
}
static void StateAndCosts(void)
{
    for (DWORD variant=0;variant<4;variant++)
    {
        BgFile source=Fixture(0x00552078,FALSE,FALSE),out={0};DWORD binding=0;
        for (DWORD pc=0x200;source.data[pc]!=0xb8;pc+=8)
        {
            if (source.data[pc]==0xc0)
            {
                BOOL alternate=binding++%2;
                if (variant==0) Cmd(source.data+pc,alternate?0xc0400002:0xc0000002,0x31); /* clamp vs repeat */
                if (variant==1) Cmd(source.data+pc,0xc0000001,0x31|((alternate?0x35u:0x36u)<<12)); /* detail image */
                if (variant==2) Cmd(source.data+pc,0xc0000002,alternate?1508:1511); /* dynamic water */
            }
        }
        if (variant==3) Cmd(source.data+0x210,0x06000000,0x0e000800); /* unknown nested-list state */
        OK(BgFileBatchOpaque(&source,&out,&why));
        if (variant<2)
        {
            OK(out.data);Equivalent(&source,&out);
            OK(Textures(out.data+0x200,Get(out.data+0x1fc))==2);
        }
        else OK(!out.data);
        BgFileFree(&source);BgFileFree(&out);
    }
    {
        BgFile source=Fixture(0x00552078,FALSE,FALSE),compiled={0};BgDocument doc={0};
        OK(BgDocumentLoad(source.data,source.size,1,&doc,&why));
        BgDocumentRoom *room=&doc.rooms[1];
        BgDocumentVertex *vertices=calloc(80,sizeof(*vertices));OK(vertices);
        for(DWORD v=0;v<80;v++) vertices[v].room=1;
        for(DWORD f=0;f<6;f++) for(DWORD c=0;c<3;c++)
        {
            vertices[f*12+c]=room->vertices[room->faces[f].vertexindices[c]];
            room->faces[f].vertexindices[c]=f*12+c;
        }
        /* Keep the secondary geometry valid in the larger vertex array. */
        for(DWORD f=6;f<12;f++) for(DWORD c=0;c<3;c++) room->faces[f].vertexindices[c]=(f-6)*12+c;
        free(room->vertices);room->vertices=vertices;room->vertexcount=80;
        OK(BgDocumentCompile(&doc,&source,&compiled,&why));
        DWORD primary=Get(compiled.data+60)&0xffffff;
        /* Dense remapping and exact attribute reuse recover the eight native
         * vertices despite widely spaced editor IDs, so sorting now wins. */
        OK(Textures(compiled.data+primary,Get(compiled.data+primary-4))==3);
        DWORD loads=0;
        for(DWORD pc=primary;compiled.data[pc]!=0xb8;pc+=8) if(compiled.data[pc]==4)
        { loads++;OK(((Get(compiled.data+pc)>>20)&15)+1==8); }
        OK(loads==1);Equivalent(&source,&compiled);
        OK(BgFileValidateVertexBatches(&compiled,&why));
        BgDocumentFree(&doc);BgFileFree(&source);BgFileFree(&compiled);
    }
    puts("PASS: distinct wrapping/detail bindings, dynamic/nested-list barriers and vertex-load cost fallback.");
}
static DWORD Commands(const BgFile *bg, DWORD room, DWORD layer, unsigned char opcode)
{
    DWORD table=Get(bg->data+4)&0xffffffu;
    DWORD pc=Get(bg->data+table+room*24+4+layer*4)&0xffffffu,count=0;
    if (!pc) return 0;
    for (;bg->data[pc]!=0xb8;pc+=8) count+=bg->data[pc]==opcode;
    return count;
}

static void VertexReuse(void)
{
    /* Equal positions alone must never merge UV seams, flags, normals/colors
     * or alpha. Only variant zero is a byte-identical duplicate. */
    for (int variant=0;variant<11;variant++)
    {
        BgFile source=Fixture(0x00552078,FALSE,FALSE),out={0};
        BgDocument doc={0},snapshot={0},loaded={0};
        OK(BgDocumentLoad(source.data,source.size,1,&doc,&why));
        BgDocumentRoom *room=&doc.rooms[1];
        room->facecount=doc.facecount=2;
        room->faces[1]=room->faces[0];room->faces[1].id++;
        room->faces[1].vertexindices[0]=7;room->vertices[7]=room->vertices[0];
        room->vertices[7].id=900;room->vertices[7].usecount=1;
        BgDocumentVertex *v=&room->vertices[7];
        switch(variant)
        {
        case 1:v->x++;break;case 2:v->y++;break;case 3:v->z++;break;
        case 4:v->flag++;break;case 5:v->s++;break;case 6:v->t++;break;
        case 7:v->r++;break;case 8:v->g++;break;case 9:v->b++;break;case 10:v->a--;break;
        }
        OK(BgDocumentClone(&doc,&snapshot,&why));
        OK(BgDocumentCompile(&doc,&source,&out,&why));
        OK(BgFileValidateVertexBatches(&out,&why));
        OK(BgDocumentLoad(out.data,out.size,1,&loaded,&why));
        EquivalentDocuments(&doc,&loaded);
        OK(loaded.rooms[1].vertexcount==(variant?4u:3u));
        OK(Commands(&out,1,0,4)==1);
        OK(!memcmp(room->vertices,snapshot.rooms[1].vertices,room->vertexcount*sizeof(*room->vertices)));
        OK(!memcmp(room->faces,snapshot.rooms[1].faces,room->facecount*sizeof(*room->faces)));
        for(int layer=0;layer<2;layer++)for(DWORD g=0;g<room->layers[layer].groupcount;g++)
        {
            BgDocumentDrawGroup *a=&room->layers[layer].groups[g],*b=&snapshot.rooms[1].layers[layer].groups[g];
            OK(a->commandsize==b->commandsize && !memcmp(a->commands,b->commands,a->commandsize));
        }
        BgDocumentFree(&doc);BgDocumentFree(&snapshot);BgDocumentFree(&loaded);BgFileFree(&out);BgFileFree(&source);
    }
    puts("PASS: exact XYZ/flag/UV/RGBA reuse, seam/alpha differences and unchanged live IDs/state.");
}

static void MutableAliases(void)
{
    for(int water=0;water<2;water++)
    {
        DWORD image=water?1508:201;
        BgFile source=Fixture(0x00552078,FALSE,FALSE),out={0};BgDocument doc={0},loaded={0};
        OK(BgDocumentLoad(source.data,source.size,1,&doc,&why));
        BgDocumentRoom *room=&doc.rooms[1];
        room->vertices=realloc(room->vertices,16*sizeof(*room->vertices));OK(room->vertices);
        memcpy(room->vertices+8,room->vertices,8*sizeof(*room->vertices));room->vertexcount=16;
        room->faces[0].textureid=image;BgMaterialSetTexture(&room->faces[0].material,image);
        /* A separate ordinary triangle has the same bytes as the light/water.
         * The first secondary triangle intentionally shares the mutable IDs. */
        for(int c=0;c<3;c++)room->faces[1].vertexindices[c]=8+c;
        OK(BgDocumentCompile(&doc,&source,&out,&why));OK(BgFileValidateVertexBatches(&out,&why));
        OK(BgDocumentLoad(out.data,out.size,1,&loaded,&why));EquivalentDocuments(&doc,&loaded);
        BgDocumentRoom *result=&loaded.rooms[1];
        const BgDocumentFace *mutable=NULL,*independent=NULL,*shared=NULL;
        for(DWORD f=0;f<result->facecount;f++)
        {
            const BgDocumentFace *face=&result->faces[f];
            if(!face->layer && face->textureid==image)mutable=face;
            if(!face->layer && face->textureid==0x32
                && result->vertices[face->vertexindices[0]].x==0)independent=face;
            if(face->layer && !shared)shared=face;
        }
        OK(mutable && independent && shared);
        for(int c=0;c<3;c++)
        {
            OK(mutable->vertexindices[c]==shared->vertexindices[c]);
            for(int d=0;d<3;d++)OK(mutable->vertexindices[c]!=independent->vertexindices[d]);
        }
        BgDocumentFree(&doc);BgDocumentFree(&loaded);BgFileFree(&source);BgFileFree(&out);
    }
    puts("PASS: mutable light/water aliases survive across layers without absorbing identical static vertices.");
}

static void RedundantState(void)
{
    BgFile source=Fixture(0x00552078,FALSE,FALSE),out={0};BgDocument doc={0},loaded={0};
    OK(BgDocumentLoad(source.data,source.size,1,&doc,&why));
    BgDocumentRoom *room=&doc.rooms[1];
    for(DWORD f=0;f<room->facecount;f++)
    {
        BgDocumentFace *face=&room->faces[f];
        BgDocumentDrawGroup *group=&room->layers[face->layer].groups[face->drawgroup];
        group->commands=realloc(group->commands,group->commandsize+24);OK(group->commands);
        Cmd(group->commands+group->commandsize,0xb900031d,face->layer?0x005049d8:0x00552078);
        /* Two partial writes share the low-mode register. The first alpha
         * compare write must survive even though its value equals a default. */
        Cmd(group->commands+group->commandsize+8,0xb9000002,0);
        Cmd(group->commands+group->commandsize+16,0xfb000000,0x123456ff);
        group->commandsize+=24;group->commandcapacity=group->commandsize;
    }
    OK(BgDocumentCompile(&doc,&source,&out,&why));OK(BgDocumentLoad(out.data,out.size,1,&loaded,&why));
    EquivalentDocuments(&doc,&loaded);OK(BgFileValidateVertexBatches(&out,&why));
    for(int layer=0;layer<2;layer++)
    { OK(Commands(&out,1,layer,0xb9)==2);OK(Commands(&out,1,layer,0xfb)==1);OK(Commands(&out,1,layer,4)==1); }
    BgDocumentFree(&doc);BgDocumentFree(&loaded);BgFileFree(&source);BgFileFree(&out);
    puts("PASS: redundant full/partial state writes, full environment color and ordered secondary batching.");
}

static void WholeFile(const BgFile *source)
{
    BgFile out={0},again={0};
    unsigned char *snapshot=malloc(source->size);OK(snapshot);memcpy(snapshot,source->data,source->size);
    OK(BgFileOptimize(source,&out,&why));OK(!memcmp(snapshot,source->data,source->size));
    if(out.data)
    {
        Equivalent(source,&out);OK(BgFileValidateVertexBatches(&out,&why));
        OK(BgFileOptimize(&out,&again,&why));OK(!again.data);
        printf("Full export: %u -> %u bytes; decoded geometry/materials and secondary order preserved.\n",source->size,out.size);
    }
    free(snapshot);BgFileFree(&out);BgFileFree(&again);
}

int main(int argc,char **argv)
{
    Synthetic();StateAndCosts();VertexReuse();MutableAliases();RedundantState();
    for (int a=1;a<argc;a++)
    {
        FILE *f=fopen(argv[a],"rb");BgFile source={0},out={0},again={0};DWORD before=0,after=0;
        OK(f);fseek(f,0,SEEK_END);source.size=ftell(f);rewind(f);source.data=malloc(source.size);OK(source.data);
        OK(fread(source.data,1,source.size,f)==source.size);fclose(f);
        OK(BgFileBatchOpaque(&source,&out,&why));
        if (out.data)
        {
            Equivalent(&source,&out);OK(out.size==source.size);OK(BgFileValidateVertexBatches(&out,&why));
            OK(BgFileBatchOpaque(&out,&again,&why));OK(!again.data);
        }
        DWORD table=Get(source.data+4)&0xffffff;
        for (DWORD r=table+24;Get(source.data+r+4);r+=24)
        {
            DWORD primary=Get(source.data+r+4)&0xffffff,secondary=Get(source.data+r+8)&0xffffff;
            before+=Textures(source.data+primary,Get(source.data+primary-4));
            const unsigned char *data=out.data?out.data:source.data;
            after+=Textures(data+primary,Get(data+primary-4));
            if (secondary) OK(!memcmp(data+secondary-4,source.data+secondary-4,Get(source.data+secondary-4)+4));
        }
        printf("%s: texture selections %u -> %u; BG size %u bytes (unchanged).\n",argv[a],before,after,source.size);
        WholeFile(&source);
        BgFileFree(&source);BgFileFree(&out);
    }
    return 0;
}
