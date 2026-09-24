static void SameBank(const RomFile *a,const RomFile *b)
{
    TexRomBank ab,bb;
    OK(TexRomReadBank(a,&ab,&why)&&TexRomReadBank(b,&bb,&why));
    OK(ab.count==bb.count&&ab.hash==bb.hash&&ab.imagebytes==bb.imagebytes);
    OK(!memcmp(a->data+ab.images,b->data+bb.images,ab.imagebytes));
    OK(!memcmp(a->data+ab.table,b->data+bb.table,(ab.count+1)*8));
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
    // Keeping the base never resolves a genuinely different new image at an
    // imported ID. Failed checks/publication must leave all source files intact.
    OK(TexRomReadBank(&changed,&bank,&why));changed.data[bank.table+8]^=1;
    Save(large,changed.data,changed.size);
    OK(!ProjectRebaseCheck(source,large,TRUE,&report,&why)&&strstr(why,"Image 0001"));
    OK(!ProjectRebaseCreate(source,large,TRUE,parent,"KeptButConflictingImport",&rejected,&report,&why));
    OK(!rejected.dir[0]);Path(path,parent,"KeptButConflictingImport");OK(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES);
    test_fail_move=1;
    OK(!ProjectRebaseCreate(source,different,TRUE,parent,"KeptPublishFailure",&rejected,&report,&why));
    Path(path,parent,"KeptPublishFailure");OK(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES);
    OK(Hash(base)==sourceHash&&Hash(different)==incomingHash);NoTemps(parent);OK(RomExportValidateProject(source,&why));
    RomFree(&before);RomFree(&rebased);RomFree(&original);RomFree(&changed);free(texture);
    puts("PASS: explicit base-image choice, changed formats/sizes/settings, exact payload/flags, new/retained IDs, repeated rebase/export, import conflicts and rollback.");
}
