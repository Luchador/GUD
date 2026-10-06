/* Production functions are supplied by run.py. */
static void Put16(u8 *p, u32 value) {p[0]=value>>8; p[1]=value;}
static void Put32(u8 *p, u32 value) {p[0]=value>>24; p[1]=value>>16; p[2]=value>>8; p[3]=value;}

static void Reset(int poolBytes)
{
    memset(g_Ram.data, 0xa7, sizeof(g_Ram.data));
    g_mempPools[MEMPOOL_STAGE] = (MemoryPool){g_Ram.data+0x200000,
        g_Ram.data+0x200003, g_Ram.data+0x400000, NULL}; /* Test unaligned bank. */
    texInitPool(&g_MainPool, g_Ram.data+0x1000, poolBytes);
    g_TexCacheCount=0;
    g_PayloadCopies=g_StageAllocations=0;
}

static void Fixture(int format, int width, int height, int lods)
{
    int palette = format >= TEXFORMAT_RGBA16_CI8 ? 2 : 0;
    int payload = texRawLevelBytes(format,width,height);
    free(g_Rom);
    g_RomSize=RAW_TEXTURE_BASE_HEADER_SIZE+payload;
    g_Rom=calloc(1,g_RomSize);
    memcpy(g_Rom,"GUTX",4);
    g_Rom[5]=lods; g_Rom[6]=1;
    Put16(g_Rom+8,palette); Put16(g_Rom+10,RAW_TEXTURE_BASE_HEADER_SIZE);
    Put32(g_Rom+12,g_RomSize);
    g_Rom[16]=format; g_Rom[17]=width; g_Rom[18]=height;
    Put32(g_Rom+20,RAW_TEXTURE_BASE_HEADER_SIZE); Put32(g_Rom+24,payload);
    if (!palette) memset(g_Rom+RAW_TEXTURE_BASE_HEADER_SIZE,0x33,payload);
    g_Textures[1]=0;
}

static void CheckCanary(u8 *p, int size)
{
    for (int i=0;i<size;i++) assert(p[i]==0xa7);
}

static struct tex *Load(int id)
{
    s32 word=id;
    texLoad(&word,NULL);
    struct tex *tex=texFindInPool(id,NULL);
    assert(tex && (u32)word==osVirtualToPhysical(tex->data));
    assert(texFindByData(word)==tex);
    return tex;
}

static void ExactFit(void)
{
    for (int format=0;format<13;format++) {
        Fixture(format,32,32,6);
        int bytes=texRawAllocationBytes(g_Rom);
        assert(bytes>0);
        Reset(bytes);
        struct tex *tex=Load(1);
        assert(g_MainPool.leftpos==(u8 *)g_MainPool.rightpos);
        assert(g_StageAllocations==0 && g_PayloadCopies==1);
        CheckCanary((u8 *)g_MainPool.end,32);
        assert(tex->width==32 && tex->height==32);
        Load(1);
        assert(g_PayloadCopies==1); /* Cache hit does not load/allocate again. */
    }
    Fixture(TEXFORMAT_IA8,32,32,0);
    Reset(texRawAllocationBytes(g_Rom));
    assert(texFreeBytesInBuffer(&g_MainPool)<0x10cc);
    Load(1); /* Old loader rejected this despite a perfect fit. */
    assert(!g_TexOverflow);
}

static void Overflow(void)
{
    Fixture(TEXFORMAT_IA8,32,32,0);
    Reset(64);
    struct tex *tex=Load(1);
    u8 *end=g_mempPools[MEMPOOL_STAGE].pos;
    assert(g_TexOverflow && g_StageAllocations==1);
    assert(g_MainPool.leftpos==g_MainPool.start && g_MainPool.rightpos==g_MainPool.end);
    assert(!memcmp(tex->data,g_Rom+RAW_TEXTURE_BASE_HEADER_SIZE,1024));
    CheckCanary(g_MainPool.start,64);
    Load(1);
    assert(g_PayloadCopies==1 && g_mempPools[MEMPOOL_STAGE].pos==end);
    g_Textures[2]=0;
    struct tex *second=Load(2);
    assert(second!=tex && texFindInPool(1,NULL)==tex);
    assert(g_StageAllocations==2);
    CheckCanary(g_mempPools[MEMPOOL_STAGE].pos,32);
    /* A private temporary pool neither clears nor borrows stage overflow. */
    struct texpool privatePool;
    texInitPool(&privatePool,g_Ram.data+0x80000,64);
    assert(texFindInPool(1,NULL)==tex && texFindInPool(1,&privatePool)==NULL);
    s32 word=1;
    texLoad(&word,&privatePool);
    assert(g_StageAllocations==2 && privatePool.leftpos==privatePool.start);
    CheckCanary(privatePool.start,96);
    texInitPool(&g_MainPool,g_Ram.data+0x1000,64);
    assert(!g_TexOverflow && !texFindInPool(1,NULL));
    Load(1);
    assert(g_StageAllocations==3 && g_PayloadCopies==3);
}

static void Bounds(void)
{
    Fixture(TEXFORMAT_RGBA32,64,64,1);
    Reset(8192); /* Passes old 4300-byte check; pixels alone need 16384. */
    Load(1);
    assert(g_StageAllocations==1);
    CheckCanary(g_MainPool.start,8192+32);

    Fixture(TEXFORMAT_RGBA16_CI8,64,64,7);
    int bytes=texRawAllocationBytes(g_Rom);
    Reset(bytes);
    struct tex *tex=Load(1);
    assert(tex->maxlod==1 && g_StageAllocations==0);
    /* The rejected generated mip must not overwrite the descriptor/canary. */
    assert(texFindByData(osVirtualToPhysical(tex->data))==tex);
    CheckCanary((u8 *)g_MainPool.end,32);

    Reset(64);
    g_mempPools[MEMPOOL_STAGE].end=g_mempPools[MEMPOOL_STAGE].pos;
    s32 word=1;
    texLoad(&word,NULL);
    assert(g_StageAllocations==0 && g_PayloadCopies==0 && !g_TexOverflow);
    assert(g_MainPool.leftpos==g_MainPool.start);
    g_mempPools[MEMPOOL_STAGE].pos=NULL;
    assert(mempTryAllocBytesInBank(1,MEMPOOL_STAGE)==NULL);

    Fixture(TEXFORMAT_IA8,32,32,0);
    Reset(16384);
    Put32(g_Rom+24,0x7fffffffu);
    word=1; texLoad(&word,NULL);
    assert(!g_PayloadCopies && !g_StageAllocations);
    CheckCanary(g_MainPool.start,16384+32);
    Put32(g_Rom+24,1024); Put32(g_Rom+20,0xfffffff0u);
    assert(texRawAllocationBytes(g_Rom)==0);
}

static void NativeBank(const char *path)
{
    FILE *file=fopen(path,"rb"); assert(file);
    fseek(file,0,SEEK_END); long length=ftell(file); rewind(file);
    free(g_Rom); g_Rom=malloc(length); g_RomSize=length;
    assert(fread(g_Rom,1,length,file)==(size_t)length); fclose(file);
    int offset=0,count=0,impacts=0;
    while (offset<length) {
        u8 *record=g_Rom+offset;
        assert(!memcmp(record,"GUTX",4));
        int bytes=texRawAllocationBytes(record);
        assert(bytes>0);
        Reset(bytes);
        g_Textures[count]=offset;
        Load(count);
        assert(g_MainPool.leftpos==(u8 *)g_MainPool.rightpos);
        CheckCanary((u8 *)g_MainPool.end,32);
        if (count==0xce || count==0x5c3 || count==0x5c4 || count==0x5c6
                || count==0x5c7 || (count>=0x878 && count<=0x87f)) {
            Reset(64); /* All thirteen impact textures with a full main pool. */
            Load(count);
            assert(g_StageAllocations==1 && g_PayloadCopies==1);
            impacts++;
        }
        offset+=texReadRawU32(record+12); count++;
    }
    printf("PASS: %d native texture records fit exactly; %d impact textures load beyond -mt\n",count,impacts);
}

int main(int argc, char **argv)
{
    ExactFit(); Overflow(); Bounds();
    if (argc>1) NativeBank(argv[1]);
    free(g_Rom);
    puts("PASS: exact sizing, overflow lookup/reuse/reset, private pools, rejected mips and bounded allocation failure");
    return 0;
}
