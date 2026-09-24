#include "bgcommands.h"
#include "bgdocument.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct Opcode { unsigned char type,length; BgVisArgument argument; const char *name,*explanation; } Opcode;
static const Opcode opcodes[]={
    {0x00,1,BG_VIS_NONE,"END","Ends this command branch. The zero cell also terminates the native visibility stream."},
    {0x01,1,BG_VIS_VALUE,"PUSH","Pushes the instruction's signed argument onto the 20-entry boolean/value stack."},
    {0x02,1,BG_VIS_NONE,"POP","Discards the top stack value. The native stack wraps on underflow and overflow."},
    {0x03,1,BG_VIS_NONE,"AND","Pops two values and pushes their bitwise AND."},
    {0x04,1,BG_VIS_NONE,"OR","Pops two values and pushes their bitwise OR."},
    {0x05,1,BG_VIS_NONE,"NOT","Replaces the top value with 1 if it was zero, otherwise 0."},
    {0x06,1,BG_VIS_NONE,"XOR","Pops two values and pushes their bitwise XOR."},
    {0x14,3,BG_VIS_ROOM_RANGE,"TEST CAMERA ROOM","Pushes 1 when the current camera/player room lies in the inclusive range; otherwise pushes 0."},
    {0x1e,1,BG_VIS_NONE,"RESET SCREEN REGION","Sets the visibility result to true and resets the working rectangle to the player's full screen."},
    {0x1f,2,BG_VIS_ONE_PORTAL,"TEST PORTAL","Projects this portal, clips it to the screen, and sets the result to true if a visible rectangle remains. Replaces the working rectangle."},
    {0x20,2,BG_VIS_ONE_ROOM,"ADD ROOM IF VISIBLE","If the visibility result is true and this room intersects the working screen rectangle, schedules it for rendering with that rectangle."},
    {0x21,1,BG_VIS_NONE,"CLEAR RESULT","Sets the visibility result to false. The working rectangle is retained."},
    {0x22,2,BG_VIS_ONE_PORTAL,"UNION PORTAL REGION","If the portal has a visible screen rectangle, unions it with the working rectangle. If the result was false, starts a new rectangle and sets the result true."},
    {0x23,2,BG_VIS_ONE_PORTAL,"INTERSECT PORTAL REGION","While the result is true, intersects the working rectangle with this portal's visible screen rectangle. Sets the result false if they do not overlap."},
    {0x24,2,BG_VIS_ONE_ROOM,"SUPPRESS ROOM","Sets this room's per-frame suppression mask. bgSetRoomOnScreen then skips it, including attempts from the normal portal traversal."},
    {0x25,3,BG_VIS_ROOM_RANGE,"SUPPRESS ROOM RANGE","Sets the per-frame suppression mask for every room in this inclusive range."},
    {0x26,2,BG_VIS_ONE_ROOM,"PRELOAD ROOM","Attempts to load this room's model without marking it visible. This branch stops further preload attempts after one room needs a load."},
    {0x27,3,BG_VIS_ROOM_RANGE,"PRELOAD ROOM RANGE","Checks rooms in ascending order, stopping further load attempts in this branch after one room needs a load. Does not mark rooms visible."},
    {0x50,1,BG_VIS_NONE,"BRANCH","Recursively processes a branch with the current execution flag. On return the runtime advances past the returned cell using that cell's length."},
    {0x51,1,BG_VIS_NONE,"THROW","When active, sets the throwing flag. Disables execution while scanning toward a control boundary."},
    {0x52,1,BG_VIS_NONE,"CATCH","Clears the throwing flag and returns the following instruction to the calling branch."},
    {0x5a,1,BG_VIS_NONE,"IF","Pops a condition and processes the following branch with condition AND the current execution flag. The native code pops even while scanning an inactive branch."},
    {0x5b,1,BG_VIS_NONE,"ELSE","Toggles this branch's execution flag. This describes the native interpreter, including its behavior inside inactive nested branches."},
    {0x5c,1,BG_VIS_NONE,"END IF","Returns the following instruction to the calling branch."}
};
static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static const Opcode *Find(unsigned int type)
{ for (size_t i=0;i<sizeof(opcodes)/sizeof(*opcodes);i++) if (opcodes[i].type==type) { return opcodes+i; }return NULL; }
const char *BgVisName(unsigned int opcode) { const Opcode *op=Find(opcode);return op ? op->name : "UNKNOWN"; }
const char *BgVisExplanation(unsigned int opcode)
{ const Opcode *op=Find(opcode);return op ? op->explanation : "The current GUD visibility interpreter returns from the branch on an unknown opcode. Raw cells are retained here for inspection."; }
void BgVisFree(BgVisProgram *p)
{ free(p->data);free(p->instructions);memset(p,0,sizeof(*p)); }
static void ResolvePortal(const BgFile *bg,const BgPortalFile *portals,BgVisInstruction *i)
{
    DWORD wanted=i->arg[0]&0xffffffu;i->portal=BG_VIS_NO_TARGET;
    if (i->operandType[0]==0x65)
    {
        if (portals && portals->portals && i->arg[0]<portals->portalcount)
        { const BgPortal *p=portals->portals+i->arg[0];i->portal=i->arg[0];i->matches=1;i->room1=p->connectedroom1;i->room2=p->connectedroom2; }
        else { i->warnings|=BG_VIS_PORTAL; }return;
    }
    for (DWORD p=0;portals && portals->portals && p<portals->portalcount;p++)
    {
        const BgPortal *portal=portals->portals+p;DWORD geometry=portal->geometryoffset;
        if (i->arg[0]&BG_PORTAL_NEW_GEOMETRY)
        { if (geometry!=i->arg[0]) { continue; } }
        else if (geometry&BG_PORTAL_NEW_GEOMETRY)
        { DWORD slot=geometry&~BG_PORTAL_NEW_GEOMETRY;geometry=slot<BG_MAX_PORTALS ? (bg ? bg->newportaloffsets[slot] : 0) : 0; }
        if (!(i->arg[0]&BG_PORTAL_NEW_GEOMETRY) && (!geometry || geometry!=wanted)) { continue; }
        if (!i->matches) { i->portal=p;i->room1=portal->connectedroom1;i->room2=portal->connectedroom2; }i->matches++;
    }
    if (i->portal==BG_VIS_NO_TARGET) { i->warnings|=BG_VIS_PORTAL; }
}
static BOOL DecodeCells(const unsigned char *data, DWORD size, DWORD at,
    const BgFile *bg, const BgPortalFile *portals, DWORD rooms, BgVisProgram *out, const char **why)
{
    DWORD capacity=0,depth=0;unsigned char stack[BG_VIS_NESTING_LIMIT],otherwise[BG_VIS_NESTING_LIMIT];
    memset(out,0,sizeof(*out));*why="";
    out->present=TRUE;out->offset=at;
    while (out->size/8<BG_VIS_CELL_LIMIT)
    {
        if (at>size || size-at<8) { strcpy(out->problem,"The visibility stream is truncated or has no END cell.");break; }
        if (out->count==capacity)
        {
            DWORD next=capacity ? capacity*2 : 64;BgVisInstruction *grown=realloc(out->instructions,next*sizeof(*grown));
            if (!grown) { BgVisFree(out);*why="Out of memory decoding BG commands.";return FALSE; }out->instructions=grown;capacity=next;
        }
        BgVisInstruction *i=out->instructions+out->count++;memset(i,0,sizeof(*i));
        i->offset=at;i->opcode=data[at];i->length=data[at+1];i->portal=BG_VIS_NO_TARGET;i->depth=depth;i->bytes=8;
        const Opcode *op=Find(i->opcode);if (op) { i->argument=op->argument; }else { i->warnings|=BG_VIS_UNKNOWN; }
        if (!i->opcode)
        {
            if (depth) { i->warnings|=BG_VIS_FLOW; }out->complete=TRUE;out->size=at-out->offset+8;
            out->warnings+=i->warnings!=0;break;
        }
        if (!i->length || (DWORD)i->length*8>size-at || (op && i->length<op->length))
        {
            i->warnings|=BG_VIS_LENGTH;out->warnings++;out->size=at-out->offset+8;
            strcpy(out->problem,"Stopped at an invalid instruction length; remaining cells cannot be decoded safely.");break;
        }
        i->bytes=(DWORD)i->length*8;
        if (op && i->length!=op->length) { i->warnings|=BG_VIS_LENGTH; }
        if (i->argument==BG_VIS_VALUE) { i->arg[0]=Read32(data+at+4); }
        for (DWORD a=1;a<i->length;a++)
        {
            unsigned char type=data[at+a*8];
            if (!type)
            {
                i->warnings|=BG_VIS_OPERAND;strcpy(out->problem,"An END cell appears inside an instruction. The native loader stops here before resolving later portal operands.");
            }
            if (op && a<op->length)
            {
                i->arg[a-1]=Read32(data+at+a*8+4);i->operandType[a-1]=type;
                if (type!=0x65 && !(type==0x64 && i->argument==BG_VIS_ONE_PORTAL)) { i->warnings|=BG_VIS_OPERAND; }
            }
        }
        if (i->argument==BG_VIS_ONE_PORTAL)
        {
            if (!(i->warnings&BG_VIS_OPERAND)) { ResolvePortal(bg,portals,i); }
            else { i->warnings|=BG_VIS_PORTAL; }
        }
        if (i->argument==BG_VIS_ONE_ROOM || i->argument==BG_VIS_ROOM_RANGE)
        {
            if (!i->arg[0] || i->arg[0]>rooms || (i->argument==BG_VIS_ROOM_RANGE && (i->arg[1]<i->arg[0] || i->arg[1]>rooms)))
            { i->warnings|=BG_VIS_ROOM; }
        }
        if (i->opcode==0x5c || i->opcode==0x52)
        {
            if (depth && stack[depth-1]==(i->opcode==0x5c ? 0x5a : 0x50)) { i->depth=--depth; }
            else { i->warnings|=BG_VIS_FLOW; }
        }
        if (i->opcode==0x5b)
        {
            if (depth && stack[depth-1]==0x5a && !otherwise[depth-1]) { i->depth=depth-1;otherwise[depth-1]=1; }
            else { i->warnings|=BG_VIS_FLOW; }
        }
        if (i->opcode==0x5a || i->opcode==0x50)
        {
            if (depth==BG_VIS_NESTING_LIMIT) { i->warnings|=BG_VIS_FLOW;strcpy(out->problem,"Command nesting exceeds the inspector's limit."); }
            else { stack[depth]=i->opcode;otherwise[depth++]=0; }
        }
        out->warnings+=i->warnings!=0;at+=i->bytes;out->size=at-out->offset;
        if (out->problem[0]) { break; }
    }
    if (!out->complete && !out->problem[0]) { strcpy(out->problem,"The visibility stream exceeds the inspector's cell limit."); }
    if (out->size)
    {
        out->data=malloc(out->size);if (!out->data) { BgVisFree(out);*why="Out of memory copying BG commands.";return FALSE; }
        memcpy(out->data,data+out->offset,out->size);
    }
    return TRUE;
}
BOOL BgVisDecode(const BgFile *bg,const BgPortalFile *portals,DWORD rooms,BgVisProgram *out,const char **why)
{
    memset(out,0,sizeof(*out));*why="";
    if (!bg || !bg->data || bg->size<20) { *why="Open a level with a valid background to inspect its commands.";return FALSE; }
    if (Read32(bg->data)) { out->singleDisplayList=TRUE;out->complete=TRUE;return TRUE; }
    DWORD pointer=Read32(bg->data+12);if (!pointer) { out->complete=TRUE;return TRUE; }
    DWORD at=pointer&0xffffffu;
    if (at<20 || at>bg->size || (at&3))
    { out->present=TRUE;out->offset=at;strcpy(out->problem,"The visibility stream pointer is invalid.");return TRUE; }
    return DecodeCells(bg->data,bg->size,at,bg,portals,rooms,out,why);
}
BOOL BgVisDecodeDocument(const BgFile *bg,const BgDocument *doc,BgVisProgram *out,const char **why)
{
    if (!doc || !doc->viscommandsloaded)
    { return BgVisDecode(bg,doc && !doc->portalwarning ? &doc->portals : NULL,doc ? doc->roomcount : 0,out,why); }
    if (!doc->viscommandssize) { memset(out,0,sizeof(*out));out->complete=TRUE;*why=""; }
    else if (!DecodeCells(doc->viscommands,doc->viscommandssize,0,bg,
        doc->portalwarning ? NULL : &doc->portals,doc->roomcount,out,why)) { return FALSE; }
    out->relativeOffsets=TRUE;
    return TRUE;
}

void BgVisSummary(const BgVisInstruction *i,char *out,size_t size)
{
    switch (i->argument)
    {
    case BG_VIS_VALUE: snprintf(out,size,"%ld (0x%08lX)",(long)(LONG)i->arg[0],(unsigned long)i->arg[0]);break;
    case BG_VIS_ONE_ROOM: snprintf(out,size,"Room %lu",(unsigned long)i->arg[0]);break;
    case BG_VIS_ROOM_RANGE:
        if (i->arg[0]==i->arg[1]) { snprintf(out,size,"Room %lu",(unsigned long)i->arg[0]); }
        else { snprintf(out,size,"Rooms %lu through %lu (inclusive)",(unsigned long)i->arg[0],(unsigned long)i->arg[1]); }break;
    case BG_VIS_ONE_PORTAL:
        if (i->portal==BG_VIS_NO_TARGET) { snprintf(out,size,"Unresolved %s 0x%08lX",i->operandType[0]==0x65 ? "portal index" : "polygon",(unsigned long)i->arg[0]); }
        else if (i->operandType[0]==0x65) { snprintf(out,size,"Portal %lu: rooms %lu / %lu (literal index)",(unsigned long)i->portal,(unsigned long)i->room1,(unsigned long)i->room2); }
        else { snprintf(out,size,"Portal %lu: rooms %lu / %lu; polygon 0x%08lX%s",(unsigned long)i->portal,(unsigned long)i->room1,(unsigned long)i->room2,(unsigned long)i->arg[0],i->matches>1 ? " (shared)" : ""); }break;
    default: snprintf(out,size,"%s",i->opcode==0x5a ? "Pop condition; enter branch" : i->opcode==0x1e ? "Result = true; full screen" : i->opcode==0x21 ? "Result = false" : "");break;
    }
}
static void Append(char *out,size_t size,const char *format,...)
{
    size_t n=strlen(out);if (n>=size) { return; }
    va_list args;va_start(args,format);vsnprintf(out+n,size-n,format,args);va_end(args);
}
void BgVisWarnings(const BgVisInstruction *i,char *out,size_t size)
{
    if (!size) { return; }out[0]=0;
    if (i->warnings&BG_VIS_UNKNOWN) { Append(out,size,"Unknown opcode: the runtime returns from this branch. "); }
    if (i->warnings&BG_VIS_LENGTH) { Append(out,size,"Unexpected or invalid instruction length. "); }
    if (i->warnings&BG_VIS_OPERAND) { Append(out,size,"Unexpected operand cell type. "); }
    if (i->warnings&BG_VIS_ROOM) { Append(out,size,"Room ID/range is outside the current background. "); }
    if (i->warnings&BG_VIS_PORTAL) { Append(out,size,"Portal operand cannot be resolved against the current portal table. A missing polygon address falls back to portal 0 in game; an invalid literal index is unsafe. "); }
    if (i->warnings&BG_VIS_FLOW) { Append(out,size,"Unmatched control boundary or excessive nesting; indentation may be incomplete. "); }
}
char *BgVisReport(const BgVisProgram *p,const char *name)
{
    size_t size=1024+(size_t)p->count*1536+(size_t)p->size*8;char *text=calloc(size,1);if (!text) { return NULL; }
    Append(text,size,"BG Commands - %s\r\n%lu instructions; %lu bytes; %s offset 0x%06lX\r\n",name ? name : "",(unsigned long)p->count,(unsigned long)p->size,p->relativeOffsets ? "command stream" : "BG file",(unsigned long)p->offset);
    if (!p->present) { Append(text,size,"%s\r\n",p->singleDisplayList ? "Single-display-list background: no visibility stream." : "This background has no special visibility commands."); }
    if (p->problem[0]) { Append(text,size,"%s\r\n",p->problem); }
    for (DWORD n=0;n<p->count;n++)
    {
        const BgVisInstruction *i=p->instructions+n;char summary[256],warning[768];BgVisSummary(i,summary,sizeof(summary));BgVisWarnings(i,warning,sizeof(warning));
        Append(text,size,"\r\n%lu  0x%06lX  %*s%s [0x%02X]  %s\r\n",(unsigned long)n,(unsigned long)i->offset,(int)(i->depth>16 ? 32 : i->depth*2),"",BgVisName(i->opcode),i->opcode,summary);
        Append(text,size,"%s\r\n",BgVisExplanation(i->opcode));if (warning[0]) { Append(text,size,"Warning: %s\r\n",warning); }
        for (DWORD b=0;b<i->bytes;b+=8)
        { const unsigned char *cell=p->data+i->offset-p->offset+b;Append(text,size,"  %06lX: %02X %02X %02X %02X  %02X %02X %02X %02X\r\n",(unsigned long)i->offset+b,cell[0],cell[1],cell[2],cell[3],cell[4],cell[5],cell[6],cell[7]); }
    }
    return text;
}

BgVisArgument BgVisArgumentType(unsigned int opcode)
{ const Opcode *op=Find(opcode);return op ? op->argument : BG_VIS_NONE; }
BOOL BgVisCanEdit(const BgVisProgram *p)
{
    if (!p || !p->complete || p->singleDisplayList || p->problem[0]) { return FALSE; }
    for (DWORD n=0;n<p->count;n++)
    {
        if (p->instructions[n].warnings&(BG_VIS_UNKNOWN|BG_VIS_LENGTH|BG_VIS_OPERAND|BG_VIS_FLOW)) { return FALSE; }
    }
    return TRUE;
}
BOOL BgVisReturnCell(const BgVisProgram *p,DWORD row)
{
    /* BRANCH advances over the instruction returned by CATCH. Keep that cell
     * attached to its block so edits cannot accidentally skip a new command. */
    return row>0 && row<p->count && p->instructions[row-1].opcode==0x52;
}
BOOL BgVisDeleteRange(const BgVisProgram *p,DWORD row,DWORD *first,DWORD *last)
{
    if (row>=p->count || !p->instructions[row].opcode || BgVisReturnCell(p,row)) { return FALSE; }
    *first=*last=row;
    unsigned op=p->instructions[row].opcode;
    if (op==0x5c || op==0x52)
    {
        unsigned open=op==0x5c ? 0x5a : 0x50;
        while (*first && !(p->instructions[*first].opcode==open && p->instructions[*first].depth==p->instructions[row].depth)) { --*first; }
        if (p->instructions[*first].opcode!=open) { return FALSE; }
        op=open;
    }
    if (op==0x5a || op==0x50)
    {
        unsigned close=op==0x5a ? 0x5c : 0x52;
        *last=*first+1;
        while (*last<p->count && !(p->instructions[*last].opcode==close && p->instructions[*last].depth==p->instructions[*first].depth)) { ++*last; }
        if (*last>=p->count) { return FALSE; }
        if (op==0x50 && *last+1<p->count && p->instructions[*last+1].opcode) { ++*last; }
    }
    return TRUE;
}
static void Write32(unsigned char *p,DWORD v)
{ p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v; }
static void WriteCell(unsigned char *p,unsigned type,unsigned length,DWORD arg)
{ memset(p,0,8);p[0]=(unsigned char)type;p[1]=(unsigned char)length;Write32(p+4,arg); }
BOOL BgVisEdit(BgDocument *doc,const BgFile *bg,BgVisEditRequest *r)
{
    BgVisProgram p={0},check={0};unsigned char added[32]={0},*data=NULL;
    DWORD at=0,remove=0,add=0,size,first,last;
    r->why="This background's commands cannot be edited safely.";
    if (!doc || !doc->rooms || !doc->viscommandsloaded) { return FALSE; }
    if (!BgVisDecodeDocument(bg,doc,&p,&r->why)) { return FALSE; }
    if (!BgVisCanEdit(&p)) { r->why="Fix malformed or unsupported command structure before editing this stream.";goto fail; }
    if ((p.count && r->row>=p.count) || (!p.count && (r->row || r->operation!=BG_VIS_INSERT)))
    { r->why="The selected command is no longer available.";goto fail; }
    if (p.count) { at=p.instructions[r->row].offset; }
    if (r->operation==BG_VIS_DELETE)
    {
        if (!BgVisDeleteRange(&p,r->row,&first,&last))
        { r->why="The final END and branch return cells are managed with their blocks.";goto fail; }
        at=p.instructions[first].offset;
        remove=p.instructions[last].offset+p.instructions[last].bytes-at;
    }
    else
    {
        const Opcode *op=Find(r->opcode);
        if (!op || !r->opcode || r->opcode==0x5c || r->opcode==0x52)
        { r->why="END, END IF, and CATCH are managed automatically with their blocks.";goto fail; }
        if (r->operation==BG_VIS_REPLACE)
        {
            unsigned old=p.instructions[r->row].opcode;
            if (!old || old==0x5a || old==0x5b || old==0x5c || old==0x50 || old==0x52
                || r->opcode==0x5a || r->opcode==0x5b || r->opcode==0x50 || BgVisReturnCell(&p,r->row))
            { r->why="Add or delete control blocks as a unit.";goto fail; }
            remove=p.instructions[r->row].bytes;
        }
        else if (r->operation==BG_VIS_INSERT)
        {
            if (p.count && r->after && p.instructions[r->row].opcode)
            { at+=p.instructions[r->row].bytes; }
            /* Insertion at a branch return cell goes after it, even when
             * 'before' was chosen: the runtime must continue to skip it. */
            for (DWORD n=1;n<p.count;n++) if (p.instructions[n].offset==at && BgVisReturnCell(&p,n))
            {
                if (!p.instructions[n].opcode)
                { r->why="This legacy branch returns through END. Insert inside the branch, before CATCH.";goto fail; }
                at+=p.instructions[n].bytes;break;
            }
        }
        else { r->why="Invalid BG command edit.";goto fail; }
        if (op->argument==BG_VIS_ONE_ROOM || op->argument==BG_VIS_ROOM_RANGE)
        {
            if (!r->arg[0] || r->arg[0]>doc->roomcount
                || (op->argument==BG_VIS_ROOM_RANGE && (r->arg[1]<r->arg[0] || r->arg[1]>doc->roomcount)))
            { r->why="Choose an existing room, or an inclusive range with the first room no greater than the last.";goto fail; }
        }
        WriteCell(added,r->opcode,op->length,op->argument==BG_VIS_VALUE ? r->arg[0] : 0);
        add=op->length*8;
        for (DWORD n=1;n<op->length;n++) { WriteCell(added+n*8,0x65,0,r->arg[n-1]); }
        if (op->argument==BG_VIS_ONE_PORTAL)
        {
            if (doc->portalwarning || r->arg[0]>=doc->portals.portalcount)
            { r->why="Choose an existing portal.";goto fail; }
            DWORD geometry=doc->portals.portals[r->arg[0]].geometryoffset;
            WriteCell(added+8,0x64,0,(geometry&BG_PORTAL_NEW_GEOMETRY) ? geometry : 0x0f000000u|geometry);
        }
        if (r->opcode==0x5a)
        {
            if (r->withElse) { WriteCell(added+add,0x5b,1,0);add+=8; }
            WriteCell(added+add,0x5c,1,0);add+=8;
        }
        if (r->opcode==0x50)
        {
            WriteCell(added+add,0x52,1,0);add+=8;
            WriteCell(added+add,0x02,1,0);add+=8; /* skipped return cell */
        }
    }
    size=p.size-remove+add+(p.count ? 0 : 8);
    if (size>BG_VIS_CELL_LIMIT*8) { r->why="The edited command stream is too large.";goto fail; }
    data=calloc(size,1);if (!data) { r->why="Out of memory editing BG commands.";goto fail; }
    if (at) { memcpy(data,p.data,at); }
    if (add) { memcpy(data+at,added,add); }
    if (p.size>at+remove) { memcpy(data+at+add,p.data+at+remove,p.size-at-remove); }
    if (!DecodeCells(data,size,0,bg,doc->portalwarning ? NULL : &doc->portals,doc->roomcount,&check,&r->why)) { goto fail; }
    if (!BgVisCanEdit(&check))
    { r->why="That position would break the command structure. ELSE must be inside an IF block that has no ELSE.";goto fail; }
    DWORD rooms=0;
    for (DWORD n=0;n<check.count;n++) { rooms+=check.instructions[n].opcode==0x20; }
    if (rooms>152)
    { r->why="The game has 152 explicit visible-room slots. Use at most 152 ADD ROOM commands in a stream.";goto fail; }
    r->selected=check.count ? check.count-1 : 0;
    for (DWORD n=0;n<check.count;n++) if (check.instructions[n].offset>=at) { r->selected=n;break; }
    free(doc->viscommands);doc->viscommands=data;doc->viscommandssize=size;doc->dirty=TRUE;
    BgVisFree(&p);BgVisFree(&check);r->why="";return TRUE;
fail:
    free(data);BgVisFree(&p);BgVisFree(&check);return FALSE;
}
