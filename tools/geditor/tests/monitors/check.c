#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "monitorload.h"
/* Include the production placement helpers; unrelated file/UI paths are
   discarded at link time, as in the other portable editor harnesses. */
#include "objectload.c"

static void Put32(unsigned char *p, uint32_t v)
{ p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
#include "scripts.h"

static void Fixture(RomFile *rom)
{
    unsigned size = CODE_SIZE + 52 * 4 + 50 * 12 + 116;
    memset(rom, 0, sizeof(*rom));
    rom->data = calloc(size, 1); rom->size = size;
    assert(rom->data);
    rom->info.entrycount = 4;
    rom->info.entries[0] = (RomManifestEntry){0x434d4150,0,CODE_SIZE,0x80000000};
    rom->info.entries[1] = (RomManifestEntry){0x4d4f4e41,CODE_SIZE,CODE_SIZE+208,52};
    rom->info.entries[2] = (RomManifestEntry){0x4d4f4e54,CODE_SIZE+208,CODE_SIZE+808,12};
    rom->info.entries[3] = (RomManifestEntry){0x4d4f4e44,CODE_SIZE+808,size,116};
    Populate(rom->data, rom->data + CODE_SIZE);
    for (unsigned i=0; i<50; i++)
    { unsigned char *p=rom->data+CODE_SIZE+208+i*12; Put32(p,100+i); p[4]=p[5]=32; }
    memcpy(rom->data+CODE_SIZE+808,initial,116);
}

static void Scripts(RomFile *rom)
{
    const char *why = "";
    MonitorBank bank = {0}, copy = {0};
    MonitorAnimation s;
    float uv[4][2];
    assert(MonitorBankLoadRom(&bank,rom,&why));
    assert(MonitorBankClone(&copy,&bank));
    assert(copy.code != bank.code && !memcmp(copy.code,bank.code,bank.codesize));
    MonitorBankFree(&copy);
    for (unsigned id=0; id<bank.scriptcount; id++)
    {
        MonitorAnimationStart(&s,&bank,id,id+1);
        for (int tick=0; tick<18000; tick++)
        {
            MonitorAnimationTick(&s,&bank);
            assert(!s.stopped && MonitorAnimationImage(&s,&bank));
            MonitorAnimationUvs(&s,uv);
            for (int i=0; i<4; i++) { assert(isfinite(uv[i][0]) && isfinite(uv[i][1])); }
        }
    }
    /* Bond logo scrolls exactly one tile over 20 ticks. */
    MonitorAnimationStart(&s,&bank,0,1);
    for (int i=0; i<20; i++) { MonitorAnimationTick(&s,&bank); }
    assert(s.texture==0 && fabsf(s.xmid.value-1.5f)<1e-5f);
    /* Astrological images hold for 80 ticks plus the pause-command frame. */
    MonitorAnimationStart(&s,&bank,2,1);
    for (int i=0; i<81; i++) { MonitorAnimationTick(&s,&bank); assert(s.texture==IMGSHUTTLE1); }
    MonitorAnimationTick(&s,&bank); assert(s.texture==IMGSHUTTLE2);
    /* The authored red solid reaches its specified tint. */
    MonitorAnimationStart(&s,&bank,49,1);
    for (int i=0; i<100; i++) { MonitorAnimationTick(&s,&bank); }
    assert(s.color[0]==220 && s.color[1]==40 && s.color[2]==40);
    MonitorAnimationStart(&s,&bank,-1,1); assert(s.script==bank.scripts[0]);
    MonitorAnimationStart(&s,&bank,999,1); assert(s.script==bank.scripts[0]);
    /* Bad opcodes, truncated operands, bad jumps and non-yielding loops must
       never hang the UI or read outside the copied ROM segment. */
    s.script=bank.address; s.offset=0; Put32(bank.code,99);
    MonitorAnimationTick(&s,&bank); assert(s.stopped);
    MonitorAnimationStart(&s,&bank,0,1); Put32(bank.code,9); Put32(bank.code+4,1);
    MonitorAnimationTick(&s,&bank); assert(s.stopped);
    MonitorAnimationStart(&s,&bank,0,1); Put32(bank.code,11);
    MonitorAnimationTick(&s,&bank); assert(s.stopped);
    MonitorAnimationStart(&s,&bank,0,1); s.script=bank.address+bank.codesize-4; Put32(bank.code+bank.codesize-4,7);
    MonitorAnimationTick(&s,&bank); assert(s.stopped);
    s.texture=99; assert(!MonitorAnimationImage(&s,&bank));
    s.texture=0xffffffff; assert(!MonitorAnimationImage(&s,&bank));
    MonitorBankFree(&bank);
    rom->info.entries[2].romend++; assert(!MonitorBankLoadRom(&bank,rom,&why)); rom->info.entries[2].romend--;
    rom->info.entries[1].flags=0xffffffff; assert(!MonitorBankLoadRom(&bank,rom,&why)); rom->info.entries[1].flags=52;
    rom->info.entries[3].romend=rom->size+1; assert(!MonitorBankLoadRom(&bank,rom,&why)); rom->info.entries[3].romend=rom->size;
    Put32(rom->data+CODE_SIZE,0); assert(!MonitorBankLoadRom(&bank,rom,&why)); Populate(rom->data,rom->data+CODE_SIZE);
    puts("PASS: all 52 ROM scripts for five minutes each; timing, fades, UVs and malformed data.");
}

static void Models(const char *root, RomFile *rom)
{
    const char *names[]={"PtvscreenZ","Ptv1Z","Ptv4screenZ","Pconsole1Z"};
    const int counts[]={1,1,4,4};
    for (int n=0; n<4; n++)
    {
        char path[1024]; unsigned char *raw; long size;
        ModelCacheEntry model={0}; const char *why="";
        ObjectBuilder builder={0}; MonitorGeometry geometry={0};
        SetupFile setup={0}; SetupObject object={0}; ObjectPlacement p={0};
        FILE *f;
        snprintf(path,sizeof(path),"%s/assets/obseg/prop/%s.bin",root,names[n]);
        f=fopen(path,"rb"); assert(f); fseek(f,0,SEEK_END); size=ftell(f); rewind(f);
        raw=malloc(size); assert(raw && fread(raw,1,size,f)==(size_t)size); fclose(f);
        model.tris=ModelLoadGeometry(raw,size,&model.tricount,&model.tritags,&model.renderflags,&why);
        assert(model.tris && model.tricount);
        for (int i=0; i<counts[n]; i++)
        { model.hasscreen[i]=ModelReadMonitorScreen(raw,size,i,model.screens[i]); assert(model.hasscreen[i]); }
        assert(!ModelReadMonitorScreen(raw,12,0,model.screens[0]));
        /* Place an actual cabinet with all placeholder faces suppressed, then
           restore exactly two animated triangles for each native screen. */
        p.basis.side[0]=p.basis.up[1]=p.basis.look[2]=1;
        p.basis.pos[0]=100; p.scale[0]=p.scale[1]=p.scale[2]=.1f;
        object.type=counts[n]==1?PROPDEF_MONITOR:PROPDEF_MULTI_MONITOR;
        object.flags=PROPFLAG_FIXED_MONITOR;
        setup.objects=&object; setup.objectcount=1; setup.size=596; setup.data=calloc(596,1);
        ObjectPlaceModel(&builder,&model,&p.basis,p.scale,FALSE,p.center,0,counts[n]);
        DWORD staticcount=builder.tricount;
        assert(ObjectPlaceMonitorScreens(&builder,&geometry,&setup,0,&model,&p,rom,&why));
        assert(geometry.count==(DWORD)counts[n] && builder.tricount==staticcount+counts[n]*2);
        for (DWORD i=staticcount; i<builder.tricount; i++)
        { assert((builder.renderflags[i] & (BG_RENDER_MONITOR|BG_RENDER_DECAL|BG_RENDER_DEPTH_TEST)) ==
            (BG_RENDER_MONITOR|BG_RENDER_DECAL|BG_RENDER_DEPTH_TEST)); }
        assert(fabsf(builder.tris[staticcount*3].x-(100+model.screens[0][0].x*.1f))<1e-5f);
        /* Z-buffer disabling takes priority over fixed/decal flags. */
        object.flags2=PROPFLAG2_DISABLE_ZBUFFER;
        DWORD before=builder.tricount;
        assert(ObjectPlaceMonitorScreens(&builder,&geometry,&setup,0,&model,&p,rom,&why));
        assert(builder.renderflags[before]==BG_RENDER_MONITOR);
        free(raw); free(model.tris); free(model.tritags); free(model.renderflags);
        free(builder.tris); free(builder.tritags); free(builder.renderflags); free(builder.objectindices);
        free(setup.data); MonitorGeometryFree(&geometry);
    }
    puts("PASS: native single/four-screen model extraction, placement and decal/Z-buffer rules.");
}

static void OwnerReferences(void)
{
    SetupFile setup={0}; SetupObject objects[2]={{0}}; DWORD target;
    setup.size=40+128+8+256+4; setup.data=calloc(setup.size,1); setup.objects=objects; setup.objectcount=2;
    Put32(setup.data+12,40); setup.data[43]=3; setup.data[171]=2; setup.data[179]=10; setup.data[435]=48;
    objects[0].sourceoffset=40; objects[1].sourceoffset=176;
    assert(SetupObjectRelativeTarget(&setup,176,-2,&target) && target==0);
    assert(!SetupObjectRelativeTarget(&setup,176,-1,&target)); /* non-object command */
    assert(!SetupObjectRelativeTarget(&setup,176,-3,&target));
    assert(!SetupObjectRelativeTarget(&setup,176,1,&target));
    free(setup.data);
    puts("PASS: attachment references count all native commands, not only objects.");
}

int main(int argc, char **argv)
{
    RomFile rom;
    assert(argc==2); Fixture(&rom);
    Scripts(&rom); Models(argv[1],&rom); OwnerReferences();
    free(rom.data); return 0;
}
