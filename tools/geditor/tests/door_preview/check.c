#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exercise the production placement helper; unrelated file/UI paths are
   discarded at link time, as in the other portable editor harnesses. */
#include "objectload.c"
#include "doors.h"

static void Put32(unsigned char *p, uint32_t v)
{ p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }

static void FreeBuilder(ObjectBuilder *builder)
{
    free(builder->tris); free(builder->tritags);
    free(builder->renderflags); free(builder->objectindices);
}

static void Near(float a, float b)
{ assert(fabsf(a-b) <= 1e-4f * fmaxf(1, fmaxf(fabsf(a),fabsf(b)))); }

static void FaceNormal(const BgVertex *v, float n[3])
{
    float a[3]={v[1].x-v[0].x,v[1].y-v[0].y,v[1].z-v[0].z};
    float b[3]={v[2].x-v[0].x,v[2].y-v[0].y,v[2].z-v[0].z};
    n[0]=a[1]*b[2]-a[2]*b[1];
    n[1]=a[2]*b[0]-a[0]*b[2];
    n[2]=a[0]*b[1]-a[1]*b[0];
}

static void CheckMirror(ModelCacheEntry *model, unsigned short flags[2])
{
    ObjectBasis basis={.side={1,0,0},.up={0,1,0},.look={0,0,1},.pos={10,20,30}};
    float center[3], scale[3]={2,3,4};
    ObjectBuilder plain={0}, flipped={0}, again={0}, prop={0}, propflip={0};
    size_t bytes=model->tricount*3*sizeof(BgVertex);
    BgVertex *original=malloc(bytes);
    assert(original); memcpy(original,model->tris,bytes);
    for (int i=0; i<3; i++) { center[i]=(model->min[i]+model->max[i])*.5f; }
    ObjectPlaceModel(&plain,model,&basis,scale,TRUE,flags[0],center,0,0);
    ObjectPlaceModel(&flipped,model,&basis,scale,TRUE,flags[1],center,1,0);
    ObjectPlaceModel(&again,model,&basis,scale,TRUE,flags[0],center,0,0);
    assert(!plain.failed && !flipped.failed && !again.failed);
    assert(plain.tricount==model->tricount && flipped.tricount==plain.tricount);
    /* Both doors share the model cache. A flipped instance must not change
       either the cached mesh or subsequent ordinary instances. */
    assert(!memcmp(original,model->tris,bytes) && !memcmp(plain.tris,again.tris,bytes));
    for (DWORD tri=0; tri<plain.tricount; tri++)
    {
        float a[3],b[3];
        assert(plain.tritags[tri]==flipped.tritags[tri]);
        assert(plain.renderflags[tri]==flipped.renderflags[tri]);
        assert(plain.objectindices[tri]==0 && flipped.objectindices[tri]==1);
        for (int corner=0; corner<3; corner++)
        {
            const BgVertex *p=&plain.tris[tri*3+(corner==0?0:3-corner)];
            const BgVertex *f=&flipped.tris[tri*3+corner];
            /* Door local Z is pad-side/world X with this basis. A mirror
               swaps the front/back surfaces while retaining height/width. */
            Near(f->x,2*basis.pos[0]-p->x); Near(f->y,p->y); Near(f->z,p->z);
            assert(f->s==p->s && f->t==p->t);
            assert(f->r==p->r && f->g==p->g && f->b==p->b && f->a==p->a);
            if (model->renderflags[tri]&BG_RENDER_ENVIRONMENT)
            {
                Near(f->environment.normal[0],-p->environment.normal[0]);
                Near(f->environment.normal[1],p->environment.normal[1]);
                Near(f->environment.normal[2],p->environment.normal[2]);
            }
        }
        /* Winding must keep the outward face, for culling and ray picking. */
        FaceNormal(&plain.tris[tri*3],a); FaceNormal(&flipped.tris[tri*3],b);
        Near(b[0],-a[0]); Near(b[1],a[1]); Near(b[2],a[2]);
    }
    /* The same bit in other prop types is not a door mirror flag. */
    ObjectPlaceModel(&prop,model,&basis,scale,FALSE,0,center,0,0);
    ObjectPlaceModel(&propflip,model,&basis,scale,FALSE,DOORFLAG_FLIP,center,0,0);
    assert(!prop.failed && !propflip.failed && !memcmp(prop.tris,propflip.tris,bytes));
    FreeBuilder(&plain); FreeBuilder(&flipped); FreeBuilder(&again);
    FreeBuilder(&prop); FreeBuilder(&propflip); free(original);
}

int main(int argc, char **argv)
{
    SetupFile setup={0}; SetupObject objects[2]={{0}};
    unsigned char records[40+2*256]={0}, unchanged[sizeof(records)];
    unsigned short flags[2];
    char path[1024]; FILE *file; long size; unsigned char *raw;
    ModelCacheEntry model={0}; const char *why="";
    assert(argc==2);
    setup.data=records; setup.size=sizeof(records); setup.objects=objects; setup.objectcount=2;
    for (int i=0; i<2; i++)
    {
        SetupObjectProperties properties;
        objects[i].type=PROPDEF_DOOR; objects[i].sourceoffset=40+i*256;
        objects[i].modelid=300; objects[i].pad=46+i;
        for (int word=0; word<64; word++) { Put32(records+objects[i].sourceoffset+word*4,doorwords[i][word]); }
        assert(SetupFileGetObjectProperties(&setup,i,&properties,&why));
        flags[i]=properties.door.flags;
    }
    assert(flags[0]==0 && flags[1]==DOORFLAG_FLIP);
    memcpy(unchanged,records,sizeof(records));
    snprintf(path,sizeof(path),"%s/assets/obseg/prop/PtrainextdoorZ.bin",argv[1]);
    file=fopen(path,"rb"); assert(file); fseek(file,0,SEEK_END); size=ftell(file); rewind(file);
    raw=malloc(size); assert(raw && fread(raw,1,size,file)==(size_t)size); fclose(file);
    model.tris=ModelLoadGeometry(raw,size,&model.tricount,&model.tritags,&model.renderflags,&why);
    assert(model.tris && model.tricount);
    assert(ModelReadPlacementBounds(raw,size,model.min,model.max));
    CheckMirror(&model,flags);
    assert(!memcmp(unchanged,records,sizeof(records)));
    free(raw); free(model.tris); free(model.tritags); free(model.renderflags);
    /* An environment-mapped replacement door also needs reflected normals
       under non-uniform fitting. UVs and vertex colors still travel together. */
    BgVertex vertices[3]={0};
    unsigned short tags=BG_TRI_CULL_BACK; BgRenderFlags render=BG_RENDER_ENVIRONMENT;
    memset(&model,0,sizeof(model));
    model.tris=vertices; model.tritags=&tags; model.renderflags=&render; model.tricount=1;
    for (int i=0; i<3; i++)
    {
        (&vertices[i].x)[i]=1; vertices[i].s=i; vertices[i].t=3-i;
        vertices[i].r=20+i; vertices[i].g=40+i; vertices[i].b=60+i; vertices[i].a=255;
        vertices[i].environment.normal[0]=.2f;
        vertices[i].environment.normal[1]=.4f;
        vertices[i].environment.normal[2]=.8f;
    }
    CheckMirror(&model,flags);
    puts("PASS: Depot door flags, mirrored native model, winding, UVs/colors, normals and shared cache.");
    return 0;
}
