/* Exercise the compiler's joint fog/alpha scope contract with the real
 * texture dispatcher, environment LUT, AA walker and one-cycle optimizer. */
static Gfx *fog_scope(Gfx *p,u32 fog,u32 alpha)
{
    gDPNoOpTag(p++,BG_ALPHA_TAG|alpha);
    gDPNoOpTag(p++,BG_FOG_TAG|fog);
    gDPNoOpTag(p++,BG_ALPHA_TAG|BG_ALPHA_SYNC);
    gDPNoOpTag(p++,BG_FOG_TAG|BG_FOG_CYCLE);
    gDPNoOpTag(p++,BG_ALPHA_TAG|BG_ALPHA_FOG);
    for(u32 s=BG_ALPHA_BLENDER;s<=BG_ALPHA_LAST_SLOT;s++)gDPNoOpTag(p++,BG_ALPHA_TAG|s);
    for(u32 s=BG_FOG_BLENDER;s<=BG_FOG_LAST_SLOT;s++)gDPNoOpTag(p++,BG_FOG_TAG|s);
    return p;
}

static double fog_rgb_cycle(Gfx combine,int cycle,double combined,double tex0,double tex1,double shade)
{
    u32 w0=combine.words.w0,w1=combine.words.w1;
    u32 a=cycle?(w0>>5)&15:(w0>>20)&15;
    u32 b=cycle?(w1>>24)&15:(w1>>28)&15;
    u32 c=cycle?w0&31:(w0>>15)&31;
    u32 d=cycle?(w1>>6)&7:(w1>>15)&7;
    double inputs[32]={combined,tex0,tex1,.7,shade,.3,1};
    return (inputs[a]-inputs[b])*inputs[c]+inputs[d];
}

static void fog_participation_checks(void)
{
    const u32 alphas[]={BG_ALPHA_AUTO,BG_ALPHA_OPAQUE,BG_ALPHA_TEXTURE,BG_ALPHA_CONSTANT,BG_ALPHA_TEXTURE_CONSTANT};
    for(int single=0;single<2;single++) for(int enabled=0;enabled<2;enabled++)
    for(int layer=0;layer<2;layer++) for(int cleared=0;cleared<2;cleared++)
    for(int material=0;material<3;material++)
    for(unsigned int a=0;a<sizeof(alphas)/sizeof(*alphas);a++) {
        Gfx raw[128],expanded[160],reference[8],alternate[200],*p=raw;
        Gfx *runtime=(Gfx *)(g_TestRam+0x30000);
        Gfx mux;
        if(material==0) { gDPSetCombineMode(&mux,G_CC_MODULATEI,G_CC_MODULATEI); }
        else if(material==1) { gDPSetCombineMode(&mux,G_CC_MODULATEIA,G_CC_MODULATEIA); }
        else { gDPSetCombineMode(&mux,G_CC_SHADE,G_CC_SHADE); }
        if(!single) { mux.words.w0=(mux.words.w0&~0x1ffu)|0x1ffu; mux.words.w1=(mux.words.w1&0xf003fe00u)|0x0ffc0038u; }
        u32 low=single?(G_RM_AA_ZB_OPA_TERR|G_RM_AA_ZB_OPA_TERR2):(G_RM_PASS|G_RM_AA_ZB_OPA_TERR2);
        enum CCRMLUT lut=enabled?(layer?CCRMLUT_SECONDARY_ADDFOG:CCRMLUT_PRIMARY_ADDFOG):(layer?CCRMLUT_SECONDARY:CCRMLUT_PRIMARY);
        g_TestEnvironment.FogEnabled=enabled;
        gDPPipeSync(p++);
        gSPSetOtherMode(p++,G_SETOTHERMODE_H,0,23,single?G_CYC_1CYCLE:G_CYC_2CYCLE);
        gDPSetRenderMode(p++,low,0);
        gDPSetEnvColor(p++,0x12,0x34,0x56,128);
        if(cleared) {gSPClearGeometryMode(p++,G_FOG);}
        else {gSPSetGeometryMode(p++,enabled?G_FOG:0);}
        *p++=mux;
        memcpy(reference,raw,(p-raw)*8);gSPEndDisplayList(&reference[p-raw]);
        bgApplyDynamicCCRMLUT(reference,NULL,lut);
        Gfx original=reference[p-raw-1]; u32 originalLow=reference[2].words.w1;
        for(int draw=0;draw<4;draw++) {
            if(draw) p=fog_scope(p,draw==1?BG_FOG_ON:draw==2?BG_FOG_OFF:BG_FOG_AUTO,draw==3?BG_ALPHA_AUTO:alphas[a]);
            *p++=mux;
            p->words.w0=0x04200030;p++->words.w1=0x0e000000;
            gSP1Triangle(p++,0,1,2,0);
        }
        gSPEndDisplayList(p++);
        int requests=g_TestTextureRequests;
        int bytes=texLoadFromGdl(raw,(p-raw)*8,expanded,NULL);
        assert(bytes==(p-raw)*8 && requests==g_TestTextureRequests);
        bgApplyDynamicCCRMLUT(expanded,expanded+bytes/8,lut);
        memcpy(runtime,expanded,bytes);
        /* Explicit fog draws must not be undone by AA-off one-cycle selection. */
        assert(bgBuildOneCycleGdl(expanded,bytes,alternate,sizeof(alternate))>=0);
        for(int aa=0;aa<3;aa++) {
            renderSetAaEnabled(aa!=1);renderApplySettings();
            assert(renderApplyDisplayListSettings(runtime,runtime+bytes/8));
            BgOneCycleState state;bgOneCycleResetState(&state);
            int draws=0,activeFog=enabled&&!cleared;
            for(int i=0;i<bytes/8;i++) {
                Gfx cmd=runtime[i];u32 op=cmd.words.w0>>24;
                assert(bgOneCycleReadState(&state,cmd,TRUE));
                if(op==(u8)G_SETGEOMETRYMODE && (cmd.words.w1&G_FOG))activeFog=1;
                if(op==(u8)G_CLEARGEOMETRYMODE && (cmd.words.w1&G_FOG))activeFog=0;
                if(op==(u8)G_VTX) assert(activeFog==(draws==1?enabled:draws==2?0:enabled&&!cleared));
                if(op!=(u8)G_TRI1)continue;
                int promoted=draws==1&&single&&enabled;
                assert((state.high&BG_CYCLE_MASK)==(single&&!promoted?G_CYC_1CYCLE:G_CYC_2CYCLE));
                if(draws==1||draws==2) {
                    BgOneCycleState chosen;
                    assert(!bgOneCycleChooseState(&state,&chosen,FALSE,TRUE,FALSE));
                    u32 first=single&&!promoted?originalLow&0xcccc0000u:draws==1&&enabled?G_RM_FOG_SHADE_A:G_RM_PASS;
                    u32 second=promoted?(originalLow&0xcccc0000u)>>2:originalLow&0x33330000u;
                    assert((state.low&0xffff0000u)==(first|second));
                    assert(state.envAlpha==128);
                    for(int sample=0;sample<7;sample++) {
                        double tex=sample/6.0,shade=.27,other=1-tex;
                        double rgb0=fog_rgb_cycle(state.combine,0,0,tex,other,shade);
                        double rgb=fog_rgb_cycle(state.combine,1,single&&!promoted?0:rgb0,
                            single&&!promoted?tex:other,single&&!promoted?other:tex,shade);
                        double expected=material==2?shade:tex*shade;
                        assert(rgb-expected<1e-12 && expected-rgb<1e-12);
                        if(alphas[a]!=BG_ALPHA_AUTO || promoted) {
                            double opacity=128.0/255;
                            double expectedAlpha=alphas[a]==BG_ALPHA_TEXTURE||alphas[a]==BG_ALPHA_TEXTURE_CONSTANT?tex:1;
                            if(alphas[a]==BG_ALPHA_CONSTANT||alphas[a]==BG_ALPHA_TEXTURE_CONSTANT)expectedAlpha*=opacity;
                            if(alphas[a]==BG_ALPHA_AUTO)expectedAlpha=material==1?tex*opacity:opacity;
                            double alpha0=preset_alpha_cycle(state.combine,0,0,tex,other,shade,opacity,0);
                            double alpha=preset_alpha_cycle(state.combine,1,single&&!promoted?0:alpha0,tex,other,shade,opacity,0);
                            assert(alpha-expectedAlpha<1e-12 && expectedAlpha-alpha<1e-12);
                        }
                    }
                } else {
                    assert(state.fogPolicy==BG_FOG_AUTO && state.alphaSource==BG_ALPHA_AUTO);
                    assert((state.low&0xffff0000u)==(originalLow&0xffff0000u));
                    assert(!memcmp(&state.combine,&original,sizeof(Gfx)));
                }
                draws++;
            }
            assert(draws==4);
        }
        assert(!memcmp(runtime,expanded,bytes));
    }
    /* Standard two-cycle Auto opacity must remain independent of fog in
     * primary as well as secondary streams, for both On and Off. */
    for(int layer=0;layer<2;layer++) for(u32 policy=BG_FOG_ON;policy<=BG_FOG_OFF;policy++) {
        Gfx commands[32],*p=commands; BgOneCycleState state;
        g_TestEnvironment.FogEnabled=TRUE;
        gSPSetOtherMode(p++,G_SETOTHERMODE_H,0,23,G_CYC_2CYCLE|G_TL_LOD);
        gDPSetRenderMode(p++,G_RM_PASS,G_RM_AA_ZB_XLU_SURF2);
        p=fog_scope(p,policy,BG_ALPHA_AUTO);
        gDPSetCombineMode(p++,G_CC_TRILERP,G_CC_MODULATEIA2);
        gSPEndDisplayList(p++);
        bgApplyDynamicCCRMLUT(commands,p,layer?CCRMLUT_SECONDARY_ADDFOG:CCRMLUT_PRIMARY_ADDFOG);
        bgOneCycleResetState(&state);
        for(Gfx *q=commands;q<p;q++) assert(bgOneCycleReadState(&state,*q,TRUE));
        double first=preset_alpha_cycle(state.combine,0,0,.2,.8,.1,.6,.5);
        double result=preset_alpha_cycle(state.combine,1,first,.2,.8,.1,.6,.5);
        assert(result-.3<1e-12 && .3-result<1e-12);
    }
    /* A convertible ordinary material proves that the fog policy itself
     * blocks optimization, rather than unrelated missing texture state. */
    {
        BgOneCycleState native, chosen;
        bgOneCycleResetState(&native);
        for (unsigned int i=0;i<sizeof(standard)/sizeof(*standard);i++)
            assert(bgOneCycleReadState(&native,standard[i],FALSE));
        assert(bgOneCycleChooseState(&native,&chosen,FALSE,FALSE,FALSE));
        native.fogPolicy=BG_FOG_ON;
        assert(!bgOneCycleChooseState(&native,&chosen,FALSE,FALSE,FALSE));
        native.fogPolicy=BG_FOG_OFF;
        assert(!bgOneCycleChooseState(&native,&chosen,FALSE,FALSE,FALSE));
    }
    puts("PASS fog: one-cycle Runway promotion, independent RGB/alpha equations, On/Off/Auto restoration, all LUTs, geometry scopes and AA toggles.");
}
