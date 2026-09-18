#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include "actionblocks.h"
#include "setupmeta.h"

#define ACTION_MAX_BLOCKS 4096u
#define ACTION_MAX_INSTRUCTIONS 65536u
#define ACTION_MAX_BYTES 65536u /* Native saved instruction offsets are u16. */
#define ACTION_SETUP_MAX (16u * 1024u * 1024u)
#define ACTION_META_RECORD (12u + ACTION_NAME_SIZE + ACTION_NOTE_SIZE)
const ActionOpcode g_ActionOpcodes[ACTION_OPCODE_COUNT] = {
#include "actionopcodes.inc"
};
static BOOL Fail(const char **why, const char *text) { *why=text; return FALSE; }
static void Text(char *to, size_t n, const char *from)
{ if (n) { snprintf(to,n,"%s",from ? from : ""); } }
static DWORD Read(const unsigned char *p, unsigned int n)
{ DWORD value=0; while (n--) { value=(value<<8)|*p++; } return value; }
static void Write(unsigned char *p, unsigned int n, DWORD value)
{ while (n) { p[--n]=(unsigned char)value; value>>=8; } }
static void FreeBlock(ActionBlock *b)
{
    for (DWORD i=0;i<b->count;i++) { free(b->instructions[i].bytes); }
    free(b->instructions); memset(b,0,sizeof(*b));
}
void ActionDocumentFree(ActionDocument *d)
{
    for (DWORD i=0;i<d->count;i++) { FreeBlock(&d->blocks[i]); }
    free(d->blocks); free(d->assignments); memset(d,0,sizeof(*d));
}
DWORD ActionFindInstruction(const ActionBlock *b, DWORD uid)
{
    for (DWORD i=0;i<b->count;i++) { if (b->instructions[i].uid==uid) { return i; } }
    return ACTION_MISSING_TARGET;
}
int ActionTargetParameter(unsigned int op)
{
    if (op>=ACTION_OPCODE_COUNT) { return -1; }
    for (int i=0;i<g_ActionOpcodes[op].paramcount;i++)
    { if (g_ActionOpcodes[op].params[i].kind==ACTION_LABEL) { return i; } }
    return -1;
}
DWORD ActionReadValue(const ActionInstruction *ins, unsigned int p)
{
    const ActionOpcode *op=&g_ActionOpcodes[ins->bytes[0]];
    if (p>=op->paramcount) { return 0; }
    return Read(ins->bytes+op->params[p].offset,op->params[p].width);
}
DWORD ActionFindTarget(const ActionBlock *b, DWORD instruction)
{
    const ActionInstruction *ins=&b->instructions[instruction];
    int p=ActionTargetParameter(ins->bytes[0]);
    DWORD label;
    if (p<0) { return 0; }
    label=ActionReadValue(ins,p);
    for (DWORD i=ins->bytes[0]==1 ? 0 : instruction;i<b->count;i++)
    {
        if (b->instructions[i].bytes[0]==4) { break; }
        if (b->instructions[i].bytes[0]==2 && b->instructions[i].bytes[1]==label)
        { return b->instructions[i].uid; }
    }
    return ACTION_MISSING_TARGET;
}
static BOOL Decode(ActionDocument *d, ActionBlock *b, const unsigned char *data,
                   DWORD available, const char **why)
{
    DWORD at=0,capacity=0;
    while (at<available && b->count<ACTION_MAX_INSTRUCTIONS && at<ACTION_MAX_BYTES)
    {
        DWORD n; unsigned int op=data[at];
        ActionInstruction *ins;
        if (op>=ACTION_OPCODE_COUNT) { return Fail(why,"Unknown Action Block opcode; the original setup has not been changed."); }
        n=g_ActionOpcodes[op].size;
        if (op==0xad)
        {
            const unsigned char *end=memchr(data+at+1,0,available-at-1);
            if (!end) { return Fail(why,"Unterminated debug-text instruction."); }
            n=(DWORD)(end-data)-at+1;
        }
        if (!n || n>available-at || n>ACTION_MAX_BYTES-at)
        { return Fail(why,"Truncated or oversized Action Block instruction."); }
        if (b->count==capacity)
        {
            ActionInstruction *grown;
            capacity=capacity ? capacity*2 : 32;
            grown=realloc(b->instructions,(size_t)capacity*sizeof(*grown));
            if (!grown) { return Fail(why,"Out of memory decoding Action Blocks."); }
            b->instructions=grown;
        }
        ins=&b->instructions[b->count]; memset(ins,0,sizeof(*ins));
        ins->bytes=malloc(n);
        if (!ins->bytes) { return Fail(why,"Out of memory decoding an instruction."); }
        memcpy(ins->bytes,data+at,n); ins->size=n; ins->uid=d->nextuid++; b->count++;
        at+=n;
        if (op==4)
        {
            b->sourcesize=at;
            for (DWORD i=0;i<b->count;i++) { b->instructions[i].target=ActionFindTarget(b,i); }
            return TRUE;
        }
    }
    return Fail(why,"The Action Block has no bounded End of block instruction.");
}
static DWORD BlockBytes(const ActionBlock *b)
{
    DWORD size=0;
    for (DWORD i=0;i<b->count;i++) { size+=b->instructions[i].size; }
    return size;
}
static BOOL AppendBlock(ActionDocument *d, DWORD id, BOOL global, ActionBlock **out, const char **why)
{
    ActionBlock *grown;
    if (d->count>=ACTION_MAX_BLOCKS) { return Fail(why,"Too many Action Blocks."); }
    for (DWORD i=0;global && i<d->count;i++)
    { if (d->blocks[i].id==id) { return Fail(why,"Duplicate Action Block ID."); } }
    grown=realloc(d->blocks,(size_t)(d->count+1)*sizeof(*grown));
    if (!grown) { return Fail(why,"Out of memory adding an Action Block."); }
    d->blocks=grown; *out=&d->blocks[d->count++]; memset(*out,0,sizeof(**out));
    (*out)->id=id; (*out)->global=global; return TRUE;
}
static BOOL LoadNames(ActionDocument *d, const SetupFile *setup, const char **why)
{
    const unsigned char *p=setup->actionmeta; DWORD size=setup->actionmetasize;
    if (!size) { return TRUE; }
    if (!p || size<8 || (memcmp(p,"AIN1",4) && memcmp(p,"AIN2",4)) || Read(p+4,4)!=(size-8)/ACTION_META_RECORD
        || (size-8)%ACTION_META_RECORD)
    { return Fail(why,"The saved Action Block names are damaged."); }
    for (DWORD at=8;at<size;at+=ACTION_META_RECORD)
    {
        DWORD blockindex=Read(p+at,4), index=Read(p+at+4,4), opcode=Read(p+at+8,4);
        const char *name=(const char *)p+at+12, *note=name+ACTION_NAME_SIZE;
        if (!memchr(name,0,ACTION_NAME_SIZE) || !memchr(note,0,ACTION_NOTE_SIZE))
        { return Fail(why,"An Action Block name or note is unterminated."); }
        if (blockindex>=d->count) { return Fail(why,"Saved Action Block notes refer to a missing table entry."); }
        {
            ActionBlock *b=&d->blocks[blockindex];
            if (index==ACTION_MISSING_TARGET)
            {
                /* AIN2 uses the block record's formerly reserved opcode word
                 * for flags. AIN1 remains the exact enabled-only format. */
                if (opcode>(p[3]=='2' ? 1u : 0u)) { return Fail(why,"Unknown saved Action Block flags."); }
                Text(b->name,sizeof(b->name),name); b->disabled=(opcode&1)!=0;
            }
            else if (index<b->count && opcode==b->instructions[index].bytes[0])
            {
                Text(b->instructions[index].name,ACTION_NAME_SIZE,name);
                Text(b->instructions[index].note,ACTION_NOTE_SIZE,note);
            }
            else { return Fail(why,"Saved Action Block notes do not match their instructions."); }
        }
    }
    return TRUE;
}
BOOL ActionDocumentLoad(const SetupFile *s, ActionDocument *out, const char **why)
{
    DWORD table,at; BOOL ended=FALSE;
    memset(out,0,sizeof(*out)); out->nextuid=1;
    if (!s || !s->data || s->size<40 || s->size>ACTION_SETUP_MAX)
    { return Fail(why,"No valid level setup is loaded."); }
    table=Read(s->data+20,4);
    if (table && (table<40 || table>s->size || (table&3))) { goto badtable; }
    if (table)
    {
        for (at=table;at<=s->size && s->size-at>=8;at+=8)
        {
            ActionBlock *b; DWORD offset=Read(s->data+at,4),id=Read(s->data+at+4,4);
            if (!offset) { ended=TRUE; break; }
            if (offset<40 || offset>=s->size || id<=0x400 || id>0xffff) { goto badtable; }
            if (!AppendBlock(out,id,FALSE,&b,why)) { goto fail; }
            b->sourceoffset=offset;
            if (!Decode(out,b,s->data+offset,s->size-offset,why)) { goto fail; }
        }
        if (!ended) { goto badtable; }
    }
    if (s->charactercount)
    {
        if (!s->characters || s->charactercount>65536) { goto badtable; }
        out->assignments=malloc((size_t)s->charactercount*sizeof(*out->assignments));
        if (!out->assignments) { Fail(why,"Out of memory reading character behaviors."); goto fail; }
        out->charactercount=s->charactercount;
        for (DWORD i=0;i<s->charactercount;i++) { out->assignments[i]=s->characters[i].ailistid; }
    }
    if (!LoadNames(out,s,why)) { goto fail; }
    return TRUE;
badtable:
    Fail(why,"The setup's Action Block table is invalid.");
fail:
    ActionDocumentFree(out); return FALSE;
}
static const RomManifestEntry *Entry(const RomFile *r, DWORD kind)
{
    const RomManifestEntry *found=NULL;
    for (DWORD i=0;i<r->info.entrycount && i<ROM_MAX_ENTRIES;i++)
    { if (r->info.entries[i].kind==kind) { if (found) { return NULL; } found=&r->info.entries[i]; } }
    return found;
}
static BOOL Map(const RomFile *r, const RomManifestEntry *map, DWORD ptr, DWORD size, DWORD *offset)
{
    if (!map || map->romstart>map->romend || map->romend>r->size || ptr<map->flags
        || ptr-map->flags>map->romend-map->romstart
        || size>map->romend-map->romstart-(ptr-map->flags)) { return FALSE; }
    *offset=map->romstart+(ptr-map->flags); return TRUE;
}
BOOL ActionParameterIsPad(const ActionInstruction *ins, unsigned int parameter)
{
    const ActionOpcode *op = &g_ActionOpcodes[ins->bytes[0]];
    if (parameter >= op->paramcount) { return FALSE; }
    /* Aim/facing commands pack a pad OR character target in the same
     * operand. Character targeting (bit 4) takes priority over pad (bit 8). */
    return op->params[parameter].kind == ACTION_PAD
        || (ins->bytes[0] >= 0x14 && ins->bytes[0] <= 0x17 && parameter == 1
            && (ActionReadValue(ins, 0) & 12) == 8);
}

BOOL ActionDocumentLoadGlobals(ActionDocument *d, const RomFile *r, const char **why)
{
    const RomManifestEntry *catalog=Entry(r,0x4149474c), *map=Entry(r,0x434d4150);
    static const char *const names[]={"Aim at Bond","Inactive","Standard guard","Idle animations",
        "Use keyboard","Simple deaf guard","Attack Bond","Simple guard","Run to Bond",
        "Raise alarm","Startle and run to Bond","Send clone or run to Bond","Standard clone",
        "Persistent chase and attack","Wait one second","End level","Draw pistol and attack","Remove character"};
    DWORD before=d->count,table,count; ActionBlock *b;
    if (!catalog) { return TRUE; } /* Optional catalog in older project base ROMs. */
    if (!r->data || catalog->romstart>r->size || r->size-catalog->romstart<16
        || !map || catalog->romstart<map->romstart || catalog->romstart>map->romend
        || map->romend-catalog->romstart<16) { goto bad; }
    count=Read(r->data+catalog->romstart+4,4);
    if (count>1025 || Read(r->data+catalog->romstart+8,4)!=8
        || Read(r->data+catalog->romstart+12,4)!=1
        || !Map(r,map,Read(r->data+catalog->romstart,4),count*8,&table)) { goto bad; }
    for (DWORD i=0;i<count;i++)
    {
        DWORD offset,id=Read(r->data+table+i*8+4,4);
        if (id>0x400 || !Map(r,map,Read(r->data+table+i*8,4),1,&offset)) { goto bad; }
        if (!AppendBlock(d,id,TRUE,&b,why)) { goto rollback; }
        if (id<sizeof(names)/sizeof(*names)) { Text(b->name,sizeof(b->name),names[id]); }
        if (!Decode(d,b,r->data+offset,map->romend-offset,why)) { goto rollback; }
    }
    d->globalsloaded=TRUE;
    return TRUE;
bad:
    Fail(why,"The ROM's shared Action Block catalog is invalid.");
rollback:
    while (d->count>before) { FreeBlock(&d->blocks[--d->count]); }
    return FALSE;
}
BOOL ActionDocumentClone(const ActionDocument *s, ActionDocument *out, const char **why)
{
    memset(out,0,sizeof(*out)); out->nextuid=s->nextuid; out->changed=s->changed; out->globalsloaded=s->globalsloaded;
    if (s->charactercount)
    {
        out->assignments=malloc((size_t)s->charactercount*sizeof(*out->assignments));
        if (!out->assignments) { goto fail; }
        memcpy(out->assignments,s->assignments,(size_t)s->charactercount*sizeof(*out->assignments));
        out->charactercount=s->charactercount;
    }
    for (DWORD i=0;i<s->count;i++)
    {
        ActionBlock *b;
        if (!AppendBlock(out,s->blocks[i].id,s->blocks[i].global,&b,why)) { goto fail; }
        *b=s->blocks[i]; b->instructions=NULL; b->count=0;
        if (s->blocks[i].count)
        {
            b->instructions=calloc(s->blocks[i].count,sizeof(*b->instructions));
            if (!b->instructions) { goto fail; }
        }
        for (DWORD j=0;j<s->blocks[i].count;j++)
        {
            ActionInstruction *ins=&b->instructions[j];
            *ins=s->blocks[i].instructions[j]; ins->bytes=malloc(ins->size);
            if (!ins->bytes) { goto fail; }
            memcpy(ins->bytes,s->blocks[i].instructions[j].bytes,ins->size); b->count++;
        }
    }
    return TRUE;
fail:
    ActionDocumentFree(out); return Fail(why,"Out of memory copying Action Blocks.");
}
static BOOL Editable(ActionDocument *d, DWORD b, const char **why)
{
    return b<d->count && !d->blocks[b].global
        ? TRUE : Fail(why,"Duplicate a shared block into this level before editing it.");
}
BOOL ActionDocumentSetEnabled(ActionDocument *d, DWORD index, BOOL enabled, const char **why)
{
    if (!Editable(d,index,why)) { return FALSE; }
    if (d->blocks[index].disabled!=!enabled)
    {
        d->blocks[index].disabled=!enabled;
        d->changed=TRUE; /* Metadata only; do not rewrite the original script. */
    }
    return TRUE;
}
BOOL ActionDocumentAddBlock(ActionDocument *d, DWORD source, BOOL background, DWORD *out, const char **why)
{
    unsigned char idle[]={2,0,3,1,0,4};
    unsigned char *data=NULL; DWORD size=0,id,i; ActionBlock *b;
    for (id=background ? 0x1000 : 0x401;id<=(background ? 0xffffu : 0xfffu);id++)
    {
        for (i=0;i<d->count && d->blocks[i].id!=id;i++) {}
        if (i==d->count) { break; }
    }
    if (id>(background ? 0xffffu : 0xfffu)) { return Fail(why,"No free Action Block IDs in this category."); }
    if (source<d->count)
    {
        for (i=0;i<d->blocks[source].count;i++) { size+=d->blocks[source].instructions[i].size; }
        data=malloc(size);
        if (!data) { return Fail(why,"Out of memory duplicating a block."); }
        for (i=0,size=0;i<d->blocks[source].count;i++)
        {
            ActionInstruction *ins=&d->blocks[source].instructions[i];
            memcpy(data+size,ins->bytes,ins->size); size+=ins->size;
        }
    }
    if (!AppendBlock(d,id,FALSE,&b,why)) { free(data); return FALSE; }
    if (!Decode(d,b,data ? data : idle,data ? size : sizeof(idle),why))
    { free(data); FreeBlock(b); d->count--; return FALSE; }
    free(data); b->sourceoffset=0; b->changed=TRUE;
    if (source<d->count-1)
    {
        b->disabled=d->blocks[source].disabled;
        Text(b->name,sizeof(b->name),d->blocks[source].name);
        for (i=0;i<b->count;i++)
        {
            Text(b->instructions[i].name,ACTION_NAME_SIZE,d->blocks[source].instructions[i].name);
            Text(b->instructions[i].note,ACTION_NOTE_SIZE,d->blocks[source].instructions[i].note);
        }
    }
    else { Text(b->name,sizeof(b->name),background ? "New level logic" : "New behavior"); Text(b->instructions[0].name,ACTION_NAME_SIZE,"Wait"); }
    d->changed=TRUE; *out=d->count-1; return TRUE;
}
static BOOL References(const ActionBlock *b, DWORD id)
{
    for (DWORD i=0;i<b->count;i++)
    {
        const ActionOpcode *op=&g_ActionOpcodes[b->instructions[i].bytes[0]];
        for (int p=0;p<op->paramcount;p++)
        { if (op->params[p].kind==ACTION_BLOCK && ActionReadValue(&b->instructions[i],p)==id) { return TRUE; } }
    }
    return FALSE;
}
BOOL ActionDocumentDeleteBlock(ActionDocument *d, const SetupFile *setup, DWORD index, const char **why)
{
    DWORD id;
    if (!Editable(d,index,why)) { return FALSE; }
    id=d->blocks[index].id;
    for (DWORD i=0;i<setup->objectcount;i++)
    {
        const SetupObject *o=&setup->objects[i];
        if (!o->deleted && (o->type==39 || o->type==40) && o->sourceoffset<=setup->size
            && setup->size-o->sourceoffset>=0x84 && Read(setup->data+o->sourceoffset+0x80,4)==id)
        { return Fail(why,"This block is assigned to a vehicle or aircraft."); }
    }
    for (DWORD i=0;i<d->charactercount;i++)
    { if (!setup->characters[i].deleted && d->assignments[i]==id) { return Fail(why,"This block is assigned to a character. Assign another behavior first."); } }
    for (DWORD i=0;i<d->count;i++)
    { if (i!=index && References(&d->blocks[i],id)) { return Fail(why,"Another block refers to this block. Update that reference first."); } }
    FreeBlock(&d->blocks[index]);
    memmove(d->blocks+index,d->blocks+index+1,(d->count-index-1)*sizeof(*d->blocks));
    d->count--; d->changed=TRUE; return TRUE;
}
static BOOL TargetsUnchanged(const ActionBlock *b)
{
    for (DWORD i=0;i<b->count;i++)
    { if (ActionFindTarget(b,i)!=b->instructions[i].target) { return FALSE; } }
    return TRUE;
}
BOOL ActionBlockInsert(ActionDocument *d, DWORD index, DWORD before, unsigned int op, const char **why)
{
    ActionBlock *b; ActionInstruction ins={0},*grown; int p;
    if (!Editable(d,index,why)) { return FALSE; }
    b=&d->blocks[index];
    if (op>=ACTION_OPCODE_COUNT || op==4 || before>=b->count || b->count>=ACTION_MAX_INSTRUCTIONS)
    { return Fail(why,"Insert before the final End of block; each block has one terminator."); }
    ins.size=op==0xad ? 2 : g_ActionOpcodes[op].size;
    if (ins.size>ACTION_MAX_BYTES-BlockBytes(b)) { return Fail(why,"A block must fit the game's 16-bit instruction offsets (64 KiB)."); }
    ins.bytes=calloc(ins.size,1);
    if (!ins.bytes) { return Fail(why,"Out of memory adding an instruction."); }
    ins.bytes[0]=op; ins.uid=d->nextuid;
    if (op==2)
    {
        unsigned int label;
        for (label=0;label<256;label++)
        {
            DWORD i;
            for (i=0;i<b->count;i++)
            { if (b->instructions[i].bytes[0]==2 && b->instructions[i].bytes[1]==label) { break; } }
            if (i==b->count) { break; }
        }
        if (label==256) { free(ins.bytes); return Fail(why,"All 256 native label values are in use."); }
        ins.bytes[1]=label;
    }
    for (int i=0;i<g_ActionOpcodes[op].paramcount;i++)
    {
        const ActionParam *param=&g_ActionOpcodes[op].params[i];
        if (param->kind==ACTION_CHARACTER) { Write(ins.bytes+param->offset,param->width,253); }
        if (param->kind==ACTION_BLOCK) { Write(ins.bytes+param->offset,param->width,1); }
    }
    if (op==0x0a)
    { Write(ins.bytes+3,2,65535); Write(ins.bytes+5,2,65535); ins.bytes[8]=16; }
    p=ActionTargetParameter(op);
    if (p>=0)
    {
        DWORD i;
        for (i=op==1 ? 0 : before;i<b->count;i++)
        { if (b->instructions[i].bytes[0]==2) { break; } }
        if (i==b->count)
        { free(ins.bytes); return Fail(why,"Add a Section below this position first, so the new branch has a destination."); }
        ins.target=b->instructions[i].uid;
        ins.bytes[g_ActionOpcodes[op].params[p].offset]=b->instructions[i].bytes[1];
    }
    grown=realloc(b->instructions,(b->count+1)*sizeof(*grown));
    if (!grown) { free(ins.bytes); return Fail(why,"Out of memory adding an instruction."); }
    b->instructions=grown;
    memmove(grown+before+1,grown+before,(b->count-before)*sizeof(*grown));
    grown[before]=ins; b->count++;
    /* A new section may repair a previously missing destination. Existing
     * valid branches keep their occurrence identity. */
    for (DWORD i=0;i<b->count;i++) if (b->instructions[i].target==ACTION_MISSING_TARGET)
    { b->instructions[i].target=ActionFindTarget(b,i); }
    d->nextuid++; b->changed=TRUE; d->changed=TRUE; return TRUE;
}
BOOL ActionBlockDelete(ActionDocument *d, DWORD index, DWORD row, const char **why)
{
    ActionBlock *b;
    if (!Editable(d,index,why)) { return FALSE; }
    b=&d->blocks[index];
    if (row>=b->count || b->instructions[row].bytes[0]==4)
    { return Fail(why,"The final End of block must be retained."); }
    for (DWORD i=0;i<b->count;i++)
    { if (b->instructions[i].target==b->instructions[row].uid) { return Fail(why,"A branch uses this section. Redirect the branch before deleting it."); } }
    free(b->instructions[row].bytes);
    memmove(b->instructions+row,b->instructions+row+1,(b->count-row-1)*sizeof(*b->instructions));
    b->count--; b->changed=TRUE; d->changed=TRUE; return TRUE;
}
BOOL ActionBlockMove(ActionDocument *d, DWORD index, DWORD row, int direction, const char **why)
{
    ActionBlock *b; DWORD other; ActionInstruction swap;
    if (!Editable(d,index,why)) { return FALSE; }
    b=&d->blocks[index];
    if (direction!=1 && direction!=-1) { return Fail(why,"Invalid move direction."); }
    other=row+direction;
    if (row>=b->count-1 || other>=b->count-1) { return Fail(why,"The End of block must stay last."); }
    swap=b->instructions[row]; b->instructions[row]=b->instructions[other]; b->instructions[other]=swap;
    if (!TargetsUnchanged(b))
    {
        swap=b->instructions[row]; b->instructions[row]=b->instructions[other]; b->instructions[other]=swap;
        return Fail(why,"That move would change a branch destination. Native forward/first-label searches must still reach the same section.");
    }
    b->changed=TRUE; d->changed=TRUE; return TRUE;
}
BOOL ActionInstructionSet(ActionDocument *d, DWORD index, DWORD row,
    const DWORD values[8], DWORD target, const char *name, const char *note,
    const char *text, const char **why)
{
    ActionBlock *b; ActionInstruction next,old; const ActionOpcode *op; int targetparam;
    if (!Editable(d,index,why)) { return FALSE; }
    b=&d->blocks[index];
    if (row>=b->count) { return Fail(why,"No instruction is selected."); }
    old=b->instructions[row]; next=old; op=&g_ActionOpcodes[old.bytes[0]];
    if (strlen(name)>=ACTION_NAME_SIZE || strlen(note)>=ACTION_NOTE_SIZE)
    { return Fail(why,"The name or note is too long."); }
    next.size=old.bytes[0]==0xad ? (DWORD)strlen(text)+2 : old.size;
    if (next.size>ACTION_MAX_BYTES || next.size>ACTION_MAX_BYTES-(BlockBytes(b)-old.size))
    { return Fail(why,"A block must fit the game's 16-bit instruction offsets (64 KiB)."); }
    next.bytes=calloc(next.size,1);
    if (!next.bytes) { return Fail(why,"Out of memory editing an instruction."); }
    next.bytes[0]=old.bytes[0];
    if (old.bytes[0]==0xad) { memcpy(next.bytes+1,text,next.size-1); }
    for (int p=0;p<op->paramcount;p++)
    {
        DWORD max=op->params[p].width==4 ? 0xffffffffu : (1u<<(8*op->params[p].width))-1;
        if (values[p]>max) { free(next.bytes); return Fail(why,"A parameter exceeds its native range."); }
        Write(next.bytes+op->params[p].offset,op->params[p].width,values[p]);
    }
    /* Section numbers are retained; readable names identify occurrences. */
    if (old.bytes[0]==2) { next.bytes[1]=old.bytes[1]; }
    targetparam=ActionTargetParameter(old.bytes[0]);
    if (targetparam>=0)
    {
        DWORD dest=ActionFindInstruction(b,target);
        if (target==ACTION_MISSING_TARGET && old.target==ACTION_MISSING_TARGET
            && values[targetparam]==ActionReadValue(&old,targetparam))
        { /* Preserve a legacy unresolved jump until explicitly repaired. */ }
        else
        {
            if (dest==ACTION_MISSING_TARGET || b->instructions[dest].bytes[0]!=2)
            { free(next.bytes); return Fail(why,"Choose a section for the branch destination."); }
            next.bytes[op->params[targetparam].offset]=b->instructions[dest].bytes[1]; next.target=target;
        }
    }
    Text(next.name,sizeof(next.name),name); Text(next.note,sizeof(next.note),note);
    b->instructions[row]=next;
    if (!TargetsUnchanged(b))
    { b->instructions[row]=old; free(next.bytes); return Fail(why,"This native jump cannot reach that occurrence. Choose a reachable section."); }
    if (next.size!=old.size || memcmp(next.bytes,old.bytes,old.size)) { b->changed=TRUE; d->changed=TRUE; }
    if (strcmp(next.name,old.name) || strcmp(next.note,old.note)) { d->changed=TRUE; }
    free(old.bytes); return TRUE;
}
BOOL ActionDocumentAssign(ActionDocument *d, DWORD character, DWORD block, const char **why)
{
    if (character>=d->charactercount || block>=d->count || d->blocks[block].id>=0x1000)
    { return Fail(why,"Choose a character and a character behavior, not automatic level logic."); }
    for (DWORD i=0;i<block;i++) if (d->blocks[i].id==d->blocks[block].id)
    { return Fail(why,"This ID resolves to an earlier block. Duplicate this block to obtain a unique assignable ID."); }
    if (d->assignments[character]!=d->blocks[block].id)
    { d->assignments[character]=d->blocks[block].id; d->changed=TRUE; }
    return TRUE;
}
double ActionDisplayValue(const ActionParam *p, DWORD value)
{
    if (p->kind==ACTION_METERS) { return value/10.0; }
    if (p->kind==ACTION_SECONDS60) { return value/60.0; }
    if (p->kind==ACTION_SECONDS30) { return value/30.0; }
    return value;
}
BOOL ActionParseValue(const ActionParam *p, const char *text, DWORD *value)
{
    char *end; double n,max=p->width==4 ? 4294967295.0 : (double)((1u<<(8*p->width))-1);
    errno=0; n=strtod(text,&end);
    if (end==text || errno || !isfinite(n) || n<0) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    if (*end) { return FALSE; }
    if (p->kind==ACTION_METERS) { n*=10; }
    if (p->kind==ACTION_SECONDS60) { n*=60; }
    if (p->kind==ACTION_SECONDS30) { n*=30; }
    if (n>max || (p->kind!=ACTION_METERS && p->kind!=ACTION_SECONDS60 && p->kind!=ACTION_SECONDS30 && floor(n)!=n))
    { return FALSE; }
    *value=(DWORD)floor(n+0.5); return TRUE;
}
void ActionBlockTitle(const ActionBlock *b, char *text, size_t size)
{
    snprintf(text,size,"%s%s  [%04lX]%s",b->global ? "Shared: " : "",
        b->name[0] ? b->name : b->id>=0x1000 ? "Level logic" : "Behavior",(unsigned long)b->id,
        b->disabled ? "  [Disabled]" : "");
}
void ActionInstructionFormat(const ActionBlock *b, DWORD row, char *text, size_t size)
{
    const ActionInstruction *ins=&b->instructions[row];
    const ActionOpcode *op=&g_ActionOpcodes[ins->bytes[0]];
    size_t used;
    if (ins->bytes[0]==2)
    {
        snprintf(text,size,"Section: %s  [label %02X, step %lu]",
            ins->name[0] ? ins->name : "Unnamed",ins->bytes[1],(unsigned long)row+1); return;
    }
    snprintf(text,size,"%s%s%s",ins->name,ins->name[0] ? ": " : "",op->title);
    for (int p=0;p<op->paramcount;p++)
    {
        const ActionParam *param=&op->params[p]; DWORD value=ActionReadValue(ins,p);
        used=strlen(text); if (used>=size) { break; }
        if (param->kind==ACTION_LABEL)
        {
            DWORD dest=ActionFindInstruction(b,ins->target);
            if (dest==ACTION_MISSING_TARGET) { snprintf(text+used,size-used," -> MISSING SECTION"); }
            else { snprintf(text+used,size-used," -> %s (step %lu)",
                b->instructions[dest].name[0] ? b->instructions[dest].name : "section",(unsigned long)dest+1); }
        }
        else if (param->kind==ACTION_METERS || param->kind==ACTION_SECONDS60 || param->kind==ACTION_SECONDS30)
        { snprintf(text+used,size-used,"  %s=%.4g %s",param->name,ActionDisplayValue(param,value),param->kind==ACTION_METERS ? "m" : "s"); }
        else if (param->kind==ACTION_BLOCK || param->kind==ACTION_MASK)
        { snprintf(text+used,size-used,"  %s=0x%lX",param->name,(unsigned long)value); }
        else if (param->kind==ACTION_CHARACTER && value==253)
        { snprintf(text+used,size-used,"  %s=this character",param->name); }
        else { snprintf(text+used,size-used,"  %s=%lu",param->name,(unsigned long)value); }
    }
}
static BOOL Issue(ActionIssue **out, DWORD *count, DWORD b, DWORD i, BOOL error, const char *text)
{
    ActionIssue *grown;
    if (*count>=4096)
    {
        /* Never let a flood of advisory warnings hide a blocking error. */
        if (error)
        {
            for (DWORD slot=0;slot<*count;slot++) if (!(*out)[slot].error)
            { (*out)[slot].block=b; (*out)[slot].instruction=i; (*out)[slot].error=TRUE; Text((*out)[slot].text,sizeof((*out)[slot].text),text); break; }
        }
        return TRUE;
    }
    grown=realloc(*out,(*count+1)*sizeof(*grown));
    if (!grown) { return FALSE; }
    *out=grown; grown+=(*count)++; grown->block=b; grown->instruction=i; grown->error=error;
    Text(grown->text,sizeof(grown->text),text); return TRUE;
}
/* Remove the yielding/terminating vertices, then use Kahn's algorithm to find
 * possible cycles in the remaining control flow. This is a warning, since a
 * condition may make a syntactically possible cycle infeasible. */
static BOOL BusyCycle(const ActionBlock *b, BOOL *cycle)
{
    DWORD *degree=calloc(b->count,sizeof(*degree)), *queue=malloc(b->count*sizeof(*queue));
    DWORD (*edges)[2]=malloc(b->count*sizeof(*edges)),head=0,tail=0;
    if (!degree || !queue || !edges) { free(degree); free(queue); free(edges); return FALSE; }
    for (DWORD i=0;i<b->count;i++)
    {
        unsigned int op=b->instructions[i].bytes[0]; int target=ActionTargetParameter(op);
        edges[i][0]=edges[i][1]=ACTION_MISSING_TARGET;
        if (op==3 || op==4 || op==7 || (op==5 && ActionReadValue(&b->instructions[i],0)==253)) { continue; }
        if (op!=0 && op!=1 && i+1<b->count) { edges[i][0]=i+1; degree[i+1]++; }
        if (target>=0)
        {
            DWORD dest=ActionFindInstruction(b,b->instructions[i].target);
            if (dest!=ACTION_MISSING_TARGET) { edges[i][1]=dest; degree[dest]++; }
        }
    }
    for (DWORD i=0;i<b->count;i++) { if (!degree[i]) { queue[tail++]=i; } }
    while (head<tail)
    {
        DWORD i=queue[head++];
        for (int e=0;e<2;e++)
        { DWORD dest=edges[i][e]; if (dest!=ACTION_MISSING_TARGET && !--degree[dest]) { queue[tail++]=dest; } }
    }
    *cycle=tail<b->count; free(degree); free(queue); free(edges); return TRUE;
}
BOOL ActionDocumentValidate(const ActionDocument *d, const SetupFile *setup,
    ActionIssue **out, DWORD *count, const char **why)
{
    *out=NULL; *count=0;
    if (!setup || setup->charactercount!=d->charactercount)
    { return Fail(why,"The setup changed while Action Blocks were being edited."); }
    for (DWORD b=0;b<d->count;b++)
    {
        const ActionBlock *block=&d->blocks[b]; BOOL cycle;
        if (block->global) { continue; }
        for (DWORD earlier=0;earlier<b;earlier++) if (d->blocks[earlier].id==block->id)
        {
            if (!Issue(out,count,b,0,FALSE,"Duplicate block ID: switches use the first entry. Automatic level-logic entries still start independently.")) { goto memory; }
            break;
        }
        for (DWORD i=0;i<block->count;i++)
        {
            const ActionInstruction *ins=&block->instructions[i];
            const ActionOpcode *op=&g_ActionOpcodes[ins->bytes[0]];
            if (ins->target==ACTION_MISSING_TARGET || ins->target!=ActionFindTarget(block,i))
            { if (!Issue(out,count,b,i,block->changed,block->changed ? "The branch has no reachable destination section." : "Existing unresolved branch: no reachable destination section. Preserved until this block is edited.")) { goto memory; } }
            for (int p=0;p<op->paramcount;p++)
            {
                const ActionParam *param=&op->params[p]; DWORD value=ActionReadValue(ins,p),j;
                if (param->kind==ACTION_BLOCK)
                {
                    for (j=0;j<d->count && d->blocks[j].id!=value;j++) {}
                    /* Shared IDs cannot be checked when the optional catalog is absent. */
                    if (j==d->count && (value>0x400 || d->globalsloaded)
                        && !Issue(out,count,b,i,block->changed,block->changed ? "The referenced Action Block does not exist." : "Existing reference to a missing Action Block. Preserved until this block is edited.")) { goto memory; }
                }
                if (param->kind==ACTION_CHARACTER && value<248)
                {
                    for (j=0;j<setup->charactercount;j++)
                    { if (!setup->characters[j].deleted && setup->characters[j].chrnum==value) { break; } }
                    if (j==setup->charactercount && !Issue(out,count,b,i,FALSE,"Character ID is not initially present; it may be spawned by another script.")) { goto memory; }
                }
                if (param->kind==ACTION_PAD && value<9000 && value>=setup->padcount
                    && !Issue(out,count,b,i,FALSE,"Pad index is outside this level's ordinary pad table; check whether this command uses a preset.")) { goto memory; }
                if (ActionParameterIsPad(ins,p) && value!=9000)
                {
                    const SetupPad *pad = value>=10000
                        ? (value-10000<setup->boundpadcount ? &setup->boundpads[value-10000].pad : NULL)
                        : (value<setup->padcount ? &setup->pads[value] : NULL);
                    if (pad && pad->deleted && !Issue(out,count,b,i,TRUE,"This instruction references a deleted pad.")) { goto memory; }
                }
            }
            if (block->id>=0x1000 && ins->bytes[0]>=8 && ins->bytes[0]<=0x31
                && ins->bytes[0]!=0x24 && ins->bytes[0]!=0x25
                && !Issue(out,count,b,i,FALSE,"This command needs a real character or vehicle. Automatic level logic uses a phantom character.")) { goto memory; }
            if (ins->bytes[0]==7 && !Issue(out,count,b,i,FALSE,"Return restarts the stored return behavior. Its ID must have been set before this command.")) { goto memory; }
            if (ins->bytes[0]==5 && ActionReadValue(ins,0)==253 && ActionReadValue(ins,1)==block->id
                && !Issue(out,count,b,i,FALSE,"This restarts the same block immediately; ensure the loop reaches a wait.")) { goto memory; }
        }
        if (!BusyCycle(block,&cycle)) { goto memory; }
        if (cycle && !Issue(out,count,b,0,FALSE,"A possible loop has no Wait for next AI update. If taken indefinitely, it can freeze the game.")) { goto memory; }
    }
    for (DWORD i=0;i<d->charactercount;i++)
    {
        DWORD id=d->assignments[i],b;
        if (setup->characters[i].deleted || (id<=0x400 && !d->globalsloaded)) { continue; }
        for (b=0;b<d->count && d->blocks[b].id!=id;b++) {}
        if ((b==d->count || id>=0x1000) && !Issue(out,count,0,0,id!=setup->characters[i].ailistid,"A character is assigned to missing or automatic level logic.")) { goto memory; }
    }
    for (DWORD i=0;i<setup->objectcount;i++)
    {
        const SetupObject *o=&setup->objects[i];
        if (!o->deleted && (o->type==39 || o->type==40) && o->sourceoffset<=setup->size
            && setup->size-o->sourceoffset>=0x84)
        {
            DWORD id=Read(setup->data+o->sourceoffset+0x80,4), b;
            for (b=0;b<d->count && d->blocks[b].id!=id;b++) {}
            if (id>0x400 && b==d->count && !Issue(out,count,0,0,FALSE,"A vehicle or aircraft uses a missing Action Block.")) { goto memory; }
        }
    }
    return TRUE;
memory:
    free(*out); *out=NULL; *count=0; return Fail(why,"Out of memory validating Action Blocks.");
}
static BOOL SaveNames(const ActionDocument *d, unsigned char **out, DWORD *size, const char **why)
{
    DWORD records=0,at=8,localindex=0; unsigned char *data; BOOL disabled=FALSE;
    for (DWORD b=0;b<d->count;b++) if (!d->blocks[b].global)
    {
        if (d->blocks[b].name[0] || d->blocks[b].disabled) { records++; }
        disabled|=d->blocks[b].disabled;
        for (DWORD i=0;i<d->blocks[b].count;i++)
        { if (d->blocks[b].instructions[i].name[0] || d->blocks[b].instructions[i].note[0]) { records++; } }
    }
    *out=NULL; *size=0;
    if (!records) { return TRUE; }
    if (records>(SETUP_META_MAX-8)/ACTION_META_RECORD) { return Fail(why,"Too many Action Block notes."); }
    *size=8+records*ACTION_META_RECORD; data=calloc(*size,1);
    if (!data) { return Fail(why,"Out of memory saving Action Block names."); }
    memcpy(data,disabled ? "AIN2" : "AIN1",4); Write(data+4,4,records);
    for (DWORD b=0;b<d->count;b++) if (!d->blocks[b].global)
    {
        const ActionBlock *block=&d->blocks[b];
        if (block->name[0] || block->disabled)
        {
            Write(data+at,4,localindex); Write(data+at+4,4,ACTION_MISSING_TARGET);
            Write(data+at+8,4,block->disabled ? 1 : 0);
            Text((char *)data+at+12,ACTION_NAME_SIZE,block->name); at+=ACTION_META_RECORD;
        }
        for (DWORD i=0;i<block->count;i++)
        {
            const ActionInstruction *ins=&block->instructions[i];
            if (!ins->name[0] && !ins->note[0]) { continue; }
            Write(data+at,4,localindex); Write(data+at+4,4,i); Write(data+at+8,4,ins->bytes[0]);
            Text((char *)data+at+12,ACTION_NAME_SIZE,ins->name);
            Text((char *)data+at+12+ACTION_NAME_SIZE,ACTION_NOTE_SIZE,ins->note); at+=ACTION_META_RECORD;
        }
        localindex++;
    }
    *out=data; return TRUE;
}
BOOL ActionDocumentCompile(const ActionDocument *d, const SetupFile *source, SetupFile *out, const char **why)
{
    ActionIssue *issues=NULL; DWORD count=0,locals=0,oldcount=0,at,total,table;
    BOOL nativechanged=FALSE; unsigned char *data=NULL,*meta=NULL; DWORD metasize=0;
    memset(out,0,sizeof(*out));
    if (!ActionDocumentValidate(d,source,&issues,&count,why)) { return FALSE; }
    for (DWORD i=0;i<count;i++) if (issues[i].error)
    { free(issues); return Fail(why,"Fix the errors in Validate before applying Action Blocks."); }
    free(issues);
    if (!SaveNames(d,&meta,&metasize,why)) { return FALSE; }
    total=(source->size+3u)&~3u;
    for (DWORD b=0;b<d->count;b++) if (!d->blocks[b].global)
    {
        const ActionBlock *block=&d->blocks[b]; locals++;
        if (block->changed)
        {
            nativechanged=TRUE;
            for (DWORD i=0;i<block->count;i++)
            {
                if (block->instructions[i].size>ACTION_SETUP_MAX-total)
                { free(meta); return Fail(why,"Action Blocks exceed the setup resource limit."); }
                total+=block->instructions[i].size;
            }
        }
    }
    table=Read(source->data+20,4);
    if (table) for (at=table;at<=source->size && source->size-at>=8 && Read(source->data+at,4);at+=8) { oldcount++; }
    if (oldcount!=locals) { nativechanged=TRUE; }
    table=(total+3u)&~3u;
    if (table>ACTION_SETUP_MAX || (locals+1)*8>ACTION_SETUP_MAX-table)
    { free(meta); return Fail(why,"Action Block table exceeds the setup resource limit."); }
    if (nativechanged)
    {
        total=table+(locals+1)*8; data=calloc(total,1);
        if (!data) { free(meta); return Fail(why,"Out of memory compiling Action Blocks."); }
        memcpy(data,source->data,source->size); at=(source->size+3u)&~3u; locals=0;
        Write(data+20,4,table);
        for (DWORD b=0;b<d->count;b++) if (!d->blocks[b].global)
        {
            const ActionBlock *block=&d->blocks[b]; DWORD offset=block->sourceoffset;
            if (block->changed)
            {
                offset=at;
                for (DWORD i=0;i<block->count;i++)
                { memcpy(data+at,block->instructions[i].bytes,block->instructions[i].size); at+=block->instructions[i].size; }
            }
            Write(data+table+locals*8,4,offset); Write(data+table+locals*8+4,4,block->id); locals++;
        }
    }
    if (!SetupFileClone(source,out,why)) { free(meta); free(data); return FALSE; }
    if (nativechanged) { free(out->data); out->data=data; out->size=total; }
    free(out->actionmeta); out->actionmeta=meta; out->actionmetasize=metasize;
    for (DWORD i=0;i<d->charactercount;i++)
    {
        SetupCharacter *chr=&out->characters[i];
        if (chr->ailistid==d->assignments[i]) { continue; }
        if (chr->deleted || chr->sourceoffset>out->size || out->size-chr->sourceoffset<28
            || out->data[chr->sourceoffset+3]!=9)
        { SetupFileFree(out); return Fail(why,"The character assignment points to an invalid setup record."); }
        chr->ailistid=d->assignments[i]; Write(out->data+chr->sourceoffset+10,2,chr->ailistid);
    }
    out->dirty=TRUE; return TRUE;
}

BOOL ActionSetupBuildRuntime(const SetupFile *source, unsigned char **out, DWORD *size, const char **why)
{
    ActionDocument doc={0}; DWORD disabled=0,table,stub; unsigned char *data;
    *out=NULL; *size=source->size;
    if (!source->actionmetasize) { return TRUE; }
    if (!ActionDocumentLoad(source,&doc,why)) { return FALSE; }
    for (DWORD i=0;i<doc.count;i++) { disabled+=doc.blocks[i].disabled!=FALSE; }
    if (!disabled) { ActionDocumentFree(&doc); return TRUE; }
    if (source->size>ACTION_SETUP_MAX-4)
    { ActionDocumentFree(&doc); return Fail(why,"The disabled Action Blocks exceed the setup resource limit."); }
    stub=(source->size+3u)&~3u;
    data=calloc(stub+4,1);
    if (!data) { ActionDocumentFree(&doc); return Fail(why,"Out of memory exporting disabled Action Blocks."); }
    memcpy(data,source->data,source->size);
    /* Keep every ID/table occurrence, including automatic level threads.
     * Redirect entries instead of overwriting bytecode: an enabled alias can
     * share the same original instructions. Compaction removes unused scripts
     * from the ROM copy; the project retains them for exact re-enabling. */
    data[stub]=4; /* AI_EndList returns immediately for every script owner. */
    table=Read(data+20,4);
    for (DWORD i=0;i<doc.count;i++) if (doc.blocks[i].disabled)
    { Write(data+table+i*8,4,stub); }
    ActionDocumentFree(&doc); *out=data; *size=stub+4; return TRUE;
}
