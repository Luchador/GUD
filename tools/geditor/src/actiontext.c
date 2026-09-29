#include "actiontext.h"
#include <stdio.h>
#include <string.h>

void ActionTextFree(ActionText *text)
{
    for (DWORD i=0;i<TEXT_BANK_MAX_FILES;i++) { TextBankFree(&text->banks[i].text); }
    memset(text,0,sizeof(*text));
}

void ActionTextLoad(ActionText *text, const char *dir, const RomFile *rom)
{
    TextBankFile files[TEXT_BANK_MAX_FILES]; DWORD count=0, mapped=0;
    const char *why="";
    memset(text,0,sizeof(*text));
    if (!rom || !rom->data)
    { snprintf(text->error,sizeof(text->error),"Project base ROM unavailable."); return; }
    if (!TextBankCatalog(rom,files,&count,&why))
    { snprintf(text->error,sizeof(text->error),"%s",why); return; }
    for (DWORD i=0;i<count;i++)
    {
        if (files[i].id>=TEXT_BANK_MAX_FILES) { continue; }
        ActionTextBank *bank=&text->banks[files[i].id];
        snprintf(bank->name,sizeof(bank->name),"%s",files[i].name); mapped++;
        if (!TextBankLoadProject(dir,rom,bank->name,&bank->text,&why))
        { snprintf(bank->error,sizeof(bank->error),"%s",why); }
    }
    if (!mapped) { snprintf(text->error,sizeof(text->error),"The ROM does not provide text bank IDs."); }
}

const char *ActionTextResolve(const ActionText *text, DWORD id,
    const char **bankname, const char **why)
{
    const ActionTextBank *bank;
    const char *value;
    *bankname=""; *why="";
    if (id>0xffff) { *why="Text ID is outside the 16-bit range."; return NULL; }
    if (text->error[0]) { *why=text->error; return NULL; }
    bank=&text->banks[id>>10]; *bankname=bank->name;
    if (!bank->name[0]) { *why="Text bank unavailable."; return NULL; }
    if (bank->error[0]) { *why=bank->error; return NULL; }
    if ((id&1023)>=bank->text.count) { *why="Text slot is outside this bank."; return NULL; }
    value=TextBankString(&bank->text,id&1023);
    if (!value) { *why="Unused text slot."; }
    return value;
}

/* List rows are single-line ASCII. Preserve special game bytes visibly rather
 * than interpreting them as Windows code-page characters or control codes. */
void ActionTextSummary(const ActionText *text, DWORD id, char *out, size_t size)
{
    const char *name,*why,*value=ActionTextResolve(text,id,&name,&why);
    size_t at=0; BOOL space=FALSE;
    if (!size) { return; }
    out[0]=0;
    if (!value) { snprintf(out,size,"[%s]",why); return; }
    if (!*value) { snprintf(out,size,"[Empty string]"); return; }
    for (const unsigned char *p=(const unsigned char *)value;*p;p++)
    {
        char token[6]; size_t n;
        if (*p==' ' || *p=='\n' || *p=='\r' || *p=='\t') { space=at!=0; continue; }
        if (*p<32 || *p>126) { snprintf(token,sizeof(token),"\\x%02X",*p); }
        else if (*p=='\\' || *p=='"') { token[0]='\\'; token[1]=*p; token[2]=0; }
        else { token[0]=*p; token[1]=0; }
        n=strlen(token);
        if (n+(space ? 1u : 0u)>=size-at)
        {
            if (size>=4) { at=at<size-4 ? at : size-4; memcpy(out+at,"...",4); }
            return;
        }
        if (space) { out[at++]=' '; space=FALSE; }
        memcpy(out+at,token,n+1); at+=n;
    }
    if (!at) { snprintf(out,size,"[Whitespace only]"); }
}

void ActionTextInstructionFormat(const ActionText *text, const ActionBlock *block,
    DWORD instruction, char *out, size_t size)
{
    if (!size) { return; }
    out[0]=0;
    if (instruction>=block->count) { return; }
    ActionInstructionFormat(block,instruction,out,size);
    const ActionInstruction *ins=&block->instructions[instruction];
    const ActionOpcode *op=&g_ActionOpcodes[ins->bytes[0]];
    for (int p=0;p<op->paramcount;p++) if (op->params[p].kind==ACTION_TEXT)
    {
        char preview[512]; size_t used=strlen(out);
        ActionTextSummary(text,ActionReadValue(ins,p),preview,sizeof(preview));
        snprintf(out+used,size-used,"  \"%s\"",preview);
    }
}
