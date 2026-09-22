/* The same production renderer is exercised near/far and near again, with
 * attachment hit entries retained and sunglasses on a separately sorted joint. */
static void attachment_checks(void)
{
    const float distances[]={1999.0f,2000.0f,2000.1f,3000.0f};
    for(unsigned int i=0;i<sizeof(distances)/sizeof(distances[0]);i++) {
        reset();prop.pos.x=distances[i];
        assert(chrHideDistantAttachments(&prop)==(i>=2));
    }
    reset();prop.pos.x=1600;prop.pos.z=1600;assert(chrHideDistantAttachments(&prop));
    prop.pos=(coord3d){{{0,2001,0}}};assert(chrHideDistantAttachments(&prop));
    view.m[3][1]=2;assert(!chrHideDistantAttachments(&prop));
    view.m[0][0]=view.m[1][1]=view.m[2][2]=0.1f;
    assert(!chrHideDistantAttachments(&prop)); /* Render scale does not change metres. */
    missingCamera=1;prop.pos.y=30000;assert(!chrHideDistantAttachments(&prop));missingCamera=0;

    /* Independent projection oracle across zoom levels and viewport heights.
     * The 60-degree baseline remains exact; zoom only extends the cutoff. */
    const float fovs[]={60,59,50,45,40,30,20,15,7,75};
    const float heights[]={240,224,120,112,480};
    for(unsigned int f=0;f<sizeof(fovs)/sizeof(fovs[0]);f++)
    for(unsigned int h=0;h<sizeof(heights)/sizeof(heights[0]);h++) {
        double tangent=tan(fovs[f]*3.14159265358979323846/360.0);
        float cutoff=(float)(2000.0*tan(3.14159265358979323846/6.0)/tangent);
        if(cutoff<2000.0f)cutoff=2000.0f;
        reset();
        testPlayer.c_perspfovy=fovs[f];testPlayer.c_halfheight=heights[h]*0.5f;
        testPlayer.c_scaley=(float)(tangent/testPlayer.c_halfheight);
        view.m[3][0]=1000;view.m[3][1]=-500;view.m[3][2]=250;
        prop.pos=(coord3d){{{1000,-500,250+cutoff*0.9999f}}};
        assert(!chrHideDistantAttachments(&prop));
        prop.pos.z=250+cutoff*1.0001f;assert(chrHideDistantAttachments(&prop));
    }
    /* An invalid/uninitialized cached projection falls back to 20 metres. */
    reset();testPlayer.c_perspfovy=30;testPlayer.c_scaley=0;prop.pos.z=2001;
    assert(chrHideDistantAttachments(&prop));

    for(int zoom=0;zoom<2;zoom++)
    for(int glasses=0;glasses<2;glasses++) for(int faded=0;faded<2;faded++) {
        u32 expectedRng=0;int expectedBody=0,expectedRelations=0,expectedScorch=0;
        for(int pass=0;pass<3;pass++) {
            ModelNode extra[7]={0}, *bodyParts[5]={0}, *headParts[2];
            union ModelRoData ro[7]={0}; union ModelRwData headRw={0},glassesRw={0},capRw={0};
            ModelFileHeader head={0};ModelHitEntry joint={0};
            ChrRecord before;ModelHitEntry links[6];
            reset();g_OcclusionEnabled=0;chr.fadealpha=faded?120:255;
            prop.pos.z=zoom?3000:pass==1?2001:1999;
            if(zoom) {
                /* Fixed 30m guard: zoom in, zoom out, zoom in again. */
                testPlayer.c_perspfovy=pass==1?60:30;
                testPlayer.c_scaley=tanf(testPlayer.c_perspfovy*3.14159265358979323846f/360.0f)/testPlayer.c_halfheight;
            }
            models[1].rw.Gunfire.visible=1;
            memset(nodeDraws,0,sizeof(nodeDraws));
            for(int n=0;n<7;n++) {
                extra[n].Opcode=MODELNODE_OPCODE_DL;extra[n].Data=&ro[n];
                ro[n].DisplayList=data[11].DisplayList;
            }
            headers[0].numSwitches=5;headers[0].Switches=bodyParts;bodyParts[4]=&nodes[9];
            nodes[9].rwData=&headRw;headRw.HeadPlaceholder.ModelFileHeader=&head;
            head.Switches=headParts;head.numSwitches=2;headParts[0]=&nodes[8];headParts[1]=&extra[5];
            extra[5].Opcode=MODELNODE_OPCODE_SWITCH;extra[5].rwData=&capRw;
            extra[5].Parent=&nodes[0];ro[5].Switch.Controls=&extra[6];nodes[13].Next=&extra[5];
            extra[6].Opcode=MODELNODE_OPCODE_DLCOLLISION;extra[6].Parent=&extra[5];extra[6].testId=4;
            nodes[8].rwData=&glassesRw;glassesRw.Switch.visible=glasses;
            data[8].Switch.Controls=&extra[0];nodes[8].Child=glasses?&extra[0]:NULL;
            /* A plain sunglass DL plus a child joint with its own hit entry. */
            extra[0].Parent=&nodes[8];extra[0].Next=&extra[1];extra[0].testId=3;
            extra[1].Parent=&nodes[8];extra[1].Opcode=MODELNODE_OPCODE_GROUPSIMPLE;
            extra[1].Child=&extra[2];extra[2].Parent=&extra[1];extra[2].testId=3;
            if(glasses) {
                joint.model=&models[0];joint.rootnode=&extra[1];joint.next=chain;
                chain[0].prev=&joint;chr.hitChain=&joint;
            }
            nodes[3].Child=&extra[3];extra[3].Parent=&nodes[3];extra[3].testId=1;
            nodes[4].Child=&extra[4];extra[4].Parent=&nodes[4];extra[4].testId=2;
            g_BulletImpactBuffer[0].prop=&weaponProp;g_BulletImpactBuffer[0].room=1;
            g_ImpactTypes[0].unk1=2;g_ImpactTypes[0].unk2=0;
            before=chr;memcpy(links,chain,sizeof(chain));
            assert(chrGetHeadSwitch(&models[0],0)==&nodes[8]);
            assert(chrGetHatCoveredHeadNode(&models[0])==&extra[5]);
            Gfx *gdl=chrRenderChr(&prop,commands,0);
            assert(!memcmp(links,chain,sizeof(chain)) && !freed && !conversions);
            gdl=chrRenderChr(&prop,gdl,1);before.hitChain=NULL;
            assert(!memcmp(&before,&chr,sizeof(chr)));
            assert(conversions==3 && freed==1 && !chr.hitChain && gdl>commands);
            assert(glassesRw.Switch.visible==glasses && nodes[8].Child==(glasses?&extra[0]:NULL));
            assert(capRw.Switch.visible==0 && extra[5].Child==NULL && !!nodeDraws[4]==(pass==1));
            assert(nodeDraws[0]>0);
            assert(!!nodeDraws[1]==(pass!=1) && !!nodeDraws[2]==(pass!=1));
            assert(!!nodeDraws[3]==(glasses && pass!=1));
            if(pass==0) {
                expectedRng=rng;expectedBody=nodeDraws[0];expectedRelations=relations;expectedScorch=scorch.unk02;
            } else {
                assert(rng==expectedRng && nodeDraws[0]==expectedBody && relations==expectedRelations);
                assert(scorch.unk02==expectedScorch);
            }
        }
    }
    reset();assert(!chrGetHeadSwitch(&models[0],0));
    puts("attachment distance: 20m baseline, FOV magnification, viewport independence, zoom transitions, both passes, sunglasses/cap restoration, RNG and hit/matrix lifecycle passed");
}
