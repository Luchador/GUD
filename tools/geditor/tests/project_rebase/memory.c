static void MemoryRebases(const GEditorProject *source,const char *incoming,const char *parent)
{
    DWORD size;unsigned char *data=Read(incoming,&size);
    const DWORD slot=CMAP+SHIFT+0xe800;
    char path[MAX_PATH],basepath[MAX_PATH],exported[MAX_PATH];
    GEditorProject supported,loaded,updated,again;ProjectRebaseReport report;RomFile rom;
    LevelMemory value;LevelMemoryTable table;
    Put32(data+MANIFEST+SHIFT+20,18);
    Entry(data,SHIFT,17,"LMEM",slot-SHIFT,slot-SHIFT+64,64);
    unsigned char *row=data+LEVELS+SHIFT;
    Put32(row+20,0x80000000+SHIFT*2+0xe800);
    Entry(data,SHIFT,3,"STGT",LEVELS,LEVELS+44,1);
    memset(data+slot,0,64);strcpy((char *)data+slot,"-ml0 -me0 -mgfx70 -mvtx50 -mt500 -ma200");
    Path(path,parent,"memory.z64");Save(path,data,size);
    /* Upgrade an older project without losing its existing edits. */
    OK(ProjectRebaseCreate(source,path,parent,"MemorySupported",&supported,&report,&why));
    OK(supported.memory.count==1&&!supported.memoryOverrides.count);
    OK(LevelMemoryGet(&supported.memory,&supported.memoryOverrides,21,&value));value.kib[0]=1000;
    OK(LevelMemorySet(&supported.memory,&supported.memoryOverrides,&value,&why));
    OK(ProjectSave(&supported,&why)&&ProjectRead(supported.geppath,&loaded));
    OK(RomExportRefreshProjectLevelMetadata(&loaded,&why)&&loaded.memoryOverrides.count==1);
    OK(RomExportCreate(&loaded,"MemoryPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&rom,&why)&&LevelMemoryReadRom(&rom,&table,&why));
    OK(table.rows[0].kib[0]==1000&&table.rows[0].kib[2]==500);RomFree(&rom);
    Path(basepath,supported.dir,"base.z64");DWORD original=Hash(basepath),metadata=Hash(supported.geppath);
    /* New ROM texture defaults merge with the project's graphics override. */
    strcpy((char *)data+slot,"-ml0 -me0 -mgfx70 -mvtx50 -mt600 -ma200");Save(path,data,size);
    OK(ProjectRebaseCreate(&supported,path,parent,"MemoryUpdated",&updated,&report,&why));
    OK(LevelMemoryGet(&updated.memory,&updated.memoryOverrides,21,&value)&&value.kib[0]==1000&&value.kib[2]==600);
    OK(ProjectRead(updated.geppath,&loaded)&&RomExportRefreshProjectLevelMetadata(&loaded,&why));
    OK(RomExportCreate(&loaded,"MemoryUpdatedPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&rom,&why)&&LevelMemoryReadRom(&rom,&table,&why));
    OK(table.rows[0].kib[0]==1000&&table.rows[0].kib[2]==600);RomFree(&rom);
    OK(ProjectRebaseCreate(&updated,path,parent,"MemoryAgain",&again,&report,&why));
    /* Conflicts and loss of memory support reject publication, retaining the source. */
    strcpy((char *)data+slot,"-ml0 -me0 -mgfx90 -mvtx50 -mt600 -ma200");Save(path,data,size);
    OK(!ProjectRebaseCheck(&supported,path,&report,&why)&&report.conflicts&&strstr(report.details,"Display lists"));
    Reject(&supported,path,parent,"MemoryConflict");
    Reject(&supported,incoming,parent,"MemoryUnsupported");
    OK(Hash(basepath)==original&&Hash(supported.geppath)==metadata);
    strcpy((char *)data+slot,"-ml0 -me0 -mgfx1000 -mvtx50 -mt600 -ma200");Save(path,data,size);
    OK(ProjectRebaseCreate(&supported,path,parent,"MemoryMatched",&again,&report,&why)&&!again.memoryOverrides.count);
    OK(RomExportValidateProject(&supported,&why));NoTemps(parent);free(data);
    puts("PASS: memory manifest upgrade, memory save/reopen/export, new defaults, repeat rebase, matching override cleanup and conflict/unsupported-ROM rollback.");
}
