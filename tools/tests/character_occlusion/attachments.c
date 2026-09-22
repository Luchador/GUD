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

    for(int glasses=0;glasses<2;glasses++) for(int faded=0;faded<2;faded++) {
        u32 expectedRng=0;int expectedBody=0,expectedRelations=0,expectedScorch=0;
        for(int pass=0;pass<3;pass++) {
            ModelNode extra[7]={0}, *bodyParts[5]={0}, *headParts[2];
            union ModelRoData ro[7]={0}; union ModelRwData headRw={0},glassesRw={0},capRw={0};
            ModelFileHeader head={0};ModelHitEntry joint={0};
            ChrRecord before;ModelHitEntry links[6];
            reset();g_OcclusionEnabled=0;chr.fadealpha=faded?120:255;
            prop.pos.z=pass==1?2001:1999;models[1].rw.Gunfire.visible=1;
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
    puts("attachment distance: 20m boundary, diagonal/vertical/camera distances, both passes, sunglasses joints, cap-covered heads, near/far restoration, RNG and hit/matrix lifecycle passed");
}
