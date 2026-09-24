static Gfx *renderFrame(Gfx *gdl,s32 room,s32 layer)
{
    frameVertexCount=frameMatrixCount=0;frameBytes=freeBytes;
    Gfx *end=doorShadowRenderRoom(gdl,room,layer);
    if(end!=gdl) {
        assert(frameVertexCount==frameMatrixCount);
        assert(end==gdl+3*frameVertexCount+2);
        for(int i=0;i<frameVertexCount;i++) {
            Gfx expected;
            gSPMatrix(&expected,OS_K0_TO_PHYSICAL(&frameMatrixBuffers[i]),G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
            assert(!memcmp(gdl+3*i,&expected,sizeof(expected)));
            gSPSegment(&expected,SPSEGMENT_BG_VTX,OS_K0_TO_PHYSICAL(frameVertexBuffers[i]));
            assert(!memcmp(gdl+3*i+1,&expected,sizeof(expected)));
            assert((gdl[3*i+2].words.w0>>24)==(u8)G_DL);
        }
        Gfx expected;
        gSPMatrix(&expected,OS_K0_TO_PHYSICAL(&roomMatrix),G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);
        assert(!memcmp(end-2,&expected,sizeof(expected)));
        gSPSegment(&expected,SPSEGMENT_BG_VTX,OS_K0_TO_PHYSICAL(g_BgRoomInfo[room].vertices));
        assert(!memcmp(end-1,&expected,sizeof(expected)));
    }
    return end;
}
static u32 be32(u8 *p) { return (u32)p[0]<<24|(u32)p[1]<<16|(u32)p[2]<<8|p[3]; }
static void word(u8 *p,int offset,u32 value) { memcpy(p+offset,&value,4); }
static double area(Vtx *v)
{
    double x[3],z[3];
    for(int i=0;i<3;i++) {x[i]=transformedPosition(v+i,&frameMatrix,0);z[i]=transformedPosition(v+i,&frameMatrix,2);}
    return ((x[1]-x[0])*(z[2]-z[0])-(z[1]-z[0])*(x[2]-x[0]))/2;
}
static double rgb(Gfx command,int cycle,double combined,double texture,double shade)
{
    u32 a=command.words.w0,b=command.words.w1;
    double inputs[32]={combined,texture,texture,.7,shade,.3,1};
    return (inputs[cycle?(a>>5)&15:(a>>20)&15]-inputs[cycle?(b>>24)&15:(b>>28)&15])
        *inputs[cycle?a&31:(a>>15)&31]+inputs[cycle?(b>>6)&7:(b>>15)&7];
}
static void precisionChecks(u8 *p)
{
    Gfx output[16];
    u8 saved[DOOR_SHADOW_BYTES];memcpy(saved,p,sizeof(saved));
    double worstPosition=0,worstUV=0;
    int fewestSteps=1000;
    // The same 100-unit surface near and far from its original room origin.
    for(int offset=0;offset<=30000;offset+=30000) for(int direction=0;direction<4;direction++) {
        memcpy(p,saved,sizeof(saved));
        word(p,DOOR_SHADOW_DIRECTION,direction);word(p,DOOR_SHADOW_DOOR,1);
        for(int v=0;v<6;v++) for(int a=0;a<3;a+=2) {
            s16 *coordinate=(s16 *)(p+DOOR_SHADOW_VERTICES+v*16+a*2);
            *coordinate+=offset;
        }
        testPlayer.current_model_pos.f[0]=offset;testPlayer.current_model_pos.f[2]=offset;
        doorShadowInit((PropDefHeaderRecord *)p);
        g_TestDoor.maxFrac=100;freeBytes=SHADOW_FRAME_BYTES;
        double previous=-1;int steps=0;
        for(int tick=0;tick<=100;tick++) {
            g_TestDoor.openPosition=37+tick*.01f;
            assert(renderFrame(output,1,0)==output+5);
            double edge=-1,expected=(direction&1)?100-g_TestDoor.openPosition:g_TestDoor.openPosition;
            for(int v=0;v<18;v++) {
                double x=transformedPosition(frameVertices+v,&frameMatrix,0);
                double z=transformedPosition(frameVertices+v,&frameMatrix,2);
                assert(x>=0&&x<=100&&z>=0&&z<=100);
                double coordinate=direction<2?x:z;
                if(coordinate>0&&coordinate<100) {
                    if(edge<0)edge=coordinate;else assert(edge==coordinate); // shared boundary
                }
                double u=v<9?64*x-34*z:30*x;
                double uvError=fmax(fabs(frameVertices[v].v.tc[0]-u),fabs(frameVertices[v].v.tc[1]-32*z));
                worstUV=fmax(worstUV,uvError);
                assert(uvError<=.501); // half of one native 1/32-texel step
                assert(frameVertices[v].v.cn[3]==255);
            }
            assert(edge>=0);
            double error=fabs(edge-expected);
            worstPosition=fmax(worstPosition,error);
            // The far-from-origin input has a small additional f32 ULP error.
            assert(error<=1.0/128+.002);
            if(tick) {
                assert((direction&1)?edge<=previous:edge>=previous);
                if(edge!=previous)steps++;
                assert(fabs(edge-previous)<=1.0/64+.00001);
            }
            previous=edge;
        }
        if(steps<fewestSteps)fewestSteps=steps;
        assert(steps==64); // The old renderer only changed position once here.
        doorShadowFreeRoom(1);
    }
    memset(&testPlayer,0,sizeof(testPlayer));
    // Full s16 X span must fit without wrapping; Z can retain fine precision.
    memcpy(p,saved,sizeof(saved));word(p,DOOR_SHADOW_DIRECTION,2);word(p,DOOR_SHADOW_DOOR,1);
    for(int v=0;v<6;v++) {
        s16 *x=(s16 *)(p+DOOR_SHADOW_VERTICES+v*16);*x=*x?32767:-32768;
    }
    doorShadowInit((PropDefHeaderRecord *)p);g_TestDoor.openPosition=37.37f;
    assert(renderFrame(output,1,0)==output+5);
    double zEdge=0;
    for(int v=0;v<18;v++) {
        double x=transformedPosition(frameVertices+v,&frameMatrix,0);
        double z=transformedPosition(frameVertices+v,&frameMatrix,2);
        assert(x>=-32768&&x<=32767);
        if(z>0&&z<100)zEdge=z;
    }
    assert(fabs(zEdge-37.37)<1.0/128);
    g_TestDoor.openPosition=0;assert(renderFrame(output,1,0)==output+5);
    int foundLow=0,foundHigh=0;
    for(int v=0;v<18;v++) {foundLow|=frameVertices[v].v.ob[0]==-32768;foundHigh|=frameVertices[v].v.ob[0]==32767;}
    assert(foundLow&&foundHigh);doorShadowFreeRoom(1);
    // Vertical and sloping planes, non-unit level scale, player translation,
    // and the actual fixed-point conversion (including global render scale).
    for(int plane=0;plane<3;plane++) {
        memcpy(p,saved,sizeof(saved));word(p,DOOR_SHADOW_DIRECTION,0);word(p,DOOR_SHADOW_DOOR,1);
        for(int v=0;v<6;v++) {
            s16 *xyz=(s16 *)(p+DOOR_SHADOW_VERTICES+v*16);
            if(plane==0){xyz[1]=xyz[2];xyz[2]=17;}
            if(plane==1){xyz[1]=xyz[0];xyz[0]=-29;word(p,DOOR_SHADOW_DIRECTION,2);}
            if(plane==2)xyz[1]=2*xyz[0]+3*xyz[2];
            p[DOOR_SHADOW_VERTICES+v*16+15]=(v%3)*100;
        }
        for(int a=0;a<3;a++) {
            f32 origin=(a-1)*8192.25f;memcpy(p+DOOR_SHADOW_ORIGIN+a*4,&origin,4);
            testPlayer.current_model_pos.f[a]=origin*4.285714f+12.75f;
        }
        doorShadowInit((PropDefHeaderRecord *)p);
        g_LevelInverseScale=4.285714f;g_MtxConversionScale[0]=65536*.4f;
        g_TestDoor.openPosition=37.37f;
        assert(renderFrame(output,1,0)==output+5);
        DoorShadowPoint ideal[18];DoorShadowRuntime *s=&g_DoorShadows[0];
        doorShadowSplit(s->source,doorShadowWord(p,DOOR_SHADOW_DIRECTION),g_TestDoor.openPosition/100,
            doorShadowWord(p,DOOR_SHADOW_LIGHT),doorShadowWord(p,DOOR_SHADOW_DARK),ideal);
        for(int v=0;v<18;v++) for(int a=0;a<3;a++) {
            double actual=transformedPosition(frameVertices+v,&frameMatrix,a);
            double expected=((ideal[v].position[a]+*(f32 *)(p+DOOR_SHADOW_ORIGIN+a*4))
                *g_LevelInverseScale-testPlayer.current_model_pos.f[a])*.4;
            // Includes 16.16 matrix quantization, vertex rounding and f32 math.
            assert(fabs(actual-expected)<.3);
            assert(matrixValue(&frameMatrix,a,a)>0);
        }
        for(int v=0;v<18;v++) {
            // Derive barycentrics independently with a double-precision 3D
            // least-squares projection, allowing for small off-plane rounding.
            const DoorShadowPoint *q=s->source+(v/9)*3;
            double e[2][3],d[3],aa=0,ab=0,bb=0,ad=0,bd=0;
            for(int a=0;a<3;a++) {
                e[0][a]=q[1].position[a]-q[0].position[a];e[1][a]=q[2].position[a]-q[0].position[a];
                d[a]=frameVertices[v].v.ob[a]/s->vertexScale[a]+s->center[a]-q[0].position[a];
                aa+=e[0][a]*e[0][a];ab+=e[0][a]*e[1][a];bb+=e[1][a]*e[1][a];
                ad+=e[0][a]*d[a];bd+=e[1][a]*d[a];
            }
            double b=(ad*bb-bd*ab)/(aa*bb-ab*ab),c=(bd*aa-ad*ab)/(aa*bb-ab*ab);
            for(int a=0;a<2;a++) {
                double uv=q[0].uv[a]+b*(q[1].uv[a]-q[0].uv[a])+c*(q[2].uv[a]-q[0].uv[a]);
                assert(fabs(frameVertices[v].v.tc[a]-uv)<1);
            }
            double alpha=q[0].alpha+b*(q[1].alpha-q[0].alpha)+c*(q[2].alpha-q[0].alpha);
            assert(fabs(frameVertices[v].v.cn[3]-alpha)<.6);
        }
        doorShadowFreeRoom(1);
    }
    g_LevelInverseScale=1;g_MtxConversionScale[0]=65536;memset(&testPlayer,0,sizeof(testPlayer));
    memcpy(p,saved,sizeof(saved));
    printf("PASS precision: %d distinct steps/unit, max position error %.6f units, UV error %.6f texels; large coordinates, vertical/sloped surfaces, level/render scales.\n",
        fewestSteps,worstPosition,worstUV/32);
}

static void multipleShadowChecks(u8 *p)
{
    u32 storage[DOOR_SHADOW_BYTES/4*2+1]={0};u8 *records=(u8 *)storage;
    Gfx output[16];
    memcpy(records,p,DOOR_SHADOW_BYTES);memcpy(records+DOOR_SHADOW_BYTES,p,DOOR_SHADOW_BYTES);
    records[DOOR_SHADOW_BYTES*2+3]=PROPDEF_END;
    f32 origin=1000;memcpy(records+DOOR_SHADOW_BYTES+DOOR_SHADOW_ORIGIN,&origin,4);
    doorShadowInit((PropDefHeaderRecord *)records);assert(g_DoorShadowCount==2);
    freeBytes=SHADOW_FRAME_BYTES*2;
    assert(renderFrame(output,1,0)==output+8&&frameBytes==0);
    assert(matrixValue(&frameMatrixBuffers[0],3,0)==1050&&matrixValue(&frameMatrixBuffers[1],3,0)==50);
    freeBytes=SHADOW_FRAME_BYTES*2-1;
    assert(renderFrame(output,1,0)==output+5&&frameVertexCount==1&&frameMatrixCount==1);
    doorShadowFreeRoom(1);freeBytes=SHADOW_FRAME_BYTES;
}

int main(int argc,char **argv)
{
    assert(argc==2);
    // Adapt only disk endianness and the host ABI; execute the production renderer.
    u32 storage[DOOR_SHADOW_BYTES/4+1]={0}; u8 *p=(u8 *)storage, raw[DOOR_SHADOW_BYTES];
    FILE *fp=fopen(argv[1],"rb");assert(fp&&fread(raw,1,sizeof(raw),fp)==sizeof(raw));fclose(fp);
    memcpy(p,raw,sizeof(raw));
    for(int at=4;at<DOOR_SHADOW_VERTICES;at+=4)word(p,at,be32(raw+at));
    for(int v=0;v<6;v++) for(int field=0;field<6;field++) {
        int at=DOOR_SHADOW_VERTICES+v*16+field*2;
        s16 value=(s16)((u16)raw[at]<<8|raw[at+1]);memcpy(p+at,&value,2);
    }
    for(int at=DOOR_SHADOW_GDL;at<DOOR_SHADOW_BYTES;at+=4)word(p,at,be32(raw+at));
    p[DOOR_SHADOW_BYTES+3]=PROPDEF_END;
    word(p,DOOR_SHADOW_LIGHT,0xffffff);word(p,DOOR_SHADOW_DARK,0x404040);
    g_TestDoor.type=PROPDEF_DOOR;g_TestDoor.prop=&g_TestDoor;
    freeBytes=SHADOW_FRAME_BYTES;
    for(int fog=0;fog<2;fog++) for(int layer=0;layer<2;layer++) {
        Gfx output[16],*end;
        word(p,DOOR_SHADOW_LAYER,layer);g_TestEnvironment.FogEnabled=fog;
        doorShadowInit((PropDefHeaderRecord *)p);
        assert(g_DoorShadowCount==1&&doorShadowHasSecondary(1)==layer);
        assert(g_BgRoomInfo[1].maxbounds.f[0]==100&&g_BgRoomInfo[1].maxbounds.f[2]==100);
        int before=allocations;
        for(int travel=0;travel<2;travel++) for(int fraction=0;fraction<=100;fraction+=25) {
            g_TestDoor.maxFrac=travel?90:1200;g_TestDoor.openPosition=g_TestDoor.maxFrac*fraction/100.0f;
            end=renderFrame(output,1,layer);
            assert(end==output+5&&allocations==before+1); // matrix, segment, draw, matrix/segment restore; cache reused
            assert(renderFrame(output,1,!layer)==output);
            double light=0,total=0;
            for(int t=0;t<6;t++) {
                double a=area(frameVertices+t*3);assert(a>=0);total+=a;
                if(frameVertices[t*3].v.cn[0]==255)light+=a;
            }
            assert(total==10000&&light==100*fraction);
        }
        u32 geometry=0xffffffffu;int loads=0,tris=0;Gfx combine={0};
        for(Gfx *cmd=g_DoorShadows[0].gdl;(cmd->words.w0>>24)!=(u8)G_ENDDL;cmd++) {
            u32 op=cmd->words.w0>>24;
            if(op==(u8)G_SETGEOMETRYMODE)geometry|=cmd->words.w1;
            if(op==(u8)G_CLEARGEOMETRYMODE)geometry&=~cmd->words.w1;
            if(op==(u8)G_VTX) { assert(!!(geometry&G_FOG)==fog);loads++; }
            if(op==(u8)G_SETCOMBINE)combine=*cmd;
            if(op==(u8)G_TRI1) {
                for(int color=0;color<3;color++) {
                    double shade=color==0?0:color==1?64.0/255:1;
                    double result=rgb(combine,1,rgb(combine,0,0,.8,shade),.8,shade);
                    assert(fabs(result-.8*shade)<1e-9);
                }
                tris++;
            }
        }
        assert(loads==2&&tris==6);
        before=verticesAllocated;
        int beforeMatrices=matricesAllocated;
        const int budgets[]={0,sizeof(frameVertices),SHADOW_FRAME_BYTES-1};
        for(unsigned b=0;b<sizeof(budgets)/sizeof(*budgets);b++) {
            freeBytes=budgets[b];
            assert(renderFrame(output,1,layer)==output&&verticesAllocated==before&&matricesAllocated==beforeMatrices);
        }
        freeBytes=SHADOW_FRAME_BYTES;
        doorShadowFreeRoom(1);assert(!g_DoorShadows[0].gdl);
        failalloc=1;assert(renderFrame(output,1,layer)==output&&reclaim);failalloc=0;
        assert(renderFrame(output,1,layer)==output+5);doorShadowFreeRoom(1);
    }
    Gfx output[16];word(p,DOOR_SHADOW_LAYER,0);word(p,DOOR_SHADOW_DOOR,0xffffffffu);
    doorShadowInit((PropDefHeaderRecord *)p);assert(!g_DoorShadows[0].door);
    assert(renderFrame(output,1,0)==output+5);
    for(int t=0;t<6;t++)if(area(frameVertices+t*3)>0)assert(frameVertices[t*3].v.cn[0]==64);
    doorShadowFreeRoom(1);assert(allocations==frees);
    // A room origin can be recentered by later BG edits. World placement stays fixed.
    word(p,DOOR_SHADOW_DOOR,1);doorShadowInit((PropDefHeaderRecord *)p);
    g_TestDoor.maxFrac=0;g_TestDoor.openPosition=90;
    ptr_bgdata_room_fileposition_list[1].pos.f[0]=25;
    assert(renderFrame(output,1,0)==output+5);
    for(int v=0;v<18;v++) {
        double x=transformedPosition(&frameVertices[v],&frameMatrix,0);
        assert(x>=0&&x<=100); // world position is independent of the new room origin
    }
    for(int t=0;t<6;t++)if(area(frameVertices+t*3)>0)assert(frameVertices[t*3].v.cn[0]==64);
    doorShadowFreeRoom(1);assert(allocations==frees);
    precisionChecks(p);multipleShadowChecks(p);collisionChecks(p);assert(allocations==frees);
    word(p,12,0xf8);doorShadowInit((PropDefHeaderRecord *)p);assert(!g_DoorShadowCount);
    doorShadowReset();assert(renderFrame(output,1,0)==output);
    puts("PASS runtime: real texture expansion/fog LUT, sliding/swinging travel, closed/missing doors, room bounds, both passes, cache reuse/unload, allocation failure and vertex budget.");
    return 0;
}
