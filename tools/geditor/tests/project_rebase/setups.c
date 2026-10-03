/* Saved setups are authoritative, including snapshots already in base.z64. */
static void SetupAmmoFixture(unsigned char *data,DWORD shift,DWORD modelsize,DWORD solo,DWORD mp)
{
    const DWORD offsets[]={MODEL+modelsize+256,MODEL+modelsize},sizes[]={256,256},ammo[]={solo,mp};
    Entry(data,shift,1,"OBSG",OBJECTS,MODEL+modelsize+512,0);
    Put32(data+TABLE+shift+12+8,MODEL+modelsize+256+shift);
    for (int i=0;i<2;i++)
    {
        unsigned char *s=data+offsets[i]+shift;memset(s,0,sizes[i]);
        Put32(s+8,40);Put32(s+40,SETUP_INTRO_AMMO);Put32(s+44,AMMO_9MM);
        Put32(s+48,ammo[i]);Put32(s+56,9);
        Put32(s+24,60);Put32(s+28,104);
    }
}
static void SetupAmmoInRom(const char *path,const char *name,LONG ammo)
{
    RomFile rom={0};DWORD offset,span,count;SetupFile setup={0};SetupIntroEntry *entries=NULL;
    OK(RomLoad(path,&rom,&why)&&RomFindFile(&rom,name,&offset,&span,&why));
    setup.data=rom.data+offset;setup.size=span;
    OK(SetupFileGetIntroEquipment(&setup,&entries,&count,&why));
    OK(count==1&&entries[0].type==SETUP_INTRO_AMMO&&entries[0].value[0]==AMMO_9MM&&entries[0].value[1]==ammo);
    free(entries);RomFree(&rom);
}
static void SavedSetupRebases(const char *parent,const unsigned char *model,DWORD modelsize)
{
    unsigned char *old=Fixture(0,model,modelsize),*next=Fixture(SHIFT,model,modelsize);
    char oldpath[MAX_PATH],newpath[MAX_PATH],path[MAX_PATH],exported[MAX_PATH],name[64];
    const char *names[]={"UsetuptestZ","Ump_setuptestZ"};
    const char *files[]={"setup/UsetuptestZ.set","setup/Ump_setuptestZ.set"};
    DWORD offset,span,hashes[2],basehash;
    RomFile rom={0};GEditorProject project,rebased,loaded,again;ProjectRebaseReport report;
    SetupAmmoFixture(old,0,modelsize,40,50);SetupAmmoFixture(next,SHIFT,modelsize,140,150);
    Path(oldpath,parent,"setup-old.z64");Save(oldpath,old,SIZE);
    Path(newpath,parent,"setup-new.z64");Save(newpath,next,SIZE);
    OK(RomLoad(oldpath,&rom,&why));
    OK(ProjectCreate("SavedSetups",parent,&rom.info,&project,&why));
    OK(RomExportStoreProjectBase(&project,&rom,&why));
    Folder(project.dir,"setup");
    for (int i=0;i<2;i++)
    {
        OK(RomFindFile(&rom,names[i],&offset,&span,&why));
        Path(path,project.dir,files[i]);Save(path,rom.data+offset,span);
    }
    Path(path,project.dir,"base.z64");basehash=Hash(path);
    for (int edited=0;edited<2;edited++)
    {
        for (int i=0;i<2;i++)
        {
            if (edited)
            {
                SetupFile setup={0},compiled={0};ActionDocument actions={0};DWORD block;
                OK(SetupLoadProjectFile(project.dir,names[i],&setup,&why));
                Put32(setup.data+48,i ? 80 : 70);
                OK(ActionDocumentLoad(&setup,&actions,&why));
                OK(ActionDocumentAddBlock(&actions,0,FALSE,&block,&why));
                strcpy(actions.blocks[block].name,"Keep setup metadata");
                OK(ActionDocumentSetEnabled(&actions,block,FALSE,&why));
                OK(ActionDocumentCompile(&actions,&setup,&compiled,&why)&&compiled.actionmetasize);
                OK(SetupSaveProjectFile(project.dir,&compiled,&why));
                ActionDocumentFree(&actions);SetupFileFree(&setup);SetupFileFree(&compiled);
            }
            Path(path,project.dir,files[i]);hashes[i]=Hash(path);
        }
        for (int choice=PROJECT_REBASE_STOP;choice<=PROJECT_REBASE_USE_ROM;choice++)
        {
            ProjectRebaseOptions options={PROJECT_REBASE_STOP,choice,PROJECT_REBASE_STOP};
            OK(ProjectRebaseCheckWithOptions(&project,newpath,&options,&report,&why));
            for (int i=0;i<2;i++) { Path(path,project.dir,files[i]);OK(Hash(path)==hashes[i]); }
            snprintf(name,sizeof(name),"Setups-%d-%d",edited,choice);
            OK(ProjectRebaseCreateWithOptions(&project,newpath,&options,parent,name,&rebased,&report,&why));
            for (int i=0;i<2;i++) { Same(project.dir,rebased.dir,files[i]); }
            OK(report.setupskept==2&&report.updated==0&&report.resolved==(edited ? 2u : 0u)&&!report.conflicts);
            OK(strstr(report.details,"UsetuptestZ")&&strstr(report.details,"Ump_setuptestZ")&&strstr(report.details,"project always wins"));
            OK(ProjectRead(rebased.geppath,&loaded)&&RomExportValidateProject(&loaded,&why));
            snprintf(name,sizeof(name),"Setups-playable-%d-%d",edited,choice);
            OK(RomExportCreate(&loaded,name,parent,exported,sizeof(exported),&why));
            SetupAmmoInRom(exported,names[0],edited ? 70 : 40);
            SetupAmmoInRom(exported,names[1],edited ? 80 : 50);
            if (edited)
            {
                SetupFile setup={0};ActionDocument actions={0};
                OK(SetupLoadProjectFile(loaded.dir,names[0],&setup,&why));
                OK(ActionDocumentLoad(&setup,&actions,&why)&&actions.count==1&&actions.blocks[0].disabled);
                OK(!strcmp(actions.blocks[0].name,"Keep setup metadata"));
                ActionDocumentFree(&actions);SetupFileFree(&setup);
            }
            snprintf(name,sizeof(name),"Setups-again-%d-%d",edited,choice);
            OK(ProjectRebaseCreateWithOptions(&loaded,newpath,&options,parent,name,&again,&report,&why));
            for (int i=0;i<2;i++) { Same(project.dir,again.dir,files[i]); }
        }
    }
    /* Absent local file still uses the new base; never synthesize a stale
     * override. A corrupt saved setup must block publication, not disappear. */
    for (int i=0;i<2;i++) { Path(path,project.dir,files[i]);OK(DeleteFile(path)); }
    OK(ProjectRebaseCreate(&project,newpath,FALSE,parent,"SetupFallback",&rebased,&report,&why));
    OK(!report.setupskept&&RomExportCreate(&rebased,"SetupFallbackPlayable",parent,exported,sizeof(exported),&why));
    SetupAmmoInRom(exported,names[0],140);SetupAmmoInRom(exported,names[1],150);
    unsigned char broken[64]={0};Put32(broken+8,0x1000);
    Path(path,project.dir,files[0]);Save(path,broken,sizeof(broken));DWORD brokenhash=Hash(path);
    ProjectRebaseOptions useRom={PROJECT_REBASE_STOP,PROJECT_REBASE_USE_ROM,PROJECT_REBASE_STOP};
    OK(!ProjectRebaseCreateWithOptions(&project,newpath,&useRom,parent,"BrokenSetup",&again,&report,&why));
    OK(Hash(path)==brokenhash);Path(path,parent,"BrokenSetup");OK(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES);
    Path(path,project.dir,"base.z64");OK(Hash(path)==basehash);NoTemps(parent);
    free(old);free(next);RomFree(&rom);
    puts("PASS: solo/MP setups matching old base and conflicting edits always stay byte-identical under STOP/Keep project/Use ROM; metadata, reopen, ROM export and repeat rebase survive; absent setup uses new base, corrupt setup blocks publication.");
}
