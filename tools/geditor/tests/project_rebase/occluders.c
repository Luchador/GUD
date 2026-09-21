static void OccluderRebases(const GEditorProject *source,const char *incoming,const char *parent)
{
    GEditorProject authored,rebased,again;
    ProjectRebaseReport report;
    SetupFile setup={0},loaded={0}; SetupPadRef pad;
    RomFile rom={0};
    char path[MAX_PATH],exported[MAX_PATH],basepath[MAX_PATH];
    DWORD size,offset,span;
    unsigned char *data=Read(incoming,&size);
    OK(ProjectRebaseCreate(source,incoming,parent,"OccluderAuthoring",&authored,&report,&why));
    OK(SetupLoadProjectFile(authored.dir,"Ump_setuptestZ",&setup,&why));
    OK(SetupFileAddOccluder(&setup,1,(double[]){50,250,50},&pad,&why));
    OK(SetupSaveProjectFile(authored.dir,&setup,&why));
    SetupFileFree(&setup);
    /* Old runtime cannot export these boxes, but this must not prevent the
     * project from being upgraded to a runtime that understands them. */
    OK(!RomExportValidateProject(&authored,&why) && strstr(why,"occluders"));
    Put32(data+MANIFEST+SHIFT+20,18);
    Entry(data,SHIFT,17,"OCCL",0,0,OCCLUDER_VERSION);
    Put32(data+MANIFEST+SHIFT+24+17*16+4,0);
    Path(path,parent,"occluders.z64");Save(path,data,size);free(data);
    OK(ProjectRebaseCreate(&authored,path,parent,"OccluderSupported",&rebased,&report,&why));
    OK(SetupLoadProjectFile(rebased.dir,"Ump_setuptestZ",&loaded,&why));
    OK(loaded.boundpads[pad.index].pad.occluder && loaded.boundpads[pad.index].pad.pos[1]==250);
    SetupFileFree(&loaded);
    OK(RomExportCreate(&rebased,"OccluderPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&rom,&why));
    OK(RomFindFile(&rom,"Ump_setuptestZ",&offset,&span,&why));
    DWORD table=Get32(rom.data+offset+28);
    OK(Get32(rom.data+offset+table+pad.index*68+40)==OCCLUDER_PAD_TAG);
    RomFree(&rom);
    OK(ProjectRebaseCreate(&rebased,path,parent,"OccluderAgain",&again,&report,&why));
    OK(SetupLoadProjectFile(again.dir,"Ump_setuptestZ",&loaded,&why));
    OK(loaded.boundpads[pad.index].pad.occluder);SetupFileFree(&loaded);
    Path(basepath,rebased.dir,"base.z64");DWORD hash=Hash(basepath);
    OK(!ProjectRebaseCheck(&rebased,incoming,&report,&why) && strstr(why,"occluders"));
    OK(Hash(basepath)==hash);
    puts("PASS: occluder authoring on old base, capability upgrade, save/reopen/export, repeat rebase and unsupported downgrade rejection.");
}
