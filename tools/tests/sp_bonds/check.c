static void Selection(void) {
    const char *actors[]={"Brosnan","Connery","Dalton","Moore"};
    const char *headnames[]={"","CheadconneryZ","CheaddaltonZ","CheadmooreZ"};
    const char *bodynames[]={"","CconneryZ","CdaltonZ","CmooreZ"};
    const int stages[]={LEVELID_DAM,LEVELID_FACILITY,LEVELID_RUNWAY,LEVELID_SURFACE,
        LEVELID_BUNKER1,LEVELID_SILO,LEVELID_FRIGATE,LEVELID_SURFACE2,LEVELID_BUNKER2,
        LEVELID_STATUE,LEVELID_ARCHIVES,LEVELID_STREETS,LEVELID_DEPOT,LEVELID_TRAIN,
        LEVELID_JUNGLE,LEVELID_CONTROL,LEVELID_CAVERNS,LEVELID_CRADLE,LEVELID_CUBA,
        LEVELID_AZTEC,LEVELID_EGYPT};
    const int bodies[]={BODY_Formal_Wear,BODY_Brosnan_Tuxedo,BODY_Jungle_Fatigues,
        BODY_Special_Operations_Uniform,BODY_Parka,BODY_Brosnan_Tuxedo,
        BODY_Brosnan_Tuxedo,BODY_Brosnan_Tuxedo,BODY_Brosnan_Tuxedo};
    const int heads[]={HEAD_Male_Brosnan_Default,HEAD_Male_Brosnan_Tuxedo,HEAD_Male_Brosnan_Jungle,
        HEAD_Male_Brosnan_Boiler,HEAD_Male_Brosnan_Default,HEAD_Male_Brosnan_Tuxedo,
        HEAD_Male_Brosnan_Tuxedo,HEAD_Male_Brosnan_Tuxedo,HEAD_Male_Brosnan_Tuxedo};
    assert(ARRAYCOUNT(LtitleE)>=296);
    for(int bond=0;bond<4;bond++) {
        char expected[40];
        selected_folder_num=bond;
        assert(fileGetBondForCurrentFolder()==bond && frontGetWalletBondForFolder(bond)==bond);
        assert(frontGetSoloAgentTextId(bond)==0x9d24+bond);
        snprintf(expected,sizeof(expected),": 007 (%s)",actors[bond]);
        assert(!strcmp(LtitleE[frontGetSoloAgentTextId(bond)&0x3ff],expected));
        for(int s=0;s<ARRAYCOUNT(stages);s++) for(int cuff=0;cuff<ARRAYCOUNT(bodies);cuff++) {
            int b,h,bonus=stages[s]==LEVELID_AZTEC || stages[s]==LEVELID_EGYPT;
            frontGetSoloCharacterModels(bond,stages[s],cuff,&b,&h);
            assert(b==(bonus ? (bond ? customCharacterFind(bodynames[bond],1) : BODY_Brosnan_Tuxedo) : bodies[cuff]));
            assert(h==(bond ? customCharacterFind(headnames[bond],2) : (bonus ? HEAD_Male_Brosnan_Tuxedo : heads[cuff])));
        }
    }
    assert(fileGetBondForFolder(-1)==BOND_BROSNAN && fileGetBondForFolder(4)==BOND_BROSNAN);
    assert(frontGetSoloAgentTextId(-1)==0x9d24 && frontGetSoloAgentTextId(4)==0x9d24);
    int b,h;
    frontGetSoloCharacterModels(9,LEVELID_CUBA,CUFF_BOILER,&b,&h);
    assert(b==BODY_Special_Operations_Uniform && h==HEAD_Male_Brosnan_Boiler);
    /* Import order changes must not change actor selection. */
    for(int i=0;i<g_CustomPropCount;i++) if(fixtures[i].kind) fixtures[i].characterId+=10;
    frontGetSoloCharacterModels(BOND_DALTON,LEVELID_AZTEC,CUFF_BLUE,&b,&h);
    assert(b==customCharacterFind("CdaltonZ",1) && h==customCharacterFind("CheaddaltonZ",2));
    for(int i=0;i<g_CustomPropCount;i++) if(fixtures[i].kind) fixtures[i].characterId-=10;
    /* Main missions need only the head; bonus missions require a complete pair. */
    for(int i=0;i<g_CustomPropCount;i++) if(!strcmp(fixtures[i].name,"CconneryZ")) {
        fixtures[i].kind=0;
        frontGetSoloCharacterModels(BOND_CONNERY,LEVELID_CUBA,CUFF_JUNGLE,&b,&h);
        assert(b==BODY_Jungle_Fatigues && h==customCharacterFind("CheadconneryZ",2));
        frontGetSoloCharacterModels(BOND_CONNERY,LEVELID_EGYPT,CUFF_FOLDER,&b,&h);
        assert(b==BODY_Brosnan_Tuxedo && h==HEAD_Male_Brosnan_Tuxedo);
        fixtures[i].kind=1;
    }
    g_CustomProps=NULL;g_CustomPropCount=0;
    frontGetSoloCharacterModels(BOND_MOORE,LEVELID_CUBA,CUFF_BOILER,&b,&h);
    assert(b==BODY_Special_Operations_Uniform && h==HEAD_Male_Brosnan_Boiler);
    frontGetSoloCharacterModels(BOND_MOORE,LEVELID_AZTEC,CUFF_BOILER,&b,&h);
    assert(b==BODY_Brosnan_Tuxedo && h==HEAD_Male_Brosnan_Tuxedo);
    g_CustomProps=fixtures;g_CustomPropCount=ARRAYCOUNT(fixtures);
}

static void ResetStage(void) {
    for(int i=0;i<128;i++) {
        free(headers[i].RootNode);
        headers[i].RootNode=NULL;headers[i].numRecords=i>=80 ? 160 : 40;
        snprintf(filenames[i],sizeof(filenames[i]),"C%d",i);
        CitemZ_entries[i].header=&headers[i];CitemZ_entries[i].filename=filenames[i];
    }
    memset(&playerprop,0,sizeof(playerprop));
    player.prop=&playerprop;player.bodyModel=NULL;player.bondtype=CUFF_BOILER;
    gunasset.numRecords=16;PitemZ_entries[0].header=&gunasset;PitemZ_entries[0].filename="Pgun";
    stageLoads=scratchLoads=gunLoads=removed=animations=setupCalls=0;
    players=1;helditem=3;g_CameraMode=0;g_bondviewForceDisarm=0;
}

static void Loading(void) {
    const int stages[]={LEVELID_DAM,LEVELID_CUBA,LEVELID_AZTEC,LEVELID_EGYPT};
    for(int s=0;s<ARRAYCOUNT(stages);s++) for(int bond=0;bond<4;bond++) {
        ResetStage();stage=stages[s];selected_folder_num=bond;
        int b,h;
        frontGetSoloCharacterModels(bond,stage,player.bondtype,&b,&h);
        for(int repeat=0;repeat<3;repeat++) {
            g_CameraMode=repeat==0 ? CAMERAMODE_SWIRL : 0;
            bviewLoadPlayerChr();
            assert(loadedBody==b && loadedHead==h && playerprop.chr==&chr);
            assert(playerprop.type==PROP_TYPE_VIEWER && chr.chrflags==CHRFLAG_INIT);
            assert(chosenItem==(repeat==0 ? 7 : helditem));
            assert(animations==repeat+1 && gunLoads==repeat+1 && removed==2*(repeat+1));
            assert(stageLoads==(bond ? 2 : 0));
            assert(scratchLoads==(bond ? 0 : 2*(repeat+1)));
            bviewLoadPlayerChr(); /* Existing animated body is reused. */
            assert(setupCalls==repeat+1);
            bondviewRemovePlayerBody();
            assert(playerprop.chr==NULL && player.bodyModel==NULL && g_bondviewForceDisarm);
        }
    }
    ResetStage();selected_folder_num=BOND_DALTON;helditem=-1;
    bviewLoadPlayerChr();assert(gunLoads==0 && stageLoads==2);
    bondviewRemovePlayerBody();
    /* MP still uses its own chosen pair and manager instance, regardless of folder/stage. */
    ResetStage();players=2;selected_folder_num=BOND_MOORE;stage=LEVELID_EGYPT;
    mpBody=customCharacterFind("CdaltonZ",1);mpHead=customCharacterFind("CheaddaltonZ",2);
    bviewLoadPlayerChr();
    assert(loadedBody==mpBody && loadedHead==mpHead && stageLoads==2);
    assert(scratchLoads==0 && gunLoads==0 && removed==0 && animations==0);
    bondviewRemovePlayerBody();assert(playerprop.chr!=NULL);
    ResetStage();
}

int main(void) {
    Selection();Loading();
    puts("PASS: four wallet identities/briefing IDs; all 21 stages and 9 cuffs; import order/fallbacks; SP stage geometry caching, private animation buffers, held guns, teardown/reload; MP unchanged");
    return 0;
}
