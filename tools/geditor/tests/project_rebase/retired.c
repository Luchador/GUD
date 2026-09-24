/* An old catalog with all nine retired stages and 26 resources. Put the
 * obsolete records before retained ones so every real file index moves. */
static unsigned char *WithRetired(const unsigned char *incoming)
{
    static const char *stems[]={"sho","eld","lue","rit","ear","lee","lip","wax","pam"};
    static const DWORD ids[]={42,44,47,49,51,52,53,55,56};
    unsigned char *data=malloc(SIZE),*rows;
    DWORD strings=CMAP+SHIFT+0xe800, table=CMAP+SHIFT+0xd400;
    DWORD levels=CMAP+SHIFT+0xe400, index=1, textstart;
    DWORD vbase=0x80000000u+SHIFT*2;
    char name[64];
    OK(data);memcpy(data,incoming,SIZE);
    textstart=Get32(data+MANIFEST+SHIFT+24+1*16+8);
    Entry(data,SHIFT,1,"OBSG",OBJECTS,textstart-SHIFT+9*32,0);
    Entry(data,SHIFT,3,"STGT",levels-SHIFT,levels-SHIFT+10*44,10);
    Entry(data,SHIFT,5,"FTBL",table-SHIFT,0,0);
    memcpy(data+levels,data+LEVELS+SHIFT,44);
    memcpy(data+table,data+TABLE+SHIFT,12);
    for (DWORD i=0;i<9;i++)
    {
        DWORD ptr[4];
        for (DWORD field=0;field<4;field++)
        {
            if (field==0) { snprintf(name,sizeof(name),"%s",stems[i]); }
            if (field==1) { snprintf(name,sizeof(name),"Usetup%sZ",stems[i]); }
            if (field==2) { snprintf(name,sizeof(name),"bg/bg_%s_all_p.seg",stems[i]); }
            if (field==3) { snprintf(name,sizeof(name),"Tbg_%s_all_p_stanZ",stems[i]); }
            ptr[field]=vbase+strings-CMAP-SHIFT;
            strcpy((char *)data+strings,name);strings+=strlen(name)+1;
        }
        rows=data+levels+(i+1)*44;Put32(rows,ids[i]);
        for (DWORD field=0;field<4;field++) { Put32(rows+4+field*4,ptr[field]); }
        Float(rows+24,1);Float(rows+28,1);Float(rows+32,1);memset(rows+36,0xff,6);
        rows=data+table+index*12;Put32(rows,index++);Put32(rows+4,ptr[2]);Put32(rows+8,OBJECTS+SHIFT+64);
        /* SHO never had a stan file-table entry; the others alias real data. */
        if (i)
        {
            rows=data+table+index*12;Put32(rows,index++);Put32(rows+4,ptr[3]);Put32(rows+8,OBJECTS+SHIFT+576);
        }
        snprintf(name,sizeof(name),"L%sE",stems[i]);
        rows=data+table+index*12;Put32(rows,index++);Put32(rows+4,vbase+strings-CMAP-SHIFT);
        strcpy((char *)data+strings,name);strings+=strlen(name)+1;Put32(rows+8,textstart+i*32);
    }
    OK(index==27 && strings<CMAP+SHIFT+0x10000);
    for (DWORD i=1;i<=5;i++,index++)
    { rows=data+table+index*12;memcpy(rows,data+TABLE+SHIFT+i*12,12);Put32(rows,index); }
    return data;
}

static void RetiredRebases(const GEditorProject *source,const char *incoming,const char *parent)
{
    GEditorProject legacy,updated,loaded,again;ProjectRebaseReport report;
    RomFile rom={0},output={0};DWORD size,offset,span,oldhash;char path[MAX_PATH],base[MAX_PATH],exported[MAX_PATH],bad[MAX_PATH];
    unsigned char *next=Read(incoming,&size),*old=WithRetired(next),*edited;
    OK(ProjectRebaseCreate(source,incoming,FALSE,parent,"WithUnused",&legacy,&report,&why));
    RomLevel saved=legacy.levels[0];
    Path(base,legacy.dir,"base.z64");Save(base,old,SIZE);oldhash=Hash(base);
    OK(RomLoad(base,&rom,&why) && rom.info.levelcount==10);
    legacy.levelcount=rom.info.levelcount;memcpy(legacy.levels,rom.info.levels,legacy.levelcount*sizeof(legacy.levels[0]));
    legacy.levels[0]=saved;OK(ProjectSave(&legacy,&why));
    for (DWORD i=1;i<27;i++)
    {
        char name[64];OK(RomGetFileByIndex(&rom,i,name,sizeof(name),&offset,&span));
        if (RomExportProjectResourcePath(&legacy,name,path,sizeof(path))==1) { Save(path,rom.data+offset,span); }
    }
    OK(ProjectRebaseCheck(&legacy,incoming,FALSE,&report,&why));
    OK(report.levelsremoved==9 && report.resourcesremoved==26 && !report.conflicts);
    OK(ProjectRebaseCreate(&legacy,incoming,FALSE,parent,"Retired",&updated,&report,&why));
    OK(updated.levelcount==1 && updated.levels[0].levelID==saved.levelID && updated.levels[0].music==saved.music);
    OK(ProjectRead(updated.geppath,&loaded) && RomExportRefreshProjectLevelMetadata(&loaded,&why));
    OK(loaded.levelcount==1 && loaded.levels[0].fog.farclip==6000);
    Same(legacy.dir,updated.dir,"bg/bg_test.seg");Same(legacy.dir,updated.dir,"stan/Tbg_test_stanZ.stan");
    Same(legacy.dir,updated.dir,"models/native/Pjungle3_treeZ.gmodel");Same(legacy.dir,updated.dir,"models/newprops.gnp");
    Same(legacy.dir,updated.dir,"images/native/0001.gtex");Same(legacy.dir,updated.dir,"images/0001.bmp");
    for (DWORD i=1;i<27;i++)
    {
        char name[64];OK(RomGetFileByIndex(&rom,i,name,sizeof(name),NULL,NULL));
        if (RomExportProjectResourcePath(&updated,name,path,sizeof(path))==1)
        { OK(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES); }
    }
    OK(Hash(base)==oldhash && legacy.levelcount==10);
    OK(RomExportCreate(&updated,"RetiredPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&output,&why) && output.info.levelcount==1);
    OK(RomFindFile(&output,"bg/bg_test.seg",&offset,&span,&why));
    { DWORD rooms=Get32(output.data+offset+4)&0xffffffu;
      DWORD vertices=Get32(output.data+offset+rooms+24)&0xffffffu;OK(output.data[offset+vertices]==1); }
    OK(RomFindFile(&output,"Pjungle3_treeZ",&offset,&span,&why));
    OK(!RomFindFile(&output,"bg/bg_sho_all_p.seg",&offset,&span,&why));
    RomFree(&output);
    OK(ProjectRebaseCreate(&updated,incoming,FALSE,parent,"RetiredAgain",&again,&report,&why));
    OK(again.levelcount==1 && !report.levelsremoved && !report.resourcesremoved);
    /* Never discard authored data/settings just because its level is retired. */
    legacy.levels[1].music=12;
    OK(!ProjectRebaseCheck(&legacy,incoming,FALSE,&report,&why) && report.conflicts && strstr(report.details,"level settings"));
    Reject(&legacy,incoming,parent,"RetiredSettingsConflict");legacy.levels[1].music=-1;
    OK(RomExportProjectResourcePath(&legacy,"bg/bg_sho_all_p.seg",path,sizeof(path))==1);
    edited=Read(path,&size);edited[128]^=1;Save(path,edited,size);
    OK(!ProjectRebaseCheck(&legacy,incoming,FALSE,&report,&why) && strstr(report.details,"bg/bg_sho_all_p.seg"));
    Reject(&legacy,incoming,parent,"RetiredGeometryConflict");edited[128]^=1;Save(path,edited,size);free(edited);
    Path(path,legacy.dir,"setup/UsetupshoZ.set");Save(path,"custom",6);
    OK(!ProjectRebaseCheck(&legacy,incoming,FALSE,&report,&why) && strstr(report.details,"no base to compare"));
    Reject(&legacy,incoming,parent,"RetiredOrphanConflict");OK(DeleteFile(path));
    OK(RomFindFile(&rom,"LshoE",&offset,&span,&why));old[offset]=1;Save(base,old,SIZE);
    OK(!ProjectRebaseCheck(&legacy,incoming,FALSE,&report,&why) && strstr(report.details,"LshoE"));
    old[offset]=0;Save(base,old,SIZE);
    Path(bad,parent,"retired-invalid.z64");
    Put32(next+TABLE+SHIFT+2*12+4,Get32(next+TABLE+SHIFT+12+4));Save(bad,next,SIZE);
    OK(!ProjectRebaseCheck(&legacy,bad,FALSE,&report,&why) && strstr(why,"Duplicate ROM resource"));
    Reject(&legacy,bad,parent,"DuplicateResource");
    free(next);next=Read(incoming,&size);Put32(next+TABLE+SHIFT+12,17);Save(bad,next,SIZE);
    OK(!ProjectRebaseCheck(&legacy,bad,FALSE,&report,&why) && strstr(why,"invalid index"));
    Reject(&legacy,bad,parent,"InvalidResourceIndex");
    free(next);next=Read(incoming,&size);
    memmove(next+TABLE+SHIFT+4*12,next+TABLE+SHIFT+5*12,12);Put32(next+TABLE+SHIFT+4*12,4);
    memset(next+TABLE+SHIFT+5*12,0,12);Save(bad,next,SIZE);
    OK(!ProjectRebaseCheck(&legacy,bad,FALSE,&report,&why) && strstr(why,"Pjungle3_treeZ"));
    Reject(&legacy,bad,parent,"MissingRetainedModel");
    OK(ProjectRebaseCheck(&legacy,incoming,FALSE,&report,&why));
    OK(Hash(base)==oldhash);RomFree(&rom);free(old);free(next);
    puts("PASS: nine retired stages, 26 removed resources, shifted file IDs, shared aliases, model/image/environment edits, reopen/export/repeat rebase; authored retired data and malformed catalogs block without modifying the source.");
}
