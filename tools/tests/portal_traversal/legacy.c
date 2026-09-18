/* Frozen ba6fce2 queue/cutoff behavior, used only to demonstrate the old failures. */
#define LEGACY_PORTAL_QUEUE_LEN 500
static BgQueuedPortal legacyQueue[LEGACY_PORTAL_QUEUE_LEN];
static s32 legacyWrite, legacyRead;
static void legacyProcess(s32, s32, s32, s32, bbox2d *);
static void legacyEnqueue(s32 arg0, s32 arg1, s32 portalnum, s32 depth, f32 *arg4)
{
    BgQueuedPortal *entry;
    entry = &legacyQueue[legacyWrite];

    if (depth >= 2)
    {
        if (bgIncrementRoomPortalVisitCount((g_BgPortals[portalnum].connectedRoom2 ^ g_BgPortals[portalnum].connectedRoom1) ^ arg1) >= 9)
        {
            return;
        }
    }

    entry->arg0 = arg0;
    entry->roomnum = arg1;
    entry->portalnum = portalnum;
    entry->arg3 = depth;
    entry->sp10[0] = arg4[0];
    entry->sp10[1] = arg4[1];
    entry->sp10[2] = arg4[2];
    entry->sp10[3] = arg4[3];

    legacyWrite++;

    if (legacyWrite == LEGACY_PORTAL_QUEUE_LEN)
    {
        legacyWrite = 0;
    }

    /**
     * Former debug comment: "bg: pstackat: Overflow "
     */
    if (legacyWrite == legacyRead)
    {
        legacyWrite--;
    }
}
static bool legacyNext()
{
    BgQueuedPortal *entry;

    if (legacyRead == legacyWrite)
    {
        return FALSE;
    }

    entry = &legacyQueue[legacyRead];

    legacyProcess(entry->arg0, entry->roomnum, entry->portalnum, entry->arg3, (bbox2d *)entry->sp10);

    legacyRead++;

    if (legacyRead == LEGACY_PORTAL_QUEUE_LEN)
    {
        legacyRead = 0;
    }

    return TRUE;
}
static void legacyProcess(s32 value, s32 roomnum, s32 portalnum, s32 depth, bbox2d *parentbox)
{
    bbox2d screenbox;
    coord3d *playerpos;
    s32 otherroom;
    struct PortalMetric metric;
    f32 playermetric;
    f32 portalmetric;
    s32 i;
    const u8 *portalindex;
    const u8 *portalend;

    if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_DISABLED)
    {
        return;
    }



    playerpos = bondviewGetPlayerPosition();
    metric = g_PortalPlanes[portalnum];
    playermetric = ((metric.normal.z * playerpos->z) + ((metric.normal.x * playerpos->x) + (metric.normal.y * playerpos->y))) * g_LevelScale;
    portalmetric = bgGetPortalMargin(portalnum);

    if (roomnum == g_BgPortals[portalnum].connectedRoom1)
    {
        otherroom = g_BgPortals[portalnum].connectedRoom2;

        if (metric.max <= (playermetric - portalmetric))
        {
            return;
        }
    }
    else
    {
        otherroom = g_BgPortals[portalnum].connectedRoom1;

        if ((playermetric + portalmetric) <= metric.min)
        {
            return;
        }
    }

    if (((metric.min - portalmetric) < playermetric) && (playermetric < (metric.max + portalmetric)))
    {
        screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
        screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
        screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
        screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
    }
    else
    {
        if (g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL)
        {
            if (!bgGetPortalScreenBbox(portalnum, &screenbox))
            {
                return;
            }

            otherroom = (g_BgPortals[portalnum].connectedRoom1 ^ g_BgPortals[portalnum].connectedRoom2) ^ roomnum;

            if (!bgIsRoomOnScreen(otherroom, (struct rectbbox *) &screenbox))
            {
                return;
            }

            screenbox.f[0][0] = g_CurrentPlayer->screensize.f[0][0];
            screenbox.f[0][1] = g_CurrentPlayer->screensize.f[0][1];
            screenbox.f[1][0] = g_CurrentPlayer->screensize.f[1][0];
            screenbox.f[1][1] = g_CurrentPlayer->screensize.f[1][1];
        }
        else
        {
            if (!bgGetPortalScreenBbox(portalnum, &screenbox))
            {
                return;
            }

            bgRectIntersect(&screenbox, parentbox);
            bgRectIntersect(&screenbox, &g_CurrentPlayer->screensize);
        }

        if ((screenbox.max.x <= screenbox.min.x) || (screenbox.max.y <= screenbox.min.y))
        {
            return;
        }
    }

    g_PortalTraversalDepths[portalnum] = depth;

    if ((screenbox.min.x < screenbox.max.x) && (screenbox.min.y < screenbox.max.y))
    {
        if (bgSetRoomOnScreen(otherroom, depth, &screenbox, g_BgPortals[portalnum].controlbytes1 & PORTALFLAG_SPECIAL))
        {
            return;
        }
    }
    else
    {
        return;
    }

    for (portalindex = bgGetRoomPortalList(otherroom, &portalend); portalindex < portalend; portalindex++)
    {
        i = *portalindex;

        if (i != portalnum)
        {
            legacyEnqueue(value, otherroom, i, depth + 1, screenbox.f[0]);
        }
    }

    return;
}
