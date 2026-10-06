/* File-relative imports must read the hit instance, including loads from a
 * neighbouring part and a DMA crossing the boundary between two buffers. */
static void check_character_blood(void)
{
    Gfx *src=(Gfx *)(g_TestRam+0x10000), *master=(Gfx *)(g_TestRam+0x5000);
    Vertex *vertices=(Vertex *)(g_TestRam+0x20000), *blood=(Vertex *)(g_TestRam+0x24000);
    union ModelRoData ro[2]={0};union ModelRwData hit[2]={0},clean[2]={0};
    ModelNode nodes[2]={{&ro[0]},{&ro[1]}};Model wounded={hit},intact={clean};
    ModelRenderData data=character_data();ModelNodeRenderCache state={0};
    const Gfx commands[]={
        gsDPSetCombineMode(G_CC_TRILERP,G_CC_MODULATEIA2),
        {{0x04300040,0x05010000}}, /* four vertices, two in each node */
        {{0xbf000000,0x00000a14}},gsSPEndDisplayList()
    };
    reset();frameBytes=0;memcpy(src,commands,sizeof(commands));
    memset(vertices,255,4*sizeof(Vertex));memcpy(blood,vertices,4*sizeof(Vertex));
    blood[0].a=30;blood[2].a=40;
    for(int i=0;i<2;i++) {
        ro[i].DisplayListCollisions=(ModelRoData_DisplayListRecord){0};
        ro[i].DisplayListCollisions.Primary=src;ro[i].DisplayListCollisions.BaseAddr=src;
        ro[i].DisplayListCollisions.ModelType=3;ro[i].DisplayListCollisions.Vertices=vertices+i*2;
        ro[i].DisplayListCollisions.numVertices=2;
        nodes[i].Opcode=MODELNODE_OPCODE_DLCOLLISION;nodes[i].rwIndex=i;
        clean[i].DisplayListCollisions.gdl=hit[i].DisplayListCollisions.gdl=src;
        clean[i].DisplayListCollisions.Vertices=vertices+i*2;
        hit[i].DisplayListCollisions.Vertices=blood+i*2;
    }
    nodes[0].Child=&nodes[1];nodes[1].Parent=&nodes[0];
    for(int aa=0;aa<2;aa++) for(int fade=0;fade<2;fade++) {
        renderSetAaEnabled(aa);renderApplySettings();
        for(int part=0;part<2;part++) {
            hit[0].DisplayListCollisions.Vertices=part ? vertices : blood;
            hit[1].DisplayListCollisions.Vertices=part ? blood+2 : vertices+2;
            data=character_data();data.PropType=fade?8:7;data.envcolour.word=0x5a000080;
            state=(ModelNodeRenderCache){0};data.gdl=master;
            modelRenderNodeDlWithCache(&data,&wounded,&nodes[0],&state);
            Gfx *draw=(Gfx *)(g_TestRam+data.gdl[-1].words.w1);
            int loads=0;unsigned slots=0;
            for(Gfx *g=draw;g->words.w0>>24!=0xb8;g++) if(g->words.w0>>24==4) {
                int first=(g->words.w0>>16)&15,n=((g->words.w0>>20)&15)+1;
                for(int j=0;j<n;j++) {
                    unsigned slot=first+j,address=g->words.w1+j*16;
                    assert(slot<4 && !(slots&(1u<<slot)));slots|=1u<<slot;
                    if(slot/2==(unsigned)part) assert(address==K0_TO_PHYS(blood+slot));
                    else assert(address==0x05010000+slot*16);
                }
                loads++;
            }
            assert(loads==2 && slots==15);
            assert(!memcmp(src,commands,sizeof(commands))); /* never edit shared data */
            Gfx saved[32];int bytes=modelOneCycleListSize(draw);assert(bytes<=sizeof(saved));memcpy(saved,draw,bytes);
            /* Another player sharing the model must not inherit the wound. */
            data.gdl=master;modelRenderNodeDlWithCache(&data,&intact,&nodes[0],&state);
            Gfx *other=(Gfx *)(g_TestRam+data.gdl[-1].words.w1);
            int found=0;
            for(Gfx *g=other;g->words.w0>>24!=0xb8;g++) if(g->words.w0>>24==4) {assert(g->words.w1==0x05010000);found++;}
            assert(found==1 && !memcmp(saved,draw,bytes));
            frameBytes=0;
        }
    }
    renderCacheRequestReclaim();renderCacheReclaim();frameBytes=0;
    state=(ModelNodeRenderCache){0};data.gdl=master;data.PropType=7;
    modelRenderNodeDlWithCache(&data,&wounded,&nodes[0],&state);
    assert(frameBytes>0 && !renderCacheIsEnabled());
    puts("Imported character blood: own/cross-part buffers, split DMA loads, per-player isolation, AA/fade and cache-reclaim fallback pass.");
}
