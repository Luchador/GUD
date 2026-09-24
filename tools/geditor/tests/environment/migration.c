/* Import/export both ENVT layouts and migrate retired .gep keys. */
static void Migration(const char *dir)
{
    const char *keys[]={"nearfog","maxvisrng","maxobfnrng","minvisrng"};
    unsigned char bytes[3*104],before[sizeof(bytes)];
    EditorEnvironment rows[]={Row(29),Row(129)},value;
    EnvironmentTable old,next;
    EnvironmentOverrides changes={0};
    RomFile rom={.data=bytes,.size=sizeof(bytes)};
    rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,0,sizeof(bytes),0};
    rom.info.entries[1]=(RomManifestEntry){0x454e5654,0,0,104};
    memset(bytes,0,sizeof(bytes));
    for(int i=0;i<2;i++)
    {
        memcpy(bytes+i*104,rows[i].data,16);
        /* Even invalid retired ROM values cannot affect current settings. */
        for(int j=0;j<4;j++)Float(bytes+i*104+16+j*4,NAN);
        memcpy(bytes+i*104+32,rows[i].data+16,72);
    }
    memcpy(before,bytes,sizeof(bytes));
    OK(EnvironmentReadRom(&rom,&old,NULL,&why)&&old.count==2&&old.recordsize==104);
    OK(!memcmp(old.rows,rows,sizeof(rows))&&EnvironmentValidate(&old.rows[0],&why));
    value=rows[1];Edit(&value,"fogRed","45");Edit(&value,"skyheight","9000");
    Edit(&value,"propStartFade","24");
    OK(EnvironmentSet(&old,&changes,&value,&why)&&EnvironmentApplyRom(&rom,&changes,&why));
    memcpy(before+104+32,value.data+16,72);
    OK(!memcmp(bytes,before,sizeof(bytes))); /* Old slots and surrounding rows survive. */
    OK(EnvironmentApplyRom(&rom,&changes,&why)&&!memcmp(bytes,before,sizeof(bytes)));

    /* Rebase from old native layout to compact layout, keeping live overrides. */
    rom.info.entries[1].flags=88;rom.info.entries[0].romend=3*88;
    memset(bytes,0,sizeof(bytes));
    memcpy(bytes,rows,sizeof(rows));Float(bytes+88+76,30); /* New horizon default. */
    memcpy(before,bytes,sizeof(bytes));
    OK(EnvironmentReadRom(&rom,&next,NULL,&why)&&next.count==2&&next.recordsize==88);
    OK(EnvironmentRebase(&old,&next,&changes,&why));
    OK(EnvironmentGet(&next,&changes,129,&value)&&value.data[28]==45
       &&F32(value.data+32)==9000&&F32(value.data+76)==30&&F32(value.data+80)==24);
    OK(EnvironmentApplyRom(&rom,&changes,&why));memcpy(before+88,value.data,88);
    OK(!memcmp(bytes,before,sizeof(bytes)));

    GEditorProject project={0},loaded={0};
    strcpy(project.name,"Migration");strcpy(project.dir,dir);
    project.levelcount=1;project.levels[0].levelID=29;strcpy(project.levels[0].name,"Depot");
    project.levels[0].levelscale=project.levels[0].renderScale=project.levels[0].chrLODDistance=1;
    snprintf(project.geppath,sizeof(project.geppath),"%s/migration.gep",dir);
    project.environments=next;project.environmentOverrides=changes;
    OK(ProjectSave(&project,&why));
    FILE *f=fopen(project.geppath,"a");OK(f);
    for(int i=0;i<4;i++)fprintf(f,"environment = 129|%s|123.5\n",keys[i]);
    fprintf(f,"environment = 999|nearfog|0\n"); /* Obsolete-only orphan row. */
    OK(!fclose(f)&&ProjectRead(project.geppath,&loaded));
    OK(!memcmp(&loaded.environmentOverrides,&changes,sizeof(changes)));
    loaded.environments=next;
    OK(ProjectSave(&loaded,&why));f=fopen(project.geppath,"r");OK(f);
    char line[512];while(fgets(line,sizeof(line),f))
        for(int i=0;i<4;i++)OK(!strstr(line,keys[i]));
    fclose(f);
    /* A project with only retired overrides becomes a clean version-5 file. */
    memset(&project.environmentOverrides,0,sizeof(project.environmentOverrides));
    OK(ProjectSave(&project,&why));
    f=fopen(project.geppath,"r+");OK(f);fseek(f,0,SEEK_END);
    for(int i=0;i<4;i++)fprintf(f,"environment = 29|%s|0\n",keys[i]);
    OK(!fclose(f)&&ProjectRead(project.geppath,&loaded)&&!loaded.environmentOverrides.count);
    OK(ProjectSave(&loaded,&why));f=fopen(project.geppath,"r");OK(f&&fgets(line,sizeof(line),f));fclose(f);
    OK(!strcmp(line,"GEditor Project 5\n"));
    OK(!EnvironmentReadOverride(&changes,"29|nearfog|NaN"));
    OK(!EnvironmentReadOverride(&changes,"29|maxvisrng|1junk"));
    OK(!EnvironmentReadOverride(&changes,"29|unknown|1"));
    puts("PASS: old/new ENVT import and exact export, cross-layout rebase, retired override migration and obsolete-only project cleanup.");
}
