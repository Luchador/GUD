/* Stan stores private perimeter points, but authored links weld their editing
 * identities. Topology edits must also maintain variable record offsets and
 * the links/header pointers used by runtime collision queries. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "stanload.h"

static unsigned short Read16(const unsigned char *p)
{ return (unsigned short)((unsigned int)p[0] << 8 | p[1]); }
static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }
static void Write16(unsigned char *p, unsigned short n)
{ p[0] = n >> 8; p[1] = (unsigned char)n; }
static void Write32(unsigned char *p, DWORD n)
{ p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = (unsigned char)n; }
static const unsigned char *Point(const StanFile *s, DWORD t, DWORD p)
{ return s->data + s->tiles[t].sourceoffset + 8 + p * 8; }
static DWORD TileAt(const StanFile *s, DWORD offset)
{
    DWORD low = 0, high = s->tilecount;
    while (low < high)
    {
        DWORD mid = low + (high - low) / 2;
        if (s->tiles[mid].sourceoffset < offset) { low = mid + 1; } else { high = mid; }
    }
    return low < s->tilecount && s->tiles[low].sourceoffset == offset ? low : STAN_TILE_NONE;
}
static BOOL Validate(const StanFile *s, DWORD *end, const char **why)
{
    *why = "The stan tile records are inconsistent.";
    if (!s || !s->data || !s->tiles || !s->tilecount || s->tilecount > 65536
        || s->size < 20 || !(s->levelscale > 0) || !isfinite(s->levelscale)) { return FALSE; }
    DWORD first = s->tiles[0].sourceoffset, at = first;
    if (first < 12 || first > s->size - 8 || (first & 3)
        || (Read32(s->data + 4) & 0xffffffu) != first) { return FALSE; }
    for (DWORD t = 0; t < s->tilecount; t++)
    {
        const StanTile *tile = &s->tiles[t];
        DWORD bytes = 8u + tile->pointcount * 8u;
        if (tile->pointcount < 3 || tile->pointcount > STAN_TILE_MAX_POINTS
            || tile->sourceoffset != at || at > s->size || bytes > s->size - at
            || Read32(s->data + at) != (tile->id << 8 | tile->room)
            || (s->data[at + 6] >> 4) != tile->pointcount) { return FALSE; }
        for (DWORD p = 0; p < tile->pointcount; p++)
        {
            unsigned short link = tile->points[p].link;
            if (Read16(Point(s, t, p) + 6) != link) { return FALSE; }
            if (link >= 0x10 && StanLinkedTile(s, link) == STAN_TILE_NONE)
            { *why = "A stan edge has an invalid tile link."; return FALSE; }
        }
        at += bytes;
    }
    if (at > s->size - 8 || Read32(s->data + at)) { return FALSE; }
    for (DWORD p = 4; p < first; p += 4)
    {
        DWORD ptr = Read32(s->data + p);
        if (p == first - 4 ? ptr != 0 : !ptr || TileAt(s, ptr & 0xffffffu) == STAN_TILE_NONE)
        { *why = "The stan header has an invalid tile pointer."; return FALSE; }
    }
    *end = at; return TRUE;
}

BOOL StanSplitEdge(StanFile *s, const StanEdgeRef *edge, BOOL *changedout, const char **why)
{
    DWORD end, *map = NULL, a, b;
    unsigned short *detach = NULL, *clear = NULL;
    BOOL ok = FALSE;
    *changedout = FALSE;
    if (!Validate(s, &end, why)) { return FALSE; }
    if (!edge || edge->tile >= s->tilecount || edge->point >= s->tiles[edge->tile].pointcount)
    { *why = "Select a stan edge to split."; return FALSE; }
    map = StanBuildPointMap(s, why);
    if (!map) { return FALSE; }
    *why = "Out of memory splitting the stan edge.";
    detach = calloc(s->tilecount, sizeof(*detach)); clear = calloc(s->tilecount, sizeof(*clear));
    if (!detach || !clear) { goto done; }
    a = map[edge->tile * STAN_TILE_MAX_POINTS + edge->point];
    b = map[edge->tile * STAN_TILE_MAX_POINTS + (edge->point + 1) % s->tiles[edge->tile].pointcount];
    if (a == b || !memcmp(Point(s,edge->tile,edge->point),
        Point(s,edge->tile,(edge->point+1)%s->tiles[edge->tile].pointcount),6))
    { *why = "The selected stan edge is collapsed."; goto done; }
    /* As with BG splitting, detach both endpoints of every incident polygon.
     * Clearing just the selected edge would leave them welded around a fan. */
    for (DWORD t = 0; t < s->tilecount; t++)
    for (DWORD p = 0; p < s->tiles[t].pointcount; p++)
    {
        DWORD q = (p + 1) % s->tiles[t].pointcount;
        DWORD x = map[t * STAN_TILE_MAX_POINTS + p], y = map[t * STAN_TILE_MAX_POINTS + q];
        if ((x == a && y == b) || (x == b && y == a)) { detach[t] |= (1u << p) | (1u << q); }
    }
    for (DWORD t = 0; t < s->tilecount; t++)
    for (DWORD p = 0; p < s->tiles[t].pointcount; p++)
    {
        DWORD target = StanLinkedTile(s, s->tiles[t].points[p].link);
        DWORD q = (p + 1) % s->tiles[t].pointcount;
        if (target == STAN_TILE_NONE) { continue; }
        if (detach[t] & ((1u << p) | (1u << q))) { clear[t] |= 1u << p; }
        /* Also remove incoming/one-way links that could re-weld a detached
         * endpoint. Compare native XYZ; never affect an unrelated floor. */
        for (DWORD v = 0; v < s->tiles[target].pointcount; v++)
        if ((detach[target] & (1u << v))
            && (!memcmp(Point(s, t, p), Point(s, target, v), 6)
                || !memcmp(Point(s, t, q), Point(s, target, v), 6))) { clear[t] |= 1u << p; }
    }
    for (DWORD t = 0; t < s->tilecount; t++)
    for (DWORD p = 0; p < s->tiles[t].pointcount; p++) if (clear[t] & (1u << p))
    {
        s->tiles[t].points[p].link = 0;
        Write16(s->data + s->tiles[t].sourceoffset + 8 + p * 8 + 6, 0);
        *changedout = TRUE;
    }
    if (*changedout) { s->dirty = TRUE; }
    *why = ""; ok = TRUE;
done:
    free(map); free(detach); free(clear); return ok;
}

typedef struct PendingTile {
    unsigned char points[STAN_TILE_MAX_POINTS][8];
    unsigned int count;
    BOOL changed;
} PendingTile;

static void Normal(const unsigned char points[][8], unsigned int count, double n[3])
{
    n[0] = n[1] = n[2] = 0;
    for (unsigned int p = 0; p < count; p++)
    {
        const unsigned char *a = points[p], *b = points[(p + 1) % count];
        double x = (short)Read16(a), y = (short)Read16(a + 2), z = (short)Read16(a + 4);
        double u = (short)Read16(b), v = (short)Read16(b + 2), w = (short)Read16(b + 4);
        n[0] += (y-v)*(z+w); n[1] += (z-w)*(x+u); n[2] += (x-u)*(y+v);
    }
}
static BOOL ValidShape(const StanFile *s, DWORD t, const PendingTile *tile)
{
    unsigned char original[STAN_TILE_MAX_POINTS][8];
    double before[3], after[3]; int axis = 0;
    for (unsigned int p = 0; p < s->tiles[t].pointcount; p++) { memcpy(original[p], Point(s,t,p), 8); }
    Normal(original, s->tiles[t].pointcount, before); Normal(tile->points, tile->count, after);
    if (before[0]*after[0] + before[1]*after[1] + before[2]*after[2] <= 0) { return FALSE; }
    for (int k = 1; k < 3; k++) if (fabs(after[k]) > fabs(after[axis])) { axis = k; }
    /* Runtime point-inside/line walking assumes convex, ordered perimeters.
     * Check every vertex against every edge in the dominant projection. */
    int x = (axis + 1) % 3, y = (axis + 2) % 3;
    for (unsigned int p = 0; p < tile->count; p++)
    {
        const unsigned char *a = tile->points[p], *b = tile->points[(p + 1) % tile->count];
        for (unsigned int q = p + 1; q < tile->count; q++)
        { if (!memcmp(a, tile->points[q], 6)) { return FALSE; } }
        double ax = (short)Read16(a+x*2), ay = (short)Read16(a+y*2);
        double bx = (short)Read16(b+x*2), by = (short)Read16(b+y*2);
        for (unsigned int q = 0; q < tile->count; q++)
        {
            double cx = (short)Read16(tile->points[q]+x*2), cy = (short)Read16(tile->points[q]+y*2);
            if (((bx-ax)*(cy-ay)-(by-ay)*(cx-ax))*after[axis] < 0) { return FALSE; }
        }
    }
    return TRUE;
}

BOOL StanMergeVertices(StanFile *s, const StanPointRef *refs, DWORD count,
    StanPointRef *mergedout, const char **why)
{
    DWORD end, *map = NULL, *offsets = NULL, unique = 0, newend;
    unsigned char *selected = NULL, *data = NULL;
    PendingTile *pending = NULL;
    StanTile *tiles = NULL;
    int64_t sum[3] = {0}; short average[3];
    BOOL ok = FALSE; StanPointRef merged = {STAN_TILE_NONE, 0};
    if (!Validate(s, &end, why)) { return FALSE; }
    if (!refs || count < 2 || count > s->tilecount * STAN_TILE_MAX_POINTS)
    { *why = "Select at least two stan vertices to merge."; return FALSE; }
    map = StanBuildPointMap(s, why); if (!map) { return FALSE; }
    *why = "Out of memory merging stan vertices.";
    selected = calloc((size_t)s->tilecount * STAN_TILE_MAX_POINTS, 1);
    pending = calloc(s->tilecount, sizeof(*pending));
    offsets = malloc((size_t)s->tilecount * sizeof(*offsets));
    tiles = malloc((size_t)s->tilecount * sizeof(*tiles));
    if (!selected || !pending || !offsets || !tiles) { goto done; }
    for (DWORD i = 0; i < count; i++)
    {
        if (refs[i].tile >= s->tilecount || refs[i].point >= s->tiles[refs[i].tile].pointcount)
        { *why = "A selected stan vertex no longer exists."; goto done; }
        DWORD root = map[refs[i].tile * STAN_TILE_MAX_POINTS + refs[i].point];
        if (selected[root]) { continue; }
        selected[root] = 1; unique++;
        const unsigned char *p = Point(s, root / STAN_TILE_MAX_POINTS, root % STAN_TILE_MAX_POINTS);
        for (int axis = 0; axis < 3; axis++) { sum[axis] += (short)Read16(p + axis * 2); }
    }
    if (unique < 2) { *why = "Select at least two different stan vertices to merge."; goto done; }
    for (int axis = 0; axis < 3; axis++)
    { average[axis] = (short)(sum[axis] < 0 ? -((-sum[axis] + unique/2)/unique) : (sum[axis] + unique/2)/unique); }
    newend = s->tiles[0].sourceoffset;
    for (DWORD t = 0; t < s->tilecount; t++)
    {
        unsigned int n = s->tiles[t].pointcount, hits = 0, runs = 0, last = 0;
        unsigned char mask[STAN_TILE_MAX_POINTS] = {0};
        for (unsigned int p = 0; p < n; p++) { mask[p] = selected[map[t*STAN_TILE_MAX_POINTS+p]]; hits += mask[p]; }
        if (hits && n - hits + 1 < 3)
        { *why = "The merge would leave a stan tile with fewer than 3 points. Split its edge first if the shared neighboring tile must stay unchanged."; goto done; }
        for (unsigned int p = 0; p < n; p++) if (mask[p] && !mask[(p+1)%n]) { runs++; last = p; }
        if (hits && runs != 1)
        { *why = "Selected vertices in each stan tile must form one consecutive perimeter run."; goto done; }
        PendingTile *tile = &pending[t];
        for (unsigned int p = 0; p < n; p++)
        {
            if (mask[p] && p != last) { continue; }
            unsigned char *raw = tile->points[tile->count];
            memcpy(raw, Point(s,t,p), 8);
            if (mask[p])
            {
                for (int axis = 0; axis < 3; axis++) { Write16(raw + axis*2, (unsigned short)average[axis]); }
                if (merged.tile == STAN_TILE_NONE) { merged = (StanPointRef){t,tile->count}; }
            }
            tile->count++;
        }
        tile->changed = hits != 0;
        if (hits && !ValidShape(s, t, tile))
        { *why = "The merge would create a collapsed, reversed or concave stan tile."; goto done; }
        offsets[t] = newend; newend += 8 + tile->count * 8;
    }
    data = malloc(newend + s->size - end);
    if (!data) { *why = "Out of memory rebuilding stan tile records."; goto done; }
    memcpy(data, s->data, s->tiles[0].sourceoffset);
    memcpy(data + newend, s->data + end, s->size - end);
    float scale = 1.0f / s->levelscale;
    for (DWORD t = 0; t < s->tilecount; t++)
    {
        PendingTile *edit = &pending[t]; StanTile *tile = &tiles[t];
        *tile = s->tiles[t]; tile->sourceoffset = offsets[t]; tile->pointcount = (unsigned char)edit->count;
        memset(tile->points, 0, sizeof(tile->points));
        memcpy(data + offsets[t], s->data + s->tiles[t].sourceoffset, 8);
        for (unsigned int p = 0; p < edit->count; p++)
        {
            unsigned char *raw = data + offsets[t] + 8 + p*8;
            memcpy(raw, edit->points[p], 8);
            unsigned short link = Read16(raw + 6);
            if (link >= 0x10)
            {
                DWORD target = StanLinkedTile(s, link);
                DWORD relocated = (offsets[target] - offsets[0])/8 + 0x10;
                if (relocated > 0xffff) { *why = "A stan edge link exceeds the native range."; goto done; }
                link = (unsigned short)relocated; Write16(raw+6, link);
            }
            tile->points[p] = (StanPoint){(short)Read16(raw)*scale, (short)Read16(raw+2)*scale,
                (short)Read16(raw+4)*scale, link};
        }
    }
    for (DWORD p = 4; p < offsets[0]-4; p += 4)
    {
        DWORD ptr = Read32(s->data+p), target = TileAt(s, ptr & 0xffffffu);
        Write32(data+p, (ptr & 0xff000000u) | offsets[target]);
    }
    StanFile staged = *s; staged.data = data; staged.tiles = tiles;
    for (DWORD t = 0; t < s->tilecount; t++) if (pending[t].changed) { StanUpdateRepresentativeTriangle(&staged,t); }
    free(s->data); free(s->tiles); s->data = data; data = NULL; s->tiles = tiles; tiles = NULL;
    s->size = newend + s->size - end; s->dirty = TRUE;
    if (mergedout) { *mergedout = merged; }
    *why = ""; ok = TRUE;
done:
    free(map); free(selected); free(pending); free(offsets); free(tiles); free(data); return ok;
}

/* A split keeps the first child at its parent's live index and appends the
 * second. Saving already groups the records by room. Each inherited boundary
 * remembers its original link; internal diagonals get explicit sibling links. */
typedef struct StanBisectTile {
    PendingTile shape;
    DWORD parent, sibling;
    unsigned int diagonal;
} StanBisectTile;

BOOL StanBisectEdge(StanFile *s, const StanEdgeRef *edge, StanEdgeRef *out, const char **why)
{
    DWORD end, *map=NULL, *children=NULL, *offsets=NULL, additions=0, total, newend;
    DWORD a,b,nextid=0,nexteditor=0;
    unsigned int *cuts=NULL;
    unsigned char midpoint[8]={0}, *data=NULL;
    StanBisectTile *pieces=NULL;
    StanTile *tiles=NULL;
    BOOL ok=FALSE;
    if (!Validate(s,&end,why)) { return FALSE; }
    if (!edge || !out || edge->tile>=s->tilecount || edge->point>=s->tiles[edge->tile].pointcount)
    { *why="Select one stan edge to bisect."; return FALSE; }
    const unsigned char *pa=Point(s,edge->tile,edge->point);
    const unsigned char *pb=Point(s,edge->tile,(edge->point+1)%s->tiles[edge->tile].pointcount);
    for (int k=0;k<3;k++)
    {
        int sum=(short)Read16(pa+k*2)+(short)Read16(pb+k*2);
        Write16(midpoint+k*2,(unsigned short)(sum<0 ? -((-sum+1)/2) : (sum+1)/2));
    }
    if (!memcmp(midpoint,pa,6) || !memcmp(midpoint,pb,6))
    { *why="The stan edge is too small to bisect at native coordinate precision."; return FALSE; }
    map=StanBuildPointMap(s,why); if (!map) { return FALSE; }
    a=map[edge->tile*STAN_TILE_MAX_POINTS+edge->point];
    b=map[edge->tile*STAN_TILE_MAX_POINTS+(edge->point+1)%s->tiles[edge->tile].pointcount];
    *why="Out of memory bisecting stan tiles.";
    cuts=malloc(s->tilecount*sizeof(*cuts)); children=malloc(s->tilecount*sizeof(*children));
    if (!cuts || !children) { goto done; }
    for (DWORD t=0;t<s->tilecount;t++)
    {
        unsigned int n=s->tiles[t].pointcount;
        cuts[t]=STAN_TILE_MAX_POINTS; children[t]=STAN_TILE_NONE;
        if (((s->tiles[t].id>>8)&0x7fffu)>nextid) { nextid=(s->tiles[t].id>>8)&0x7fffu; }
        if (s->tiles[t].editorid>nexteditor) { nexteditor=s->tiles[t].editorid; }
        for (unsigned int p=0;p<n;p++)
        {
            DWORD x=map[t*STAN_TILE_MAX_POINTS+p],y=map[t*STAN_TILE_MAX_POINTS+(p+1)%n];
            if ((x!=a || y!=b) && (x!=b || y!=a)) { continue; }
            if (cuts[t]!=STAN_TILE_MAX_POINTS)
            { *why="A stan tile repeats the selected edge. Repair its perimeter first."; goto done; }
            cuts[t]=p; children[t]=s->tilecount+additions++;
        }
    }
    *why="Bisecting this edge exceeds the native stan tile or identity limits.";
    if (!additions || additions>65536u-s->tilecount || additions>0x7fffu || nextid>0x7fffu-additions
        || nexteditor>UINT32_MAX-additions) { goto done; }
    total=s->tilecount+additions;
    pieces=calloc(total,sizeof(*pieces)); offsets=malloc(total*sizeof(*offsets)); tiles=malloc(total*sizeof(*tiles));
    if (!pieces || !offsets || !tiles) { *why="Out of memory bisecting stan tiles."; goto done; }
    for (DWORD t=0;t<s->tilecount;t++)
    {
        unsigned int n=s->tiles[t].pointcount, start=cuts[t];
        StanBisectTile *first=&pieces[t];
        first->parent=t; first->sibling=STAN_TILE_NONE;
        if (start==STAN_TILE_MAX_POINTS)
        {
            first->shape.count=n;
            for (unsigned int p=0;p<n;p++) { memcpy(first->shape.points[p],Point(s,t,p),8); }
            continue;
        }
        StanBisectTile *second=&pieces[children[t]];
        second->parent=t; second->sibling=t; first->sibling=children[t];
        /* A triangle becomes two triangles. Larger convex tiles become two
         * convex polygons along the midpoint-to-opposite-vertex diagonal. */
        unsigned int opposite=(n+1)/2;
        memcpy(first->shape.points[0],Point(s,t,start),8);
        memcpy(first->shape.points[1],midpoint,8); first->diagonal=1;
        first->shape.count=2;
        for (unsigned int p=opposite;p<n;p++)
        { memcpy(first->shape.points[first->shape.count++],Point(s,t,(start+p)%n),8); }
        memcpy(second->shape.points[0],midpoint,8);
        Write16(second->shape.points[0]+6,Read16(Point(s,t,start)+6));
        second->shape.count=1;
        for (unsigned int p=1;p<=opposite;p++)
        { memcpy(second->shape.points[second->shape.count++],Point(s,t,(start+p)%n),8); }
        second->diagonal=second->shape.count-1;
        if (!ValidShape(s,t,&first->shape) || !ValidShape(s,t,&second->shape))
        { *why="The midpoint would create a collapsed, reversed or concave stan tile."; goto done; }
    }
    newend=s->tiles[0].sourceoffset;
    for (DWORD t=0;t<total;t++) { offsets[t]=newend; newend+=8+pieces[t].shape.count*8; }
    if ((newend-offsets[0])/8+0x10>0x10000u || newend>0xffffffu
        || s->size-end>UINT32_MAX-newend)
    { *why="Bisecting this edge exceeds the native stan link range."; goto done; }
    data=malloc(newend+s->size-end);
    if (!data) { *why="Out of memory rebuilding stan tile records."; goto done; }
    memcpy(data,s->data,offsets[0]); memcpy(data+newend,s->data+end,s->size-end);
    for (DWORD t=0;t<total;t++)
    {
        StanBisectTile *piece=&pieces[t]; PendingTile *shape=&piece->shape;
        StanTile *tile=&tiles[t]; *tile=s->tiles[piece->parent];
        tile->sourceoffset=offsets[t]; tile->pointcount=(unsigned char)shape->count;
        if (t>=s->tilecount)
        {
            /* The high bit distinguishes p/q (walkable/non-walkable), and
             * the low byte encodes the native letter/suffix. Preserve both. */
            tile->id=(++nextid<<8)|(tile->id&0x8000ffu); tile->editorid=++nexteditor;
        }
        memset(tile->points,0,sizeof(tile->points));
        memcpy(data+offsets[t],s->data+s->tiles[piece->parent].sourceoffset,8);
        Write32(data+offsets[t],tile->id<<8|tile->room);
        for (unsigned int p=0;p<shape->count;p++)
        {
            unsigned char *raw=data+offsets[t]+8+p*8;
            unsigned short link=Read16(shape->points[p]+6);
            DWORD target=STAN_TILE_NONE;
            if (piece->sibling!=STAN_TILE_NONE && p==piece->diagonal) { target=piece->sibling; }
            else if (link>=0x10)
            {
                target=StanLinkedTile(s,link);
                if (children[target]!=STAN_TILE_NONE)
                {
                    DWORD matched=STAN_TILE_NONE, candidates[2]={target,children[target]};
                    const unsigned char *p0=shape->points[p],*p1=shape->points[(p+1)%shape->count];
                    for (int c=0;c<2;c++)
                    {
                        const PendingTile *other=&pieces[candidates[c]].shape;
                        for (unsigned int q=0;q<other->count;q++)
                        if (!memcmp(p0,other->points[(q+1)%other->count],6) && !memcmp(p1,other->points[q],6))
                        {
                            if (matched!=STAN_TILE_NONE)
                            { *why="A stan link has more than one matching boundary after bisection."; goto done; }
                            matched=candidates[c];
                        }
                    }
                    if (matched==STAN_TILE_NONE)
                    { *why="An existing stan link does not match the subdivided tile boundary. Repair that connection first."; goto done; }
                    target=matched;
                }
            }
            if (target!=STAN_TILE_NONE) { link=(unsigned short)((offsets[target]-offsets[0])/8+0x10); }
            memcpy(raw,shape->points[p],8); Write16(raw+6,link);
            float scale=1.0f/s->levelscale;
            tile->points[p]=(StanPoint){(short)Read16(raw)*scale,(short)Read16(raw+2)*scale,(short)Read16(raw+4)*scale,link};
        }
    }
    for (DWORD p=4;p<offsets[0]-4;p+=4)
    {
        DWORD ptr=Read32(s->data+p),target=TileAt(s,ptr&0xffffffu);
        Write32(data+p,(ptr&0xff000000u)|offsets[target]);
    }
    StanFile staged=*s; staged.data=data; staged.tiles=tiles; staged.tilecount=total; staged.size=newend+s->size-end;
    for (DWORD t=0;t<total;t++) if (pieces[t].sibling!=STAN_TILE_NONE) { StanUpdateRepresentativeTriangle(&staged,t); }
    free(s->data); free(s->tiles); *s=staged; data=NULL; tiles=NULL; s->dirty=TRUE;
    *out=(StanEdgeRef){edge->tile,0}; *why=""; ok=TRUE;
done:
    free(map); free(children); free(offsets); free(cuts); free(pieces); free(data); free(tiles); return ok;
}

/* Signed interior-side test in the XZ plane, as used by the runtime's Stan
 * walk. Positive area is native floor winding; vertical risers have zero. */
static double StanBridgeSide(const unsigned char *a, const unsigned char *b, const unsigned char *p)
{
    double ax=(short)Read16(a), az=(short)Read16(a+4);
    double bx=(short)Read16(b), bz=(short)Read16(b+4);
    double px=(short)Read16(p), pz=(short)Read16(p+4);
    return (bz-az)*(px-ax)-(bx-ax)*(pz-az);
}

static BOOL StanBridgePrepare(const StanFile *s, const StanEdgeRef edges[2],
    PendingTile pieces[2], DWORD *end, DWORD *nextid, DWORD *nexteditor, const char **why)
{
    const unsigned char *points[4];
    static const unsigned char diagonals[2][2][3] = {{{1,0,3},{1,3,2}},{{1,0,2},{0,3,2}}};
    if (!Validate(s,end,why)) { return FALSE; }
    *why="Select two boundary edges on different stan tiles.";
    if (!edges || edges[0].tile==edges[1].tile) { return FALSE; }
    for (int e=0;e<2;e++)
    {
        if (edges[e].tile>=s->tilecount || edges[e].point>=s->tiles[edges[e].tile].pointcount
            || s->tiles[edges[e].tile].room>STAN_MAX_ROOM) { return FALSE; }
        const StanTile *tile=&s->tiles[edges[e].tile];
        points[e*2]=Point(s,edges[e].tile,edges[e].point);
        points[e*2+1]=Point(s,edges[e].tile,(edges[e].point+1)%tile->pointcount);
        if (tile->points[edges[e].point].link>=0x10)
        { *why="Only unlinked stan boundary edges can be bridged. Split or unlink the existing connection first."; return FALSE; }
    }
    *why="Bridge Edges requires four distinct stan endpoints. Use Link Tiles for edges that already coincide.";
    for (int a=0;a<4;a++) for (int b=0;b<a;b++)
    { if (!memcmp(points[a],points[b],6)) { return FALSE; } }
    /* Do not put a second surface on an occupied boundary, including an
     * incoming-only connection or an unlinked but coincident tile edge. */
    *nextid=*nexteditor=0;
    for (DWORD t=0;t<s->tilecount;t++)
    {
        const StanTile *tile=&s->tiles[t];
        DWORD number=(tile->id>>8)&0x7fffu;
        if (number>*nextid) { *nextid=number; }
        if (tile->editorid>*nexteditor) { *nexteditor=tile->editorid; }
        for (DWORD p=0;p<tile->pointcount;p++) for (int e=0;e<2;e++)
        {
            if (t==edges[e].tile && p==edges[e].point) { continue; }
            const unsigned char *a=Point(s,t,p), *b=Point(s,t,(p+1)%tile->pointcount);
            if ((!memcmp(a,points[e*2],6) && !memcmp(b,points[e*2+1],6))
                || (!memcmp(b,points[e*2],6) && !memcmp(a,points[e*2+1],6)))
            { *why="A selected stan boundary is already shared by another edge. Use Link Tiles or choose an open boundary."; return FALSE; }
        }
    }
    *why="The bridge would overlap a source tile. Choose boundary edges facing the gap.";
    for (int e=0;e<2;e++) for (int p=0;p<2;p++)
    { if (StanBridgeSide(points[e*2],points[e*2+1],points[(1-e)*2+p])>0) { return FALSE; } }
    /* Two triangles handle unequal heights without making a nonplanar Stan
     * quad. Native floors need positive XZ area; vertical stair risers may
     * have zero XZ area but still need nonzero 3D area and coherent winding. */
    int diagonal;
    for (diagonal=0;diagonal<2;diagonal++)
    {
        BOOL valid=TRUE;
        double normals[2][3];
        for (int t=0;t<2;t++)
        {
            const unsigned char *c=diagonals[diagonal][t];
            memset(&pieces[t],0,sizeof(pieces[t])); pieces[t].count=3;
            for (int p=0;p<3;p++) { memcpy(pieces[t].points[p],points[c[p]],6); }
            Normal(pieces[t].points,3,normals[t]);
            double area=normals[t][0]*normals[t][0]+normals[t][1]*normals[t][1]+normals[t][2]*normals[t][2];
            if (!area || normals[t][1]<0) { valid=FALSE; }
        }
        double dot=normals[0][0]*normals[1][0]+normals[0][1]*normals[1][1]+normals[0][2]*normals[1][2];
        if (valid && dot>=0) { break; }
    }
    if (diagonal==2)
    { *why="These edges would form a crossed, reversed or zero-area stan bridge. Check their positions and winding."; return FALSE; }
    *why="The bridge exceeds the native stan tile, identity or edge-link limits.";
    if (s->tilecount>65534u || *nextid>0x7ffdu || *nexteditor>UINT32_MAX-2
        || *end>0xffffffu-64 || s->size>UINT32_MAX-64
        || (*end+64-s->tiles[0].sourceoffset)/8+0x10>0x10000u) { return FALSE; }
    *why=""; return TRUE;
}

BOOL StanCanBridgeEdges(const StanFile *s, const StanEdgeRef edges[2], const char **why)
{
    PendingTile pieces[2]; DWORD end,nextid,nexteditor; const char *unused;
    return StanBridgePrepare(s,edges,pieces,&end,&nextid,&nexteditor,why ? why : &unused);
}

BOOL StanBridgeEdges(StanFile *s, const StanEdgeRef edges[2], DWORD out[2], const char **why)
{
    PendingTile pieces[2]; DWORD end,nextid,nexteditor;
    StanFile staged;
    if (!StanBridgePrepare(s,edges,pieces,&end,&nextid,&nexteditor,why)) { return FALSE; }
    staged=*s;
    staged.data=malloc(s->size+64);
    staged.tiles=malloc((s->tilecount+2)*sizeof(*staged.tiles));
    if (!staged.data || !staged.tiles)
    { free(staged.data); free(staged.tiles); *why="Out of memory bridging stan edges."; return FALSE; }
    staged.size+=64; staged.tilecount+=2;
    memcpy(staged.data,s->data,end);
    memcpy(staged.data+end+64,s->data+end,s->size-end);
    memcpy(staged.tiles,s->tiles,s->tilecount*sizeof(*s->tiles));
    for (int t=0;t<2;t++)
    {
        DWORD index=s->tilecount+t, offset=end+t*32;
        StanTile *tile=&staged.tiles[index];
        *tile=s->tiles[edges[0].tile];
        tile->id=(++nextid<<8)|(tile->id&0x8000ffu); tile->editorid=++nexteditor;
        tile->sourceoffset=offset; tile->pointcount=3;
        memset(tile->points,0,sizeof(tile->points));
        memcpy(staged.data+offset,s->data+s->tiles[edges[0].tile].sourceoffset,8);
        Write32(staged.data+offset,tile->id<<8|tile->room);
        for (int p=0;p<3;p++)
        {
            const unsigned char *a=pieces[t].points[p], *b=pieces[t].points[(p+1)%3];
            DWORD target=STAN_TILE_NONE;
            for (int e=0;e<2;e++)
            {
                const StanTile *source=&s->tiles[edges[e].tile];
                if (!memcmp(a,Point(s,edges[e].tile,(edges[e].point+1)%source->pointcount),6)
                    && !memcmp(b,Point(s,edges[e].tile,edges[e].point),6))
                {
                    target=edges[e].tile;
                    unsigned short link=(unsigned short)((offset-s->tiles[0].sourceoffset)/8+0x10);
                    staged.tiles[target].points[edges[e].point].link=link;
                    Write16(staged.data+source->sourceoffset+8+edges[e].point*8+6,link);
                }
            }
            for (int q=0;q<3;q++)
            {
                if (!memcmp(a,pieces[1-t].points[(q+1)%3],6) && !memcmp(b,pieces[1-t].points[q],6))
                { target=s->tilecount+1-t; }
            }
            DWORD targetoffset=target<s->tilecount ? s->tiles[target].sourceoffset : end+(1-t)*32;
            unsigned short link=target==STAN_TILE_NONE ? 0
                : (unsigned short)((targetoffset-s->tiles[0].sourceoffset)/8+0x10);
            unsigned char *raw=staged.data+offset+8+p*8;
            memcpy(raw,a,6); Write16(raw+6,link);
            float scale=1.0f/s->levelscale;
            tile->points[p]=(StanPoint){(short)Read16(raw)*scale,(short)Read16(raw+2)*scale,(short)Read16(raw+4)*scale,link};
        }
        StanUpdateRepresentativeTriangle(&staged,index);
    }
    free(s->data); free(s->tiles); *s=staged; s->dirty=TRUE;
    if (out) { out[0]=s->tilecount-2; out[1]=s->tilecount-1; }
    *why=""; return TRUE;
}
