#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "levelissues.h"
#include "patrolpaths.h"
#include "../../../src/propconstants.h"

#define ISSUE_LIMIT 10000u

void LevelIssuesFree(LevelIssueReport *report)
{ free(report->items); memset(report, 0, sizeof(*report)); }

static BOOL Add(LevelIssueReport *report, const LevelIssue *issue)
{
    if (report->count == ISSUE_LIMIT) { report->truncated = TRUE; return TRUE; }
    if (report->count == report->capacity)
    {
        DWORD capacity = report->capacity ? report->capacity * 2 : 32;
        if (capacity > ISSUE_LIMIT) { capacity = ISSUE_LIMIT; }
        LevelIssue *grown = realloc(report->items, capacity * sizeof(*grown));
        if (!grown) { return FALSE; }
        report->items = grown; report->capacity = capacity;
    }
    report->items[report->count++] = *issue;
    if (issue->error) { report->errors++; } else { report->warnings++; }
    return TRUE;
}

static const SetupPad *Pad(const SetupFile *setup, SetupPadRef ref)
{
    if (!setup) { return NULL; }
    if (ref.bound) { return setup->boundpads && ref.index < setup->boundpadcount ? &setup->boundpads[ref.index].pad : NULL; }
    return setup->pads && ref.index < setup->padcount ? &setup->pads[ref.index] : NULL;
}

static BOOL Position(const SetupPad *pad, float scale, float pos[3])
{
    if (!pad || pad->deleted || !isfinite(scale) || scale <= 0) { return FALSE; }
    for (int axis = 0; axis < 3; axis++)
    {
        pos[axis] = pad->pos[axis] * (1.0f / scale);
        if (!isfinite(pos[axis])) { return FALSE; }
    }
    return TRUE;
}

/* Only placement records own pads. A carried gun's pad field is a character
 * ID; interpreting it as a pad would select an unrelated object. */
static void PadOwner(const SetupFile *setup, LevelIssue *issue)
{
    DWORD owners = 0, selection = 0;
    for (DWORD kind = 0; kind < 2; kind++)
    {
        DWORD count = kind ? setup->charactercount : setup->objectcount;
        for (DWORD i = 0; i < count; i++)
        {
            SetupPadRef ref;
            DWORD candidate = i | (kind ? SETUP_CHARACTER_SELECTION_BIT : 0);
            if (!kind && setup->objects[i].type != PROPDEF_DOOR
                && (setup->objects[i].flags & (PROPFLAG_INSIDEANOTHEROBJ | PROPFLAG_ASSIGNEDTOCHR))) { continue; }
            if (SetupFileGetModelPad(setup, candidate, &ref)
                && ref.bound == issue->pad.bound && ref.index == issue->pad.index)
            { owners++; selection = candidate; }
        }
    }
    if (owners == 1)
    {
        issue->target = LEVEL_ISSUE_MODEL; issue->index = selection;
        if (selection & SETUP_CHARACTER_SELECTION_BIT)
            snprintf(issue->subject, sizeof(issue->subject), "%s pad %lu / Guard %u",
                issue->pad.bound ? "Bound" : "Ordinary", (unsigned long)issue->pad.index,
                setup->characters[selection & ~SETUP_CHARACTER_SELECTION_BIT].chrnum);
        else
            snprintf(issue->subject, sizeof(issue->subject), "%s pad %lu / Object %lu",
                issue->pad.bound ? "Bound" : "Ordinary", (unsigned long)issue->pad.index, (unsigned long)selection);
    }
}

BOOL LevelIssuesBuild(const BgDocument *bg, const SetupFile *setup,
    const StanFile *stan, float scale, LevelIssueReport *out, const char **why)
{
    StanFile saved = {0}; unsigned char *bytes = NULL; DWORD size;
    BOOL canresolve = FALSE;
    memset(out, 0, sizeof(*out));
    *why = "The level's decoded data is incomplete.";
    if (!setup || !stan || !bg || (setup->padcount && !setup->pads)
        || (setup->boundpadcount && !setup->boundpads) || (setup->objectcount && !setup->objects)
        || (setup->charactercount && !setup->characters) || (stan->tilecount && !stan->tiles)
        || (bg->portals.portalcount && !bg->portals.portals)) { return FALSE; }
    if (!isfinite(scale) || scale <= 0)
    { *why = "The level scale is invalid; issue locations cannot be calculated."; return FALSE; }
    if (stan->data && stan->tilecount)
    {
        const char *reason = "";
        /* Match save/export's room ordering and duplicate-name lookup. */
        canresolve = StanPrepareSave(stan, &bytes, &size, &reason)
            && StanLoadNative(bytes, size, scale, &saved, &reason);
        free(bytes);
        if (!canresolve)
        {
            LevelIssue issue = {0}; issue.kind = LEVEL_ISSUE_STAN_DATA; issue.error = TRUE;
            strcpy(issue.subject, "STAN data");
            snprintf(issue.description, sizeof(issue.description), "Could not check the saved STAN layout: %s", reason);
            if (!Add(out, &issue)) { goto memory; }
        }
    }
    else if (setup->padcount || setup->boundpadcount)
    {
        LevelIssue issue = {0}; issue.kind = LEVEL_ISSUE_MISSING_STAN;
        strcpy(issue.subject, "Pad resolution");
        strcpy(issue.description, "No STAN collision data is loaded. Pad references could not be checked.");
        if (!Add(out, &issue)) { goto memory; }
    }
    for (DWORD bound = 0; bound < 2; bound++)
    {
        DWORD count = bound ? setup->boundpadcount : setup->padcount;
        for (DWORD i = 0; i < count; i++)
        {
            LevelIssue issue = {0}; float pos[3]; char name[16];
            issue.pad = (SetupPadRef){i, bound != 0};
            const SetupPad *pad = Pad(setup, issue.pad);
            if (pad->deleted) { continue; }
            snprintf(issue.subject, sizeof(issue.subject), "%s pad %lu", bound ? "Bound" : "Ordinary", (unsigned long)i);
            if (!Position(pad, scale, pos))
            {
                issue.kind = LEVEL_ISSUE_PAD_POSITION; issue.error = TRUE;
                strcpy(issue.description, "The pad has a non-finite position. It cannot be located safely in the viewport.");
            }
            else
            {
                if (!canresolve || StanResolveSavedPadName(&saved, pad->stanname, pos, name)) { continue; }
                issue.kind = LEVEL_ISSUE_UNRESOLVED_PAD; issue.target = LEVEL_ISSUE_PAD;
                PadOwner(setup, &issue);
                snprintf(issue.description, sizeof(issue.description),
                    "Cannot resolve this pad against STAN (current reference: %s). Check floor coverage and the pad position. Save/export preserves it unchanged.",
                    pad->stanname[0] ? pad->stanname : "none");
            }
            if (!Add(out, &issue)) { goto memory; }
        }
    }
    for (DWORD i = 0; i < stan->tilecount; i++)
    {
        const StanTile *tile = &stan->tiles[i]; LevelIssue issue = {0};
        issue.index = i; issue.target = LEVEL_ISSUE_STAN; issue.error = TRUE;
        snprintf(issue.subject, sizeof(issue.subject), "STAN tile %lu (room %u)", (unsigned long)i, tile->room);
        if (bg->rooms && tile->room > bg->roomcount)
        {
            issue.kind = LEVEL_ISSUE_STAN_ROOM;
            snprintf(issue.description, sizeof(issue.description), "Room %u does not exist in the background.", tile->room);
            if (!Add(out, &issue)) { goto memory; }
        }
        for (DWORD edge = 0; edge < tile->pointcount && edge < STAN_TILE_MAX_POINTS; edge++)
        {
            if (tile->points[edge].link >= 0x10 && StanLinkedTile(stan, tile->points[edge].link) == STAN_TILE_NONE)
            {
                issue.kind = LEVEL_ISSUE_STAN_LINK;
                snprintf(issue.description, sizeof(issue.description), "Edge %lu links to a missing STAN tile (0x%04X).", (unsigned long)edge, tile->points[edge].link);
                if (!Add(out, &issue)) { goto memory; }
                break;
            }
        }
    }
    if (bg->portalwarning)
    {
        LevelIssue issue = {0}; issue.kind = LEVEL_ISSUE_PORTAL; issue.error = TRUE;
        strcpy(issue.subject, "Portal data");
        snprintf(issue.description, sizeof(issue.description), "%s", bg->portalwarning);
        if (!Add(out, &issue)) { goto memory; }
    }
    for (DWORD i = 0; i < bg->portals.portalcount; i++)
    {
        const BgPortal *portal = &bg->portals.portals[i];
        if (!portal->connectedroom1 || !portal->connectedroom2
            || portal->connectedroom1 > bg->roomcount || portal->connectedroom2 > bg->roomcount
            || portal->connectedroom1 == portal->connectedroom2)
        {
            LevelIssue issue = {0}; issue.kind = LEVEL_ISSUE_PORTAL; issue.error = TRUE;
            issue.target = LEVEL_ISSUE_PORTAL_FACE; issue.index = i;
            snprintf(issue.subject, sizeof(issue.subject), "Portal %lu", (unsigned long)i);
            snprintf(issue.description, sizeof(issue.description), "Invalid room connection: %u to %u.", portal->connectedroom1, portal->connectedroom2);
            if (!Add(out, &issue)) { goto memory; }
        }
    }
    if (setup->data)
    {
        PatrolDocument patrol = {0}; const char *reason = ""; LevelIssue issue = {0};
        issue.kind = LEVEL_ISSUE_PATROL;
        if (!PatrolDocumentLoad(setup, &patrol, &reason))
        {
            issue.error = TRUE; strcpy(issue.subject, "Patrol paths / waypoints");
            snprintf(issue.description, sizeof(issue.description), "%s", reason);
            if (!Add(out, &issue)) { goto memory; }
        }
        else for (DWORD i = 0; i < patrol.count; i++) if (!patrol.paths[i].count)
        {
            snprintf(issue.subject, sizeof(issue.subject), "Patrol path %u", patrol.paths[i].id);
            strcpy(issue.description, "This patrol path has no waypoints. Add pads in Tools > Patrol Paths before assigning guards to it.");
            if (!Add(out, &issue)) { PatrolDocumentFree(&patrol); goto memory; }
        }
        PatrolDocumentFree(&patrol);
    }
    for (DWORD i = 0; i < setup->charactercount; i++) if (!setup->characters[i].deleted)
        for (DWORD j = 0; j < i; j++) if (!setup->characters[j].deleted && setup->characters[i].chrnum == setup->characters[j].chrnum)
        {
            LevelIssue issue = {0}; issue.kind = LEVEL_ISSUE_CHARACTER_ID; issue.error = TRUE;
            issue.index = i | SETUP_CHARACTER_SELECTION_BIT;
            float position[3];
            if (SetupFileGetModelPad(setup, issue.index, &issue.pad)
                && Position(Pad(setup, issue.pad), scale, position)) { issue.target = LEVEL_ISSUE_MODEL; }
            snprintf(issue.subject, sizeof(issue.subject), "Guard %u (character %lu)", setup->characters[i].chrnum, (unsigned long)i);
            snprintf(issue.description, sizeof(issue.description), "This guard shares its ID with character %lu. AI and equipment ownership may be ambiguous.", (unsigned long)j);
            if (!Add(out, &issue)) { goto memory; }
            break;
        }
    StanFileFree(&saved); *why = ""; return TRUE;
memory:
    StanFileFree(&saved); LevelIssuesFree(out); *why = "Out of memory collecting level issues."; return FALSE;
}

BOOL LevelIssueLocate(const LevelIssue *issue, const BgDocument *bg,
    const SetupFile *setup, const StanFile *stan, float scale, LevelIssueLocation *out)
{
    if (!issue || !out) { return FALSE; }
    memset(out, 0, sizeof(*out)); out->target = issue->target; out->index = issue->index; out->pad = issue->pad;
    if (issue->target == LEVEL_ISSUE_PAD || issue->target == LEVEL_ISSUE_MODEL)
    {
        float pos[3]; SetupPadRef ref;
        if (issue->target == LEVEL_ISSUE_MODEL && (!SetupFileGetModelPad(setup, issue->index, &ref)
            || ref.bound != issue->pad.bound || ref.index != issue->pad.index)) { return FALSE; }
        if (!Position(Pad(setup, issue->pad), scale, pos)) { return FALSE; }
        for (int axis = 0; axis < 3; axis++) { out->min[axis] = pos[axis] - 25; out->max[axis] = pos[axis] + 25; }
        return TRUE;
    }
    for (int axis = 0; axis < 3; axis++) { out->min[axis] = DBL_MAX; out->max[axis] = -DBL_MAX; }
    DWORD count;
    if (issue->target == LEVEL_ISSUE_STAN && stan && stan->tiles && issue->index < stan->tilecount)
        count = stan->tiles[issue->index].pointcount;
    else if (issue->target == LEVEL_ISSUE_PORTAL_FACE && bg && bg->portals.portals && issue->index < bg->portals.portalcount)
        count = bg->portals.portals[issue->index].pointcount;
    else { return FALSE; }
    if (count < 3 || count > (issue->target == LEVEL_ISSUE_STAN ? STAN_TILE_MAX_POINTS : BG_PORTAL_MAX_POINTS)) { return FALSE; }
    for (DWORD i = 0; i < count; i++)
    {
        double point[3];
        if (issue->target == LEVEL_ISSUE_STAN)
        { const StanPoint *p = &stan->tiles[issue->index].points[i]; point[0] = p->x; point[1] = p->y; point[2] = p->z; }
        else
        { const BgPortalPoint *p = &bg->portals.portals[issue->index].points[i]; point[0] = p->x; point[1] = p->y; point[2] = p->z; }
        for (int axis = 0; axis < 3; axis++)
        {
            if (!isfinite(point[axis])) { return FALSE; }
            out->min[axis] = fmin(out->min[axis], point[axis]); out->max[axis] = fmax(out->max[axis], point[axis]);
        }
    }
    return TRUE;
}
