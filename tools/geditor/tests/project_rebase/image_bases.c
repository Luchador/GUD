static void SingleImageBank(const RomFile *rom)
{
    TexRomBank bank;DWORD count=0;
    OK(TexRomReadBank(rom,&bank,&why));
    for(DWORD at=0;at+100<=rom->size;)
    {
        TexInfoRecord record;
        if(!memcmp(rom->data+at,"GUTX",4)&&TexInfoReadRecord(rom->data+at,rom->size-at,&record))
        { count++;at+=record.size; }
        else at+=4;
    }
    OK(count==bank.count);
}

static void SameBank(const RomFile *a,const RomFile *b)
{
    TexRomBank ab,bb;
    OK(TexRomReadBank(a,&ab,&why)&&TexRomReadBank(b,&bb,&why));
    OK(ab.count==bb.count&&ab.hash==bb.hash&&ab.imagebytes==bb.imagebytes);
    OK(!memcmp(a->data+ab.images,b->data+bb.images,ab.imagebytes));
    OK(!memcmp(a->data+ab.table,b->data+bb.table,(ab.count+1)*8));
    SingleImageBank(b);
}

static void SameImage(const RomFile *a,const RomFile *b,DWORD id)
{
    TexRomBank ab,bb;DWORD aa,ba,size;
    OK(TexRomReadBank(a,&ab,&why)&&TexRomReadBank(b,&bb,&why)&&id<ab.count&&id<bb.count);
    OK(!memcmp(a->data+ab.table+id*8,b->data+bb.table+id*8,8));
    aa=ab.images;ba=bb.images;
    for(DWORD i=0;i<id;i++) { aa+=Get32(a->data+ab.table+i*8)&0xffffffu;ba+=Get32(b->data+bb.table+i*8)&0xffffffu; }
    size=Get32(a->data+ab.table+id*8)&0xffffffu;
    OK(!memcmp(a->data+aa,b->data+ba,size));
    SingleImageBank(b);
}

static void IncomingImagePreview(const char *project,const RomFile *rom,DWORD id)
{
    TexRomBank bank;int w,h,pw,ph;DWORD at;
    TexPixel *pixels=malloc(256*256*sizeof(*pixels)),*preview=malloc(256*256*sizeof(*preview));
    OK(pixels&&preview&&TexRomReadBank(rom,&bank,&why)&&id<bank.count);
    at=bank.images;
    for(DWORD i=0;i<id;i++) at+=Get32(rom->data+bank.table+i*8)&0xffffffu;
    OK(TexDecodeRecord(rom->data+at,Get32(rom->data+bank.table+id*8)&0xffffffu,pixels,&w,&h));
    OK(TexLoadSavedProjectImage(project,id,preview,&pw,&ph)&&w==pw&&h==ph);
    OK(!memcmp(pixels,preview,w*h*sizeof(*pixels)));free(pixels);free(preview);
}

static void ExistingImageBases(const GEditorProject *source,const char *incoming,const char *parent)
{
    GEditorProject kept,grown,shrunk,again,rejected;
    ProjectRebaseReport report;
    RomFile original={0},changed={0},rebased={0},before={0},after={0};
    TexRomBank bank;
    char path[MAX_PATH],base[MAX_PATH],different[MAX_PATH],large[MAX_PATH],exported[MAX_PATH];
    DWORD sourceHash,incomingHash,id,bytes;
    unsigned char *texture;
    TexPixel pixels[16*17];TexImportOptions options={0,1,3,4};
    for(id=0;id<16*17;id++)pixels[id]=(TexPixel){id%256,id/8,170,255};
    OK(TexEncodeRecord(pixels,16,17,&options,&texture,&bytes,&why));
    Path(base,source->dir,"base.z64");sourceHash=Hash(base);
    OK(RomLoad(base,&original,&why));
    Path(different,parent,"changed-base-image.z64");Path(large,parent,"changed-base-and-new-images.z64");
    OK(RomExportCreate(source,"BeforeImageChoice",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&before,&why));
    // A changed format/dimension/mipmap payload, then settings-only differences.
    for(int mode=0;mode<2;mode++) {
        const unsigned char *records[1]={texture};DWORD sizes[1]={bytes};unsigned char surfaces[1]={0x34};
        OK(RomLoad(incoming,&changed,&why)&&TexRomReadBank(&changed,&bank,&why));
        if(!mode)OK(TexRomUpdateImages(&changed,&bank,records,sizes,surfaces,1,&why));
        Put32(changed.data+bank.table,(Get32(changed.data+bank.table)&0xffffffu)|0xfa000000u);
        Put32(changed.data+bank.table+4,0x38d20000u); // legacy detail flags must not be normalized
        // Older exports carry abandoned banks. Both image-conflict choices
        // must discard these, including the path with no retained images.
        OK(TexRomReadBank(&changed,&bank,&why));
        memcpy(changed.data+0x140000,changed.data+bank.images,bank.imagebytes);
        Save(different,changed.data,changed.size);RomFree(&changed);incomingHash=Hash(different);
        OK(!ProjectRebaseCheck(source,different,FALSE,&report,&why)&&strstr(why,"Image 0000"));
        OK(ProjectRebaseCheck(source,different,TRUE,&report,&why));
        OK(report.imagespreserved==1&&!report.imagesretained&&!report.imagesadded&&strstr(report.details,"Image 0000"));
        OK(Hash(base)==sourceHash&&Hash(different)==incomingHash);NoTemps(parent);
        OK(ProjectRebaseCreate(source,different,TRUE,parent,mode?"KeepImageSettings":"KeepImageData",&kept,&report,&why));
        Path(path,kept.dir,"base.z64");OK(RomLoad(path,&rebased,&why));SameBank(&original,&rebased);RomFree(&rebased);
        Same(source->dir,kept.dir,"images/0000.bmp");Same(source->dir,kept.dir,"images/native/0001.gtex");
        OK(RomExportCreate(&kept,mode?"KeptSettingsROM":"KeptImageROM",parent,exported,sizeof(exported),&why));
        OK(RomLoad(exported,&after,&why));SameBank(&before,&after);RomFree(&after);
        // The other UI choice adopts complete incoming settings and refreshes
        // an existing BMP, even when only surface/detail flags changed.
        ProjectRebaseOptions take={PROJECT_REBASE_USE_ROM,PROJECT_REBASE_KEEP_PROJECT,PROJECT_REBASE_KEEP_PROJECT};
        OK(ProjectRebaseCreateWithOptions(source,different,&take,parent,mode?"TakeImageSettings":"TakeImageData",&kept,&report,&why));
        OK(report.imagesupdated==1&&!report.imagespreserved);
        OK(RomLoad(different,&changed,&why));
        Path(path,kept.dir,"base.z64");OK(RomLoad(path,&after,&why));SameBank(&changed,&after);
        IncomingImagePreview(kept.dir,&changed,0);
        OK(RomExportCreate(&kept,mode?"TakenSettingsROM":"TakenImageROM",parent,exported,sizeof(exported),&why));
        RomFree(&after);OK(RomLoad(exported,&after,&why));SameImage(&changed,&after,0);
        for(DWORD i=1;i<4;i++) SameImage(&before,&after,i);
        RomFree(&after);RomFree(&changed);
    }
    // Incoming stock slots may absorb identical project imports. Add a fifth
    // slot as well: the old prefix is kept, new slots retain incoming bytes.
    OK(RomLoad(incoming,&changed,&why));OK(ImageEditsExportToRom(source->dir,&changed,&why));
    OK(TexRomReadBank(&changed,&bank,&why)&&bank.count==4);
    {
        const unsigned char *records[5]={texture,NULL,NULL,NULL,texture};
        DWORD sizes[5]={bytes,0,0,0,bytes};unsigned char surfaces[5]={0x34,0,0,0,0x56};
        OK(TexRomUpdateImages(&changed,&bank,records,sizes,surfaces,5,&why));
    }
    Save(large,changed.data,changed.size);
    OK(ProjectRebaseCheck(source,large,TRUE,&report,&why)&&report.imagespreserved==1&&report.imagesadded==4);
    OK(ProjectRebaseCreate(source,large,TRUE,parent,"KeepImagesWithNewSlots",&grown,&report,&why));
    Path(path,grown.dir,"base.z64");OK(RomLoad(path,&rebased,&why));
    {
        TexRomBank a,b,c;DWORD oldbytes,newbytes;
        OK(TexRomReadBank(&original,&a,&why)&&TexRomReadBank(&changed,&b,&why)&&TexRomReadBank(&rebased,&c,&why));
        oldbytes=Get32(original.data+a.table)&0xffffffu;newbytes=Get32(changed.data+b.table)&0xffffffu;
        OK(c.count==5&&!memcmp(original.data+a.images,rebased.data+c.images,oldbytes));
        OK(!memcmp(changed.data+b.images+newbytes,rebased.data+c.images+oldbytes,b.imagebytes-newbytes));
        OK(!memcmp(changed.data+b.table+8,rebased.data+c.table+8,5*8));
    }
    OK(ProjectRebaseCreate(&grown,large,TRUE,parent,"KeepImagesAgain",&again,&report,&why));
    OK(ProjectRebaseCheck(&grown,different,TRUE,&report,&why)&&report.imagespreserved==1&&report.imagesretained==4);
    OK(ProjectRebaseCreate(&grown,different,TRUE,parent,"KeepImagesFewerSlots",&shrunk,&report,&why));
    Path(path,shrunk.dir,"base.z64");OK(RomLoad(path,&after,&why));SameBank(&rebased,&after);RomFree(&after);
    // Saved overrides on top of retained bases still export after fingerprint migration.
    OK(RomExportCreate(&grown,"BeforeRetainedShrink",parent,exported,sizeof(exported),&why));
    RomFree(&before);OK(RomLoad(exported,&before,&why));
    OK(RomExportCreate(&shrunk,"AfterRetainedShrink",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&after,&why));SameBank(&before,&after);RomFree(&after);
    // The image choice also covers conflicting imports at incoming stock IDs.
    OK(TexRomReadBank(&changed,&bank,&why));changed.data[bank.table+8]^=1;
    Save(large,changed.data,changed.size);
    OK(ProjectRebaseCheck(source,large,TRUE,&report,&why)&&report.imagespreserved==2&&strstr(report.details,"Image 0001"));
    OK(ProjectRebaseCreate(source,large,TRUE,parent,"KeptConflictingImport",&kept,&report,&why));
    Same(source->dir,kept.dir,"images/0001.bmp");
    OK(RomExportCreate(&kept,"KeptImportROM",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&after,&why));
    // Incoming slot 4 remains present and slot 1 retains the project edit.
    SameBank(&before,&after);RomFree(&after);
    ProjectRebaseOptions take={PROJECT_REBASE_USE_ROM,PROJECT_REBASE_KEEP_PROJECT,PROJECT_REBASE_KEEP_PROJECT};
    OK(ProjectRebaseCreateWithOptions(source,large,&take,parent,"TakenConflictingImport",&kept,&report,&why));
    OK(report.imagesupdated==2&&!report.imagespreserved);
    Path(path,kept.dir,"images/native/0001.gtex");OK(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES);
    IncomingImagePreview(kept.dir,&changed,0);IncomingImagePreview(kept.dir,&changed,1);
    OK(RomExportCreate(&kept,"TakenImportROM",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&after,&why));SameBank(&changed,&after);RomFree(&after);
    OK(ProjectRebaseCreateWithOptions(&kept,large,&take,parent,"TakenImagesAgain",&again,&report,&why));
    OK(!report.imagesupdated&&!report.imagespreserved);
    // A smaller incoming bank still retains the old suffix without reverting
    // a chosen incoming image in the shared prefix.
    OK(ProjectRebaseCreateWithOptions(&kept,different,&take,parent,"TakenImagesFewerSlots",&shrunk,&report,&why));
    OK(report.imagesretained==4);
    OK(RomLoad(different,&after,&why));IncomingImagePreview(shrunk.dir,&after,0);RomFree(&after);
    // An ordinary replacement/deletion at a shared base ID must disappear
    // only when the user takes a competing ROM change. Uncontested edits stay.
    GEditorProject edited;
    OK(ProjectRebaseCreate(source,incoming,TRUE,parent,"EditableImageSource",&edited,&report,&why));
    for(DWORD i=0;i<16*17;i++) pixels[i]=(TexPixel){255,0,255,255};
    OK(ImageEditsReplace(edited.dir,0,pixels,16,17,&options,"C:\\glass.bmp",&why));
    OK(ImageEditsSave(edited.dir,&why));ImageEditsReset();
    Path(path,edited.dir,"images/native/0000.gtex");DWORD editHash=Hash(path);
    OK(ProjectRebaseCreateWithOptions(&edited,incoming,&take,parent,"KeepUncontestedImage",&again,&report,&why));
    Same(edited.dir,again.dir,"images/native/0000.gtex");Same(edited.dir,again.dir,"images/0000.bmp");
    for(int deleted=0;deleted<2;deleted++)
    {
        if(deleted) { OK(ImageEditsDelete(edited.dir,0,&why));OK(ImageEditsSave(edited.dir,&why));ImageEditsReset(); }
        OK(ProjectRebaseCreateWithOptions(&edited,different,&take,parent,
            deleted?"TakeDeletedImage":"TakeReplacedImage",&again,&report,&why));
        OK(report.imagesupdated==1);
        Path(path,again.dir,"images/native/0000.gtex");OK(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES);
        OK(RomLoad(different,&after,&why));IncomingImagePreview(again.dir,&after,0);RomFree(&after);
        OK(RomExportValidateProject(&again,&why));
        if(!deleted) { Path(path,edited.dir,"images/native/0000.gtex");OK(Hash(path)==editHash); }
    }
    test_fail_move=1;
    OK(!ProjectRebaseCreate(source,different,TRUE,parent,"KeptPublishFailure",&rejected,&report,&why));
    Path(path,parent,"KeptPublishFailure");OK(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES);
    OK(Hash(base)==sourceHash&&Hash(different)==incomingHash);NoTemps(parent);OK(RomExportValidateProject(source,&why));
    RomFree(&before);RomFree(&rebased);RomFree(&original);RomFree(&changed);free(texture);
    puts("PASS: project/ROM image choices, changed formats/sizes/settings, exact payload/flags, previews, replacements/deletions, imported-ID conflicts, retained suffixes, repeated rebase/export and rollback.");
}
