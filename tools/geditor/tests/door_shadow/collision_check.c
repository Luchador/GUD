static bool shadowRay(f32 x,f32 y,f32 z,f32 endY,HitThing *hit)
{
    coord3d from={{x,y,z}},to={{x,endY,z}};
    return bgTestBulletHitBackground(&from,&to,1,hit);
}

static void collisionChecks(const u8 *fixture)
{
    u32 storage[DOOR_SHADOW_BYTES/4*2+1]={0};u8 *p=(u8 *)storage;
    HitThing hit;
    Gfx output[16];
    int before=allocations;
    memcpy(p,fixture,DOOR_SHADOW_BYTES);
    p[DOOR_SHADOW_BYTES+3]=PROPDEF_END;
    *(u32 *)(p+DOOR_SHADOW_DOOR)=1;
    g_TestDoor.maxFrac=100;
    assert(sizeof(Vertex)==16);
    /* Both halves, original diagonal and outer edges stay solid in either
     * render layer and at every opening, including with no render memory. */
    failalloc=1;freeBytes=0;
    for(int layer=0;layer<2;layer++) for(int direction=0;direction<4;direction++) {
        *(u32 *)(p+DOOR_SHADOW_LAYER)=layer;*(u32 *)(p+DOOR_SHADOW_DIRECTION)=direction;
        doorShadowInit((PropDefHeaderRecord *)p);
        for(int opening=0;opening<=100;opening+=25) {
            g_TestDoor.openPosition=opening;
            for(int x=0;x<=100;x+=25) for(int z=0;z<=100;z+=25) {
                assert(shadowRay(x,100,z,-100,&hit));
                assert(hit.hitpos.x==x&&hit.hitpos.y==0&&hit.hitpos.z==z);
                assert(hit.normal.x==0&&hit.normal.y==-10000&&hit.normal.z==0);
                assert(hit.vtx0>=(Vertex *)(p+DOOR_SHADOW_VERTICES));
                assert(hit.vtx2<(Vertex *)(p+DOOR_SHADOW_GDL));
                assert(!hit.tricmd&&hit.unk28==0&&hit.tileformat==-1&&hit.tilesize==-1);
                assert(shadowRay(x,-100,z,100,&hit)); // BG hits are two-sided
            }
            assert(!shadowRay(-1,100,50,-100,&hit));
            assert(!shadowRay(101,100,50,-100,&hit));
            assert(!shadowRay(50,100,-1,-100,&hit));
            assert(!shadowRay(50,100,101,-100,&hit));
            assert(!shadowRay(50,100,50,100,&hit)); // parallel/zero-length
            assert(!shadowRay(50,100,50,200,&hit)); // behind the ray
        }
    }
    assert(allocations==before);failalloc=0;freeBytes=SHADOW_FRAME_BYTES;
    // Match BG's forward-ray contract: callers limit hits to the shot/flight range.
    assert(shadowRay(25,100,75,50,&hit)&&hit.hitpos.y==0);
    coord3d shotFrom={{25,100,75}},shotTo={{25,-100,75}};
    assert(chrpropFindNearestBgHit(&shotFrom,&shotTo,&hit)==1&&hit.hitpos.y==0);
    shotTo.y=50;assert(!chrpropFindNearestBgHit(&shotFrom,&shotTo,&hit));
    // Stable world coordinates, independent of current BG room origin/render scale.
    *(f32 *)(p+DOOR_SHADOW_ORIGIN)=1000.25f;
    *(f32 *)(p+DOOR_SHADOW_ORIGIN+4)=200.5f;
    *(f32 *)(p+DOOR_SHADOW_ORIGIN+8)=-300.75f;
    g_LevelScale=4;
    doorShadowInit((PropDefHeaderRecord *)p);
    assert(shadowRay(1025.25f/4,400.5f/4,-225.75f/4,0,&hit));
    assert(fabs(hit.hitpos.x-1025.25f)<.001&&fabs(hit.hitpos.y-200.5f)<.001&&fabs(hit.hitpos.z+225.75f)<.001);
    // Test actual bullet room culling at fractional outer corners of a room
    // containing only the shadow, including positive maxima/negative minima.
    for(int a=0;a<3;a++) {
        g_BgRoomInfo[1].minbounds.f[a]=*(f32 *)(p+DOOR_SHADOW_ORIGIN+a*4);
        g_BgRoomInfo[1].maxbounds.f[a]=g_BgRoomInfo[1].minbounds.f[a]+(a==1?0:100);
    }
    for(int x=0;x<=100;x+=100) for(int z=0;z<=100;z+=100) {
        shotFrom.x=shotTo.x=(1000.25f+x)/4;shotFrom.z=shotTo.z=(-300.75f+z)/4;
        shotFrom.y=100;shotTo.y=0;
        assert(chrpropFindNearestBgHit(&shotFrom,&shotTo,&hit)==1);
        assert(fabs(hit.hitpos.y-200.5f/4)<.001);
    }
    memset(p+DOOR_SHADOW_ORIGIN,0,12);g_LevelScale=1;
    // Base image identity survives detail textures and editor no-op markers.
    Gfx *request=NULL,*gdl=(Gfx *)(p+DOOR_SHADOW_GDL);
    for(unsigned i=0;i<*(u32 *)(p+DOOR_SHADOW_GDL_SIZE)/8;i++)
        if((gdl[i].words.w0>>24)==G_NOOP&&!BG_EDITOR_IS_MARKER(gdl[i].words.w0,gdl[i].words.w1))request=gdl+i;
    assert(request);Gfx saved=*request;
    for(int type=0;type<=4;type++) {
        request->words.w0=0xc0000000u|type;request->words.w1=(500u<<12)|3000;
        doorShadowInit((PropDefHeaderRecord *)p);
        assert(shadowRay(25,100,75,-100,&hit)&&hit.texturenum==3000);
    }
    request->words.w1=0x4fd;doorShadowInit((PropDefHeaderRecord *)p);
    assert(!shadowRay(25,100,75,-100,&hit)); // native light-shaft exception
    request->words.w0=0xe7000000u;request->words.w1=0;
    doorShadowInit((PropDefHeaderRecord *)p);
    assert(shadowRay(25,100,75,-100,&hit)&&hit.texturenum==-1);
    *request=saved;doorShadowInit((PropDefHeaderRecord *)p);
    frameVertexCount=frameMatrixCount=0;frameBytes=freeBytes;
    assert(doorShadowRenderRoom(output,1,1,FALSE)==output+5);
    doorShadowFreeRoom(1);assert(!g_DoorShadows[0].gdl);
    assert(shadowRay(25,100,75,-100,&hit)); // cache eviction leaves collision intact

    // Nearest static BG competes with the shadow; native lights keep their bias.
    RoomVtxBatchBounds batch={-1000,-1000,-1000,1000,1000,1000};Gfx native[3]={0};
    native[1].words.w0=0xf5100000u;native[2].words.w0=0xbf000000u;
    backgroundHit=hit;backgroundHit.tricmd=native+2;backgroundHit.texturenum=42;
    backgroundHitEnabled=1;
    g_BgRoomInfo[1].vtx_batch_bounds=&batch;g_BgRoomInfo[1].num_vtx_batch_bounds=1;
    g_BgRoomInfo[1].primaryGdl=native;
    for(int y=-50;y<=50;y+=50) {
        backgroundHit.hitpos.y=y;
        assert(shadowRay(25,100,75,-100,&hit));
        assert(hit.hitpos.y==(y>0?y:0));
        assert(hit.tricmd==(y>=0?native+2:NULL));
        if(y>=0)assert(hit.tileformat==0&&hit.tilesize==2);
    }
    backgroundHit.hitpos.y=-.001f;backgroundHit.texturenum=123; // fixture light
    assert(shadowRay(25,100,75,-100,&hit)&&hit.tricmd==native+2);
    backgroundHitEnabled=0;g_BgRoomInfo[1].vtx_batch_bounds=NULL;
    g_BgRoomInfo[1].num_vtx_batch_bounds=0;g_BgRoomInfo[1].primaryGdl=NULL;
    // More than one shadow in a room: nearest wins regardless of list order.
    memcpy(p+DOOR_SHADOW_BYTES,p,DOOR_SHADOW_BYTES);
    p[DOOR_SHADOW_BYTES*2+3]=PROPDEF_END;
    for(int y=-50;y<=50;y+=100) {
        *(f32 *)(p+DOOR_SHADOW_BYTES+DOOR_SHADOW_ORIGIN+4)=y;
        doorShadowInit((PropDefHeaderRecord *)p);assert(g_DoorShadowCount==2);
        assert(shadowRay(25,100,75,-100,&hit)&&hit.hitpos.y==(y>0?y:0));
    }
    *(u32 *)(p+12)=0xf8;*(u32 *)(p+DOOR_SHADOW_BYTES+12)=0xf8;
    doorShadowInit((PropDefHeaderRecord *)p);assert(!shadowRay(25,100,75,-100,&hit));
    doorShadowReset();assert(!shadowRay(25,100,75,-100,&hit));
    puts("PASS collision: real BG entry point/triangle test, both layers/sides, all openings/directions, edges/misses, scale/origin, material IDs, nearest BG/shadow, light bias, cache eviction, no render memory, deleted/reset.");
}
