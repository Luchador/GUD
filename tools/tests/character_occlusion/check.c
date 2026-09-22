static OcclusionBox boxes[32];
static RenderPosView bodyMatrices[3], weaponMatrices[1],hatMatrices[1];
static ModelFileHeader headers[3];
static Model models[3];
static ModelNode nodes[14];
static union ModelRoData data[14];
static ModelHitEntry chain[6];
static ObjectRecord weapon,hat;
static struct Scorch scorch,hatScorch;
static PropRecord prop,weaponProp,hatProp;
static ChrRecord chr;
static sImageTableEntry image={32};
static Gfx commands[2048],list;
static Vertex flashVertices[8];

static void identity(Mtxf *m)
{
    int i;
    memset(m,0,sizeof(*m));
    for(i=0;i<4;i++) m->m[i][i]=1;
}

static void reset(void)
{
    float p[3]={0,0,100},up[3]={0,1,0},look[3]={0,0,1};
    float bounds[6]={-80,80,-80,80,-10,10};
    int i;
    memset(nodes,0,sizeof(nodes)); memset(data,0,sizeof(data));
    memset(chain,0,sizeof(chain)); memset(headers,0,sizeof(headers));
    memset(models,0,sizeof(models)); memset(&chr,0,sizeof(chr));
    memset(g_BulletImpactBuffer,0,sizeof(g_BulletImpactBuffer));
    identity(&view);
    testPlayer.c_perspfovy=60.0f;testPlayer.c_halfheight=120.0f;
    testPlayer.c_scaley=tanf(3.14159265358979323846f/6.0f)/120.0f;
    assert(occlusionBoxInit(&boxes[0],p,up,look,bounds));
    occlusionBoxView(&boxes[0],view.m[3],NULL);
    g_Occluders=boxes;g_OccluderCount=1;g_OcclusionEnabled=1;g_OcclusionRejected=0;
    for(i=0;i<3;i++) {
        identity(&bodyMatrices[i].pos); bodyMatrices[i].pos.m[3][2]=250;
        headers[i].numMatrices=i==0?3:1;headers[i].BoundingVolumeRadius=10;
        models[i].obj=&headers[i];models[i].scale=1;
    }
    identity(&weaponMatrices[0].pos);weaponMatrices[0].pos.m[3][2]=250;
    identity(&hatMatrices[0].pos);hatMatrices[0].pos.m[3][2]=250;
    models[0].render_pos=bodyMatrices;models[1].render_pos=weaponMatrices;models[2].render_pos=hatMatrices;
    for(i=0;i<14;i++) { nodes[i].Data=&data[i]; nodes[i].Opcode=MODELNODE_OPCODE_GROUPSIMPLE; }
    nodes[0].Opcode=MODELNODE_OPCODE_HEADER;data[0].Header.MatrixIndex=0;
    nodes[1].Opcode=MODELNODE_OPCODE_GROUP;data[1].Group.MatrixIDs[0]=1;
    data[2].GroupSimple.Group1=2;
    nodes[5].Opcode=MODELNODE_OPCODE_GUNFIRE;nodes[5].Parent=&nodes[3];
    data[5].Gunfire.Size=(coord3d){{{4,4,4}}};data[5].Gunfire.Image=&image;
    for(i=0;i<6;i++) {
        chain[i].model=&models[i<3?0:i==4?2:1];chain[i].rootnode=&nodes[i];
        chain[i].next=i==5?NULL:&chain[i+1];chain[i].prev=i?&chain[i-1]:NULL;
    }
    /* Draw nodes and all relation callbacks beneath the header. */
    nodes[0].Child=&nodes[6];
    for(i=6;i<13;i++) {nodes[i].Parent=&nodes[0];nodes[i].Next=&nodes[i+1];}
    nodes[13].Parent=&nodes[0];
    nodes[6].Opcode=MODELNODE_OPCODE_LOD;nodes[7].Opcode=MODELNODE_OPCODE_BSP;
    nodes[8].Opcode=MODELNODE_OPCODE_SWITCH;nodes[9].Opcode=MODELNODE_OPCODE_HEAD;
    nodes[10].Opcode=MODELNODE_OPCODE_DLCOLLISION;
    data[10].DisplayListCollisions.Primary=&list;data[10].DisplayListCollisions.Secondary=&list;data[10].DisplayListCollisions.ModelType=4;
    nodes[11].Opcode=MODELNODE_OPCODE_DL;
    data[11].DisplayList=data[10].DisplayListCollisions;
    nodes[12].Opcode=MODELNODE_OPCODE_DLPRIMARY;
    data[12].DisplayListPrimary.numVertices=2;data[12].DisplayListPrimary.Primary=&list;
    data[12].DisplayListPrimary.Vertices=flashVertices;
    nodes[13].Opcode=MODELNODE_OPCODE_BBOX;
    weapon=(ObjectRecord){&models[1],3,0};hat=(ObjectRecord){&models[2],3,0};
    scorch=(struct Scorch){&models[1],3};hatScorch=(struct Scorch){&models[2],3};
    weaponProp=(PropRecord){.obj=&weapon,.scorch=&scorch,.flags=0x55};
    hatProp=(PropRecord){.obj=&hat,.scorch=&hatScorch,.flags=0xaa};
    chr.model=&models[0];chr.fadealpha=255;chr.weapons_held[0]=&weaponProp;
    chr.handle_positiondata_hat=&hatProp;chr.hitChain=chain;chr.action=42;chr.health=123;chr.animation=72;
    prop=(PropRecord){.chr=&chr,.flags=0x9876};
    emitted=allocations=relations=conversions=freed=0;playerStats.time_other_players_on_screen=0;
    rng=123;envColor=2;
}

static void bounds(void)
{
    int i;
    reset();assert(occlusionTestCharacter(chain));assert(g_OcclusionRejected==1);
    for(i=0;i<3;i++) {
        bodyMatrices[i].pos.m[3][0]=500;
        assert(!occlusionTestCharacter(chain));assert(g_OcclusionRejected==1);
        bodyMatrices[i].pos.m[3][0]=0;
    }
    weaponMatrices[0].pos.m[3][0]=500;assert(!occlusionTestCharacter(chain));
    weaponMatrices[0].pos.m[3][0]=0;
    hatMatrices[0].pos.m[3][1]=500;assert(!occlusionTestCharacter(chain));
    hatMatrices[0].pos.m[3][1]=0;
    /* Nonuniform scale, shear, malformed bounds and invalid primary matrices. */
    hatMatrices[0].pos.m[0][0]=30;assert(!occlusionTestCharacter(chain));
    hatMatrices[0].pos.m[0][0]=1;hatMatrices[0].pos.m[0][1]=30;assert(!occlusionTestCharacter(chain));
    hatMatrices[0].pos.m[0][1]=0;
    headers[0].BoundingVolumeRadius=NAN;assert(!occlusionTestCharacter(chain));headers[0].BoundingVolumeRadius=10;
    data[1].Group.MatrixIDs[0]=3;assert(!occlusionTestCharacter(chain));data[1].Group.MatrixIDs[0]=1;
    bodyMatrices[1].pos.m[3][0]=NAN;assert(!occlusionTestCharacter(chain));bodyMatrices[1].pos.m[3][0]=0;
    models[1].rw.Gunfire.visible=1;data[5].Gunfire.Offset.x=500;
    assert(!occlusionTestCharacter(chain));models[1].rw.Gunfire.visible=0;
    assert(occlusionTestCharacter(chain));
    /* Each view uses its own camera transform, without persistent decisions. */
    view.m[3][0]=500;assert(!occlusionTestCharacter(chain));view.m[3][0]=0;
    assert(occlusionTestCharacter(chain));
    nodes[4].Opcode=MODELNODE_OPCODE_OP14;assert(!occlusionTestCharacter(chain));nodes[4].Opcode=MODELNODE_OPCODE_GROUPSIMPLE;
    nodes[1].Opcode |= 0x200;assert(!occlusionTestCharacter(chain));nodes[1].Opcode &= ~0x200;
    /* A translated, rotated, scaled view must recover the same world pose. */
    identity(&view);view.m[0][0]=view.m[2][2]=0;view.m[0][2]=-0.5f;view.m[2][0]=0.5f;
    view.m[1][1]=0.5f;view.m[3][0]=100;
    for(i=0;i<3;i++) {bodyMatrices[i].pos.m[3][0]=-500;bodyMatrices[i].pos.m[3][2]=-200;}
    weaponMatrices[0].pos=hatMatrices[0].pos=bodyMatrices[0].pos;
    assert(occlusionTestCharacter(chain));
    g_OcclusionEnabled=0;assert(!occlusionTestCharacter(chain));g_OcclusionEnabled=1;
    g_OccluderCount=0;assert(!occlusionTestCharacter(chain));
    g_OccluderCount=32;for(i=1;i<32;i++) boxes[i]=boxes[0];assert(occlusionTestCharacter(chain));
    assert(!occlusionTestCharacter(NULL));
}

static void effects(void)
{
    int visible,texture,pass,hidden;
    u32 expected;
    ModelRenderData rd;
    reset();
    for(visible=0;visible<2;visible++) for(texture=0;texture<2;texture++) for(pass=1;pass<=3;pass++) {
        expected=0;
        for(hidden=0;hidden<2;hidden++) {
            rd=(ModelRenderData){0};rd.flags=pass|(hidden?MODEL_RENDER_OCCLUDED:0);rd.gdl=commands;
            models[1].rw.Gunfire.visible=visible;data[5].Gunfire.Image=texture?&image:NULL;
            rng=123;emitted=allocations=0;
            modelRenderGunfire(&rd,&models[1],&nodes[5]);
            if(!hidden) expected=rng;
            else {assert(rng==expected);assert(!emitted && !allocations && rd.gdl==commands);}
            if(!(visible && (pass&2))) assert(rng==123);
        }
    }
    for(pass=1;pass<=3;pass++) for(hidden=0;hidden<2;hidden++) {
        rd=(ModelRenderData){0};rd.flags=pass|(hidden?MODEL_RENDER_OCCLUDED:0);rd.gdl=commands;
        rng=123;emitted=allocations=0;
        modelRenderRotatingTexture(&rd,&nodes[12]);
        if(!hidden) expected=rng;
        else {assert(rng==expected);assert(!emitted && !allocations && rd.gdl==commands);}
    }
}

static void render(void)
{
    int hidden,fade,withImpact;
    u32 expected=0;
    for(fade=0;fade<2;fade++) for(withImpact=0;withImpact<2;withImpact++) for(hidden=0;hidden<2;hidden++) {
        Gfx *gdl=commands;
        ChrRecord before;
        ModelHitEntry links[6];
        reset();g_OcclusionEnabled=hidden;chr.fadealpha=fade?120:255;
        models[1].rw.Gunfire.visible=1;
        if(withImpact) {
            g_BulletImpactBuffer[0].prop=&weaponProp;
            g_BulletImpactBuffer[0].room=1;
            g_ImpactTypes[0].unk1=2;g_ImpactTypes[0].unk2=0;
        }
        before=chr;memcpy(links,chain,sizeof(chain));
        gdl=chrRenderChr(&prop,gdl,0);
        assert(chr.hitChain==chain && !freed && !conversions);
        assert(memcmp(links,chain,sizeof(chain))==0);
        gdl=chrRenderChr(&prop,gdl,1);
        before.hitChain=NULL;
        assert(memcmp(&before,&chr,sizeof(chr))==0);
        assert(prop.flags==0x9876 && weaponProp.flags==0x55 && hatProp.flags==0xaa);
        assert(conversions==3 && freed==1 && chr.hitChain==NULL);
        assert(relations==(fade?4:8));assert(playerStats.time_other_players_on_screen==(fade?1:2));
        assert(scorch.unk02==(withImpact?(fade?3:2):fade?1:0));
        if(hidden) {
            assert(gdl==commands && !emitted && !allocations);
            assert(g_OcclusionRejected==(fade?1:2));assert(rng==expected);
        } else {assert(gdl>commands && emitted>0 && allocations==2);expected=rng;}
    }
    /* Existing zero-alpha/environment paths still release the transient chain. */
    reset();chr.fadealpha=0;chrRenderChr(&prop,commands,1);assert(freed==1 && !conversions && !emitted);
    reset();envColor=0;chrRenderChr(&prop,commands,1);assert(freed==1 && !conversions && !emitted);
    /* An exposed attachment keeps the complete character/effects rendering. */
    reset();hatMatrices[0].pos.m[3][0]=500;
    assert(chrRenderChr(&prop,commands,1)>commands);assert(g_OcclusionRejected==0 && freed==1);
}

static void attachment_checks(void);
int main(void)
{
    bounds();effects();render();attachment_checks();
    puts("character occlusion: bounds, attachments, RNG, relations, decals and render lifecycle passed");
    return 0;
}
