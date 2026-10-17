static void LevelSlotRebases(const GEditorProject *source,const char *incoming,const char *parent)
{
    DWORD size;unsigned char *data=Read(incoming,&size);
    const DWORD slot=CMAP+SHIFT+0xe800;
    char path[MAX_PATH],exported[MAX_PATH],basepath[MAX_PATH];
    GEditorProject supported,loaded,updated,again;ProjectRebaseReport report;RomFile rom={0};LevelSlotTable table;
    Put32(data+MANIFEST+SHIFT+20,18);
    Entry(data,SHIFT,17,"LVSL",slot-SHIFT,slot-SHIFT+LEVEL_MENU_ROM_SIZE,1);
    Put32(data+slot,1);
    for(unsigned i=0;i<32;i++)Put32(data+slot+4+4*i,i==20 ? -1 : i==27 ? 421 : 21);
    Path(path,parent,"levelslots.z64");Save(path,data,size);
    OK(ProjectRebaseCreate(source,path,FALSE,parent,"SlotsSupported",&supported,&report,&why));
    OK(supported.levelSlots.available&&!supported.levelSlotOverrides.mask);
    OK(LevelSlotsSet(&supported.levelSlots,&supported.levelSlotOverrides,supported.levels,supported.levelcount,0,421,&why));
    OK(LevelSlotsSet(&supported.levelSlots,&supported.levelSlotOverrides,supported.levels,supported.levelcount,31,-1,&why));
    OK(ProjectSave(&supported,&why)&&ProjectRead(supported.geppath,&loaded));
    OK(RomExportRefreshProjectLevelMetadata(&loaded,&why));
    OK(RomExportCreate(&loaded,"SlotsPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&rom,&why)&&LevelSlotsReadRom(&rom,&table,&why));
    OK(table.value.stages[0]==421&&table.value.stages[31]==-1&&table.value.stages[27]==421);RomFree(&rom);
    Path(basepath,supported.dir,"base.z64");DWORD original=Hash(basepath),metadata=Hash(supported.geppath);
    /* Changed defaults update untouched boxes; authored slots win conflicts. */
    Put32(data+slot+4+4,421);Put32(data+slot+4+31*4,421);Save(path,data,size);
    OK(ProjectRebaseCreate(&supported,path,FALSE,parent,"SlotsUpdated",&updated,&report,&why));
    OK(LevelSlotsGet(&updated.levelSlots,&updated.levelSlotOverrides,0)==421);
    OK(LevelSlotsGet(&updated.levelSlots,&updated.levelSlotOverrides,1)==421);
    OK(LevelSlotsGet(&updated.levelSlots,&updated.levelSlotOverrides,31)==-1);
    OK(ProjectRead(updated.geppath,&loaded)&&RomExportRefreshProjectLevelMetadata(&loaded,&why));
    OK(RomExportCreate(&loaded,"SlotsUpdatedPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&rom,&why)&&LevelSlotsReadRom(&rom,&table,&why));
    OK(table.value.stages[0]==421&&table.value.stages[1]==421&&table.value.stages[31]==-1);RomFree(&rom);
    OK(ProjectRebaseCreate(&updated,path,FALSE,parent,"SlotsAgain",&again,&report,&why));
    OK(!ProjectRebaseCheck(&supported,incoming,FALSE,&report,&why));
    Put32(data+slot+4,90);Save(path,data,size);
    OK(!ProjectRebaseCheck(&supported,path,FALSE,&report,&why));
    OK(Hash(basepath)==original&&Hash(supported.geppath)==metadata);
    free(data);
    puts("PASS: Level Manager upgrade, save/reopen/full export, exact SP/MP IDs, project-wins slot conflicts, new defaults, repeat rebase and downgrade/malformed-table rollback.");
}
