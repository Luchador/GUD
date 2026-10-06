/* Only engine services are stubbed. The complete production player creation
 * and teardown functions run, including their real buffer layout arithmetic. */
#define GUNRIGHT 0
#define GUNLEFT 1
#define CAMERAMODE_SWIRL 1
#define PROP_TYPE_VIEWER 2
#define CHRFLAG_INIT 1
#define ALIGN64_V3(x) ((x)&~63)
typedef int ITEM_IDS;
typedef struct {u8 raw[24];} ModelNode;
typedef struct {u32 TextureID; u8 data[8];} ModelFileTextures;
typedef struct {struct {u32 w0,w1;} words;} Gfx;
#define G_NOOP 0xc0
#define TEXTURETYPE_DETAIL 1
typedef struct {void *RootNode; s32 numRecords; ModelNode **Switches;
    ModelFileTextures *Textures; s32 numSwitches,numtextures;} ModelFileHeader;
typedef struct {void *anim; f32 scale; s32 rwdatalen; u32 *datas;} Model;
typedef struct {Model *model; s32 chrflags;} ChrRecord;
typedef struct {ChrRecord *chr; int type, pos; void *stan;} PropRecord;
typedef struct {int unused;} WeaponObjRecord;
typedef struct ItemModelFileRecord {int unused;} ItemModelFileRecord;
struct player {PropRecord *prop; Model *bodyModel; s32 bondtype;};
struct texpool {u8 *start; s32 size, used; u8 loaded[4096];};
static struct {u32 textureCount;} g_TextureRomConfig={4096};
static int textureBytes=2048;
static int romTextureBytes[4096];
static u32 rawWords[129][28000];
static struct {u32 rom_size,poolRemaining;} rawInfo[129];
static struct {u8 *hw_address;} rawFiles[129];
static int rawFirst[129], rawMarkers[129], rawLast;
static int fileGetIndex(u8 *name) {return name[0]=='P' ? 129 : atoi((char *)name+1)+1;}
#define obInfo(index) (&rawInfo[(index)-1])
#define obFile(index) (&rawFiles[(index)-1])
static void romCopy(u8 *dst,u8 *src,u32 size) {
    for(int i=0;i<129;i++) if(src==(u8 *)rawWords[i]) rawLast=i;
    memcpy(dst,src,size);
}
static void sub_GAME_7F075A90(ModelFileHeader *h,s32 vma,void *scratch) {}
static void modelIterateDisplayLists(ModelFileHeader *h,ModelNode **node,Gfx **gdl) {
    *gdl=(Gfx *)(uintptr_t)(0x05000000+rawFirst[rawLast]);
}
static int check_if_imageID_is_light(int n) {return n==4095;}
static void texLoadFromTextureNum(int n,struct texpool *p) {
    int bytes=textureBytes ? textureBytes : (n>=0 && n<4096 ? romTextureBytes[n] : 0);
    if(n>=0 && n<4096 && bytes>0 && !p->loaded[n] && p->used+bytes<=p->size) {
        p->loaded[n]=1;p->used+=bytes;
    }
}
static void *texFindInPool(int n,struct texpool *p) {return n>=0 && n<4096 && p->loaded[n] ? p : NULL;}
static void InitRawFixture(int id,int size,int markers) {
    rawInfo[id].rom_size=size;rawInfo[id].poolRemaining=0x12345678;
    rawFiles[id].hw_address=(u8 *)rawWords[id];rawFirst[id]=size-8*(markers+1);rawMarkers[id]=markers;
    memset(rawWords[id],0,sizeof(rawWords[id]));
    for(int j=0;j<markers;j++) {
        rawWords[id][rawFirst[id]/4+j*2]=0xc0000002;
        rawWords[id][rawFirst[id]/4+j*2+1]=1+(j%12)+(id>=80 ? 12 : 0);
    }
    rawWords[id][size/4-2]=0xb8000000;
}
static ModelFileHeader headers[128], gunheaders[2], gunasset;
static struct {ModelFileHeader *header; char *filename;} CitemZ_entries[128], PitemZ_entries[1];
static WeaponObjRecord dummy_08_pp7_obj[1];
static _Alignas(64) u8 gunbuffer[2][0x14820];
static char filenames[128][16];
static Model mpmodel;
static ChrRecord chr;
static PropRecord playerprop;
static struct player player;
static struct player *g_CurrentPlayer=&player;
static int g_CameraMode, g_StartingWeapons[2]={7,0}, g_bondviewForceDisarm;
static int selected_folder_num, stage=LEVELID_DAM, players=1, helditem=3;
static int stageLoads, scratchLoads, gunLoads, removed, animations, setupCalls;
static int loadedBody, loadedHead, chosenItem, mpBody, mpHead;

static s32 fileGetBondForCurrentFolder(void) {return fileGetBondForFolder(selected_folder_num);}
static f32 bondviewGetPlayerYawRadians(void) {return 0;}
static u8 *getPlayerWeaponBufferForHand(int hand) {return gunbuffer[hand];}
static s32 getSizeBufferWeaponInHand(int hand) {return sizeof(gunbuffer[hand]);}
static int get_item_in_hand_or_watch_menu(int hand) {return helditem;}
static void bondviewDeregisterPlayerRoom(struct player *p) {}
static void bondviewUpdatePlayerRoom(struct player *p) {}
static int getPlayerCount(void) {return players;}
static int bossGetStageNum(void) {return stage;}
static int get_cur_playernum(void) {return 0;}
static int get_player_mp_char_head(int n) {return mpHead;}
static int get_player_mp_char_body(int n) {return mpBody;}
static void remove_item_in_hand(int hand) {removed++;}
static void texInitPool(struct texpool *pool,u8 *buf,s32 size) {pool->start=buf;pool->size=size;pool->used=0;memset(pool->loaded,0,sizeof(pool->loaded));}
static ModelFileHeader *get_ptr_itemheader_in_hand(int hand) {return &gunheaders[hand];}
static int modelIndex(const char *name) {return atoi(name+1);}
static void fileLoad(ModelFileHeader *header,char *name) {
    int id=modelIndex(name);
    assert(header==&headers[id] && header->RootNode==NULL);
    /* Stage geometry can be larger than either gun buffer. */
    header->RootNode=calloc(1,id>=80 ? 110000 : 12000);
    assert(header->RootNode);
    stageLoads++;
}
static s32 get_pc_buffer_remaining_value(u8 *name) {
    int id=fileGetIndex(name)-1;
    return rawInfo[id].rom_size+rawMarkers[id]*128;
}
static void load_object_fill_header(ModelFileHeader *header,u8 *name,u8 *dst,s32 size,struct texpool *pool) {
    s32 count=get_pc_buffer_remaining_value(name);
    assert(dst>=gunbuffer[0] && dst+size==gunbuffer[0]+sizeof(gunbuffer[0]));
    assert(count<=size && pool->start==gunbuffer[1] && pool->size==sizeof(gunbuffer[1]));
    if(name[0]=='P') gunLoads++;
    else {
        scratchLoads++;
    }
    memset(dst,0xa5,count);
    header->RootNode=dst;
}
static void modelCalculateRwDataLen(ModelFileHeader *header) {assert(header->numRecords>0);}
static void animInit(Model *model,ModelFileHeader *header,u32 *rwdata) {
    assert((u8 *)model>=gunbuffer[0] && (u8 *)model+ANIM_MODEL_ALLOCATION_SIZE<gunbuffer[0]+sizeof(gunbuffer[0]));
    assert((u8 *)rwdata>=(u8 *)model+ANIM_MODEL_ALLOCATION_SIZE);
    memset(model,0,ANIM_MODEL_ALLOCATION_SIZE);
    model->datas=rwdata; model->anim=model; model->scale=1;
    animations++;
}
static Model *makeonebody(s32 body,s32 head,ModelFileHeader *bh,ModelFileHeader *hh,int glasses,Model *model) {
    assert(bh->RootNode && hh->RootNode);
    loadedBody=body;loadedHead=head;
    if(players==1) {
        assert(model && model->rwdatalen>=bh->numRecords+hh->numRecords);
        assert((u8 *)(model->datas+model->rwdatalen)<=gunbuffer[0]+sizeof(gunbuffer[0]));
        memset(model->datas,0,model->rwdatalen*sizeof(u32));
    } else {
        assert(model==NULL);model=&mpmodel;model->scale=1;
    }
    return model;
}
static void modelSetScale(Model *model,f32 scale) {model->scale=scale;}
static void init_GUARDdata_with_set_values(PropRecord *prop,Model *model,int *pos,f32 yaw,void *stan,void *unused) {
    chr.model=model;chr.chrflags=0;prop->chr=&chr;setupCalls++;
}
static void setsuboffset(Model *model,int *pos) {}
static void setsubroty(Model *model,f32 yaw) {}
static int getPropForHeldItem(int item) {chosenItem=item;return item<0 ? -1 : 0;}
static void something_with_generating_object(ChrRecord *c,int prop,int item,int zero,WeaponObjRecord *dst,ItemModelFileRecord *header) {
    if(players==1) {
        /* N64 stores this address in s32. Compare its low word on a 64-bit host. */
        u32 offset=(u32)(uintptr_t)dst-(u32)(uintptr_t)gunbuffer[0];
        assert(offset<sizeof(gunbuffer[0]));
        assert(offset>=(u8 *)(c->model->datas+c->model->rwdatalen)-gunbuffer[0]);
        assert(header==(ItemModelFileRecord *)&gunheaders[1]);
    } else assert(dst==NULL && header==NULL);
}
static void chrlvMergeKneelToStand(ChrRecord *c,f32 f) {}
static void chrpropCleanupForRemoval(PropRecord *prop) {prop->chr->model=NULL;}
