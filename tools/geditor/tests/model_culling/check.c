#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Exercise placement itself, including the door mirror and shared cache. */
#include "objectload.c"

static void Put32(unsigned char *p,DWORD n)
{ p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n; }
static void Command(unsigned char *data,DWORD *at,DWORD a,DWORD b)
{ Put32(data+*at,a);Put32(data+*at+4,b);*at+=8; }
static void Face(unsigned char *data,DWORD *at)
{ Command(data,at,0xbf000000u,0x00000a14u); }
static unsigned int Mode(BgRenderFlags flags)
{ return ((flags&BG_RENDER_CULL_BACK)?2:0)|((flags&BG_RENDER_CULL_FRONT)?1:0); }

static void Check(int modeltype)
{
    unsigned char data[0x500]={0},original[sizeof(data)];
    DWORD at=0x100,tri;
    const char *why="";
    ModelSource source={0};
    BgVertex *vertices;
    unsigned short *tags;
    BgRenderFlags *flags;
    DWORD count;
    /* Root GROUPSIMPLE and one mesh; no actual ROM/Windows dependencies. */
    data[0x21]=0x15;Put32(data+0x24,0x05000060);Put32(data+0x34,0x05000038);
    data[0x39]=4;Put32(data+0x3c,0x05000080);Put32(data+0x40,0x05000020);
    Put32(data+0x80,0x05000100);Put32(data+0x84,0x05000300);Put32(data+0x8c,0x050000c0);
    data[0x91]=3;data[0x92]=(unsigned char)modeltype;
    data[0xd1]=10;data[0xe3]=10;
    data[0xcf]=data[0xdf]=data[0xef]=255;
    Command(data,&at,0x04200030,0x04000000);Face(data,&at); /* inherited */
    Command(data,&at,0xb7000000,0x1000);Face(data,&at); /* FRONT set, BACK inherited */
    Command(data,&at,0xb6000000,0x2000);Face(data,&at); /* explicit FRONT */
    Command(data,&at,0xb7000000,0x2000);Face(data,&at); /* both */
    Command(data,&at,0xb6000000,0x3000);Face(data,&at); /* neither */
    Command(data,&at,0xb7000000,0x2000);Face(data,&at); /* explicit BACK */
    Command(data,&at,0xb8000000,0);
    at=0x300;
    Command(data,&at,0x04200030,0x04000000);Face(data,&at); /* type 3 carries, type 4 reapplies */
    Command(data,&at,0xb6000000,0x2000);Face(data,&at); /* partial clear must not restore BACK */
    Command(data,&at,0xb8000000,0);
    memcpy(original,data,sizeof(data));
    assert(ModelReadSource(data,sizeof(data),&source,&why));assert(source.count==8);
    {
        const unsigned int normal[]={2,3,1,3,0,2,2,0};
        const unsigned int flipped3[]={2,2,2,3,0,1,1,0};
        const unsigned int flipped4[]={2,2,2,3,0,1,2,2};
        ObjectBasis basis={.side={1,0,0},.up={0,1,0},.look={0,0,1}};
        float scale[3]={1,1,1},center[3]={0};
        ObjectBuilder plain={0},flipped={0},again={0};
        ModelCacheEntry model={0};
        model.tris=source.vertices;model.tritags=source.tags;model.renderflags=source.flags;model.tricount=source.count;
        ObjectPlaceModel(&plain,&model,&basis,scale,TRUE,0,center,0,0);
        ObjectPlaceModel(&flipped,&model,&basis,scale,TRUE,DOORFLAG_FLIP,center,1,0);
        ObjectPlaceModel(&again,&model,&basis,scale,TRUE,0,center,2,0);
        assert(plain.tricount==8 && flipped.tricount==8 && again.tricount==8);
        for(tri=0;tri<8;tri++)
        {
            assert(Mode(source.flags[tri])==normal[tri]);
            assert(Mode(plain.renderflags[tri])==normal[tri]);
            assert(Mode(again.renderflags[tri])==normal[tri]);
            assert(Mode(flipped.renderflags[tri])==(modeltype==3?flipped3:flipped4)[tri]);
            assert(source.flags[tri]&BG_RENDER_CULL_EXPLICIT);
        }
        assert(!memcmp(plain.tris,again.tris,8*3*sizeof(BgVertex)));
        assert(!memcmp(data,original,sizeof(data))); /* preview must not author defaults */
        assert(source.faces[0].state.geometryknown==0);
        assert(source.faces[1].state.geometryknown==0x1000);
        assert(source.faces[6].state.geometryknown==(modeltype==3?0x3000u:0));
        assert(source.faces[7].state.geometryknown==(modeltype==3?0x3000u:0x2000u));
        free(plain.tris);free(plain.tritags);free(plain.renderflags);free(plain.objectindices);
        free(flipped.tris);free(flipped.tritags);free(flipped.renderflags);free(flipped.objectindices);
        free(again.tris);free(again.tritags);free(again.renderflags);free(again.objectindices);
    }
    vertices=ModelLoadGeometry(data,sizeof(data),&count,&tags,&flags,&why);
    assert(vertices && count==source.count && !memcmp(flags,source.flags,count*sizeof(*flags)));
    free(vertices);free(tags);free(flags);
    vertices=ModelLoadCharacterGeometry(data,sizeof(data),&count,&tags,&flags,&why);
    assert(vertices && count==source.count && !memcmp(flags,source.flags,count*sizeof(*flags)));
    free(vertices);free(tags);free(flags);ModelFreeSource(&source);
}

int main(void)
{
    Check(3);Check(4);
    /* Legacy cached glTF: preserve explicit two-sided/front states, and resolve
       missing defaults without changing texture, transparency or depth flags. */
    assert(Mode(BgRenderResolveModelCulling(BG_RENDER_BLEND,FALSE))==2);
    assert(Mode(BgRenderResolveModelCulling(BG_RENDER_BLEND|BG_RENDER_CULL_EXPLICIT,FALSE))==0);
    assert(Mode(BgRenderResolveModelCulling(BG_RENDER_CULL_FRONT,FALSE))==3);
    assert(Mode(BgRenderResolveModelCulling(BG_RENDER_CULL_EXPLICIT|BG_RENDER_CULL_FRONT,FALSE))==1);
    assert(BgRenderResolveModelCulling(BG_RENDER_BLEND|BG_RENDER_DEPTH_TEST,FALSE)&BG_RENDER_BLEND);
    puts("PASS model culling: inherited/partial/explicit modes, both passes, props/characters, flipped doors, cache isolation and unchanged source data");
    return 0;
}
