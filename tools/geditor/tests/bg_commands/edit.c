#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgcommands.h"
#include "bgdocument.h"
#include "bghistory.h"
BOOL SetupFileCompact(SetupFile *s,const char **why) { (void)s;(void)why;abort(); }
void SetupFileFree(SetupFile *s) { (void)s;abort(); }
void StanFileFree(StanFile *s) { (void)s;abort(); }
#include "fixture.inc"
static const char *why="";
#define OK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,why);abort(); } } while(0)
static BgVisProgram Decode(const BgDocument *doc,const BgFile *bg)
{ BgVisProgram p={0};OK(BgVisDecodeDocument(bg,doc,&p,&why));OK(BgVisCanEdit(&p));return p; }
static BOOL Edit(BgDocument *doc,const BgFile *bg,BgVisEditOperation operation,DWORD row,DWORD op,DWORD a,DWORD b,BOOL after,BOOL otherwise)
{
    BgVisEditRequest r={.operation=operation,.row=row,.opcode=op,.arg={a,b},.after=after,.withElse=otherwise};
    BOOL ok=BgVisEdit(doc,bg,&r);why=r.why;return ok;
}
static void Same(const BgVisProgram *a,const BgVisProgram *b)
{
    OK(a->count==b->count);
    for (DWORD n=0;n<a->count;n++)
    {
        const BgVisInstruction *x=a->instructions+n,*y=b->instructions+n;
        OK(x->opcode==y->opcode && x->length==y->length && x->depth==y->depth);
        if (x->argument==BG_VIS_ONE_PORTAL) { OK(x->portal==y->portal && x->room1==y->room1 && x->room2==y->room2); }
        else { OK(!memcmp(x->arg,y->arg,sizeof(x->arg))); }
    }
}
static BgFile Save(const BgDocument *doc,const BgFile *base,const char *dir)
{
    BgFile live={0},disk={0};BgDocument reload={0};BgVisProgram a=Decode(doc,base),b={0};
    OK(BgDocumentCompile(doc,base,&live,&why));OK(BgFileValidateVertexBatches(&live,&why));
    OK(BgSaveProjectFile(dir,&live,&why));OK(BgLoadProjectFile(dir,live.name,&disk,&why));
    OK(BgDocumentLoad(disk.data,disk.size,doc->levelscale,&reload,&why));
    b=Decode(&reload,&disk);Same(&a,&b);OK(reload.facecount==doc->facecount && reload.roomcount==doc->roomcount);
    BgVisFree(&a);BgVisFree(&b);BgDocumentFree(&reload);BgFileFree(&disk);return live;
}
static void FixtureEdits(const char *dir)
{
    BgFile bg=Fixture(),saved={0},again={0};BgDocument doc={0};BgVisProgram p;
    OK(BgDocumentLoad(bg.data,bg.size,.5f,&doc,&why));OK(doc.viscommandsloaded && !doc.viscommandssize);
    EditHistory history={0};EditHistoryTransaction tx={0};SetupFile setup={0};StanFile stan={0};
    EditHistoryReset(&history,&doc,&setup,&stan);
    OK(EditHistoryBeginBgEdit(&history,&doc,"Add command",&tx,&why));
    OK(Edit(&doc,&bg,BG_VIS_INSERT,0,0x14,1,3,FALSE,FALSE));
    OK(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&tx,&why));
    saved=Save(&doc,&bg,dir);EditHistoryMarkBgSaved(&history,&doc);OK(!doc.dirty);
    OK(EditHistoryUndo(&history,&doc,&setup,&stan,NULL,&why));OK(!doc.viscommandssize && doc.dirty);
    again=Save(&doc,&saved,dir);OK(!Get(again.data+12));BgFileFree(&again);
    OK(EditHistoryRedo(&history,&doc,&setup,&stan,NULL,&why));OK(!doc.dirty);
    again=Save(&doc,&saved,dir);BgFileFree(&saved);saved=again;
    OK(Edit(&doc,&saved,BG_VIS_INSERT,0,0x5a,0,0,TRUE,TRUE));
    p=Decode(&doc,&saved);OK(p.count==5 && p.instructions[1].opcode==0x5a && p.instructions[2].opcode==0x5b);BgVisFree(&p);
    OK(Edit(&doc,&saved,BG_VIS_INSERT,2,0x1f,0,0,FALSE,FALSE));
    OK(Edit(&doc,&saved,BG_VIS_INSERT,3,0x20,3,0,FALSE,FALSE));
    again=Save(&doc,&saved,dir);BgFileFree(&saved);saved=again;
    p=Decode(&doc,&saved);OK(p.count==7 && p.instructions[2].portal==0 && p.instructions[2].matches==2);
    DWORD before=doc.viscommandssize;unsigned char *copy=malloc(before);memcpy(copy,doc.viscommands,before);
    OK(!Edit(&doc,&saved,BG_VIS_INSERT,3,0x14,3,1,FALSE,FALSE));
    OK(!Edit(&doc,&saved,BG_VIS_INSERT,0,0x20,4,0,FALSE,FALSE));
    OK(!Edit(&doc,&saved,BG_VIS_INSERT,0,0x1f,199,0,FALSE,FALSE));
    OK(!Edit(&doc,&saved,BG_VIS_INSERT,0,0x5b,0,0,FALSE,FALSE));
    OK(!Edit(&doc,&saved,BG_VIS_INSERT,3,0x5b,0,0,FALSE,FALSE));
    OK(!Edit(&doc,&saved,BG_VIS_DELETE,p.count-1,0,0,0,FALSE,FALSE));
    OK(doc.viscommandssize==before && !memcmp(copy,doc.viscommands,before));free(copy);BgVisFree(&p);
    OK(Edit(&doc,&saved,BG_VIS_REPLACE,3,0x20,2,0,FALSE,FALSE));
    OK(Edit(&doc,&saved,BG_VIS_DELETE,5,0,0,0,FALSE,FALSE)); /* END IF deletes its full block */
    p=Decode(&doc,&saved);OK(p.count==2);BgVisFree(&p);
    OK(Edit(&doc,&saved,BG_VIS_INSERT,1,0x50,0,0,FALSE,FALSE));
    p=Decode(&doc,&saved);OK(p.count==5 && BgVisReturnCell(&p,3));BgVisFree(&p);
    OK(!Edit(&doc,&saved,BG_VIS_DELETE,3,0,0,0,FALSE,FALSE));
    OK(Edit(&doc,&saved,BG_VIS_INSERT,2,0x1e,0,0,TRUE,FALSE)); /* after CATCH goes past skipped POP */
    p=Decode(&doc,&saved);OK(p.instructions[3].opcode==2 && p.instructions[4].opcode==0x1e);BgVisFree(&p);
    OK(Edit(&doc,&saved,BG_VIS_DELETE,1,0,0,0,FALSE,FALSE));
    p=Decode(&doc,&saved);OK(p.count==3 && p.instructions[1].opcode==0x1e);BgVisFree(&p);
    /* Unsaved portal identity is encoded only when the compiler has allocated it. */
    BgPortalPlacement placement={.room1=1,.room2=2,.width=20,.height=20,.plane=BG_PORTAL_XY};DWORD portal;
    OK(BgDocumentAddPortal(&doc,&placement,&portal,&why));
    OK(Edit(&doc,&saved,BG_VIS_INSERT,1,0x1f,portal,0,FALSE,FALSE));
    p=Decode(&doc,&saved);OK(p.instructions[1].portal==portal && (p.instructions[1].arg[0]&BG_PORTAL_NEW_GEOMETRY));BgVisFree(&p);
    OK(!BgDocumentDeletePortals(&doc,&saved,&portal,1,&why));
    again=Save(&doc,&saved,dir);BgFileFree(&saved);saved=again;
    OK(Edit(&doc,&saved,BG_VIS_DELETE,1,0,0,0,FALSE,FALSE));
    OK(BgDocumentDeletePortals(&doc,&saved,&portal,1,&why)); /* changed live commands override saved script */
    /* Repeated additions/deletions/saves do not grow the live BG indefinitely. */
    for (DWORD n=0;n<60;n++) { OK(Edit(&doc,&saved,BG_VIS_INSERT,0,0x1e,0,0,FALSE,FALSE)); }
    again=Save(&doc,&saved,dir);BgFileFree(&saved);saved=again;DWORD highwater=saved.size;
    for (unsigned cycle=0;cycle<12;cycle++)
    {
        OK(Edit(&doc,&saved,BG_VIS_DELETE,0,0,0,0,FALSE,FALSE));
        again=Save(&doc,&saved,dir);OK(again.size==highwater);BgFileFree(&saved);saved=again;
        OK(Edit(&doc,&saved,BG_VIS_INSERT,0,0x1e,0,0,FALSE,FALSE));
        again=Save(&doc,&saved,dir);OK(again.size==highwater);BgFileFree(&saved);saved=again;
    }
    EditHistoryFree(&history);BgDocumentFree(&doc);BgFileFree(&saved);BgFileFree(&bg);
    puts("PASS: first command, arguments, conditional/branch blocks, protected END/return cells, atomic validation, undo after save, native save/reload, unsaved portal references/deletion and stable repeated saves.");
}
static void LiteralReferences(const char *dir)
{
    BgFile bg=Fixture(),saved={0};BgDocument doc={0};BgVisProgram p;
    OK(BgDocumentLoad(bg.data,bg.size,1,&doc,&why));
    OK(Edit(&doc,&bg,BG_VIS_INSERT,0,0x1f,2,0,FALSE,FALSE));
    doc.viscommands[8]=0x65;Put(doc.viscommands+12,2); /* native literal-index form */
    DWORD removed=0;OK(BgDocumentDeletePortals(&doc,&bg,&removed,1,&why));
    p=Decode(&doc,&bg);OK(p.instructions[0].portal==1 && p.instructions[0].room1==2);BgVisFree(&p);
    removed=1;OK(!BgDocumentDeletePortals(&doc,&bg,&removed,1,&why));
    saved=Save(&doc,&bg,dir);BgFileFree(&saved);BgDocumentFree(&doc);BgFileFree(&bg);
    puts("PASS: native literal portal references follow table compaction and protect their target.");
}
static void RoomLimit(void)
{
    BgFile bg=Fixture();BgDocument doc={0};
    OK(BgDocumentLoad(bg.data,bg.size,1,&doc,&why));
    for (DWORD n=0;n<152;n++) { OK(Edit(&doc,&bg,BG_VIS_INSERT,0,0x20,1,0,FALSE,FALSE)); }
    DWORD size=doc.viscommandssize;unsigned char *copy=malloc(size);OK(copy);memcpy(copy,doc.viscommands,size);
    OK(!Edit(&doc,&bg,BG_VIS_INSERT,0,0x20,1,0,FALSE,FALSE));
    OK(strstr(why,"152") && doc.viscommandssize==size && !memcmp(copy,doc.viscommands,size));
    OK(Edit(&doc,&bg,BG_VIS_DELETE,0,0,0,0,FALSE,FALSE));
    OK(Edit(&doc,&bg,BG_VIS_INSERT,0,0x20,1,0,FALSE,FALSE));
    free(copy);BgDocumentFree(&doc);BgFileFree(&bg);
    puts("PASS: native 152-room capacity boundary and atomic overflow rejection.");
}
static void Corpus(const char *path,const char *dir)
{
    FILE *f=fopen(path,"rb");OK(f);OK(!fseek(f,0,SEEK_END));long size=ftell(f);rewind(f);
    BgFile bg={0},saved={0};BgDocument doc={0};bg.size=(DWORD)size;bg.data=malloc(bg.size);OK(bg.data && fread(bg.data,1,bg.size,f)==bg.size);fclose(f);
    strcpy(bg.name,"bg/bg_commands_test.seg");OK(BgDocumentLoad(bg.data,bg.size,1,&doc,&why));
    BgVisProgram original=Decode(&doc,&bg);
    EditHistory h={0};EditHistoryTransaction tx={0};SetupFile setup={0};StanFile stan={0};EditHistoryReset(&h,&doc,&setup,&stan);
    OK(EditHistoryBeginBgEdit(&h,&doc,"Add",&tx,&why));
    OK(Edit(&doc,&bg,BG_VIS_INSERT,0,0x1f,0,0,FALSE,FALSE));
    OK(EditHistoryCommitEdit(&h,&doc,&setup,&stan,&tx,&why));saved=Save(&doc,&bg,dir);EditHistoryMarkBgSaved(&h,&doc);
    OK(EditHistoryUndo(&h,&doc,&setup,&stan,NULL,&why));BgVisProgram restored=Decode(&doc,&saved);Same(&original,&restored);BgVisFree(&restored);
    BgFile undone=Save(&doc,&saved,dir);OK(EditHistoryRedo(&h,&doc,&setup,&stan,NULL,&why));
    BgFile redone=Save(&doc,&undone,dir);
    OK(Edit(&doc,&redone,BG_VIS_DELETE,0,0,0,0,FALSE,FALSE));restored=Decode(&doc,&redone);Same(&original,&restored);BgVisFree(&restored);
    BgVisFree(&original);EditHistoryFree(&h);BgDocumentFree(&doc);BgFileFree(&redone);BgFileFree(&undone);BgFileFree(&saved);BgFileFree(&bg);
    printf("PASS: %s: insert, save/reload, undo/save, redo/save, delete, and original command semantics preserved.\n",path);
}
int main(int argc,char **argv)
{ OK(argc>=2);char dir[1024];snprintf(dir,sizeof(dir),"%s/bg",argv[1]);CreateDirectory(dir,NULL);FixtureEdits(argv[1]);LiteralReferences(argv[1]);RoomLimit();for(int n=2;n<argc;n++) { Corpus(argv[n],argv[1]); }return 0; }
