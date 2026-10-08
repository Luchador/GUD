static u32 script[] = {
    TVCMD_RANDSETCMDLIST,0,0, /* RNG consumed, branch patched below. */
    TVCMD_SCROLLABSX,128,20,
    TVCMD_SCALEABSY,2048,20,
    TVCMD_SETCOLOUR,0x10203080,20,
    TVCMD_ROTATEREL,2000,
    TVCMD_SETTEXTURE,2,
    TVCMD_PAUSE,3,
    TVCMD_RESTART
};
static u32 alternate[] = {TVCMD_SETCMDLIST,0};
typedef struct { MonitorRecord screens[4]; u32 rng; } Snapshot;

static Snapshot Run(int count,int hidden,int combined,int orthogonal)
{
    Vertex original[5][4]={0};
    Gfx commands[128];
    union ModelRoData ro[5]={0};
    ModelNode nodes[7]={0};
    ModelFileHeader header={0};
    Model model={0};
    MultiMonitorObjRecord console={0};
    PropRecord prop={0};
    ModelRenderData data={0};
    Snapshot result;
    for(int i=0;i<5;i++) {
        for(int v=0;v<4;v++) { original[i][v].x=i*10+v; original[i][v].y=20-v; }
        ro[i].DisplayListCollisions.Vertices=original[i];
        ro[i].DisplayListCollisions.RwDataIndex=i;
        nodes[i].Opcode=MODELNODE_OPCODE_DLCOLLISION; nodes[i].Data=&ro[i];
        if(i) header.Switches[i-1]=&nodes[i];
        if(i<4) nodes[i].Next=&nodes[i+1];
        model.datas[i]=original[i];
    }
    nodes[4].Next=&nodes[5]; nodes[5].Opcode=MODELNODE_OPCODE_SWITCH;
    nodes[5].Next=&nodes[6]; nodes[6].Opcode=MODELNODE_OPCODE_LOD;
    header.RootNode=nodes; header.numMatrices=1;
    model.obj=&header; model.body=nodes;
    console.obj.model=&model; console.obj.type=count==1?PROPDEF_MONITOR:PROPDEF_MULTI_MONITOR;
    console.obj.flags=orthogonal?PROPFLAG_ORTHOGONAL:0;
    console.obj.state=3; /* Both bullet-mark passes need bookkeeping. */
    prop.obj=&console.obj; prop.flags=PROPRUNTIMEFLAG_ONSCREEN;
    for(int i=0;i<count;i++) {
        console.Monitor[i].cmdlist=script; console.Monitor[i].pause60=-1;
        console.Monitor[i].alpha=255; console.Monitor[i].xscale=console.Monitor[i].yscale=1;
    }
    script[1]=(u32)(uintptr_t)alternate; script[2]=32768;
    alternate[1]=(u32)(uintptr_t)&script[3];
    rng=123; impacts=impactDraws=viewConversions=worldConversions=0;
    for(int frame=0;frame<30;frame++) {
        int occluded=hidden==1 || (hidden==2 && frame%2==0);
        for(int pass=0;pass<2;pass++) {
            if(combined && pass==0) continue;
            vertexCount=textures=draws=relations=0;
            memset(commands,0,sizeof(commands));
            data.gdl=commands; data.PropType=PROP_TYPE_MAX;
            data.flags=(combined?3:pass?2:1)|(occluded?MODEL_RENDER_OCCLUDED:0);
            objRenderPropModel(&prop,&data,pass);
            assert(draws==(occluded?0:5) && relations==2);
            assert(vertexCount==((!pass||combined)?4*count:0));
            assert(textures==((!pass||combined)?count:0));
            /* Follow submitted branches: hidden screens must never execute. */
            if(occluded) for(Gfx *g=commands;g<data.gdl;) {
                assert(g->op==8); assert(g->target>g && g->target<=data.gdl); g=g->target;
            }
            if(!pass||combined) for(int i=0;i<count;i++) {
                union ModelRwData *rw=&model.rw[i+1];
                assert(rw->DisplayListCollisions.gdl>=commands && rw->DisplayListCollisions.gdl<data.gdl);
                assert(rw->DisplayListCollisions.Vertices==&vertexArena[i*4]);
                for(int v=0;v<4;v++) {
                    assert(rw->DisplayListCollisions.Vertices[v].x==original[i+1][v].x);
                    assert(rw->DisplayListCollisions.Vertices[v].r==console.Monitor[i].red);
                }
            }
        }
    }
    assert(impacts==30*(combined?1:2));
    assert(impactDraws==(hidden==1?0:hidden==2?impacts/2:impacts));
    assert(viewConversions==(orthogonal?0:30) && worldConversions==(orthogonal?30:0));
    memcpy(result.screens,console.Monitor,sizeof(result.screens)); result.rng=rng;
    /* Destruction while hidden must not rebuild its removed screens. */
    console.obj.state|=PROPSTATE_DESTROYED; objHideMonitorScreens(&console.obj);
    vertexCount=textures=draws=0; data.gdl=commands; data.flags=3|MODEL_RENDER_OCCLUDED;
    objRenderPropModel(&prop,&data,TRUE);
    assert(!vertexCount && !textures && !draws);
    for(int i=0;i<count;i++) assert(!model.rw[i+1].DisplayListCollisions.gdl);
    assert(!memcmp(result.screens,console.Monitor,sizeof(result.screens)) && rng==result.rng);
    console.obj.state=0; data.flags=1; data.gdl=commands;
    objRenderPropModel(&prop,&data,FALSE);
    assert(vertexCount==count*4 && draws==5); /* Respawn/reveal rebuilds screens. */
    return result;
}

int main(void)
{
    for(int count=1;count<=4;count+=3) for(int combined=0;combined<2;combined++)
        for(int ortho=0;ortho<2;ortho++) {
            Snapshot visible=Run(count,0,combined,ortho);
            for(int hidden=1;hidden<=2;hidden++) {
                Snapshot other=Run(count,hidden,combined,ortho);
                assert(visible.rng==other.rng);
                assert(!memcmp(visible.screens,other.screens,sizeof(other.screens)));
            }
        }
    puts("PASS: single/multi-monitor scripts and RNG identical through hide/reveal, scrolling, zoom, color, texture and pause changes.");
    puts("PASS: both/combined passes, no hidden model/screen draws, live collision data, relations, bullet marks, matrix cleanup, destruction and respawn.");
}
