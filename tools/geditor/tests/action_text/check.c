#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "actiontext.h"

static const char *why="";
#define OK(x) do { if (!(x)) { fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why); abort(); } } while (0)
static void Put(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }

static void Fixture(RomFile *rom)
{
    rom->size=0x500; rom->data=calloc(rom->size,1); OK(rom->data);
    rom->info.entrycount=4;
    rom->info.entries[0]=(RomManifestEntry){0x434d4150,0,0x400,0x80000000};
    rom->info.entries[1]=(RomManifestEntry){0x4654424c,0x20,0x38,0};
    rom->info.entries[2]=(RomManifestEntry){0x4f425347,0x400,0x500,0};
    rom->info.entries[3]=(RomManifestEntry){0x5458424b,0x80,0x90,0};
    Put(rom->data+0x24,0x80000280); Put(rom->data+0x28,0x400);
    strcpy((char *)rom->data+0x280,"LpreviewE");
    Put(rom->data+0x80,0x80000100); Put(rom->data+0x84,64);
    Put(rom->data+0x88,4); Put(rom->data+0x8c,1);
    /* Bank 37 deliberately differs from its file-table index (zero). */
    Put(rom->data+0x100+37*4,0x80000280);
    Put(rom->data+0x404,16); Put(rom->data+0x408,64); Put(rom->data+0x40c,68);
    strcpy((char *)rom->data+0x410,"Boris: First line.\n       Second line.\n");
    strcpy((char *)rom->data+0x444,"special \x80\x01 \"quote\"");
}

int main(int argc,char **argv)
{
    RomFile rom={0}; ActionText text={0}; const char *name,*value;
    const DWORD id=(37u<<10)|1; char line[1200],path[MAX_PATH]; DWORD parsed;
    OK(argc==2 || argc==3); Fixture(&rom);
    ActionTextLoad(&text,argv[1],&rom);
    value=ActionTextResolve(&text,id,&name,&why);
    OK(value && !strcmp(name,"LpreviewE") && !strcmp(value,"Boris: First line.\n       Second line.\n"));
    ActionTextSummary(&text,id,line,sizeof(line));
    OK(!strcmp(line,"Boris: First line. Second line."));
    wchar_t *wide=TextBankFormat(value);
    OK(wide && wcsstr(wide,L"\r\n       Second line.\r\n")); free(wide);
    ActionTextSummary(&text,(37u<<10)|3,line,sizeof(line));
    OK(strstr(line,"\\x80\\x01") && strstr(line,"\\\"quote\\\""));
    OK(!ActionTextResolve(&text,37u<<10,&name,&why) && strstr(why,"Unused"));
    value=ActionTextResolve(&text,(37u<<10)|2,&name,&why); OK(value && !*value);
    OK(!ActionTextResolve(&text,(37u<<10)|4,&name,&why) && strstr(why,"outside"));
    OK(!ActionTextResolve(&text,0xffff,&name,&why) && strstr(why,"unavailable"));
    OK(!ActionTextResolve(&text,0x10000,&name,&why) && strstr(why,"16-bit"));

    for (int op=0xc2;op<=0xc3;op++)
    {
        unsigned char bytes[]={(unsigned char)op,(unsigned char)(id>>8),(unsigned char)id};
        unsigned char original[3]; memcpy(original,bytes,3);
        ActionInstruction ins={0}; ins.bytes=bytes; ins.size=3;
        ActionBlock block={0}; block.instructions=&ins; block.count=1;
        OK(g_ActionOpcodes[op].params[0].kind==ACTION_TEXT);
        ActionTextInstructionFormat(&text,&block,0,line,sizeof(line));
        OK(strstr(line,"TEXT_SLOT=37889") && strstr(line,"Boris: First line. Second line."));
        OK(!memcmp(bytes,original,3));
        for (size_t n=0;n<12;n++) {
            char small[16]; memset(small,0x5a,sizeof(small));
            ActionTextInstructionFormat(&text,&block,0,small,n);
            OK((unsigned char)small[n]==0x5a && (!n || memchr(small,0,n)));
            memset(small,0x5a,sizeof(small));
            ActionTextSummary(&text,id,small,n);
            OK((unsigned char)small[n]==0x5a && (!n || memchr(small,0,n)));
        }
        OK(ActionParseValue(&g_ActionOpcodes[op].params[0],"8220",&parsed) && parsed==8220);
        OK(ActionParseValue(&g_ActionOpcodes[op].params[0],"0x201C",&parsed) && parsed==8220);
        OK(!ActionParseValue(&g_ActionOpcodes[op].params[0],"65536",&parsed));
    }

    /* Saved text overrides the base, but a corrupt saved bank never silently
     * displays old stock text. A snapshot remains valid after ROM disposal. */
    TextBank edited={0};
    OK(TextBankLoad(text.banks[37].text.data,text.banks[37].text.size,&edited,&why));
    OK(TextBankSet(&edited,1,"Project dialogue\n",&why));
    OK(TextBankSaveProject(argv[1],"LpreviewE",&edited,&why)); TextBankFree(&edited);
    OK(strstr(ActionTextResolve(&text,id,&name,&why),"Boris:"));
    ActionTextFree(&text); ActionTextLoad(&text,argv[1],&rom);
    OK(!strcmp(ActionTextResolve(&text,id,&name,&why),"Project dialogue\n"));
    OK(TextBankProjectPath(argv[1],"LpreviewE",path,sizeof(path)));
    FILE *bad=fopen(path,"wb"); OK(bad); OK(fwrite("bad",1,3,bad)==3); OK(!fclose(bad));
    ActionTextFree(&text); ActionTextLoad(&text,argv[1],&rom);
    OK(!ActionTextResolve(&text,id,&name,&why) && strstr(why,"size"));
    OK(!strcmp(name,"LpreviewE"));
    ActionTextFree(&text); rom.info.entrycount=3; ActionTextLoad(&text,argv[1],&rom);
    OK(!ActionTextResolve(&text,id,&name,&why) && strstr(why,"text bank IDs"));
    ActionTextFree(&text); ActionTextLoad(&text,argv[1],NULL);
    OK(!ActionTextResolve(&text,id,&name,&why) && strstr(why,"base ROM"));
    ActionTextFree(&text); RomFree(&rom);
    puts("PASS: catalog IDs, ROM fallback, saved overrides, missing/corrupt banks, empty/unused slots, multiline and bounded previews, unchanged text IDs.");

    if (argc==3)
    {
        OK(RomLoad(argv[2],&rom,&why)); ActionTextLoad(&text,argv[1],&rom); RomFree(&rom);
        value=ActionTextResolve(&text,8220,&name,&why);
        OK(value && !strcmp(name,"LarecE") && !strcmp(value,"Boris: Oops!\n"));
        ActionTextSummary(&text,8220,line,sizeof(line));
        printf("PASS: actual Control text ID 8220 resolves to %s\n",line);
        ActionTextFree(&text);
    }
    return 0;
}
