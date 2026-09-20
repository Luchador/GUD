#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "patrolpaths.h"
#include "actionblocks.h"
#include "setupmeta.h"

#define R SetupMetaRead32
#define W SetupMetaWrite32
#define PATROL_SETUP_MAX (16u * 1024u * 1024u)
/* SimpleGuard watches/hears Bond without idle animations interrupting patrol.
 * It hands over to the standard combat behavior when alerted. */
#define PATROL_GUARD_AI 7u
static BOOL Fail(const char **why, const char *text) { *why=text; return FALSE; }
static BOOL Range(const SetupFile *s, DWORD at, DWORD size)
{ return at>=40 && !(at&3) && at<=s->size && size<=s->size-at; }
void PatrolDocumentFree(PatrolDocument *d)
{
    for (DWORD i=0;i<d->count;i++) { free(d->paths[i].points); }
    free(d->pads); memset(d,0,sizeof(*d));
}
BOOL PatrolDocumentLoad(const SetupFile *s, PatrolDocument *d, const char **why)
{
    DWORD table,at,total=0; unsigned char ids[256]={0};
    memset(d,0,sizeof(*d));
    if (!s || !s->data || s->size<40 || s->size>PATROL_SETUP_MAX || (s->padcount && !s->pads)) { goto invalid; }
    table=R(s->data);
    if (table)
    {
        at=table;
        for (;;at+=16)
        {
            if (!Range(s,at,16)) { goto invalid; }
            DWORD pad=R(s->data+at);
            if (pad&0x80000000u) { break; }
            if (d->waypointcount==65536 || pad>=s->padcount || s->pads[pad].deleted) { goto invalid; }
            d->waypointcount++;
        }
        if (d->waypointcount)
        {
            d->pads=malloc(d->waypointcount*sizeof(*d->pads));
            if (!d->pads) { goto memory; }
            for (DWORD i=0;i<d->waypointcount;i++) { d->pads[i]=R(s->data+table+i*16); }
        }
    }
    table=R(s->data+16);
    if (table) for (;;table+=8)
    {
        if (!Range(s,table,8)) { goto invalid; }
        at=R(s->data+table);
        if (!at) { break; }
        unsigned int id=s->data[table+4];
        if (d->count==PATROL_MAX_PATHS || ids[id]) { goto invalid; }
        ids[id]=1;
        PatrolPath *p=&d->paths[d->count++];
        p->id=id; p->flags=s->data[table+5];
        p->length=(unsigned short)((s->data[table+6]<<8)|s->data[table+7]);
        DWORD start=at;
        for (;;at+=4)
        {
            if (!Range(s,at,4)) { goto invalid; }
            if (total++==65536) { goto invalid; } /* Includes each route terminator, as the preview does. */
            DWORD waypoint=R(s->data+at);
            if (waypoint&0x80000000u) { break; }
            if (waypoint>=d->waypointcount || p->count==PATROL_MAX_POINTS) { goto invalid; }
            p->count++;
        }
        if (p->count)
        {
            p->points=malloc(p->count*sizeof(*p->points));
            if (!p->points) { goto memory; }
            for (DWORD i=0;i<p->count;i++) { p->points[i]=R(s->data+start+i*4); }
        }
    }
    *why=""; return TRUE;
invalid:
    PatrolDocumentFree(d); return Fail(why,"The patrol/waypoint table is invalid, too large, or contains duplicate patrol IDs.");
memory:
    PatrolDocumentFree(d); return Fail(why,"Out of memory reading patrol paths.");
}
static int Starter(const ActionBlock *b)
{
    if (b->count!=3 || b->instructions[0].size!=2 || b->instructions[0].bytes[0]!=0x20
        || b->instructions[1].size!=4 || b->instructions[1].bytes[0]!=5
        || b->instructions[1].bytes[1]!=253 || b->instructions[2].bytes[0]!=4) { return PATROL_CUSTOM; }
    DWORD ai=ActionReadValue(&b->instructions[1],1);
    return ai==2 || ai==5 || ai==PATROL_GUARD_AI ? b->instructions[0].bytes[1] : PATROL_CUSTOM;
}
static BOOL OwnedStarter(const ActionBlock *b)
{
    char name[ACTION_NAME_SIZE]; int path=Starter(b);
    if (b->global || path<0 || ActionReadValue(&b->instructions[1],1)!=PATROL_GUARD_AI) { return FALSE; }
    snprintf(name,sizeof(name),"GEditor patrol %d",path);
    return !strcmp(name,b->name);
}
static int AssignedPath(const ActionDocument *actions, DWORD id)
{
    /* Shared behavior IDs and local setup blocks use separate ID ranges. */
    if (id==1 || id==2 || id==5 || id==PATROL_GUARD_AI) { return PATROL_NONE; }
    for (DWORD i=0;i<actions->count;i++) if (actions->blocks[i].id==id)
    { return actions->blocks[i].disabled ? PATROL_CUSTOM : Starter(&actions->blocks[i]); }
    return PATROL_CUSTOM;
}
static BOOL BlockReferenced(const ActionDocument *actions, const SetupFile *s, DWORD id)
{
    if (SetupFileGlobalBlockReference(s,id)) { return TRUE; }
    for (DWORD i=0;i<actions->charactercount;i++)
    { if (!s->characters[i].deleted && actions->assignments[i]==id) { return TRUE; } }
    for (DWORD i=0;i<s->objectcount;i++)
    {
        const SetupObject *o=&s->objects[i];
        if (!o->deleted && (o->type==39 || o->type==40) && Range(s,o->sourceoffset,0x84)
            && R(s->data+o->sourceoffset+0x80)==id) { return TRUE; }
    }
    for (DWORD b=0;b<actions->count;b++) for (DWORD i=0;i<actions->blocks[b].count;i++)
    {
        const ActionInstruction *ins=&actions->blocks[b].instructions[i];
        const ActionOpcode *op=&g_ActionOpcodes[ins->bytes[0]];
        for (int p=0;p<op->paramcount;p++)
        { if (op->params[p].kind==ACTION_BLOCK && ActionReadValue(ins,p)==id) { return TRUE; } }
    }
    return FALSE;
}
static void PruneStarters(ActionDocument *actions, const SetupFile *s)
{
    const char *unused;
    for (DWORD i=actions->count;i-->0;)
    {
        if (OwnedStarter(&actions->blocks[i])
            && !SetupFileGlobalBlockReference(s,actions->blocks[i].id))
        { ActionDocumentDeleteBlock(actions,s,i,&unused); } /* Keeps assigned/referenced starters. */
    }
}
static BOOL PathReferenced(const ActionDocument *actions, const SetupFile *s, unsigned int id)
{
    if (SetupFileGlobalPatrolReference(s,id)) { return TRUE; }
    for (DWORD b=0;b<actions->count;b++) for (DWORD i=0;i<actions->blocks[b].count;i++)
    {
        const ActionInstruction *ins=&actions->blocks[b].instructions[i];
        /* Disabled blocks still own their references: they may be re-enabled. */
        if ((ins->bytes[0]==0x20 || ins->bytes[0]==0xcb) && ins->bytes[1]==id) { return TRUE; }
    }
    return FALSE;
}
BOOL PatrolDocumentAdd(PatrolDocument *d, const SetupFile *s, DWORD *index, const char **why)
{
    ActionDocument actions={0}; unsigned int id;
    if (d->count>=PATROL_MAX_PATHS) { return Fail(why,"All 256 patrol slots are in use."); }
    if (!ActionDocumentLoad(s,&actions,why)) { return FALSE; }
    for (id=0;id<256;id++)
    {
        DWORD i;
        for (i=0;i<d->count && d->paths[i].id!=id;i++) {}
        if (i==d->count && !PathReferenced(&actions,s,id)) { break; }
    }
    ActionDocumentFree(&actions);
    if (id==256) { return Fail(why,"No free patrol IDs remain; Action Block references also reserve IDs."); }
    *index=d->count++; d->paths[*index]=(PatrolPath){.id=id,.changed=TRUE};
    d->changed=TRUE; return TRUE;
}
BOOL PatrolDocumentDelete(PatrolDocument *d, const SetupFile *s, DWORD index, const char **why)
{
    ActionDocument actions={0}; BOOL used;
    if (index>=d->count) { return Fail(why,"Select a patrol path first."); }
    if (!ActionDocumentLoad(s,&actions,why)) { return FALSE; }
    PruneStarters(&actions,s);
    used=PathReferenced(&actions,s,d->paths[index].id); ActionDocumentFree(&actions);
    if (used) { return Fail(why,"This patrol is used by an Action Block. Remove guard assignments or edit the referencing block before deleting it."); }
    free(d->paths[index].points);
    memmove(d->paths+index,d->paths+index+1,(d->count-index-1)*sizeof(*d->paths));
    d->count--; memset(d->paths+d->count,0,sizeof(*d->paths)); d->changed=TRUE; return TRUE;
}
BOOL PatrolPathInsert(PatrolDocument *d, DWORD path, DWORD before, DWORD waypoint, const char **why)
{
    DWORD total=d->count;
    if (path>=d->count || waypoint>=d->waypointcount || before>d->paths[path].count)
    { return Fail(why,"Choose a path and a navigation pad."); }
    for (DWORD i=0;i<d->count;i++) { total+=d->paths[i].count; }
    PatrolPath *p=&d->paths[path];
    if (p->count>=PATROL_MAX_POINTS || total>=65536) { return Fail(why,"The patrol point limit has been reached."); }
    DWORD *points=realloc(p->points,(p->count+1)*sizeof(*points));
    if (!points) { return Fail(why,"Out of memory adding a patrol point."); }
    p->points=points;
    memmove(points+before+1,points+before,(p->count-before)*sizeof(*points));
    points[before]=waypoint; p->count++; p->changed=d->changed=TRUE; return TRUE;
}
BOOL PatrolPathRemove(PatrolDocument *d, DWORD path, DWORD point)
{
    if (path>=d->count || point>=d->paths[path].count) { return FALSE; }
    PatrolPath *p=&d->paths[path];
    memmove(p->points+point,p->points+point+1,(p->count-point-1)*sizeof(*p->points));
    p->count--; p->changed=d->changed=TRUE; return TRUE;
}
BOOL PatrolPathMove(PatrolDocument *d, DWORD path, DWORD point, int direction)
{
    if (path>=d->count || (direction!=-1 && direction!=1)) { return FALSE; }
    PatrolPath *p=&d->paths[path]; DWORD other=point+direction;
    if (point>=p->count || other>=p->count) { return FALSE; }
    DWORD swap=p->points[point]; p->points[point]=p->points[other]; p->points[other]=swap;
    p->changed=d->changed=TRUE; return TRUE;
}
BOOL PatrolPathSetLoop(PatrolDocument *d, DWORD path, BOOL loop)
{
    if (path>=d->count) { return FALSE; }
    PatrolPath *p=&d->paths[path]; unsigned char flags=(p->flags&~1u)|(loop!=FALSE);
    if (flags!=p->flags) { p->flags=flags; p->changed=d->changed=TRUE; }
    return TRUE;
}
static BOOL ValidPath(const PatrolDocument *d, const PatrolPath *p, const char **why)
{
    if (p->count<2 || p->count>PATROL_MAX_POINTS || !p->points)
    { return Fail(why,"Each edited or assigned patrol needs at least two points."); }
    BOOL distinct=FALSE;
    for (DWORD i=0;i<p->count;i++)
    {
        if (p->points[i]>=d->waypointcount) { return Fail(why,"A patrol point is not a navigation waypoint."); }
        if (i && d->pads[p->points[i]]!=d->pads[p->points[0]]) { distinct=TRUE; }
    }
    return distinct || Fail(why,"A patrol needs at least two different navigation pads.");
}
BOOL PatrolDocumentCompile(const PatrolDocument *d, const SetupFile *s, SetupFile *out, const char **why)
{
    PatrolDocument original={0}; ActionDocument actions={0}; SetupFile copy={0};
    unsigned char ids[256]={0},*data=NULL; DWORD total=d->count,table,at; BOOL ok=FALSE;
    memset(out,0,sizeof(*out));
    if (!d->changed) { return SetupFileClone(s,out,why); }
    if (!PatrolDocumentLoad(s,&original,why) || !ActionDocumentLoad(s,&actions,why)) { goto done; }
    if (d->count>PATROL_MAX_PATHS || d->waypointcount!=original.waypointcount
        || (d->waypointcount && memcmp(d->pads,original.pads,d->waypointcount*sizeof(*d->pads))))
    { Fail(why,"The navigation pads changed while patrols were being edited."); goto done; }
    for (DWORD i=0;i<d->count;i++)
    {
        const PatrolPath *p=&d->paths[i];
        if (ids[p->id] || p->count>PATROL_MAX_POINTS || p->count>65536-total)
        { Fail(why,"Duplicate patrol IDs or too many patrol points."); goto done; }
        ids[p->id]=1; total+=p->count;
        if (p->changed && !ValidPath(d,p,why)) { goto done; }
    }
    PruneStarters(&actions,s);
    for (DWORD i=0;i<original.count;i++) if (!ids[original.paths[i].id]
        && PathReferenced(&actions,s,original.paths[i].id))
    { Fail(why,"An Action Block still references a deleted patrol."); goto done; }
    if (!(actions.changed ? ActionDocumentCompile(&actions,s,&copy,why) : SetupFileClone(s,&copy,why))) { goto done; }
    table=(copy.size+3u)&~3u; at=table+(d->count+1)*8;
    total=at+total*4;
    if (total>PATROL_SETUP_MAX) { Fail(why,"The edited patrols exceed the setup size limit."); goto done; }
    data=calloc(total,1);
    if (!data) { Fail(why,"Out of memory writing patrol paths."); goto done; }
    memcpy(data,copy.data,copy.size); W(data+16,table);
    for (DWORD i=0;i<d->count;i++)
    {
        const PatrolPath *p=&d->paths[i]; DWORD entry=table+i*8;
        W(data+entry,at); data[entry+4]=p->id; data[entry+5]=p->flags;
        if (!p->changed) { data[entry+6]=p->length>>8; data[entry+7]=p->length; }
        for (DWORD j=0;j<p->count;j++,at+=4) { W(data+at,p->points[j]); }
        W(data+at,0xffffffffu); at+=4;
    }
    free(copy.data); copy.data=data; data=NULL; copy.size=total;
    if (!SetupFileCompact(&copy,why)) { goto done; }
    copy.dirty=TRUE; *out=copy; memset(&copy,0,sizeof(copy)); ok=TRUE; *why="";
done:
    free(data); SetupFileFree(&copy); ActionDocumentFree(&actions); PatrolDocumentFree(&original); return ok;
}
int PatrolCharacterPath(const SetupFile *s, DWORD character)
{
    ActionDocument actions={0}; const char *why; int path=PATROL_CUSTOM;
    if (!s || character>=s->charactercount || s->characters[character].deleted) { return PATROL_CUSTOM; }
    DWORD id=s->characters[character].ailistid;
    if (!ActionDocumentLoad(s,&actions,&why)) { return PATROL_CUSTOM; }
    path=AssignedPath(&actions,id);
    ActionDocumentFree(&actions); return path;
}
BOOL PatrolAssignCharacter(const SetupFile *s, const PatrolAssignment *edit,
    SetupFile *out, BOOL *changed, const char **why)
{
    PatrolDocument paths={0}; ActionDocument actions={0}; BOOL ok=FALSE;
    DWORD block; memset(out,0,sizeof(*out)); *changed=FALSE;
    if (!s || !s->data || !edit || edit->path<PATROL_NONE || edit->path>255
        || edit->characterindex>=s->charactercount) { return Fail(why,"The patrol assignment is invalid."); }
    const SetupCharacter *chr=&s->characters[edit->characterindex];
    if (chr->deleted || chr->sourceoffset!=edit->sourceoffset || chr->chrnum!=edit->chrnum
        || chr->ailistid!=edit->previous || !Range(s,chr->sourceoffset,28)
        || s->data[chr->sourceoffset+3]!=9
        || (R(s->data+chr->sourceoffset+8)&0xffff)!=chr->ailistid)
    { return Fail(why,"The character changed while its patrol was being selected."); }
    if (!ActionDocumentLoad(s,&actions,why)) { goto done; }
    if (edit->path==PATROL_NONE)
    {
        /* None cancels this explicit starting patrol; ordinary/custom AI is
         * untouched if the user has not assigned one through this control. */
        if (AssignedPath(&actions,chr->ailistid)==PATROL_NONE) { ok=TRUE; goto done; }
        actions.assignments[edit->characterindex]=SETUP_BEHAVIOR_STANDARD_GUARD;
        actions.changed=TRUE;
    }
    else
    {
        if (!PatrolDocumentLoad(s,&paths,why)) { goto done; }
        DWORD p;
        for (p=0;p<paths.count && paths.paths[p].id!=edit->path;p++) {}
        if (p==paths.count) { Fail(why,"The selected patrol no longer exists."); goto done; }
        if (!ValidPath(&paths,&paths.paths[p],why)) { goto done; }
        if (AssignedPath(&actions,chr->ailistid)==edit->path) { ok=TRUE; goto done; }
        for (block=0;block<actions.count;block++)
        {
            const ActionBlock *b=&actions.blocks[block]; DWORD earlier;
            for (earlier=0;earlier<block && actions.blocks[earlier].id!=b->id;earlier++) {}
            if (earlier==block && b->id>=0x401 && b->id<0x1000 && !b->disabled
                && OwnedStarter(b) && Starter(b)==edit->path) { break; }
        }
        if (block==actions.count)
        {
            if (!ActionDocumentAddBlock(&actions,(DWORD)-1,FALSE,&block,why)) { goto done; }
            /* Do not commandeer a missing ID still mentioned by shared AI. */
            DWORD id=actions.blocks[block].id;
            for (;id<0x1000;id++)
            {
                DWORD i;
                for (i=0;i<block && actions.blocks[i].id!=id;i++) {}
                if (i==block && !BlockReferenced(&actions,s,id)) { break; }
            }
            if (id==0x1000) { Fail(why,"No free patrol behavior IDs remain."); goto done; }
            actions.blocks[block].id=id;
            if (!ActionBlockDelete(&actions,block,2,why) || !ActionBlockDelete(&actions,block,1,why)
                || !ActionBlockDelete(&actions,block,0,why)
                || !ActionBlockInsert(&actions,block,0,0x20,why)
                || !ActionBlockInsert(&actions,block,1,5,why)) { goto done; }
            DWORD values[8]={0}; values[0]=edit->path;
            if (!ActionInstructionSet(&actions,block,0,values,0,"","","",why)) { goto done; }
            values[0]=253; values[1]=PATROL_GUARD_AI;
            if (!ActionInstructionSet(&actions,block,1,values,0,"","","",why)) { goto done; }
            snprintf(actions.blocks[block].name,ACTION_NAME_SIZE,"GEditor patrol %d",edit->path);
        }
        if (!ActionDocumentAssign(&actions,edit->characterindex,block,why)) { goto done; }
    }
    PruneStarters(&actions,s);
    if (!ActionDocumentCompile(&actions,s,out,why) || !SetupFileCompact(out,why)) { SetupFileFree(out); goto done; }
    *changed=TRUE; ok=TRUE;
done:
    ActionDocumentFree(&actions); PatrolDocumentFree(&paths); if (ok) { *why=""; } return ok;
}
