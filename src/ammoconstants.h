#ifndef GUD_AMMOCONSTANTS_H
#define GUD_AMMOCONSTANTS_H

/* Native ammo IDs shared by the game and asset tools. Keep platform independent. */
typedef enum AMMOTYPE
{
    AMMO_NONE,
    AMMO_9MM,
    AMMO_9MM_2,
    AMMO_RIFLE,
    AMMO_SHOTGUN,
    AMMO_GRENADE,
    AMMO_ROCKETS,
    AMMO_REMOTEMINE,
    AMMO_PROXMINE,
    AMMO_TIMEDMINE,
    AMMO_KNIFE,
    AMMO_GRENADEROUND,
    AMMO_MAGNUM,
    AMMO_GGUN,
    AMMOTYPE_GLOBAL_MAX = AMMO_GGUN,
    // Ammo available in both SP and MP must be listed above this line.
    AMMO_DARTS,
    AMMO_EXPLOSIVEPEN,
    AMMO_BOMBCASE,
    AMMO_FLARE,
    AMMO_PITON,
    AMMO_DYNAMITE,
    AMMO_BUG,
    AMMO_MICRO_CAMERA,
    AMMO_GEKEY,
    AMMO_PLASTIQUE,
    AMMO_WATCH_LASER,
    AMMO_WATCH_MAGNET,
    AMMO_UNK,
    AMMO_CAMERA,
    AMMO_TANK,
    AMMO_TOKEN,
    AMMOTYPE_MAX
} AMMOTYPE;

#endif
