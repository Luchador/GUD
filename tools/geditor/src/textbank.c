#include "textbank.h"
#include "editorpath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static BOOL Fail(const char **why, const char *text) { *why=text; return FALSE; }
static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static void Write32(unsigned char *p, DWORD value)
{ p[0]=value>>24; p[1]=value>>16; p[2]=value>>8; p[3]=value; }

BOOL TextBankIsResource(const char *name)
{
    size_t n=strlen(name);
    if (n<3 || n>=64 || name[0]!='L' || name[n-1]!='E') { return FALSE; }
    for (size_t i=1;i<n-1;i++)
    { if (!((name[i]>='a' && name[i]<='z') || (name[i]>='A' && name[i]<='Z')
        || (name[i]>='0' && name[i]<='9') || name[i]=='_')) { return FALSE; } }
    return TRUE;
}
BOOL TextBankProjectPath(const char *dir, const char *name, char *path, size_t size)
{
    char leaf[80];
    if (!TextBankIsResource(name)) { return FALSE; }
    snprintf(leaf,sizeof(leaf),"text\\%s.bin",name);
    return EditorPathJoin(path,size,dir,leaf);
}
static const RomManifestEntry *Entry(const RomFile *rom, DWORD kind)
{
    for (DWORD i=0;i<rom->info.entrycount;i++)
    { if (rom->info.entries[i].kind==kind) { return &rom->info.entries[i]; } }
    return NULL;
}
static BOOL Map(const RomFile *rom, const RomManifestEntry *map, DWORD ptr, DWORD size, DWORD *out)
{
    if (!map || map->romstart>map->romend || map->romend>rom->size || ptr<map->flags
        || ptr-map->flags>map->romend-map->romstart
        || size>map->romend-map->romstart-(ptr-map->flags)) { return FALSE; }
    *out=map->romstart+ptr-map->flags; return TRUE;
}
BOOL TextBankCatalog(const RomFile *rom, TextBankFile *files, DWORD *count, const char **why)
{
    const RomManifestEntry *catalog=Entry(rom,0x5458424b), *map=Entry(rom,0x434d4150);
    DWORD table=0, slots=0;
    *count=0;
    if (catalog)
    {
        DWORD at=catalog->romstart;
        if (!map || at<map->romstart || at>map->romend || map->romend>rom->size || map->romend-at<16
            || Read32(rom->data+at+8)!=4 || Read32(rom->data+at+12)!=1) { goto invalid; }
        slots=Read32(rom->data+at+4);
        if (slots>64 || !Map(rom,map,Read32(rom->data+at),slots*4,&table)) { goto invalid; }
    }
    for (DWORD i=0;i<1024;i++)
    {
        char name[64]; DWORD id=TEXT_BANK_UNKNOWN_ID;
        if (!RomGetFileByIndex(rom,i,name,sizeof(name),NULL,NULL)) { break; }
        if (!TextBankIsResource(name)) { continue; }
        if (*count==TEXT_BANK_MAX_FILES) { return Fail(why,"Too many text files in the ROM."); }
        for (DWORD b=0;b<slots;b++)
        {
            DWORD ptr=Read32(rom->data+table+b*4),at; const unsigned char *end;
            if (!ptr) { continue; }
            if (!Map(rom,map,ptr,1,&at) || !(end=memchr(rom->data+at,0,map->romend-at))) { goto invalid; }
            if ((size_t)(end-(rom->data+at))==strlen(name) && !strcmp((const char *)rom->data+at,name))
            { id=b; break; }
        }
        strcpy(files[*count].name,name); files[(*count)++].id=id;
    }
    *why=""; return TRUE;
invalid:
    return Fail(why,"The ROM's text bank catalog is invalid.");
}

BOOL TextBankValidate(const unsigned char *data, DWORD size, DWORD *count, const char **why)
{
    DWORD table=size, i;
    if (count) { *count=0; }
    if (!data || size<4 || size>TEXT_BANK_MAX_SIZE) { return Fail(why,"Invalid text bank size."); }
    /* Native .data is the offset array, padded to 16 bytes; .rodata follows.
     * Keep its zero padding as unused slots, so every existing ID stays put.
     * All-zero multiplayer placeholders have no .rodata. */
    for (i=0;i<table;i+=4)
    {
        DWORD offset;
        if (i>=TEXT_BANK_MAX_SLOTS*4 || size-i<4) { goto invalid; }
        offset=Read32(data+i);
        if (!offset) { continue; }
        if (offset>=size || offset<i+4) { goto invalid; }
        if (offset<table) { table=offset; }
    }
    if ((table&3) || table/4>TEXT_BANK_MAX_SLOTS) { goto invalid; }
    for (i=0;i<table;i+=4)
    {
        DWORD offset=Read32(data+i);
        if (offset && (offset<table || !memchr(data+offset,0,size-offset))) { goto invalid; }
    }
    if (count) { *count=table/4; }
    *why=""; return TRUE;
invalid:
    return Fail(why,"Invalid text offsets or an unterminated string in this text bank.");
}
BOOL TextBankLoad(const unsigned char *data, DWORD size, TextBank *out, const char **why)
{
    memset(out,0,sizeof(*out));
    if (!TextBankValidate(data,size,&out->count,why)) { return FALSE; }
    out->data=malloc(size);
    if (!out->data) { return Fail(why,"Out of memory reading text."); }
    memcpy(out->data,data,size); out->size=size; return TRUE;
}
void TextBankFree(TextBank *bank) { free(bank->data); memset(bank,0,sizeof(*bank)); }
const char *TextBankString(const TextBank *bank, DWORD slot)
{
    DWORD offset;
    if (slot>=bank->count || !bank->data) { return NULL; }
    offset=Read32(bank->data+slot*4);
    return offset ? (const char *)bank->data+offset : NULL;
}
BOOL TextBankEqual(const unsigned char *a, DWORD asize, const unsigned char *b, DWORD bsize)
{
    DWORD acount,bcount; const char *why;
    if (!TextBankValidate(a,asize,&acount,&why) || !TextBankValidate(b,bsize,&bcount,&why) || acount!=bcount)
    { return FALSE; }
    for (DWORD i=0;i<acount;i++)
    {
        DWORD x=Read32(a+i*4),y=Read32(b+i*4);
        if ((!x)!=(!y) || (x && strcmp((const char *)a+x,(const char *)b+y))) { return FALSE; }
    }
    return TRUE;
}
BOOL TextBankSet(TextBank *bank, DWORD slot, const char *text, const char **why)
{
    DWORD size=bank->count*4, offsets[TEXT_BANK_MAX_SLOTS]={0};
    const char *strings[TEXT_BANK_MAX_SLOTS]; unsigned char *data;
    const char *old=TextBankString(bank,slot);
    if (!old || !text) { return Fail(why,"Select an existing string. Unused slots cannot be edited."); }
    if (!strcmp(old,text)) { return TRUE; }
    for (DWORD i=0;i<bank->count;i++)
    {
        strings[i]=i==slot ? text : TextBankString(bank,i);
        if (!strings[i]) { continue; }
        /* Unchanged aliases stay aliases. Editing one never changes another. */
        for (DWORD j=0;j<i;j++) if (i!=slot && j!=slot
            && Read32(bank->data+i*4)==Read32(bank->data+j*4)) { offsets[i]=offsets[j]; break; }
        if (offsets[i]) { continue; }
        size_t length=strlen(strings[i])+1;
        if (length>TEXT_BANK_MAX_SIZE-size || ((length+3)&~(size_t)3)>TEXT_BANK_MAX_SIZE-size)
        { return Fail(why,"This text file exceeds the game's briefing text buffer (450048 bytes)."); }
        offsets[i]=size; size+=(DWORD)((length+3)&~(size_t)3);
    }
    size=(size+15)&~15u;
    if (size>TEXT_BANK_MAX_SIZE) { return Fail(why,"This text file exceeds the game's text buffer."); }
    data=calloc(size,1);
    if (!data) { return Fail(why,"Out of memory editing text."); }
    for (DWORD i=0;i<bank->count;i++)
    { Write32(data+i*4,offsets[i]); if (offsets[i]) { strcpy((char *)data+offsets[i],strings[i]); } }
    free(bank->data); bank->data=data; bank->size=size; return TRUE;
}

wchar_t *TextBankFormat(const char *text)
{
    static const wchar_t hex[]=L"0123456789ABCDEF";
    size_t n=text ? strlen(text) : 0, at=0; wchar_t *out=malloc((n*4+1)*sizeof(*out));
    if (!out) { return NULL; }
    for (size_t i=0;i<n;i++)
    {
        unsigned char c=(unsigned char)text[i];
        if (c=='\n') { out[at++]='\r'; out[at++]='\n'; }
        else if (c=='\\' || c=='\r' || c=='\t')
        { out[at++]='\\'; out[at++]=c=='\r' ? 'r' : c=='\t' ? 't' : '\\'; }
        else if (c>=32 && c<=126) { out[at++]=c; }
        else { out[at++]='\\'; out[at++]='x'; out[at++]=hex[c>>4]; out[at++]=hex[c&15]; }
    }
    out[at]=0; return out;
}
static int Hex(wchar_t c)
{ return c>='0' && c<='9' ? c-'0' : c>='a' && c<='f' ? c-'a'+10 : c>='A' && c<='F' ? c-'A'+10 : -1; }
BOOL TextBankParse(const wchar_t *text, char **out, const char **why)
{
    size_t n=wcslen(text),at=0; char *data;
    *out=NULL;
    if (n>4u*TEXT_BANK_MAX_SIZE) { return Fail(why,"This string is too long."); }
    data=malloc(n+1);
    if (!data) { return Fail(why,"Out of memory reading edited text."); }
    for (size_t i=0;i<n;i++)
    {
        wchar_t c=text[i];
        if (c=='\r') { if (i+1<n && text[i+1]=='\n') { i++; } c='\n'; }
        else if (c=='\\')
        {
            c=text[++i];
            if (c=='n') { c='\n'; } else if (c=='r') { c='\r'; } else if (c=='t') { c='\t'; }
            else if (c=='x' && i+2<n && Hex(text[i+1])>=0 && Hex(text[i+2])>=0)
            { c=Hex(text[i+1])*16+Hex(text[i+2]); i+=2; }
            else if (c!='\\') { free(data); return Fail(why,"Use \\\\, \\n, \\r, \\t or \\xHH for escapes."); }
            if (!c) { free(data); return Fail(why,"\\x00 would terminate the string. Use an empty text box for an empty string."); }
        }
        else if (c>126 || (c<32 && c!='\n' && c!='\t'))
        { free(data); return Fail(why,"Use English ASCII characters. Preserve special game bytes with \\xHH; Unicode is not supported by the game fonts."); }
        data[at++]=(char)c;
    }
    data[at]=0; *out=data; *why=""; return TRUE;
}

BOOL TextBankLoadProject(const char *dir, const RomFile *rom, const char *name, TextBank *out, const char **why)
{
    char path[MAX_PATH]; DWORD attrs,offset,size,got; HANDLE file; unsigned char *data; BOOL ok;
    memset(out,0,sizeof(*out));
    if (!TextBankProjectPath(dir,name,path,sizeof(path))) { return Fail(why,"Invalid text file path."); }
    attrs=GetFileAttributes(path);
    if (attrs==INVALID_FILE_ATTRIBUTES)
    {
        DWORD error=GetLastError();
        if (error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND) { return Fail(why,"Cannot inspect the saved text file."); }
        return RomFindFile(rom,name,&offset,&size,why) && TextBankLoad(rom->data+offset,size,out,why);
    }
    file=CreateFile(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) { return Fail(why,"Cannot open the saved text file."); }
    size=GetFileSize(file,NULL);
    if (!size || size>TEXT_BANK_MAX_SIZE) { CloseHandle(file); return Fail(why,"Invalid saved text file size."); }
    data=malloc(size);
    if (!data) { CloseHandle(file); return Fail(why,"Out of memory reading text."); }
    ok=ReadFile(file,data,size,&got,NULL) && got==size; CloseHandle(file);
    if (ok) { ok=TextBankLoad(data,size,out,why); }
    else { Fail(why,"Cannot fully read the saved text file."); }
    free(data); return ok;
}
BOOL TextBankSaveProject(const char *dir, const char *name, const TextBank *bank, const char **why)
{
    char folder[MAX_PATH],path[MAX_PATH],temp[MAX_PATH]; DWORD written; HANDLE file; BOOL ok;
    if (!TextBankValidate(bank->data,bank->size,NULL,why)) { return FALSE; }
    if (!TextBankProjectPath(dir,name,path,sizeof(path)) || !EditorPathJoin(folder,sizeof(folder),dir,"text"))
    { return Fail(why,"The text file path is too long."); }
    if (!CreateDirectory(folder,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)
    { return Fail(why,"Cannot create the project's text folder."); }
    /* GetTempFileName reserves a unique sibling; never overwrite the saved
     * bank until its complete replacement has been written and closed. */
    if (!GetTempFileName(folder,"txt",0,temp)) { return Fail(why,"Cannot create a temporary text file."); }
    file=CreateFile(temp,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) { DeleteFile(temp); return Fail(why,"Cannot write the temporary text file."); }
    ok=WriteFile(file,bank->data,bank->size,&written,NULL) && written==bank->size;
    if (!CloseHandle(file)) { ok=FALSE; }
    if (ok) { ok=MoveFileEx(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH); }
    if (!ok) { DeleteFile(temp); return Fail(why,"Could not save the text file. The previous saved file was retained."); }
    *why=""; return TRUE;
}
