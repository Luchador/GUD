/* Text Editor uses the production file APIs, export and rebase paths. */
#include "textbank.h"
static void AddText(unsigned char *data,DWORD shift,DWORD modelsize)
{
    DWORD start=(MODEL+modelsize+156+15)&~15u, vbase=0x80000000u+shift*2;
    Entry(data,shift,1,"OBSG",OBJECTS,start+256,0);
    Put32(data+MANIFEST+shift+20,18);
    Entry(data,shift,17,"TXBK",CMAP+0xd300,CMAP+0xd310,0x80000000);
    Put32(data+CMAP+shift+0xd300,vbase+0xd400);Put32(data+CMAP+shift+0xd304,45);
    Put32(data+CMAP+shift+0xd308,4);Put32(data+CMAP+shift+0xd30c,1);
    for (DWORD i=0;i<2;i++)
    {
        DWORD name=CMAP+shift+0x580+i*32, at=start+shift+i*128, row=TABLE+shift+(6+i)*12;
        strcpy((char *)data+name,i ? "LoptionsE" : "LsiloE");
        Put32(data+row,6+i);Put32(data+row+4,vbase+0x580+i*32);Put32(data+row+8,at);
        Put32(data+CMAP+shift+0xd400+(i ? 43 : 34)*4,vbase+0x580+i*32);
        Put32(data+at,16);Put32(data+at+4,40);Put32(data+at+8,64);
        strcpy((char *)data+at+16,i ? "Menu title\n" : "Mission briefing\n");
        strcpy((char *)data+at+40,i ? "Cancel\n" : "Objective\n");
        strcpy((char *)data+at+64,"");
    }
}
static void TextEditing(const char *parent,const unsigned char *model,DWORD modelsize)
{
    unsigned char *old=Fixture(0,model,modelsize),*next=Fixture(SHIFT,model,modelsize);
    char oldpath[MAX_PATH],nextpath[MAX_PATH],path[MAX_PATH],exported[MAX_PATH];
    GEditorProject project,rebased;ProjectRebaseReport report;RomFile rom={0},output={0};
    TextBank bank={0},other={0},loaded={0};TextBankFile files[TEXT_BANK_MAX_FILES];
    DWORD count,offset,span,hash,basehash;
    AddText(old,0,modelsize);AddText(next,SHIFT,modelsize);
    Path(oldpath,parent,"text-old.z64");Save(oldpath,old,SIZE);
    Path(nextpath,parent,"text-new.z64");Save(nextpath,next,SIZE);
    OK(RomLoad(oldpath,&rom,&why));OK(ProjectCreate("TextOriginal",parent,&rom.info,&project,&why));
    OK(RomExportStoreProjectBase(&project,&rom,&why));
    Path(path,project.dir,"base.z64");basehash=Hash(path);
    OK(TextBankCatalog(&rom,files,&count,&why));OK(count==2);
    OK(!strcmp(files[0].name,"LsiloE")&&files[0].id==34);
    OK(!strcmp(files[1].name,"LoptionsE")&&files[1].id==43);
    OK(TextBankLoadProject(project.dir,&rom,"LsiloE",&bank,&why));
    OK(TextBankLoadProject(project.dir,&rom,"LoptionsE",&other,&why));
    OK(bank.count==4&&TextBankString(&bank,3)==NULL);
    char longtext[700];memset(longtext,'A',sizeof(longtext));longtext[sizeof(longtext)-2]='\n';longtext[sizeof(longtext)-1]=0;
    OK(TextBankSet(&bank,0,longtext,&why));OK(TextBankSet(&bank,1,"Find the new control room.\n",&why));
    OK(TextBankSet(&other,0,"Custom menu\n",&why));
    OK(TextBankSaveProject(project.dir,"LsiloE",&bank,&why));
    OK(TextBankSaveProject(project.dir,"LoptionsE",&other,&why));
    OK(TextBankLoadProject(project.dir,&rom,"LsiloE",&loaded,&why));
    OK(TextBankEqual(bank.data,bank.size,loaded.data,loaded.size));TextBankFree(&loaded);
    OK(TextBankProjectPath(project.dir,"LsiloE",path,sizeof(path)));hash=Hash(path);
    OK(TextBankSet(&bank,1,"Draft",&why));
    test_fail_write=1;OK(!TextBankSaveProject(project.dir,"LsiloE",&bank,&why));OK(Hash(path)==hash);
    test_fail_move=1;OK(!TextBankSaveProject(project.dir,"LsiloE",&bank,&why));OK(Hash(path)==hash);
    OK(TextBankSet(&bank,1,"Find the new control room.\n",&why));
    OK(RomExportCreate(&project,"TextPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&output,&why));OK(RomFindFile(&output,"LsiloE",&offset,&span,&why));
    OK(span>128 && TextBankEqual(bank.data,bank.size,output.data+offset,span));
    OK(RomFindFile(&output,"LoptionsE",&offset,&span,&why));
    OK(TextBankEqual(other.data,other.size,output.data+offset,span));RomFree(&output);
    OK(ProjectRebaseCreate(&project,nextpath,FALSE,parent,"TextRebased",&rebased,&report,&why));
    Same(project.dir,rebased.dir,"text/LsiloE.bin");Same(project.dir,rebased.dir,"text/LoptionsE.bin");
    OK(RomExportCreate(&rebased,"TextRebasedPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&output,&why));OK(RomFindFile(&output,"LsiloE",&offset,&span,&why));
    OK(TextBankEqual(bank.data,bank.size,output.data+offset,span));RomFree(&output);
    /* Independent changes to the same native bank conflict, preserving both projects. */
    DWORD at=(MODEL+modelsize+156+15)&~15u;
    strcpy((char *)next+at+SHIFT+16,"Upstream briefing\n");Save(nextpath,next,SIZE);
    OK(!ProjectRebaseCheck(&project,nextpath,FALSE,&report,&why));OK(report.conflicts);
    /* Equal text with different packing is safe to adopt from the new base. */
    TextBankFree(&bank);OK(TextBankLoad(old+at,128,&bank,&why));
    OK(TextBankSet(&bank,0,"Temporary\n",&why));OK(TextBankSet(&bank,0,"Mission briefing\n",&why));
    OK(bank.size!=128&&TextBankEqual(old+at,128,bank.data,bank.size));
    OK(TextBankSaveProject(project.dir,"LsiloE",&bank,&why));
    OK(ProjectRebaseCreate(&project,nextpath,FALSE,parent,"TextUpstream",&rebased,&report,&why));
    OK(TextBankLoadProject(rebased.dir,&rom,"LsiloE",&loaded,&why));
    OK(!strcmp(TextBankString(&loaded,0),"Upstream briefing\n"));TextBankFree(&loaded);
    /* Invalid overrides fail export rather than publishing broken string IDs. */
    OK(TextBankProjectPath(project.dir,"LsiloE",path,sizeof(path)));Put32(bank.data,0xffffffff);
    Save(path,bank.data,bank.size);OK(!RomExportValidateProject(&project,&why));
    TextBankFree(&bank);OK(TextBankLoad(old+at,128,&bank,&why));
    OK(TextBankSaveProject(project.dir,"LsiloE",&bank,&why));
    Path(path,project.dir,"base.z64");OK(Hash(path)==basehash);
    RomFree(&rom);TextBankFree(&bank);TextBankFree(&other);free(old);free(next);
    puts("PASS: text catalog IDs, project save/reopen, atomic failure rollback, growing/shrinking ROM slots, rebase retention, semantic equality, conflicts and invalid-text export rejection.");
}
