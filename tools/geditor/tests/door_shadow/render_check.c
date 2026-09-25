static u32 be32(u8 *p);
static void word(u8 *p,int offset,u32 value);

/* Walk effective hardware state at every draw, starting with the world pass's
 * dithering. Fog geometry alone is insufficient: check its blender too. */
static void checkShadowDraws(Gfx *gdl,int oneCycle,int fog,u32 dither)
{
    BgOneCycleState state;
    u32 geometry=0;
    u32 rspHigh=0xef000000u|dither;
    int triangles=0;
    bgOneCycleResetState(&state);
    state.high=dither;
    for(Gfx *p=gdl;(p->words.w0>>24)!=(u8)G_ENDDL;p++) {
        u32 op=p->words.w0>>24;
        assert(bgOneCycleReadState(&state,*p,FALSE));
        if(op==(u8)G_SETOTHERMODE_H) {
            rspHigh=shadowRspOtherMode(rspHigh,p->words.w0,p->words.w1);
            assert((rspHigh>>24)==0xef);
            assert((rspHigh&0x00ffffffu)==state.high);
        }
        if(op==(u8)G_SETGEOMETRYMODE)geometry|=p->words.w1;
        if(op==(u8)G_CLEARGEOMETRYMODE)geometry&=~p->words.w1;
        if(op==(u8)G_VTX)assert(!!(geometry&G_FOG)==fog);
        if(op!=(u8)G_TRI1)continue;
        assert((state.high&0xf0u)==dither);
        assert((state.high&BG_CYCLE_MASK)==(oneCycle?G_CYC_1CYCLE:G_CYC_2CYCLE));
        assert(!!(state.high&G_TL_LOD)==!oneCycle);
        if(fog) {
            assert((state.low&BG_FIRST_BLENDER_MASK)==(G_RM_FOG_SHADE_A));
            if(oneCycle)assert(state.low&FORCE_BL);
        } else if(!oneCycle)assert((state.low&BG_FIRST_BLENDER_MASK)==(G_RM_PASS));
        triangles++;
    }
    assert(triangles==6);
    /* HUD setup uses further partial writes; none can repair a lost EF
     * opcode. Exercise the production setup after each shadow variant. */
    Gfx hud[32],*end=gfxSetup2DTextureMode(hud);
    for(Gfx *p=hud;p<end;p++) {
        u32 op=p->words.w0>>24;
        if(op==(u8)G_SETOTHERMODE_H)
            rspHigh=shadowRspOtherMode(rspHigh,p->words.w0,p->words.w1);
        if(op==(u8)G_SETOTHERMODE_H||op==(u8)G_SETOTHERMODE_L)
            assert((rspHigh>>24)==0xef);
    }
    assert(!(rspHigh&(BG_CYCLE_MASK|BG_LOD_MASK|G_TP_PERSP|(3u<<G_MDSFT_TEXTLUT))));
}

static Gfx *drawShadow(Gfx *output,int layer,int oneCycle)
{
    frameVertexCount=frameMatrixCount=0;
    frameBytes=SHADOW_FRAME_BYTES;
    return doorShadowRenderRoom(output,1,layer,oneCycle);
}

static void renderChecks(u8 *fixture)
{
    u32 storage[DOOR_SHADOW_BYTES/4+1]={0};
    u8 *p=(u8 *)storage;
    Gfx output[32];
    const u32 headers[]={0xba000020u,0xba000818u,0xba000810u};
    for(int version=0;version<3;version++) for(int fog=0;fog<2;fog++) for(int layer=0;layer<2;layer++) {
        u8 saved[DOOR_SHADOW_BYTES];
        memcpy(p,fixture,DOOR_SHADOW_BYTES);
        p[DOOR_SHADOW_BYTES+3]=PROPDEF_END;
        word(p,DOOR_SHADOW_LAYER,layer);
        Gfx *raw=(Gfx *)(p+DOOR_SHADOW_GDL);
        /* Depot's ordinary mipmapped, opaque terrain material. */
        raw[4].words.w0=headers[version];
        raw[4].words.w1=G_CYC_2CYCLE|G_TL_LOD|G_TP_PERSP|G_TF_BILERP|G_TC_FILT;
        gDPSetRenderMode(&raw[5],G_RM_PASS,G_RM_AA_ZB_OPA_TERR2);
        memcpy(saved,p,DOOR_SHADOW_BYTES);
        renderCacheReset();g_TestEnvironment.FogEnabled=fog;
        doorShadowInit((PropDefHeaderRecord *)p);
        assert(drawShadow(output,layer,FALSE)==output+5);
        DoorShadowRuntime *s=&g_DoorShadows[0];
        assert(s->gdl&&s->oneCycleGdl);
        assert(!memcmp(p,saved,DOOR_SHADOW_BYTES));
        for(int choice=0;choice<4;choice++) {
            int oneCycle=choice&1;
            Gfx *end=drawShadow(output,layer,oneCycle);
            assert(end==output+5+(oneCycle&&layer?3:0));
            assert((end[-3].words.w0>>24)==(u8)G_DL);
            assert(end[-3].words.w1==OS_K0_TO_PHYSICAL(oneCycle?s->oneCycleGdl:s->gdl));
            if(oneCycle&&layer) {
                Gfx expected;
                gDPSetBlendColor(&expected,0,0,0,BG_CUTOUT_THRESHOLD);
                assert(!memcmp(end-4,&expected,sizeof(expected)));
            }
            for(int color=0;color<4;color++)for(int alpha=0;alpha<4;alpha++)
                checkShadowDraws(oneCycle?s->oneCycleGdl:s->gdl,oneCycle,fog,(color<<6)|(alpha<<4));
        }
        /* Required allocation failure can reclaim only optional copies, after
         * all submitted frames drain. Room unload must not double-free them. */
        Gfx *original=s->gdl,*alternate=s->oneCycleGdl;
        renderCacheRequestReclaim();
        assert(renderCacheReclaimPending()&&s->oneCycleGdl==alternate);
        renderCacheReclaim();
        assert(s->gdl==original&&!s->oneCycleGdl&&!renderCacheReclaimPending());
        assert(drawShadow(output,layer,TRUE)==output+5);
        assert(output[2].words.w1==OS_K0_TO_PHYSICAL(original));
        doorShadowFreeRoom(1);assert(allocations==frees);
        /* Failure of the optional allocation still draws the original. */
        renderCacheReset();doorShadowInit((PropDefHeaderRecord *)p);failAfter=1;
        assert(drawShadow(output,layer,TRUE)==output+5);failAfter=-1;
        assert(s->gdl&&!s->oneCycleGdl);
        doorShadowFreeRoom(1);assert(allocations==frees);
        /* Ordinary unloading frees both lists, and reloading builds fresh ones. */
        doorShadowInit((PropDefHeaderRecord *)p);
        for(int reload=0;reload<2;reload++) {
            assert(drawShadow(output,layer,FALSE)==output+5&&s->oneCycleGdl);
            doorShadowFreeRoom(1);assert(!s->gdl&&!s->oneCycleGdl&&allocations==frees);
        }
        /* Custom materials stay on their authored path. */
        for(int i=0;i<(int)doorShadowWord(p,DOOR_SHADOW_GDL_SIZE)/8;i++)
            if((raw[i].words.w0>>24)==(u8)G_SETCOMBINE) {
                /* A custom combiner is not eligible for one-cycle conversion. */
                gDPSetCombineMode(&raw[i],G_CC_PRIMITIVE,G_CC_PASS2);
            }
        doorShadowInit((PropDefHeaderRecord *)p);
        assert(drawShadow(output,layer,TRUE)==output+5&&!s->oneCycleGdl);
        doorShadowFreeRoom(1);assert(allocations==frees);
    }
    puts("PASS render parity: all three header versions, RSP opcode and subsequent HUD setup, inherited colour/fog dithering, AA On/Off mip selection, both passes, fog blender, cache reclaim/reload/failure and custom-material fallback.");
}

/* Optional real saved records, read-only. Keep ROM/setup assets out of tests. */
static void savedShadowChecks(const char *path)
{
    u32 storage[DOOR_SHADOW_BYTES/4+1]={0};
    u8 *p=(u8 *)storage,raw[DOOR_SHADOW_BYTES];
    FILE *fp=fopen(path,"rb");assert(fp&&fread(raw,1,sizeof(raw),fp)==sizeof(raw));fclose(fp);
    memcpy(p,raw,sizeof(raw));
    for(int at=4;at<DOOR_SHADOW_VERTICES;at+=4)word(p,at,be32(raw+at));
    for(int v=0;v<6;v++)for(int field=0;field<6;field++) {
        int at=DOOR_SHADOW_VERTICES+v*16+field*2;
        s16 value=(s16)((u16)raw[at]<<8|raw[at+1]);memcpy(p+at,&value,2);
    }
    for(int at=DOOR_SHADOW_GDL;at<DOOR_SHADOW_BYTES;at+=4)word(p,at,be32(raw+at));
    word(p,DOOR_SHADOW_ROOM,1); // host harness has one room
    p[DOOR_SHADOW_BYTES+3]=PROPDEF_END;
    Gfx output[32];
    int layer=doorShadowWord(p,DOOR_SHADOW_LAYER);
    renderCacheReset();g_TestEnvironment.FogEnabled=TRUE;
    doorShadowInit((PropDefHeaderRecord *)p);
    assert(drawShadow(output,layer,FALSE)==output+5);
    assert(g_DoorShadows[0].oneCycleGdl);
    checkShadowDraws(g_DoorShadows[0].gdl,FALSE,TRUE,G_CD_BAYER|G_AD_NOISE);
    checkShadowDraws(g_DoorShadows[0].oneCycleGdl,TRUE,TRUE,G_CD_BAYER|G_AD_NOISE);
    doorShadowFreeRoom(1);assert(allocations==frees);
    printf("PASS saved shadow: %s\n",path);
}
