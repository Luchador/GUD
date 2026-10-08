static DWORD ReadWord(const unsigned char *p)
{ return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }

static void CycleMaterials(void)
{
    for(int cycle=0;cycle<4;cycle++) for(int alpha=0;alpha<2;alpha++)
        for(int type=0;type<=4;type++) for(int known=0;known<2;known++) {
            BgRenderState state; BgMaterial m,old;
            BgRenderStateInit(&state,FALSE); BgMaterialInit(&m);
            if(known) BgRenderStateRead(&state,0xBA001402u,(DWORD)cycle<<20);
            m.textureword0=0xC0680000u|type; m.textureword1=0x123;
            m.combineword0=0xFC26A004u; m.combineword1=alpha?0x1F1093FFu:0x1FFC93FCu;
            m.alphasource=BG_ALPHA_VERTEX; m.fog=BG_FOG_OFF;
            m.environment=BG_ENV_LINEAR; old=m;
            BOOL repair=known && cycle==0 && type==2;
            assert(BgRenderRepairTextureCombiner(&state,&m)==repair);
            if(repair) {
                assert(m.combineword0==(alpha?0xFC121824u:0xFC127E24u));
                assert(m.combineword1==(alpha?0xFF33FFFFu:0xFFFFF9FCu));
                assert(!BgRenderRepairTextureCombiner(&state,&m));
                m.combineword0=old.combineword0; m.combineword1=old.combineword1;
            }
            assert(BgMaterialEqual(&m,&old));
            /* Custom muxes and texturing disabled are never rewritten. */
            m.combineword1=0x1F1093FEu; old=m;
            assert(!BgRenderRepairTextureCombiner(&state,&m) && BgMaterialEqual(&m,&old));
            m.combineword1=0x1F1093FFu; m.modeword0&=~255u; old=m;
            assert(!BgRenderRepairTextureCombiner(&state,&m) && BgMaterialEqual(&m,&old));
        }
    puts("PASS: one-cycle RGB/alpha repair, explicit/inherited/copy/fill modes, detail/custom exclusions and material metadata preservation.");
}

/* Inspect emitted native commands independently of BgDocumentLoad's repair.
 * A successful reload alone could otherwise conceal a bad compiled combiner. */
static DWORD InvalidNativeCombiners(const BgFile *bg)
{
    DWORD table=ReadWord(bg->data+4)&0xffffff, count=0;
    for(DWORD record=table+24;ReadWord(bg->data+record+4);record+=24)
        for(DWORD layer=0;layer<2;layer++) {
            DWORD at=ReadWord(bg->data+record+4+layer*4)&0xffffff;
            if(!at) continue;
            DWORD end=at+ReadWord(bg->data+at-4),cycle=3,c0=0,c1=0;
            for(;at+8<=end;at+=8) {
                DWORD a=ReadWord(bg->data+at),b=ReadWord(bg->data+at+4);
                if(a==0xBA001402u) cycle=(b>>20)&3;
                if((a>>24)==0xFC) { c0=a;c1=b; }
                if(((a>>24)==0xB1||(a>>24)==0xBF) && cycle==0
                    && c0==0xFC26A004u && (c1==0x1F1093FFu||c1==0x1FFC93FCu)) count++;
            }
        }
    return count;
}

static void CycleDocuments(const char *dir)
{
    BgFile source=Fixture(),compiled={0},optimized={0};
    BgDocument doc={0},before={0},loaded={0}; const char *why=""; BOOL changed;
    BgFaceRef refs[20];
    /* Both layers of room 1 start in one-cycle. Primary's last face switches
     * to two-cycle WITHOUT rewriting the mux: preserve its original meaning. */
    for(DWORD layer=0;layer<2;layer++) {
        DWORD at=ReadWord(source.data+32+24+4+layer*4)&0xffffff;
        Put(source.data+at+12,0);
        if(!layer) { Put(source.data+at+80,0xBA001402u); Put(source.data+at+84,0x100000u); }
    }
    assert(InvalidNativeCombiners(&source)==3);
    assert(BgDocumentLoad(source.data,source.size,1,&doc,&why) && doc.dirty);
    assert(Refs(&doc,refs)==20);
    for(DWORD r=1;r<=2;r++) for(DWORD f=0;f<doc.rooms[r].facecount;f++) {
        const BgDocumentFace *face=&doc.rooms[r].faces[f];
        BOOL repaired=r==1 && (face->layer || f<4);
        assert(face->material.combineword0==(repaired?0xFC121824u:0xFC26A004u));
    }
    assert(BgDocumentCompileProject(&doc,&source,&compiled,&why));
    assert(!InvalidNativeCombiners(&compiled));
    assert(BgDocumentLoad(compiled.data,compiled.size,1,&loaded,&why) && !loaded.dirty);
    SameMaterials(&doc,&loaded); BgDocumentFree(&loaded); BgFileFree(&compiled);
    /* Create ROM also goes through this optimizer for unopened levels. */
    BOOL ok=BgFileOptimize(&source,&optimized,&why);
    if(!ok || !optimized.data) fprintf(stderr,"Optimize: ok=%d output=%p reason=%s\n",ok,(void*)optimized.data,why);
    assert(ok && optimized.data);
    assert(!InvalidNativeCombiners(&optimized)); BgFileFree(&optimized);
    RoundTripMaterials(&doc,&source,dir);

    /* New assignment on a one-cycle SHADE-only face, undo/redo and reapply. */
    BgDocumentFace *face=&doc.rooms[1].faces[0];
    BgMaterialSetTexture(&face->material,BG_TEX_NONE); face->textureid=BG_TEX_NONE;
    assert(BgDocumentClone(&doc,&before,&why));
    EditHistory history={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
    SetupFile setup={0}; StanFile stan={0};
    EditHistoryReset(&history,&doc,NULL,NULL);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Texture",&tx,&why));
    assert(BgDocumentSetFaceTexture(&doc,refs,1,0x1de,&changed,&why) && changed);
    assert(face->material.combineword0==0xFC121824u && face->material.combineword1==0xFF33FFFFu);
    assert(EditHistoryCommitEdit(&history,&doc,NULL,NULL,&tx,&why));
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,&asset,&why)); SameMaterials(&doc,&before);
    assert(EditHistoryRedo(&history,&doc,&setup,&stan,&asset,&why));
    assert(BgDocumentSetFaceTexture(&doc,refs,1,0x1de,&changed,&why) && !changed);
    RoundTripMaterials(&doc,&source,dir);
    assert(BgDocumentCompile(&doc,&source,&compiled,&why) && !InvalidNativeCombiners(&compiled));
    BgFaceRef stale[2]={refs[0],{.room=1,.faceid=999999}};
    BgMaterial preserved=doc.rooms[1].faces[0].material;
    assert(!BgDocumentSetFaceTexture(&doc,stale,2,7,&changed,&why) && !changed);
    assert(BgMaterialEqual(&preserved,&doc.rooms[1].faces[0].material));
    EditHistoryFree(&history); BgDocumentFree(&before); BgDocumentFree(&doc);
    BgFileFree(&source); BgFileFree(&compiled);
    puts("PASS: old-file repair, inherited mux across cycle transitions, both layers, assignment, atomic rejection, save/reload, native export and undo/redo.");
}
