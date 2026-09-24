#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "textbank.h"
static const char *why="";
#define OK(x) do { if (!(x)) { fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why); abort(); } } while (0)
static void Put(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void RoundTrip(const unsigned char *data,DWORD size)
{
    TextBank bank={0}; DWORD count;
    OK(TextBankValidate(data,size,&count,&why)); OK(TextBankLoad(data,size,&bank,&why));
    for (DWORD i=0;i<count;i++)
    {
        const char *s=TextBankString(&bank,i); char *decoded; wchar_t *encoded;
        if (!s) { continue; }
        encoded=TextBankFormat(s); OK(encoded); OK(TextBankParse(encoded,&decoded,&why));
        OK(!strcmp(s,decoded)); OK(TextBankSet(&bank,i,decoded,&why)); free(encoded); free(decoded);
    }
    /* Viewing/accepting unchanged text is byte-exact, not merely equivalent. */
    OK(bank.size==size && !memcmp(bank.data,data,size));
    for (DWORD i=0;i<count;i++)
    {
        const char *s=TextBankString(&bank,i); char *copy;
        if (!s) { continue; }
        copy=malloc(strlen(s)+1); strcpy(copy,s);
        OK(TextBankSet(&bank,i,"A longer edited mission objective.\n\nSecond line.\n",&why));
        TextBank reloaded={0}; OK(TextBankLoad(bank.data,bank.size,&reloaded,&why));
        OK(reloaded.count==count); OK(!strcmp(TextBankString(&reloaded,i),"A longer edited mission objective.\n\nSecond line.\n"));
        TextBankFree(&reloaded); OK(TextBankSet(&bank,i,copy,&why)); free(copy);
        OK(TextBankEqual(data,size,bank.data,bank.size));
    }
    TextBankFree(&bank);
}
static void EdgeCases(void)
{
    unsigned char data[80]={0}; TextBank bank={0}; DWORD count; char *s; wchar_t *wide;
    Put(data,16);Put(data+4,16);Put(data+8,18);strcpy((char *)data+16,"shared\n"); /* Full and suffix aliases. */
    OK(TextBankLoad(data,sizeof(data),&bank,&why)); OK(bank.count==4);
    OK(TextBankSet(&bank,0,"Changed\n",&why)); OK(!strcmp(TextBankString(&bank,1),"shared\n"));
    OK(!strcmp(TextBankString(&bank,2),"ared\n")); OK(TextBankString(&bank,3)==NULL);
    OK(TextBankSet(&bank,1,"",&why)); OK(TextBankString(&bank,1) && !*TextBankString(&bank,1));
    OK(!TextBankSet(&bank,3,"unused",&why)); TextBankFree(&bank);
    char bytes[256]; for (int i=1;i<=255;i++) { bytes[i-1]=(char)i; } bytes[255]=0;
    wide=TextBankFormat(bytes);OK(wide);OK(TextBankParse(wide,&s,&why));OK(!memcmp(s,bytes,256));free(s);free(wide);
    OK(TextBankParse(L"One\r\n\r\nTwo\n",&s,&why));OK(!strcmp(s,"One\n\nTwo\n"));free(s);
    OK(TextBankParse(L"\\xFF\\t\\r\\n\\\\",&s,&why));OK(!strcmp(s,"\xff\t\r\n\\"));free(s);
    OK(!TextBankParse(L"\\x00",&s,&why));OK(!TextBankParse(L"\\xF",&s,&why));
    OK(!TextBankParse(L"\\",&s,&why));OK(!TextBankParse(L"\\z",&s,&why));
    OK(!TextBankParse(L"Smart \u2019 quote",&s,&why));
    OK(TextBankIsResource("LsiloE"));OK(!TextBankIsResource("L../LsiloE"));OK(!TextBankIsResource("LsiloE.bin"));
    memset(data,0,sizeof(data));OK(TextBankValidate(data,32,&count,&why)&&count==8);
    OK(!TextBankValidate(data,31,&count,&why));
    Put(data,3);OK(!TextBankValidate(data,sizeof(data),&count,&why));
    Put(data,80);OK(!TextBankValidate(data,sizeof(data),&count,&why));
    Put(data,16);memset(data+16,'x',64);OK(!TextBankValidate(data,sizeof(data),&count,&why));
    memset(data+16,0,64);Put(data+4,4);OK(!TextBankValidate(data,sizeof(data),&count,&why));
    Put(data+4,79);OK(TextBankValidate(data,sizeof(data),&count,&why));
    OK(TextBankLoad(data,sizeof(data),&bank,&why));
    char *large=malloc(TEXT_BANK_MAX_SIZE+1);memset(large,'X',TEXT_BANK_MAX_SIZE);large[TEXT_BANK_MAX_SIZE]=0;
    OK(!TextBankSet(&bank,0,large,&why));OK(bank.size==sizeof(data)&&!memcmp(bank.data,data,sizeof(data)));
    free(large);TextBankFree(&bank);
    puts("PASS: aliases, NULL versus empty, every nonzero byte, CRLF, escapes, Unicode rejection, malformed offsets and size limit.");
}
int main(int argc,char **argv)
{
    EdgeCases();
    for (int i=1;i<argc;i++)
    {
        FILE *f=fopen(argv[i],"rb");long size;unsigned char *data;OK(f);OK(!fseek(f,0,SEEK_END));size=ftell(f);rewind(f);
        data=malloc(size);OK(data && fread(data,1,size,f)==(size_t)size);fclose(f);RoundTrip(data,(DWORD)size);free(data);
    }
    printf("PASS: %d native banks; all strings survive edits, resizing, reload and restoration with stable IDs.\n",argc-1);
    return 0;
}
