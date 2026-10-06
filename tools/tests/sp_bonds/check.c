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
        InitRawFixture(i,i>=80 ? 27632 : 28512,i>=80 ? 9 : 52);
    }
    for(int i=0;i<g_CustomPropCount;i++) if(fixtures[i].kind==1)
        InitRawFixture(fixtures[i].characterId,54176,80);
    InitRawFixture(128,2304,8);
    textureBytes=2048;
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
            assert(stageLoads==((bond && s>=2) ? 2 : 0));
            assert(scratchLoads==((bond && s>=2) ? 0 : 2*(repeat+1)));
            bviewLoadPlayerChr(); /* Existing animated body is reused. */
            assert(setupCalls==repeat+1);
            bondviewRemovePlayerBody();
            assert(playerprop.chr==NULL && player.bodyModel==NULL && g_bondviewForceDisarm);
        }
    }
    ResetStage();selected_folder_num=BOND_DALTON;helditem=-1;stage=LEVELID_FRIGATE;
    bviewLoadPlayerChr();assert(gunLoads==0 && stageLoads==0 && scratchLoads==2);
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

static void Capacity(void) {
    struct texpool pool;
    ModelBufferRequirements r;
    int body=BODY_Formal_Wear,head=customCharacterFind("CheaddaltonZ",2);
    ResetStage();
    texInitPool(&pool,gunbuffer[1],sizeof(gunbuffer[1]));
    assert(bviewCanUsePrivateCharacterBuffers(body,head,0,gunbuffer[0],sizeof(gunbuffer[0]),&pool));
    for(int i=0;i<129;i++) assert(rawInfo[i].poolRemaining==0x12345678);
    assert(headers[body].RootNode==NULL && headers[head].RootNode==NULL);
    assert(!modelGetBufferRequirements(&headers[head],(u8 *)filenames[head],gunbuffer[0],100,&pool,&r));
    /* Extra animation words and a large held weapon must also fit. */
    headers[head].numRecords=16000;
    assert(!bviewCanUsePrivateCharacterBuffers(body,head,0,gunbuffer[0],sizeof(gunbuffer[0]),&pool));
    ResetStage();texInitPool(&pool,gunbuffer[1],sizeof(gunbuffer[1]));
    InitRawFixture(128,30000,40);
    assert(!bviewCanUsePrivateCharacterBuffers(body,head,0,gunbuffer[0],sizeof(gunbuffer[0]),&pool));
    /* Oversized raw geometry and texture exhaustion fail without a write past
     * either existing buffer; no permanent allocation occurs during preflight. */
    ResetStage();texInitPool(&pool,gunbuffer[1],sizeof(gunbuffer[1]));
    InitRawFixture(head,110000,9);
    assert(!bviewCanUsePrivateCharacterBuffers(body,head,0,gunbuffer[0],sizeof(gunbuffer[0]),&pool));
    ResetStage();texInitPool(&pool,gunbuffer[1],sizeof(gunbuffer[1]));textureBytes=10000;
    assert(!bviewCanUsePrivateCharacterBuffers(body,head,0,gunbuffer[0],sizeof(gunbuffer[0]),&pool));
    /* A failed private preflight takes the established stage path. */
    selected_folder_num=BOND_DALTON;stage=LEVELID_FRIGATE;
    bviewLoadPlayerChr();assert(stageLoads==2 && scratchLoads==0 && gunLoads==1);
    bondviewRemovePlayerBody();ResetStage();
}

static u32 ReadWord(FILE *f) {
    u8 bytes[4];assert(fread(bytes,1,4,f)==4);
    return (u32)bytes[0]<<24 | (u32)bytes[1]<<16 | (u32)bytes[2]<<8 | bytes[3];
}
static void RomCapacity(const char *path) {
    FILE *f=fopen(path,"rb");assert(f);
    ResetStage();textureBytes=0;
    u32 count=ReadWord(f);
    for(u32 i=0;i<count;i++) {
        u32 id=ReadWord(f),ns=ReadWord(f),nt=ReadWord(f),size=ReadWord(f),first=ReadWord(f);
        assert(id<129 && size<=sizeof(rawWords[id]));
        ModelFileHeader *h=id==128 ? &gunasset : &headers[id];
        h->numSwitches=ns;h->numtextures=nt;h->numRecords=160;
        rawInfo[id].rom_size=size;rawFirst[id]=first;
        for(u32 j=0;j<size/4;j++) rawWords[id][j]=ReadWord(f);
    }
    count=ReadWord(f);assert(count<=4096);
    for(u32 i=0;i<count;i++) romTextureBytes[i]=ReadWord(f);
    fclose(f);
    const int bodies[]={BODY_Brosnan_Tuxedo,BODY_Special_Operations_Uniform,
        BODY_Formal_Wear,BODY_Jungle_Fatigues,BODY_Parka};
    const char *heads[]={"CheadconneryZ","CheaddaltonZ","CheadmooreZ"};
    for(int b=0;b<5;b++) for(int h=0;h<3;h++) {
        int head=customCharacterFind(heads[h],2);
        struct texpool pool;
        texInitPool(&pool,gunbuffer[1],sizeof(gunbuffer[1]));
        int fits=bviewCanUsePrivateCharacterBuffers(bodies[b],head,0,gunbuffer[0],sizeof(gunbuffer[0]),&pool);
        printf("ROM body %d + %s: private fit=%d, textures=%d/%zu bytes\n",bodies[b],heads[h],fits,pool.used,sizeof(gunbuffer[1]));
        assert(fits);
    }
    puts("PASS: production preflight fits all 15 actual stock-outfit/imported-head pairs plus Frigate's D5K in the existing buffers");
}

int main(int argc,char **argv) {
    Selection();Loading();Capacity();
    if(argc>1) RomCapacity(argv[1]);
    puts("PASS: four wallet identities/briefing IDs; all 21 stages and 9 cuffs; import order/fallbacks; SP bounded private geometry/textures, oversized fallback, held guns, teardown/reload; MP unchanged");
    return 0;
}
