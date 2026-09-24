/* Ordinary BG intersections are independently covered by hit_textures/run.py.
 * Supply one controlled BG hit to exercise the production nearest-hit merge. */
static HitThing backgroundHit;
static int backgroundHitEnabled;
static bool bgTestRayIntersectionInRoom(coord3d *from,coord3d *to,coord3d *dir,
    RoomVtxBatchBounds *bounds,s32 room,HitThing *hit)
{
    (void)from;(void)to;(void)dir;(void)bounds;assert(room==1);
    if(backgroundHitEnabled)*hit=backgroundHit;
    return backgroundHitEnabled;
}
