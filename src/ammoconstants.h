#ifndef GUD_AMMOCONSTANTS_H
#define GUD_AMMOCONSTANTS_H

/* Native ammo IDs shared by the game and asset tools. Keep platform independent. */
typedef enum AMMOTYPE
{
    AMMO_NONE,
    AMMO_9MM,
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

/* Shared solo pickup multipliers; integer grants truncate fractional rounds. */
#define DEFAULT_AGENT_SOLO_AMMO_MULTIPLIER 2.0f
#define DEFAULT_SECRET_AGENT_SOLO_AMMO_MULTIPLIER 1.5f
#define DEFAULT_00_AGENT_SOLO_AMMO_MULTIPLIER 1.0f
#define DEFAULT_007_SOLO_AMMO_MULTIPLIER DEFAULT_00_AGENT_SOLO_AMMO_MULTIPLIER

#endif
