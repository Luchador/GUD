/* Native briefing authoring, save/reopen, export, rebase, and optional ROM corpus. */
#include "briefing.h"
#include "setupmeta.h"
#include "actionblocks.h"
static void AddBriefing(unsigned char *data,DWORD shift,DWORD modelsize)
{
    AddText(data,shift,modelsize);
    DWORD start=((MODEL+modelsize+156+15)&~15u)+256, vbase=0x80000000u+shift*2;
    Put32(data+LEVELS+shift,20);Put32(data+CMAP+shift+0x800,20);
    Entry(data,shift,1,"OBSG",OBJECTS,start+768,0);
    strcpy((char *)data+CMAP+shift+0x5c0,"UbriefsiloZ");
    Put32(data+TABLE+shift+8*12,8);Put32(data+TABLE+shift+8*12+4,vbase+0x5c0);Put32(data+TABLE+shift+8*12+8,start+shift);
    unsigned char *b=data+start+shift;
    for (int i=0;i<4;i++) { b[i*2]=0x88; }
    b[8]=0x88;b[9]=1;
    /* Relocate the fixture setup, retaining a door/tag link across old briefing records. */
    Put32(data+TABLE+shift+12+8,start+shift+48);
    unsigned char *s=b+48;Put32(s+12,40);DWORD at=40;
    for (int i=0;i<5;i++,at+=16) { Put32(s+at,35);Put32(s+at+4,i);Put32(s+at+8,0x8800); }
    Put32(s+at,22);s[at+5]=7;s[at+7]=4;at+=16; /* tag 7 -> door four commands later */
    Put32(s+at,23);Put32(s+at+8,0x8801);at+=16;
    Put32(s+at,26);Put32(s+at+4,1);at+=8;Put32(s+at,24);at+=4;
    Put32(s+at,1);at+=SetupObjectWordCount(1)*4;Put32(s+at,48);
    Put32(s+24,at+4);Put32(s+28,at+48);
    OK(at+116<=720);
}
static void BriefingEditing(const char *parent,const unsigned char *model,DWORD modelsize)
{
    unsigned char *old=Fixture(0,model,modelsize),*next=Fixture(SHIFT,model,modelsize);
    char oldpath[MAX_PATH],nextpath[MAX_PATH],path[MAX_PATH],exported[MAX_PATH];
    GEditorProject project,rebased;ProjectRebaseReport report;RomFile rom={0},output={0};
    SetupFile setup={0},copy={0};BriefingDocument doc,loaded;TextBank bank={0},built={0};DWORD off,span;
    AddBriefing(old,0,modelsize);AddBriefing(next,SHIFT,modelsize);
    Path(oldpath,parent,"brief-old.z64");Save(oldpath,old,SIZE);Path(nextpath,parent,"brief-new.z64");Save(nextpath,next,SIZE);
    OK(RomLoad(oldpath,&rom,&why));OK(ProjectCreate("BriefOriginal",parent,&rom.info,&project,&why));
    OK(RomExportStoreProjectBase(&project,&rom,&why));Folder(project.dir,"setup");
    OK(RomFindFile(&rom,"UsetuptestZ",&off,&span,&why));OK(RomExportProjectResourcePath(&project,"UsetuptestZ",path,sizeof(path))==1);Save(path,rom.data+off,span);
    OK(SetupLoadProjectFile(project.dir,"UsetuptestZ",&setup,&why));OK(BriefingLoad(&project,0,&setup,&doc,&bank,&why));
    OK(doc.bank==34 && doc.objectives[0].enabled && doc.objectives[0].conditions[0].words[1]==1);
    doc.menu[0].custom=TRUE;strcpy(doc.menu[0].text,"Menu background changed.\n");
    doc.watch[1].custom=TRUE;strcpy(doc.watch[1].text,"Watch background changed.\n");
    for (int i=0;i<6;i++)
    {
        BriefingObjective *o=&doc.objectives[i];o->enabled=TRUE;o->menu.custom=o->watch.custom=TRUE;
        snprintf(o->menu.text,sizeof(o->menu.text),"Menu objective %d\n",i+1);snprintf(o->watch.text,sizeof(o->watch.text),"watch objective %d\n",i+1);
        o->menuDifficulty=i%4;o->watchDifficulty=(i+1)%4;o->count=1;o->conditions[0]=(BriefingCondition){{26,1u<<i}};
    }
    OK(BriefingValidate(&doc,&bank,&why));OK(BriefingValidateSetup(&doc,&setup,&why));
    OK(BriefingEncode(&doc,&setup.briefmeta,&setup.briefmetasize,&why));OK(SetupFileClone(&setup,&copy,&why));
    OK(copy.briefmeta!=setup.briefmeta && copy.briefmetasize==setup.briefmetasize && !memcmp(copy.briefmeta,setup.briefmeta,setup.briefmetasize));SetupFileFree(&copy);
    /* Coexistence with disabled/named Action Blocks in the extended trailer. */
    ActionDocument actions={0};DWORD block;
    OK(ActionDocumentLoad(&setup,&actions,&why));OK(ActionDocumentAddBlock(&actions,(DWORD)-1,TRUE,&block,&why));
    OK(ActionDocumentSetEnabled(&actions,block,FALSE,&why));strcpy(actions.blocks[block].name,"Preserved logic");
    OK(ActionDocumentCompile(&actions,&setup,&copy,&why));ActionDocumentFree(&actions);
    OK(copy.actionmetasize && copy.briefmetasize==setup.briefmetasize);
    SetupFileFree(&setup);setup=copy;memset(&copy,0,sizeof(copy));
    OK(SetupSaveProjectFile(project.dir,&setup,&why));OK(SetupLoadProjectFile(project.dir,setup.name,&copy,&why));
    OK(ActionDocumentLoad(&copy,&actions,&why));OK(actions.count==1 && actions.blocks[0].disabled && !strcmp(actions.blocks[0].name,"Preserved logic"));ActionDocumentFree(&actions);
    OK(BriefingDecode(copy.briefmeta,copy.briefmetasize,&loaded,&why));OK(!memcmp(&doc,&loaded,sizeof(doc)));SetupFileFree(&copy);
    /* The raw Text Editor and custom briefing text share a bank without losing either edit. */
    OK(TextBankSet(&bank,0,"Raw text edit survives\n",&why));OK(TextBankSaveProject(project.dir,"LsiloE",&bank,&why));
    OK(RomExportCreate(&project,"BriefPlayable",parent,exported,sizeof(exported),&why));OK(RomLoad(exported,&output,&why));
    OK(RomFindFile(&output,"LsiloE",&off,&span,&why));OK(TextBankLoad(output.data+off,span,&built,&why));
    OK(!strcmp(TextBankString(&built,0),"Raw text edit survives\n"));OK(built.count==20);
    OK(RomFindFile(&output,"UbriefsiloZ",&off,&span,&why));DWORD id=(DWORD)output.data[off]*256+output.data[off+1];
    OK(!strcmp(TextBankString(&built,id&1023),doc.menu[0].text));
    for (int i=0;i<6;i++) { id=(DWORD)output.data[off+8+i*4]*256+output.data[off+9+i*4];OK(!strcmp(TextBankString(&built,id&1023),doc.objectives[i].menu.text));OK(output.data[off+11+i*4]==i%4); }
    OK(RomFindFile(&output,"UsetuptestZ",&off,&span,&why));const unsigned char *s=output.data+off;DWORD at=Get32(s+12),objectives=0;
    DWORD aitable=Get32(s+20);OK(aitable && s[Get32(s+aitable)]==4);
    OK(s[at+3]==22 && s[at+7]==1); /* Tag still points to the same door. */
    for (;s[at+3]!=48;at+=SetupObjectWordCount(s[at+3])*4)
    {
        if (s[at+3]==35 && Get32(s+at+4)==1) { OK(!strcmp(TextBankString(&built,Get32(s+at+8)&1023),doc.watch[1].text)); }
        if (s[at+3]==23) { DWORD n=Get32(s+at+4);OK(n<6 && Get32(s+at+12)==(n+1)%4);OK(!strcmp(TextBankString(&built,Get32(s+at+8)&1023),doc.objectives[n].watch.text));objectives++; }
    }
    OK(objectives==6);RomFree(&output);TextBankFree(&built);
    OK(ProjectRebaseCreate(&project,nextpath,FALSE,parent,"BriefRebased",&rebased,&report,&why));Same(project.dir,rebased.dir,"setup/UsetuptestZ.set");
    OK(RomExportCreate(&rebased,"BriefRebasedPlayable",parent,exported,sizeof(exported),&why));
    /* Disabled middle slots get hidden complete placeholders, not renumbered IDs or incomplete holes. */
    doc.objectives[1].enabled=FALSE;free(setup.briefmeta);setup.briefmeta=NULL;
    OK(BriefingEncode(&doc,&setup.briefmeta,&setup.briefmetasize,&why));OK(SetupSaveProjectFile(project.dir,&setup,&why));
    BriefingResource *resources=NULL;DWORD count;OK(BriefingCollect(&project,&rom,NULL,&resources,&count,&why));OK(count==3);
    s=resources[2].data;at=Get32(s+12);BOOL hidden=FALSE;
    for (;s[at+3]!=48;at+=SetupObjectWordCount(s[at+3])*4) if (s[at+3]==23 && Get32(s+at+4)==1)
    { OK(Get32(s+at+8)==0 && Get32(s+at+12)==4 && Get32(s+at+16)==24);hidden=TRUE; }
    OK(hidden);BriefingFreeResources(resources,count);
    /* Atomic save failure preserves prior metadata. */
    DWORD hash=Hash(path);test_fail_write=1;OK(!SetupSaveProjectFile(project.dir,&setup,&why));OK(Hash(path)==hash);
    /* An upstream briefing edit cannot silently replace authored menu definitions. */
    DWORD start=((MODEL+modelsize+156+15)&~15u)+256;next[start+SHIFT+1]=1;Save(nextpath,next,SIZE);
    OK(!ProjectRebaseCheck(&project,nextpath,FALSE,&report,&why));
    doc.objectives[0].conditions[0]=(BriefingCondition){{32,9999}};OK(!BriefingValidateSetup(&doc,&setup,&why));
    doc.objectives[0].count=0;OK(!BriefingValidate(&doc,&bank,&why));doc.objectives[0].count=1;
    strcpy(doc.objectives[0].watch.text,"Missing newline");OK(!BriefingValidate(&doc,&bank,&why));
    free(old);free(next);TextBankFree(&bank);SetupFileFree(&setup);RomFree(&rom);
    puts("PASS: six menu/watch objectives, independent text and difficulty, custom strings plus raw text edits, command-link remapping, hidden objective gaps, metadata cloning/save/reopen, atomic failure, ROM export and rebase.");
}
static void BriefingCorpus(const char *path,const char *parent)
{
    RomFile rom={0};GEditorProject project;char file[MAX_PATH];DWORD count=0;
    OK(RomLoad(path,&rom,&why));OK(ProjectCreate("BriefCorpus",parent,&rom.info,&project,&why));
    OK(RomExportStoreProjectBase(&project,&rom,&why));Folder(project.dir,"setup");
    for (DWORD l=0;l<project.levelcount;l++)
    {
        const char *bn,*br;DWORD off,span;SetupFile setup={0};TextBank bank={0};BriefingDocument doc;
        if (!BriefingNames(project.levels[l].levelID,&bn,&br)) { continue; }
        OK(RomFindFile(&rom,project.levels[l].setupname,&off,&span,&why));
        OK(RomExportProjectResourcePath(&project,project.levels[l].setupname,file,sizeof(file))==1);Save(file,rom.data+off,span);
        OK(SetupLoadProjectFile(project.dir,project.levels[l].setupname,&setup,&why));
        fprintf(stderr,"Briefing corpus: %s\n",project.levels[l].name);
        OK(BriefingLoad(&project,l,&setup,&doc,&bank,&why));OK(BriefingValidate(&doc,&bank,&why));OK(BriefingValidateSetup(&doc,&setup,&why));
        OK(BriefingEncode(&doc,&setup.briefmeta,&setup.briefmetasize,&why));OK(SetupSaveProjectFile(project.dir,&setup,&why));
        TextBankFree(&bank);SetupFileFree(&setup);count++;
    }
    OK(count==20);OK(RomExportCreate(&project,"BriefCorpusPlayable",parent,file,sizeof(file),&why));
    RomFree(&rom);printf("PASS: all %lu real ROM mission briefings load, validate, persist and export.\n",(unsigned long)count);
}
