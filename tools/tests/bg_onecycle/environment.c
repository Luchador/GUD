/* The optional fixture is saved/optimized by the production editor compiler.
 * Texture dispatch is real; the upload stub emits a descriptor lookup packet
 * so generated scales can be verified with rectangular images too. */
static void environment_checks(void)
{
    const char *path=getenv("GUD_ENV_FIXTURE");
    if(!path)return;
    FILE *fp=fopen(path,"rb");assert(fp);int streams=0,total=0;
    g_TestTexture.width=64;g_TestTexture.height=32;
    while(fgetc(fp)!=EOF) {
        fseek(fp,-1,SEEK_CUR);
        u32 layer=read_be(fp), expected=read_be(fp), vertexBytes=read_be(fp), bytes=read_be(fp);
        u8 *saved=malloc(vertexBytes), *vertices=malloc(vertexBytes);assert(saved&&vertices);
        assert(fread(saved,1,vertexBytes,fp)==vertexBytes);
        Gfx *raw=malloc(bytes),*expanded=malloc(bytes*3+64),*out=malloc(bytes*3+64);assert(raw&&expanded&&out);
        for(u32 i=0;i<bytes/8;i++){raw[i].words.w0=read_be(fp);raw[i].words.w1=read_be(fp);}
        /* Expand representative static textures, retaining all policy packets. */
        int size=texLoadFromGdl(raw,bytes,expanded,NULL);
        for(int enabled=0;enabled<2;enabled++)for(int aa=0;aa<2;aa++) {
            memcpy(vertices,saved,vertexBytes);memcpy(out,expanded,size);
            g_TestEnvironment.FogEnabled=enabled;
            enum CCRMLUT lut=enabled?(layer?CCRMLUT_SECONDARY_ADDFOG:CCRMLUT_PRIMARY_ADDFOG):(layer?CCRMLUT_SECONDARY:CCRMLUT_PRIMARY);
            bgApplyDynamicCCRMLUT(out,out+size/8,lut);
            bgApplyEnvironmentMapping(out,out+size/8,(Vtx *)vertices,vertexBytes);
            BgOneCycleState state;bgOneCycleResetState(&state);
            u32 geo=0,normal=0,mapped=0, texture=0xffffffffu;
            Gfx direction,ambient,lightCount;
            gSPLight(&direction,&GlobalLight.l[0],1);gSPLight(&ambient,&GlobalLight.a,2);
            gSPNumLights(&lightCount,NUMLIGHTS_1);
            u32 actualDirection=direction.words.w1,actualAmbient=ambient.words.w1;
            for(int i=0;i<size/8;i++) {
                Gfx cmd=aa?out[i]:renderGetAaOffCommand(out[i]);u32 op=cmd.words.w0>>24;
                assert(bgOneCycleReadState(&state,cmd,TRUE));
                if(op==(u8)G_SETGEOMETRYMODE)geo|=cmd.words.w1;
                if(op==(u8)G_CLEARGEOMETRYMODE)geo&=~cmd.words.w1;
                if(op==(u8)G_TEXTURE)texture=cmd.words.w1;
                if(cmd.words.w0==direction.words.w0)actualDirection=cmd.words.w1;
                if(cmd.words.w0==ambient.words.w0)actualAmbient=cmd.words.w1;
                if(cmd.words.w0==lightCount.words.w0)assert(cmd.words.w1==lightCount.words.w1);
                if(BG_ENV_IS_NORMAL(cmd.words.w0,cmd.words.w1))normal=cmd.words.w1&0xffffffu;
                if(op!=(u8)G_VTX)continue;
                u32 offset=cmd.words.w1&0xffffffu, count=((cmd.words.w0>>20)&15)+1;
                assert(offset+count*16<=vertexBytes);
                if(BG_ENV_GENERATED(state.environmentPolicy)) {
                    assert(count==3 && (geo&(G_LIGHTING|G_TEXTURE_GEN))==(G_LIGHTING|G_TEXTURE_GEN));
                    assert(!!(geo&G_TEXTURE_GEN_LINEAR)==(state.environmentPolicy==BG_ENV_LINEAR));
                    assert(texture==((64u*64u<<16)|(32u*64u)));
                    assert(actualDirection==(u32)(uintptr_t)&g_BgEnvironmentLight.l[0]);
                    assert(actualAmbient==(u32)(uintptr_t)&g_BgEnvironmentLight.a);
                    BgOneCycleState chosen;assert(!bgOneCycleChooseState(&state,&chosen,FALSE,TRUE,FALSE));
                    for(u32 v=0;v<3;v++) {
                        u8 *p=vertices+offset+v*16,*old=saved+offset+v*16;
                        assert(!memcmp(p,old,12)&&p[15]==old[15]);
                        assert(p[12]==(u8)(normal>>16)&&p[13]==(u8)(normal>>8)&&p[14]==(u8)normal);
                    }
                    mapped++;
                } else {
                    assert(!(geo&(G_LIGHTING|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR)));
                    assert(actualDirection==direction.words.w1&&actualAmbient==ambient.words.w1);
                    assert(!memcmp(vertices+offset,saved+offset,count*16));
                }
            }
            assert(mapped==expected && state.environmentPolicy==BG_ENV_AUTO);
            assert(!(geo&(G_LIGHTING|G_TEXTURE_GEN|G_TEXTURE_GEN_LINEAR)));
            assert(actualDirection==direction.words.w1&&actualAmbient==ambient.words.w1);
        }
        streams++;total+=expected;free(raw);free(expanded);free(out);free(vertices);free(saved);
    }
    fclose(fp);assert(total>0);
    printf("PASS environment runtime: %d compiled streams, %d mapped triangles; native scopes, private normals, scales, fog and both AA modes.\n",streams,total);
}
