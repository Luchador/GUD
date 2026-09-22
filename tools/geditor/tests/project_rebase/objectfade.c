static void ObjectFadeRebases(const GEditorProject *source,const char *incoming,const char *parent)
{
    GEditorProject authored,rebased,again;
    ProjectRebaseReport report;
    SetupFile setup={0},loaded={0}; SetupObjectProperties view;
    RomFile rom={0};
    char path[MAX_PATH],exported[MAX_PATH];
    DWORD size,index,duplicate,offset,span;
    unsigned char *data=Read(incoming,&size);
    BOOL changed;
    OK(ProjectRebaseCreate(source,incoming,parent,"FadeAuthoring",&authored,&report,&why));
    OK(SetupLoadProjectFile(authored.dir,"Ump_setuptestZ",&setup,&why));
    OK(SetupFileAddModel(&setup,FALSE,0,1,(double[]){50,0,50},&index,&why));
    SetupObjectPropertyEdit edit={.objectindex=index,.sourceoffset=setup.objects[index].sourceoffset,
        .type=setup.objects[index].type,.property=SETUP_OBJECT_FADE_DISTANCES,.value=20,.value2=30};
    OK(SetupFileSetObjectProperty(&setup,&edit,&changed,&why) && changed);
    OK(SetupFileDuplicateObject(&setup,&setup,index,&duplicate,&why));
    OK(SetupFileGetObjectProperties(&setup,duplicate,&view,&why) && view.customfade && view.fadestart==20 && view.fadeend==30);
    OK(SetupSaveProjectFile(authored.dir,&setup,&why)); SetupFileFree(&setup);
    OK(!RomExportValidateProject(&authored,&why) && strstr(why,"object fade"));
    /* Exercise the expanded reader limit, retaining unknown capabilities. */
    Put32(data+MANIFEST+SHIFT+20,33);
    for(int i=17;i<32;i++)
    {
        char tag[5]; snprintf(tag,sizeof(tag),"X%03d",i);
        Entry(data,SHIFT,i,tag,0,0,1); Put32(data+MANIFEST+SHIFT+24+i*16+4,0);
    }
    Entry(data,SHIFT,32,"OFAD",0,0,OBJECT_FADE_VERSION);
    Put32(data+MANIFEST+SHIFT+24+32*16+4,0);
    Path(path,parent,"object-fade.z64"); Save(path,data,size); free(data);
    OK(ProjectRebaseCreate(&authored,path,parent,"FadeSupported",&rebased,&report,&why));
    OK(SetupLoadProjectFile(rebased.dir,"Ump_setuptestZ",&loaded,&why));
    for(DWORD i=0;i<loaded.objectcount;i++)
        OK(SetupFileGetObjectProperties(&loaded,i,&view,&why) && view.customfade && view.fadestart==20 && view.fadeend==30);
    OK(loaded.objectcount==2); SetupFileFree(&loaded);
    OK(RomExportCreate(&rebased,"FadePlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&rom,&why) && rom.info.entrycount==33);
    OK(RomFindFile(&rom,"Ump_setuptestZ",&offset,&span,&why));
    DWORD commands=Get32(rom.data+offset+12);
    for(DWORD i=0;i<2;i++)
    {
        const unsigned char *record=rom.data+offset+commands+i*128;
        OK(Get32(record+OBJECT_FADE_TAG_OFFSET)==OBJECT_FADE_TAG);
        OK(Get32(record+OBJECT_FADE_DISTANCES_OFFSET)==((2000u<<16)|3000u));
    }
    RomFree(&rom);
    OK(ProjectRebaseCreate(&rebased,path,parent,"FadeAgain",&again,&report,&why));
    OK(!ProjectRebaseCheck(&rebased,incoming,&report,&why) && strstr(why,"object fade"));
    puts("PASS: fade duplication, compaction, save/reopen/export, 33-entry ROM, runtime upgrade, repeat rebase and unsupported downgrade rejection.");
}
