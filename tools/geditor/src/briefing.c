#include "briefing.h"
#include "setupmeta.h"
#include "editorpath.h"
#include "actionblocks.h"
#include "romexport.h"
#include "../../../src/doorshadowformat.h"
#include "../../../src/propconstants.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static BOOL Fail(const char **why,const char *s) { *why=s; return FALSE; }
static DWORD R(const unsigned char *p) { return SetupMetaRead32(p); }
static void W(unsigned char *p,DWORD v) { SetupMetaWrite32(p,v); }
static DWORD R16(const unsigned char *p) { return (DWORD)p[0]*256+p[1]; }
static void W16(unsigned char *p,DWORD v) { p[0]=v>>8;p[1]=v; }
static const struct { LONG level; const char *bank,*brief; } missions[]={
    {33,"LdamE","UbriefdamZ"},{34,"LarkE","UbriefarkZ"},{35,"LrunE","UbriefrunZ"},
    {36,"LsevxE","UbriefsevxZ"},{9,"LsevE","UbriefsevbunkerZ"},{20,"LsiloE","UbriefsiloZ"},
    {26,"LdestE","UbriefdestZ"},{43,"LsevxbE","UbriefsevxbZ"},{27,"LsevbE","UbriefsevbZ"},
    {22,"LstatE","UbriefstatueZ"},{24,"LarchE","UbriefarchZ"},{29,"LpeteE","UbriefpeteZ"},
    {30,"LdepoE","UbriefdepoZ"},{25,"LtraE","UbrieftraZ"},{37,"LjunE","UbriefjunZ"},
    {23,"LarecE","UbriefcontrolZ"},{39,"LcaveE","UbriefcaveZ"},{41,"LcradE","UbriefcradZ"},
    {28,"LaztE","UbriefaztZ"},{32,"LcrypE","UbriefcrypZ"}
};
BOOL BriefingNames(LONG level,const char **bank,const char **brief)
{
    for (size_t i=0;i<sizeof(missions)/sizeof(*missions);i++) if (missions[i].level==level)
    { *bank=missions[i].bank;*brief=missions[i].brief;return TRUE; }
    return FALSE;
}
DWORD BriefingConditionWords(DWORD type)
{
    if (type>=25 && type<=29) { return 2; }
    if (type==30 || type==32) { return 4; }
    if (type==31) { return 1; } if (type==33) { return 5; } if (type==34) { return 3; }
    return 0;
}
const char *BriefingConditionName(DWORD type)
{
    static const char *names[]={"Destroy tagged object","Stage flag set (complete)","Stage flag set (fail)",
        "Collect tagged object","Deposit tagged object","Photograph tagged object","No condition",
        "Enter room containing pad","Use weapon in room containing pad","Copy GoldenEye key"};
    return type>=25 && type<=34 ? names[type-25] : "Unknown condition";
}
static BriefingText *TextAt(BriefingDocument *d,int i)
{ return i<4 ? &d->menu[i] : i<9 ? &d->watch[i-4] : (i&1) ? &d->objectives[(i-9)/2].menu : &d->objectives[(i-9)/2].watch; }
static const char *Resolve(const BriefingDocument *d,const TextBank *bank,const BriefingText *t)
{ return t->custom ? t->text : (t->id>>10)==d->bank ? TextBankString(bank,t->id&1023) : NULL; }
static BOOL CheckText(const char *s,BOOL objective,BOOL watch,const char **why)
{
    size_t word=0,lines=0,n;
    if (!s) { return Fail(why,"A briefing references an unused or different text bank slot."); }
    n=strlen(s);
    if (n>=BRIEFING_TEXT_SIZE || (objective && watch && n>120))
    { return Fail(why,"Briefing text is limited to 1499 bytes; watch objectives to 120 bytes."); }
    for (const unsigned char *p=(const unsigned char *)s;*p;p++)
    {
        if (*p!='\n' && (*p<32 || *p>126)) { return Fail(why,"Briefings support English ASCII and newlines only."); }
        if (*p=='\n') { lines++; }
        if (*p>' ') { if (++word>31) { return Fail(why,"The game's text wrapper supports at most 31 characters in a word. Add a space or newline."); } }
        else { word=0; }
    }
    if (objective && watch && (lines>2 || !n || s[n-1]!='\n'))
    { return Fail(why,"Watch objectives need a trailing newline and at most two lines."); }
    return TRUE;
}
BOOL BriefingValidate(const BriefingDocument *d,const TextBank *bank,const char **why)
{
    const char *bn,*br;
    if (!BriefingNames(d->level,&bn,&br) || strcmp(bn,d->bankname) || strcmp(br,d->briefname) || !d->bank || d->bank>=64)
    { return Fail(why,"Invalid briefing level or bank."); }
    for (int i=0;i<21;i++)
    {
        const BriefingText *t=TextAt((BriefingDocument *)d,i);
        if (t->custom>1 || !memchr(t->text,0,sizeof(t->text))) { return Fail(why,"Invalid saved briefing text."); }
        if (i>=9 && !d->objectives[(i-9)/2].enabled) { continue; }
        if (!CheckText(Resolve(d,bank,t),i>=9,i>=9 && !(i&1),why)) { return FALSE; }
    }
    for (int i=0;i<6;i++)
    {
        const BriefingObjective *o=&d->objectives[i];
        if (o->enabled>1 || o->menuDifficulty>3 || o->watchDifficulty>3 || o->count>BRIEFING_CONDITIONS)
        { return Fail(why,"Invalid objective settings."); }
        if (o->enabled && !o->count) { return Fail(why,"Enabled objectives need at least one completion or failure condition."); }
        for (DWORD j=0;j<o->count;j++) if (!BriefingConditionWords(o->conditions[j].words[0]&255))
        { return Fail(why,"Unsupported objective condition."); }
    }
    *why="";return TRUE;
}
BOOL BriefingValidateSetup(const BriefingDocument *d,const SetupFile *setup,const char **why)
{
    for (int i=0;i<6;i++) if (d->objectives[i].enabled)
    {
        const BriefingObjective *o=&d->objectives[i];
        for (DWORD j=0;j<o->count;j++)
        {
            const BriefingCondition *c=&o->conditions[j];DWORD type=c->words[0]&255;
            if (type==32 || type==33)
            {
                DWORD pad=c->words[type==32 ? 1 : 2];
                if (pad<10000 ? pad>=setup->padcount || setup->pads[pad].deleted
                    : pad-10000>=setup->boundpadcount || setup->boundpads[pad-10000].pad.deleted || setup->boundpads[pad-10000].pad.occluder)
                { return Fail(why,"An objective condition references a missing, deleted, or occluder pad."); }
            }
        }
    }
    return TRUE;
}
/* Fixed, explicitly endian-encoded records; never serialize host padding. */
#define BRIEF_META_SIZE (188u + 21u*(8u+BRIEFING_TEXT_SIZE) + 6u*(20u+BRIEFING_CONDITIONS*20u))
BOOL BriefingEncode(const BriefingDocument *d,unsigned char **out,DWORD *size,const char **why)
{
    unsigned char *p=calloc(BRIEF_META_SIZE,1);DWORD at=188;
    *out=NULL;*size=0;if (!p) { return Fail(why,"Out of memory saving briefing edits."); }
    memcpy(p,"GBR1",4);W(p+4,d->level);W(p+8,d->bank);memcpy(p+12,d->bankname,64);memcpy(p+76,d->briefname,64);memcpy(p+140,d->baseline,48);
    for (int i=0;i<21;i++) { const BriefingText *t=TextAt((BriefingDocument *)d,i);W(p+at,t->id);W(p+at+4,t->custom);memcpy(p+at+8,t->text,BRIEFING_TEXT_SIZE);at+=8+BRIEFING_TEXT_SIZE; }
    for (int i=0;i<6;i++)
    {
        const BriefingObjective *o=&d->objectives[i];W(p+at,o->enabled);W(p+at+4,o->menuDifficulty);W(p+at+8,o->watchDifficulty);W(p+at+12,o->flags);W(p+at+16,o->count);at+=20;
        for (int j=0;j<BRIEFING_CONDITIONS;j++) for (int k=0;k<5;k++,at+=4) { W(p+at,o->conditions[j].words[k]); }
    }
    *out=p;*size=at;return TRUE;
}
BOOL BriefingDecode(const unsigned char *p,DWORD size,BriefingDocument *d,const char **why)
{
    DWORD at=188;memset(d,0,sizeof(*d));
    if (!p || size!=BRIEF_META_SIZE || memcmp(p,"GBR1",4) || !memchr(p+12,0,64) || !memchr(p+76,0,64))
    { return Fail(why,"The saved briefing metadata is invalid."); }
    d->level=R(p+4);d->bank=R(p+8);memcpy(d->bankname,p+12,64);memcpy(d->briefname,p+76,64);memcpy(d->baseline,p+140,48);
    for (int i=0;i<21;i++) { BriefingText *t=TextAt(d,i);t->id=R(p+at);t->custom=R(p+at+4);memcpy(t->text,p+at+8,BRIEFING_TEXT_SIZE);at+=8+BRIEFING_TEXT_SIZE;if (!memchr(t->text,0,BRIEFING_TEXT_SIZE)) { return Fail(why,"Unterminated saved briefing text."); } }
    for (int i=0;i<6;i++)
    {
        BriefingObjective *o=&d->objectives[i];o->enabled=R(p+at);o->menuDifficulty=R(p+at+4);o->watchDifficulty=R(p+at+8);o->flags=R(p+at+12);o->count=R(p+at+16);at+=20;
        if (o->count>BRIEFING_CONDITIONS) { return Fail(why,"Too many objective conditions."); }
        for (int j=0;j<BRIEFING_CONDITIONS;j++) for (int k=0;k<5;k++,at+=4) { o->conditions[j].words[k]=R(p+at); }
    }
    return TRUE;
}
static BOOL Bounds(const unsigned char *p,DWORD size,DWORD at,DWORD *bytes,const char **why)
{
    if (at<40 || at>size || size-at<4) { return Fail(why,"Invalid setup command list."); }
    *bytes=SetupObjectWordCount(p[at+3])*4;
    return *bytes<=size-at || Fail(why,"Truncated setup command.");
}
BOOL BriefingLoad(const GEditorProject *project,DWORD level,const SetupFile *setup,BriefingDocument *d,TextBank *bank,const char **why)
{
    RomFile rom={0};char path[MAX_PATH];const char *bn,*br;DWORD offset,span,count,bankid;TextBankFile files[64];BOOL ok=FALSE;
    memset(d,0,sizeof(*d));memset(bank,0,sizeof(*bank));
    if (!project || level>=project->levelcount || !setup || !setup->data || setup->size<40
        || !BriefingNames(project->levels[level].levelID,&bn,&br)) { return Fail(why,"Briefings are available for single-player missions."); }
    if (!EditorPathJoin(path,sizeof(path),project->dir,ROM_EXPORT_BASE_FILENAME) || !RomLoad(path,&rom,why)) { return FALSE; }
    d->level=project->levels[level].levelID;strcpy(d->bankname,bn);strcpy(d->briefname,br);
    if (!TextBankCatalog(&rom,files,&count,why)) { goto done; }
    for (DWORD i=0;i<count;i++) if (!strcmp(files[i].name,bn)) { d->bank=files[i].id;break; }
    if (d->bank==TEXT_BANK_UNKNOWN_ID || !d->bank) { Fail(why,"This ROM needs the TXBK catalog to edit briefings.");goto done; }
    if (!TextBankLoadProject(project->dir,&rom,bn,bank,why) || !RomFindFile(&rom,br,&offset,&span,why)) { goto done; }
    if (span<48) { Fail(why,"The menu briefing file is incomplete.");goto done; }
    bankid=d->bank;
    if (setup->briefmetasize)
    {
        if (!BriefingDecode(setup->briefmeta,setup->briefmetasize,d,why) || d->level!=(DWORD)project->levels[level].levelID || d->bank!=bankid || strcmp(d->bankname,bn) || strcmp(d->briefname,br))
        { Fail(why,"Saved briefing does not match this level.");goto done; }
    }
    else
    {
        memcpy(d->baseline,rom.data+offset,48);
        for (int i=0;i<4;i++) { d->menu[i].id=R16(d->baseline+i*2); }
        for (int i=0;i<10;i++)
        {
            DWORD id=R16(d->baseline+8+i*4);
            if (i>=6 && id) { Fail(why,"This mission has more than six objectives; its existing data was kept.");goto done; }
            if (i<6) { d->objectives[i].menu.id=id;d->objectives[i].menuDifficulty=id ? R16(d->baseline+10+i*4) : 0; }
        }
        int current=-1;
        for (DWORD at=R(setup->data+12),commands=0;at && commands<65536;commands++)
        {
            DWORD bytes,type; if (!Bounds(setup->data,setup->size,at,&bytes,why)) { goto done; }
            type=setup->data[at+3];if (type==48) { break; }
            if (type==35)
            { DWORD page=R(setup->data+at+4);if (page<5) { d->watch[page].id=R(setup->data+at+8)&65535; } }
            else if (type==23)
            {
                DWORD id=R(setup->data+at+4);
                if (id>=6) { Fail(why,"This mission uses an objective number outside the six supported slots.");goto done; }
                current=(int)id;BriefingObjective *o=&d->objectives[id];o->enabled=R(setup->data+at+12)<=3;o->watch.id=R(setup->data+at+8)&65535;
                o->flags=R(setup->data+at+12)&0xffffff00u;o->watchDifficulty=setup->data[at+15];o->count=0;
            }
            else if (type==24) { current=-1; }
            else if (current>=0)
            {
                BriefingObjective *o=&d->objectives[current];
                if (!BriefingConditionWords(type) || o->count==BRIEFING_CONDITIONS) { Fail(why,"Unsupported or oversized objective condition list.");goto done; }
                BriefingCondition *c=&o->conditions[o->count++];for (DWORD k=0;k<bytes/4;k++) { c->words[k]=R(setup->data+at+k*4); }
            }
            at+=bytes;
        }
        for (int i=0;i<6;i++) if (!d->objectives[i].enabled)
        {
            BriefingObjective *o=&d->objectives[i];o->menu.custom=o->watch.custom=TRUE;
            o->watchDifficulty=0;strcpy(o->menu.text,"New objective\n");strcpy(o->watch.text,"new objective\n");
        }
    }
    for (int i=0;i<21;i++)
    {
        BriefingText *t=TextAt(d,i);
        if (!t->custom)
        {
            const char *s=Resolve(d,bank,t);
            if (!s || strlen(s)>=sizeof(t->text)) { Fail(why,"A briefing string is missing or exceeds the editor's supported length.");goto done; }
            strcpy(t->text,s);
        }
    }
    ok=TRUE;*why="";
done:
    RomFree(&rom);if (!ok) { TextBankFree(bank); }return ok;
}

/* Rewrite only at export. Remove old briefing records, append authored ones,
 * and remap every native command-index reference before compacting the asset.
 * The editor's live setup, selection, and AI instruction IDs stay unchanged. */
BOOL BriefingRewriteSetup(const unsigned char *src,DWORD size,const BriefingDocument *d,const DWORD ids[21],unsigned char **out,DWORD *outsize,const char **why)
{
    DWORD *offsets=NULL,*map=NULL,count=0,kept=0,at,bytes,start,extra=5*16+4;unsigned char *data=NULL;BOOL ok=FALSE;
    *out=NULL;*outsize=0;if (!src || size<40) { return Fail(why,"Invalid setup for briefing export."); }
    offsets=malloc(65536*sizeof(*offsets));map=malloc(65536*sizeof(*map));
    if (!offsets || !map) { Fail(why,"Out of memory building objective references.");goto done; }
    start=R(src+12);at=start;
    for (;count<65536;count++)
    {
        if (!Bounds(src,size,at,&bytes,why)) { goto done; }
        offsets[count]=at;DWORD type=src[at+3];map[count]=(type>=23 && type<=35) ? (DWORD)-1 : kept++;
        if (type==48) { break; }at+=bytes;
    }
    if (count==65536) { Fail(why,"Unterminated setup command list.");goto done; }
    int last=-1;for (int i=0;i<6;i++) if (d->objectives[i].enabled) { last=i; }
    for (int i=0;i<=last;i++)
    { extra+=20;for (DWORD j=0;j<d->objectives[i].count;j++) { extra+=BriefingConditionWords(d->objectives[i].conditions[j].words[0]&255)*4; } }
    if (size>16u*1024*1024 || (at-start)>16u*1024*1024-size || extra>16u*1024*1024-size-(at-start)) { Fail(why,"Setup is too large for briefing export.");goto done; }
    data=malloc(size+(at-start)+extra);if (!data) { Fail(why,"Out of memory compiling objectives.");goto done; }
    memcpy(data,src,size);at=size;W(data+12,at);
    for (DWORD i=0;i<count;i++) if (map[i]!=(DWORD)-1)
    {
        DWORD pos=offsets[i],type=src[pos+3],fields=0;bytes=SetupObjectWordCount((unsigned char)type)*4;
        memcpy(data+at,src+pos,bytes);
        if (type==PROPDEF_LINK || type==PROPDEF_SWITCH || type==PROPDEF_LOCK_DOOR) { fields=2; }
        if (type==PROPDEF_SAFE_ITEM) { fields=3; }if (type==PROPDEF_RENAME) { fields=1; }
        DWORD positions[6],widths[6],n=0;
        for (DWORD j=0;j<fields;j++) { positions[n]=4+j*4;widths[n++]=4; }
        if (type==PROPDEF_TAG) { positions[n]=6;widths[n++]=2; }
        if ((type==PROPDEF_DOOR || type==PROPDEF_MONITOR) && R(src+pos+(type==PROPDEF_DOOR ? 128 : 244)))
        { positions[n]=type==PROPDEF_DOOR ? 128 : 244;widths[n++]=4; }
        /* The native inside-object flag is defined for standard object records. */
        if (SetupTypeCreatesObject((unsigned char)type) && (R(src+pos+8)&PROPFLAG_INSIDEANOTHEROBJ)) { positions[n]=6;widths[n++]=2; }
        for (DWORD j=0;j<n;j++)
        {
            DWORD off=positions[j];long long target=(long long)i+(widths[j]==2 ? (long long)(short)R16(src+pos+off) : (long long)(LONG)R(src+pos+off));
            if (target<0 || target>=count || map[target]==(DWORD)-1) { Fail(why,"A setup link targets a removed briefing/objective record.");goto done; }
            LONG delta=(LONG)map[target]-(LONG)map[i];
            if (widths[j]==2) { if (delta< -32768 || delta>32767) { Fail(why,"A setup link no longer fits its native offset.");goto done; } W16(data+at+off,delta); }
            else { W(data+at+off,(DWORD)delta); }
        }
        if (type==PROPDEF_DOOR_SHADOW)
        {
            DWORD target=R(src+pos+DOOR_SHADOW_DOOR);
            if (target>=count || map[target]==(DWORD)-1) { Fail(why,"Invalid door shadow link during briefing export.");goto done; }
            W(data+at+DOOR_SHADOW_DOOR,map[target]);
        }
        at+=bytes;
    }
    for (int i=0;i<5;i++) { W(data+at,35);W(data+at+4,i);W(data+at+8,ids[4+i]);W(data+at+12,0);at+=16; }
    for (int i=0;i<=last;i++)
    {
        /* A hidden, already-complete placeholder avoids incomplete gaps in
         * objectiveIsAllComplete while retaining stable script objective IDs. */
        if (!d->objectives[i].enabled)
        { W(data+at,23);W(data+at+4,i);W(data+at+8,0);W(data+at+12,4);W(data+at+16,24);at+=20;continue; }
        const BriefingObjective *o=&d->objectives[i];W(data+at,23);W(data+at+4,i);W(data+at+8,ids[10+i*2]);W(data+at+12,o->flags|o->watchDifficulty);at+=16;
        for (DWORD j=0;j<o->count;j++)
        {
            const BriefingCondition *c=&o->conditions[j];DWORD type=c->words[0]&255,n=BriefingConditionWords(type);
            for (DWORD k=0;k<n;k++,at+=4)
            { DWORD v=c->words[k];if ((type==30 || type==32) && k>=2) { v=0; }if (type==33 && k>=3) { v=0; }W(data+at,v); }
        }
        W(data+at,24);at+=4;
    }
    W(data+at,48);at+=4;ok=SetupCompactNative(data,at,out,outsize,why);
done:
    free(offsets);free(map);free(data);return ok;
}
static BOOL BuildBank(const BriefingDocument *d,const TextBank *bank,TextBank *out,DWORD ids[21],const char **why)
{
    DWORD custom=0,count,delta,size;const char *strings[21];
    for (int i=0;i<21;i++)
    {
        const BriefingText *t=TextAt((BriefingDocument *)d,i);ids[i]=t->id;
        if (i>=9 && !d->objectives[(i-9)/2].enabled) { ids[i]=0;continue; }
        if (t->custom) { ids[i]=(d->bank<<10)|(bank->count+custom);strings[custom++]=t->text; }
    }
    count=(bank->count+custom+3)&~3u;
    if (count>1024) { return Fail(why,"This text bank has no room for the briefing's custom strings."); }
    delta=(count-bank->count)*4;size=bank->size+delta;
    for (DWORD i=0;i<custom;i++) { size+=(DWORD)(strlen(strings[i])+4)&~3u; }size=(size+15)&~15u;
    if (size>TEXT_BANK_MAX_SIZE) { return Fail(why,"Briefing text exceeds the native bank capacity."); }
    memset(out,0,sizeof(*out));out->data=calloc(size,1);if (!out->data) { return Fail(why,"Out of memory compiling briefing text."); }
    out->size=size;out->count=count;memcpy(out->data+count*4,bank->data+bank->count*4,bank->size-bank->count*4);
    for (DWORD i=0;i<bank->count;i++) { DWORD off=R(bank->data+i*4);W(out->data+i*4,off ? off+delta : 0); }
    DWORD at=bank->size+delta;
    for (DWORD i=0;i<custom;i++) { W(out->data+(bank->count+i)*4,at);strcpy((char *)out->data+at,strings[i]);at+=(DWORD)(strlen(strings[i])+4)&~3u; }
    return TRUE;
}
void BriefingFreeResources(BriefingResource *r,DWORD count)
{ for (DWORD i=0;i<count;i++) { free(r[i].data); }free(r); }
BOOL BriefingCollect(const GEditorProject *project,const RomFile *rom,const RomFile *target,BriefingResource **out,DWORD *count,const char **why)
{
    BriefingResource *r=calloc(project->levelcount*3+1,sizeof(*r));DWORD used=0;BOOL ok=FALSE;
    TextBankFile catalog[TEXT_BANK_MAX_FILES];DWORD catalogcount=0;BOOL catalogread=FALSE;
    static char error[384];const char *levelname="";
    *out=NULL;*count=0;if (!r) { return Fail(why,"Out of memory compiling briefings."); }
    for (DWORD l=0;l<project->levelcount;l++)
    {
        const char *bn,*br;char path[MAX_PATH];SetupFile setup={0};BriefingDocument *d=NULL;TextBank bank={0},built={0};
        DWORD ids[21],offset,span,nativeSize;unsigned char *runtime=NULL;BOOL next=FALSE;
        if (!BriefingNames(project->levels[l].levelID,&bn,&br)) { continue; }
        if (RomExportProjectResourcePath(project,project->levels[l].setupname,path,sizeof(path))!=1) { continue; }
        if (GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES) { continue; }
        if (!SetupLoadProjectFile(project->dir,project->levels[l].setupname,&setup,why)) { goto done; }
        if (!setup.briefmetasize) { SetupFileFree(&setup);continue; }
        levelname=project->levels[l].name;
        if (!catalogread) { if (!TextBankCatalog(rom,catalog,&catalogcount,why)) { SetupFileFree(&setup);goto done; }catalogread=TRUE; }
        d=malloc(sizeof(*d));if (!d) { Fail(why,"Out of memory reading briefings.");goto leveldone; }
        if (!BriefingDecode(setup.briefmeta,setup.briefmetasize,d,why)) { goto leveldone; }
        if (d->level!=(DWORD)project->levels[l].levelID) { Fail(why,"Saved briefing does not match this level.");goto leveldone; }
        DWORD bankid=TEXT_BANK_UNKNOWN_ID,originalcount;
        for (DWORD b=0;b<catalogcount;b++) if (!strcmp(catalog[b].name,bn)) { bankid=catalog[b].id; }
        if (bankid!=d->bank) { Fail(why,"The saved briefing text bank ID does not match this ROM.");goto leveldone; }
        if (!RomFindFile(rom,bn,&offset,&span,why) || !TextBankValidate(rom->data+offset,span,&originalcount,why)) { goto leveldone; }
        if (!TextBankLoadProject(project->dir,rom,bn,&bank,why)) { goto leveldone; }
        if (bank.count!=originalcount) { Fail(why,"The saved text file must preserve the original string slots.");goto leveldone; }
        if (!BriefingValidate(d,&bank,why) || !BriefingValidateSetup(d,&setup,why)
            || !BuildBank(d,&bank,&built,ids,why)) { goto leveldone; }
        strcpy(r[used].name,bn);r[used].data=built.data;r[used++].size=built.size;built.data=NULL;
        if (!RomFindFile(rom,br,&offset,&span,why)) { goto leveldone; }
        if (span<48) { Fail(why,"Incomplete native menu briefing.");goto leveldone; }
        strcpy(r[used].name,br);r[used].data=calloc(48,1);r[used].size=48;if (!r[used].data) { Fail(why,"Out of memory compiling menu briefing.");goto leveldone; }
        unsigned char *brief=r[used++].data;
        for (int i=0;i<4;i++) { W16(brief+i*2,ids[i]); }
        for (int i=0;i<6;i++) if (d->objectives[i].enabled) { W16(brief+8+i*4,ids[9+i*2]);W16(brief+10+i*4,d->objectives[i].menuDifficulty); }
        if (target)
        {
            if (!RomFindFile(target,br,&offset,&span,why)) { goto leveldone; }
            if (span<48) { Fail(why,"Incomplete target menu briefing.");goto leveldone; }
            if (memcmp(target->data+offset,d->baseline,48) && memcmp(target->data+offset,brief,48))
            { Fail(why,"The new ROM changed a menu briefing edited in this project. Review its briefing definitions before rebasing.");goto leveldone; }
        }
        if (!ActionSetupBuildRuntime(&setup,&runtime,&nativeSize,why)) { goto leveldone; }
        strcpy(r[used].name,setup.name);
        if (!BriefingRewriteSetup(runtime ? runtime : setup.data,runtime ? nativeSize : setup.size,d,ids,&r[used].data,&r[used].size,why)) { goto leveldone; }
        used++;next=TRUE;
leveldone:
        free(runtime);TextBankFree(&bank);TextBankFree(&built);free(d);SetupFileFree(&setup);if (!next) { goto done; }
    }
    *out=r;*count=used;ok=TRUE;
done:
    if (!ok) { snprintf(error,sizeof(error),"%s%s%s",levelname,*levelname ? ": " : "",*why);*why=error;BriefingFreeResources(r,used); }return ok;
}
