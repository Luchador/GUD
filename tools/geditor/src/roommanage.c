#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <src/propconstants.h>
#include <src/doorshadowformat.h>
#include "roommanage.h"
#include "roomedit.h"
#include "bgcommands.h"
#include "actionblocks.h"

static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static void Write32(unsigned char *p, DWORD n)
{ p[0]=n>>24; p[1]=n>>16; p[2]=n>>8; p[3]=n; }
static BOOL Error(char *why, size_t size, const char *text)
{ snprintf(why,size,"%s",text); return FALSE; }

BOOL RoomManageAdd(BgDocument *bg, DWORD *created, const char **why)
{
    *why="Open a room-based background first.";
    if (!bg || !bg->rooms || !bg->roomcount) return FALSE;
    *why="The game supports at most 138 rooms (room zero is reserved).";
    if (bg->roomcount>=STAN_MAX_ROOM) return FALSE;
    *why="Out of memory creating a room.";
    /* A bounds-only vertex keeps native empty-room bounds finite. It is not
     * rendered, and export compaction removes it once real faces are added. */
    BgDocumentVertex *v=calloc(1,sizeof(*v));
    if (!v) return FALSE;
    BgDocumentRoom *rooms=realloc(bg->rooms,(bg->roomcount+2)*sizeof(*rooms));
    if (!rooms) { free(v); return FALSE; }
    bg->rooms=rooms; *created=++bg->roomcount;
    memset(&rooms[*created],0,sizeof(*rooms));
    v->id=bg->nextvertexid++; v->room=(unsigned short)*created;
    rooms[*created].vertices=v; rooms[*created].vertexcount=1;
    rooms[*created].layers[0].sourcepresent=TRUE;
    bg->dirty=TRUE; *why=""; return TRUE;
}

static DWORD PadRoom(const SetupFile *s, const StanFile *stan, float scale, DWORD value)
{
    const SetupPad *pad;
    if (value==9000) return 0; /* Character's runtime pad preset. */
    if (value>=10000) {
        value-=10000;
        pad=value<s->boundpadcount ? &s->boundpads[value].pad : NULL;
    } else pad=value<s->padcount ? &s->pads[value] : NULL;
    if (!pad) return 0;
    /* A script reference remains live even if its pad was marked deleted. */
    SetupPad live=*pad; live.deleted=FALSE;
    return RoomEditPadRoom(stan,&live,scale);
}

static BOOL Visibility(const BgDocument *bg,const BgFile *source,DWORD room,
    BgVisProgram *vis,char *why,size_t size)
{
    const char *error="";
    if (!BgVisDecodeDocument(source,bg,vis,&error)) return Error(why,size,error);
    if (!vis->complete || vis->warnings || vis->problem[0] || vis->singleDisplayList)
        return Error(why,size,"Cannot verify the global visibility commands. Repair their warnings before removing a room.");
    for (DWORD i=0;i<vis->count;i++) {
        BgVisInstruction *c=&vis->instructions[i];
        if ((c->argument==BG_VIS_ONE_ROOM && c->arg[0]==room)
            || (c->argument==BG_VIS_ROOM_RANGE && c->arg[0]<=room && room<=c->arg[1])) {
            snprintf(why,size,"Room %lu is referenced by global visibility command %lu (%s). Edit that command first.",
                (unsigned long)room,(unsigned long)i,BgVisName(c->opcode));
            return FALSE;
        }
    }
    return TRUE;
}

BOOL RoomManageCanRemove(const BgDocument *bg,const BgFile *source,
    const SetupFile *s,const StanFile *stan,DWORD room,char *why,size_t size)
{
    ActionDocument actions={0}; BgVisProgram vis={0}; const char *error="";
    if (!bg || !bg->rooms || !room || room>bg->roomcount || !s || !s->data || !stan
        || (stan->tilecount && (!stan->tiles || !stan->data)) || (s->padcount && !s->pads)
        || (s->boundpadcount && !s->boundpads) || (s->objectcount && !s->objects)
        || !isfinite(bg->levelscale) || bg->levelscale<=0)
        return Error(why,size,"Select a valid room in a loaded level.");
    if (bg->roomcount<=1) return Error(why,size,"The level must retain at least one room.");
    if (!s->globalrefs)
        return Error(why,size,"Cannot verify shared Action Blocks. Rebase onto a current GUD ROM before removing rooms.");
    if (!ActionDocumentLoad(s,&actions,&error)) return Error(why,size,error);
    /* Room-related AI commands refer to pads, not literal room IDs. Check
     * every block, including disabled/unassigned blocks and packed aim pads. */
    for (DWORD b=0;b<actions.count;b++) for (DWORD i=0;i<actions.blocks[b].count;i++) {
        ActionInstruction *ins=&actions.blocks[b].instructions[i];
        for (unsigned p=0;p<g_ActionOpcodes[ins->bytes[0]].paramcount;p++)
            if (ActionParameterIsPad(ins,p) && PadRoom(s,stan,bg->levelscale,ActionReadValue(ins,p))==room) {
                snprintf(why,size,"Room %lu is referenced through a pad by Action Block 0x%04lX, instruction %lu.",
                    (unsigned long)room,(unsigned long)actions.blocks[b].id,(unsigned long)i);
                ActionDocumentFree(&actions); return FALSE;
            }
    }
    ActionDocumentFree(&actions);
    for (DWORD bound=0;bound<2;bound++) for (DWORD i=0;i<(bound?s->boundpadcount:s->padcount);i++) {
        const SetupPad *pad=bound?&s->boundpads[i].pad:&s->pads[i];
        DWORD value=i+(bound?10000:0);
        BOOL shared=SetupFileGlobalPadReference(s,value);
        if ((!pad->deleted || shared) && PadRoom(s,stan,bg->levelscale,value)==room) {
            snprintf(why,size,shared ? "Room %lu is referenced by a shared Action Block through %s pad %lu."
                : "Room %lu is referenced by %s pad %lu. Move or remove that pad first.",
                (unsigned long)room,bound?"bound":"ordinary",(unsigned long)i);
            return FALSE;
        }
    }
    BOOL ok=Visibility(bg,source,room,&vis,why,size); BgVisFree(&vis);
    if (!ok) return FALSE;
    if (bg->portalwarning) return Error(why,size,"Repair the background's portal data before removing rooms.");
    for (DWORD i=0;i<bg->portals.portalcount;i++) {
        const BgPortal *p=&bg->portals.portals[i];
        if (p->connectedroom1==room || p->connectedroom2==room) {
            snprintf(why,size,"Room %lu is connected to portal %lu. Relink or remove that portal first.",(unsigned long)room,(unsigned long)i);
            return FALSE;
        }
    }
    for (DWORD i=0;i<s->objectcount;i++) if (!s->objects[i].deleted && s->objects[i].type==PROPDEF_DOOR_SHADOW) {
        DWORD at=s->objects[i].sourceoffset;
        if (at>s->size || s->size-at<DOOR_SHADOW_BYTES)
            return Error(why,size,"A door shadow has incomplete room data.");
        if (Read32(s->data+at+DOOR_SHADOW_ROOM)==room)
            return Error(why,size,"A door shadow belongs to this room. Remove or reassign it first.");
    }
    if (bg->rooms[room].facecount) return Error(why,size,"Move or delete this room's background faces before removing the room.");
    for (DWORD i=0;i<stan->tilecount;i++) {
        const StanTile *tile=&stan->tiles[i];
        if (tile->room==room)
            return Error(why,size,"Move or delete this room's STAN tiles before removing the room.");
        DWORD bytes=8u+tile->pointcount*8u,at=tile->sourceoffset;
        if (tile->pointcount<3 || tile->pointcount>STAN_TILE_MAX_POINTS
            || at>stan->size || bytes>stan->size-at
            || Read32(stan->data+at)!=(tile->id<<8|tile->room)
            || stan->data[at+6]>>4!=tile->pointcount)
            return Error(why,size,"The STAN tile records are inconsistent. Repair them before removing rooms.");
    }
    if (bg->roomcount>STAN_MAX_ROOM) return Error(why,size,"The background exceeds the game's room limit.");
    why[0]=0; return TRUE;
}

BOOL RoomManageRemove(BgDocument *bg,const BgFile *source,SetupFile *s,
    StanFile *stan,DWORD room,char *why,size_t size)
{
    BgVisProgram vis={0};
    if (!RoomManageCanRemove(bg,source,s,stan,room,why,size)) return FALSE;
    if (!Visibility(bg,source,room,&vis,why,size)) { BgVisFree(&vis); return FALSE; }
    /* All allocation/validation precedes mutation. Keep literal portal IDs,
     * polygon addresses, pad indices and native STAN IDs unchanged. */
    for (DWORD i=0;i<vis.count;i++) {
        BgVisInstruction *c=&vis.instructions[i];
        DWORD count=c->argument==BG_VIS_ROOM_RANGE?2:c->argument==BG_VIS_ONE_ROOM?1:0;
        for (DWORD a=0;a<count;a++) if (c->arg[a]>room)
            Write32(vis.data+c->offset-vis.offset+8*(a+1)+4,c->arg[a]-1);
    }
    free(bg->viscommands); bg->viscommands=vis.data; vis.data=NULL;
    bg->viscommandssize=vis.size; bg->viscommandsloaded=TRUE; BgVisFree(&vis);
    BgDocumentRoom *removed=&bg->rooms[room];
    free(removed->vertices); free(removed->faces);
    for (int l=0;l<2;l++) {
        for (DWORD i=0;i<removed->layers[l].groupcount;i++) free(removed->layers[l].groups[i].commands);
        free(removed->layers[l].groups);
    }
    memmove(removed,removed+1,(bg->roomcount-room)*sizeof(*removed));
    memset(&bg->rooms[bg->roomcount],0,sizeof(*removed)); bg->roomcount--;
    for (DWORD r=room;r<=bg->roomcount;r++) {
        for (DWORD i=0;i<bg->rooms[r].vertexcount;i++) bg->rooms[r].vertices[i].room=(unsigned short)r;
        for (DWORD i=0;i<bg->rooms[r].facecount;i++) bg->rooms[r].faces[i].room=(unsigned short)r;
    }
    for (DWORD i=0;i<bg->portals.portalcount;i++) {
        BgPortal *p=&bg->portals.portals[i];
        if (p->connectedroom1>room) p->connectedroom1--;
        if (p->connectedroom2>room) p->connectedroom2--;
    }
    for (DWORD i=0;i<stan->tilecount;i++) if (stan->tiles[i].room>room) {
        stan->tiles[i].room--;
        stan->data[stan->tiles[i].sourceoffset+3]=stan->tiles[i].room;
        stan->dirty=TRUE;
    }
    for (DWORD i=0;i<s->objectcount;i++) if (!s->objects[i].deleted && s->objects[i].type==PROPDEF_DOOR_SHADOW) {
        unsigned char *p=s->data+s->objects[i].sourceoffset+DOOR_SHADOW_ROOM;
        DWORD n=Read32(p); if (n>room) { Write32(p,n-1); s->dirty=TRUE; }
    }
    bg->dirty=TRUE; why[0]=0; return TRUE;
}
