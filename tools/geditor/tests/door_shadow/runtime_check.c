static u32 be32(u8 *p) { return (u32)p[0]<<24|(u32)p[1]<<16|(u32)p[2]<<8|p[3]; }
static void word(u8 *p,int offset,u32 value) { memcpy(p+offset,&value,4); }
static double area(Vtx *v)
{
    return ((double)(v[1].v.ob[0]-v[0].v.ob[0])*(v[2].v.ob[2]-v[0].v.ob[2])
        -(double)(v[1].v.ob[2]-v[0].v.ob[2])*(v[2].v.ob[0]-v[0].v.ob[0]))/2;
}
static double rgb(Gfx command,int cycle,double combined,double texture,double shade)
{
    u32 a=command.words.w0,b=command.words.w1;
    double inputs[32]={combined,texture,texture,.7,shade,.3,1};
    return (inputs[cycle?(a>>5)&15:(a>>20)&15]-inputs[cycle?(b>>24)&15:(b>>28)&15])
        *inputs[cycle?a&31:(a>>15)&31]+inputs[cycle?(b>>6)&7:(b>>15)&7];
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
    freeBytes=sizeof(frameVertices);
    for(int fog=0;fog<2;fog++) for(int layer=0;layer<2;layer++) {
        Gfx output[16],*end;
        word(p,DOOR_SHADOW_LAYER,layer);g_TestEnvironment.FogEnabled=fog;
        doorShadowInit((PropDefHeaderRecord *)p);
        assert(g_DoorShadowCount==1&&doorShadowHasSecondary(1)==layer);
        assert(g_BgRoomInfo[1].maxbounds.f[0]==100&&g_BgRoomInfo[1].maxbounds.f[2]==100);
        int before=allocations;
        for(int travel=0;travel<2;travel++) for(int fraction=0;fraction<=100;fraction+=25) {
            g_TestDoor.maxFrac=travel?90:1200;g_TestDoor.openPosition=g_TestDoor.maxFrac*fraction/100.0f;
            end=doorShadowRenderRoom(output,1,layer);
            assert(end==output+3&&allocations==before+1); // segment, draw, restore; cache reused
            assert(doorShadowRenderRoom(output,1,!layer)==output);
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
        freeBytes=0;before=verticesAllocated;
        assert(doorShadowRenderRoom(output,1,layer)==output&&verticesAllocated==before);
        freeBytes=sizeof(frameVertices);
        doorShadowFreeRoom(1);assert(!g_DoorShadows[0].gdl);
        failalloc=1;assert(doorShadowRenderRoom(output,1,layer)==output&&reclaim);failalloc=0;
        assert(doorShadowRenderRoom(output,1,layer)==output+3);doorShadowFreeRoom(1);
    }
    Gfx output[16];word(p,DOOR_SHADOW_LAYER,0);word(p,DOOR_SHADOW_DOOR,0xffffffffu);
    doorShadowInit((PropDefHeaderRecord *)p);assert(!g_DoorShadows[0].door);
    assert(doorShadowRenderRoom(output,1,0)==output+3);
    for(int t=0;t<6;t++)if(area(frameVertices+t*3)>0)assert(frameVertices[t*3].v.cn[0]==64);
    doorShadowFreeRoom(1);assert(allocations==frees);
    // A room origin can be recentered by later BG edits. World placement stays fixed.
    word(p,DOOR_SHADOW_DOOR,1);doorShadowInit((PropDefHeaderRecord *)p);
    g_TestDoor.maxFrac=0;g_TestDoor.openPosition=90;
    ptr_bgdata_room_fileposition_list[1].pos.f[0]=25;
    assert(doorShadowRenderRoom(output,1,0)==output+3);
    for(int v=0;v<18;v++)assert(frameVertices[v].v.ob[0]>=-25&&frameVertices[v].v.ob[0]<=75);
    for(int t=0;t<6;t++)if(area(frameVertices+t*3)>0)assert(frameVertices[t*3].v.cn[0]==64);
    doorShadowFreeRoom(1);assert(allocations==frees);
    word(p,12,0xf8);doorShadowInit((PropDefHeaderRecord *)p);assert(!g_DoorShadowCount);
    doorShadowReset();assert(doorShadowRenderRoom(output,1,0)==output);
    puts("PASS runtime: real texture expansion/fog LUT, sliding/swinging travel, closed/missing doors, room bounds, both passes, cache reuse/unload, allocation failure and vertex budget.");
    return 0;
}
