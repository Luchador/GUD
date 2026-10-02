/* Whole-model migration and explicit binary-asset conflict decisions. */
static void AssetRebases(const GEditorProject *source,const char *incoming,const char *parent)
{
    DWORD size,n,offset,span,localSize,checkSize;char rompath[MAX_PATH],path[MAX_PATH],preview[MAX_PATH],exported[MAX_PATH];
    unsigned char *rom=Read(incoming,&size),*local=NULL,*bytes=NULL,*check=NULL;
    ProjectRebaseReport report;GEditorProject kept,taken,again,fresh,automatic,matching;RomFile old={0},built={0};ModelSource model={0};
    ProjectRebaseOptions options={FALSE,PROJECT_REBASE_STOP,PROJECT_REBASE_STOP};
    Path(path,source->dir,"base.z64");OK(RomLoad(path,&old,&why));
    OK(RomFindFile(&old,"Pjungle3_treeZ",&offset,&span,&why));
    OK(ModelEditsReadReplacement(source->dir,"Pjungle3_treeZ",old.data+offset,span,&local,&localSize,&why)==1);
    OK(ModelReadSource(old.data+offset,span,&model,&why));DWORD vertex=model.vertexoffsets[0];ModelFreeSource(&model);
    DWORD sourceBaseHash=Hash(path);Path(path,source->dir,"models/native/Pjungle3_treeZ.gmodel");DWORD sourceModelHash=Hash(path);
    bytes=Read(path,&n);
    /* Our project moves X; the incoming model moves Y, and both edit the BG. */
    rom[MODEL+SHIFT+vertex+3]^=1;rom[OBJECTS+SHIFT+64+128]=2;
    Path(rompath,parent,"AssetChanges.z64");Save(rompath,rom,size);
    OK(!ProjectRebaseCheckWithOptions(source,rompath,&options,&report,&why));
    OK(report.conflicts==2&&strstr(report.details,"bg/bg_test.seg")&&strstr(report.details,"Pjungle3_treeZ"));
    options.levelConflicts=PROJECT_REBASE_KEEP_PROJECT;options.modelConflicts=PROJECT_REBASE_KEEP_PROJECT;
    OK(ProjectRebaseCheckWithOptions(source,rompath,&options,&report,&why));
    OK(report.resolved==2&&report.modelskept==1&&!report.modelsupdated);
    OK(ProjectRebaseCreateWithOptions(source,rompath,&options,parent,"KeepAssets",&kept,&report,&why));NoTemps(parent);
    Same(source->dir,kept.dir,"bg/bg_test.seg");Same(source->dir,kept.dir,"models/objects/Pjungle3_treeZ.gltf");
    Path(path,kept.dir,"models/native/Pjungle3_treeZ.gmodel");check=Read(path,&checkSize);
    OK(checkSize==n&&!memcmp(bytes,check,4)&&!memcmp(bytes+8,check+8,n-8));
    OK(Get32(check+4)==ModelDataHash(rom+MODEL+SHIFT,span));free(check);
    OK(ProjectRead(kept.geppath,&again)&&RomExportValidateProject(&again,&why));
    OK(RomExportCreate(&kept,"KeptAssetsPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&built,&why)&&RomFindFile(&built,"Pjungle3_treeZ",&offset,&span,&why));
    OK(span==localSize&&!memcmp(built.data+offset,local,span));RomFree(&built);
    OK(ProjectRebaseCreateWithOptions(&kept,rompath,&options,parent,"KeepAssetsAgain",&again,&report,&why));
    Same(kept.dir,again.dir,"models/native/Pjungle3_treeZ.gmodel");
    options.levelConflicts=PROJECT_REBASE_USE_ROM;options.modelConflicts=PROJECT_REBASE_USE_ROM;
    OK(ProjectRebaseCreateWithOptions(source,rompath,&options,parent,"TakeAssets",&taken,&report,&why));
    OK(report.resolved==2&&report.modelsupdated==1&&!report.modelskept);
    Path(path,taken.dir,"models/native/Pjungle3_treeZ.gmodel");OK(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES);
    Path(path,taken.dir,"bg/bg_test.seg");check=Read(path,&checkSize);OK(check[128]==2);free(check);
    Path(preview,taken.dir,"models/objects/Pjungle3_treeZ.gltf");GltfModelImport imported={0};
    OK(GltfReadModelImport(preview,ModelDataHash(rom+MODEL+SHIFT,localSize),&imported,&why));GltfFreeModelImport(&imported);
    OK(RomExportCreate(&taken,"TakenAssetsPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&built,&why)&&RomFindFile(&built,"Pjungle3_treeZ",&offset,&span,&why));
    OK(span==localSize&&!memcmp(built.data+offset,rom+MODEL+SHIFT,span));RomFree(&built);
    /* Updated stock model, without a project override: strict defaults adopt it. */
    OK(ProjectCreate("UneditedAssets",parent,&old.info,&fresh,&why));OK(RomExportStoreProjectBase(&fresh,&old,&why));
    Folder(fresh.dir,"images");Path(path,source->dir,"images/0000.bmp");
    Path(preview,fresh.dir,"images/0000.bmp");OK(CopyFile(path,preview,TRUE));
    options.levelConflicts=PROJECT_REBASE_STOP;options.modelConflicts=PROJECT_REBASE_STOP;
    OK(ProjectRebaseCheckWithOptions(&fresh,rompath,&options,&report,&why)&&report.modelsupdated==1&&!report.conflicts);
    OK(ProjectRebaseCreateWithOptions(&fresh,rompath,&options,parent,"AutomaticAssets",&automatic,&report,&why));
    Path(preview,automatic.dir,"models/objects/Pjungle3_treeZ.gltf");OK(GetFileAttributes(preview)!=INVALID_FILE_ATTRIBUTES);
    /* Identical local/incoming native edits retain material metadata and only
     * advance the override's base fingerprint, without a conflict choice. */
    memcpy(rom+MODEL+SHIFT,local,localSize);Save(rompath,rom,size);
    options.levelConflicts=PROJECT_REBASE_KEEP_PROJECT;
    OK(ProjectRebaseCreateWithOptions(source,rompath,&options,parent,"MatchingAssets",&matching,&report,&why));
    OK(report.modelskept==1&&report.resolved==1);
    Path(path,matching.dir,"models/native/Pjungle3_treeZ.gmodel");check=Read(path,&checkSize);
    OK(checkSize==n&&!memcmp(bytes+8,check+8,n-8)&&Get32(check+4)==ModelDataHash(local,localSize));free(check);
    /* Choosing the ROM must not suppress malformed native data or stale edits. */
    options.modelConflicts=PROJECT_REBASE_USE_ROM;
    Path(path,kept.dir,"models/native/Pjungle3_treeZ.gmodel");check=Read(path,&checkSize);check[4]^=1;Save(path,check,checkSize);
    OK(!ProjectRebaseCheckWithOptions(&kept,rompath,&options,&report,&why));check[4]^=1;Save(path,check,checkSize);free(check);
    memset(rom+MODEL+SHIFT,0,localSize);Save(rompath,rom,size);
    OK(!ProjectRebaseCheckWithOptions(source,rompath,&options,&report,&why));
    Path(path,source->dir,"base.z64");OK(Hash(path)==sourceBaseHash);
    Path(path,source->dir,"models/native/Pjungle3_treeZ.gmodel");OK(Hash(path)==sourceModelHash);
    OK(RomExportValidateProject(source,&why));NoTemps(parent);
    free(bytes);free(local);free(rom);RomFree(&old);
    puts("PASS: changed stock models adopt automatically; competing BG/model edits stop or resolve explicitly; model fingerprints/previews migrate; native ROM exports and repeat rebase pass; corrupt assets remain blocked and source files unchanged.");
}

/* Optional local ROM pair: use their real geometry, texture banks and catalogs. */
static void AssetCorpus(const char *base,const char *incoming,const char *parent)
{
    RomFile old={0},next={0},built={0};GEditorProject project,rebased,reopened;
    ProjectRebaseReport report;char path[MAX_PATH],exported[MAX_PATH],name[64];
    TexRomBank bank;TexPixel *pixels=malloc(256*256*sizeof(*pixels));DWORD off,span,other,otherSize,changed=0;
    OK(pixels&&RomLoad(base,&old,&why)&&RomLoad(incoming,&next,&why));
    OK(ProjectCreate("AssetCorpus",parent,&old.info,&project,&why));
    OK(RomExportStoreProjectBase(&project,&old,&why));
    Folder(project.dir,"bg");Folder(project.dir,"stan");Folder(project.dir,"setup");Folder(project.dir,"text");Folder(project.dir,"images");
    OK(TexRomReadBank(&old,&bank,&why));off=bank.images;
    for (DWORD i=0;i<bank.count;i++)
    {
        int width,height;span=Get32(old.data+bank.table+i*8)&0xffffffu;
        OK(TexDecodeRecord(old.data+off,span,pixels,&width,&height));
        snprintf(name,sizeof(name),"images/%04lX.bmp",(unsigned long)i);Path(path,project.dir,name);
        OK(TexWriteBmp(path,pixels,width,height));off+=span;
    }
    free(pixels);
    for (DWORD i=1;RomGetFileByIndex(&old,i,name,sizeof(name),&off,&span);i++)
        if (RomExportProjectResourcePath(&project,name,path,sizeof(path))==1) { Save(path,old.data+off,span); }
    OK(ProjectRebaseCheck(&project,incoming,TRUE,&report,&why));
    OK(!report.conflicts&&report.modelsupdated>0);
    OK(ProjectRebaseCreate(&project,incoming,TRUE,parent,"AssetsFromROM",&rebased,&report,&why));
    OK(ProjectRead(rebased.geppath,&reopened)&&RomExportValidateProject(&reopened,&why));
    OK(RomExportCreate(&reopened,"AssetsFromROMPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&built,&why));
    for (DWORD i=1;RomGetFileByIndex(&old,i,name,sizeof(name),&off,&span);i++)
    {
        if (!strchr("CPG",name[0])||!name[0]) { continue; }
        OK(RomFindFile(&next,name,&other,&otherSize,&why));
        if (span==otherSize&&!memcmp(old.data+off,next.data+other,span)) { continue; }
        DWORD result,resultSize;OK(RomFindFile(&built,name,&result,&resultSize,&why));
        OK(resultSize==otherSize&&!memcmp(built.data+result,next.data+other,otherSize));
        const char *folder=name[0]=='C' ? "characters" : name[0]=='P' ? "objects" :
            (!strcmp(name,"GcartblueZ")||!strcmp(name,"GcartridgeZ")||!strcmp(name,"GcartrifleZ")||!strcmp(name,"GcartshellZ")) ? "casings" : "guns";
        char relative[128];snprintf(relative,sizeof(relative),"models/%s/%s.gltf",folder,name);Path(path,rebased.dir,relative);
        GltfModelImport preview={0};OK(GltfReadModelImport(path,ModelDataHash(next.data+other,otherSize),&preview,&why));GltfFreeModelImport(&preview);
        fprintf(stderr,"Updated ROM model: %s\n",name);changed++;
    }
    OK(changed==report.modelsupdated);NoTemps(parent);
    RomFree(&old);RomFree(&next);RomFree(&built);
    printf("PASS: local ROM pair rebases, reopens and exports with %lu updated models matching incoming native bytes and valid refreshed previews.\n",(unsigned long)changed);
}
