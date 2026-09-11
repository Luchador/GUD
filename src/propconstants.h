#ifndef GUD_PROPCONSTANTS_H
#define GUD_PROPCONSTANTS_H

/* Canonical setup object flags and record types shared by the game and asset
 * tools. Keep this header free of platform-specific includes. Game code
 * receives these enums through bondconstants.h. Runtime PropRecord.flags
 * belongs to propruntimeflags.h and must never be exposed as setup options. */

/* Saved ObjectRecord.flags (u32). Aliases here are object-type-specific
 * meanings within this field, never flags from another record type. */
typedef enum PROPFLAG
{
    PROPFLAG_ALLOWFALL                         = 0x00000001, // Fall to Ground
    PROPFLAG_ONSIDE                            = 0x00000002, // ObjectRecord.flags: sideways placement, anchored at the model's Z-min face.
    PROPFLAG_UPSIDEDOWN                        = 0x00000004, // In-air upside-down placement.
    PROPFLAG_INAIR                             = 0x00000008, // In Air
    PROPFLAG_SCALE_TO_PAD_BOUNDS               = 0x00000010, // Scale to Pad Bounds
    PROPFLAG_SCALE_TO_X_BOUNDS                 = 0x00000020, // Scale X to Pad Bounds
    PROPFLAG_SCALE_TO_Y_BOUNDS                 = 0x00000040, // Scale Y to Pad Bounds
    PROPFLAG_SCALE_TO_Z_BOUNDS                 = 0x00000080, // Scale Z to Pad Bounds
    PROPFLAG_FORCE_COLLISIONS                  = 0x00000100,
    PROPFLAG_ORTHOGONAL                        = 0x00000200, // Use world space model matrices with the combined projection-view matrix. Used for environment mapping.
    PROPFLAG_ILLUMINATED                       = 0x00000400, // ignore Stan Colour?
    PROPFLAG_FREE_STANDING_GLASS               = 0x00000800, // Free Standing Glass
    PROPFLAG_ABSOLUTEPOSITION                  = 0x00001000, // Absolute positioning.
    PROPFLAG_AIUNDROPPABLE                     = 0x00002000, // Item Not Droppedz
    PROPFLAG_ASSIGNEDTOCHR                     = 0x00004000, // Assigned to Actor
    PROPFLAG_INSIDEANOTHEROBJ                  = 0x00008000, // Embedded Object
    PROPFLAG_FORCEMORTAL                       = 0x00010000, // unknown
    PROPFLAG_INVINCIBLE                        = 0x00020000, // Invincible
    PROPFLAG_ALLOW_PICKUP                      = 0x00040000, // Allow Pickup (chr_type)
    PROPFLAG_COLLECT_BY_INTERACT               = 0x00080000, // Collect Object by Interaction Button Only
    PROPFLAG_UNCOLLECTABLE                     = 0x00100000, // Item Not Collectable
    PROPFLAG_BOUND_AND_DESTROY                 = 0x00200000, // Bounce and destroy if Shot
    PROPFLAG_BOUNCE                            = 0x00400000, // Bounce if shot
    PROPFLAG_COLLECTABLE_ALLOW_UNARMED_DAMAGE  = 0x00800000, // Allows collectable objects to take damage through the ITEM_UNARMED damage path.
    PROPFLAG_EMBEDDED                          = 0x01000000, // Embedded Object
    PROPFLAG_CANNOT_ACTIVATE                   = 0x02000000, // Cannot Activate Door/Object
    PROPFLAG_TRANSPARENT_TO_AI                 = 0x04000000, // AI Sees Through Door/Object
    PROPFLAG_DOOR_TWOWAY                       = 0x08000000, // Open Away From Player
    PROPFLAG_WEAPON_LEFTHANDED                 = 0x10000000, // Left-Handed weapon
    PROPFLAG_GLASS_HASPORTAL                   = 0x10000000, // Glass Has Portal
    PROPFLAG_CULL_BEHIND_DOOR                  = 0x10000000, // Area Behind Door Invisible
    PROPFLAG_FIXED_MONITOR                     = 0x10000000, // Monitor Fixed
    PROPFLAG_CCTV_DISABLED                     = 0x10000000, // Disable security camera
    PROPFLAG_IS_DRONE_GUN                      = 0x10000000, // drone gun
    PROPFLAG_AMMO_DETONATE                     = 0x10000000, // Explosive ammo pickup detonates on its next weapon tick.
    PROPFLAG_DOOR_OPENTOFRONT                  = 0x20000000, // Open Backwards
    PROPFLAG_SPECIAL_FUNC                      = 0x20000000, // Special Function
    PROPFLAG_INMOTION                          = 0x20000000, // projectile/prop is in motion
    PROPFLAG_CONCEAL_GUN                       = 0x20000000, // Conceal Weapon
    PROPFLAG_MONITOR_RENDERPOSTBG              = 0x40000000,
    PROPFLAG_NO_PORTAL_CLOSE                   = 0x40000000, // Area Behind Door Visible
    PROPFLAG_NO_AMMO                           = 0x40000000, // No Ammo on pickup
    PROPFLAG_STARTOPEN                         = 0x80000000, // Door open By Default
    PROPFLAG_IS_DOUBLE                         = 0x80000000  // Weapon paired for player
}PROPFLAG;

/* Saved ObjectRecord.flags2 (u32). */
typedef enum PROPFLAG2
{
    PROPFLAG2_DRONEGUN              = 0x00000001, // Activate Drone Gun
    PROPFLAG2_LIGHTWEIGHT           = 0x00000002, // lightweight (move when shot)
    PROPFLAG2_DOOR_HIDE_LOCKED_MSG  = 0x00000004, // Locked door message not shown, used by Dam's double gates.
    PROPFLAG2_NO_LOAD_MP            = 0x00000008, // Don't Load in Multiplayer
    PROPFLAG2_NO_LOAD_A             = 0x00000010, // Don't Load on Agent Mode
    PROPFLAG2_NO_LOAD_SA            = 0x00000020, // Don't Load on Secret Agent Mode
    PROPFLAG2_NO_LOAD_00A           = 0x00000040, // Don't Load on 00 Agent Mode
    PROPFLAG2_NO_LOAD_007           = 0x00000080, // Don't Load on 007 Mode
    PROPFLAG2_NOFALL                = 0x00000100, // Immobile
    PROPFLAG2_FALLWITHOUTROTATION   = 0x00000200, // Used with mines
    PROPFLAG2_LINKEDTOSAFE          = 0x00000400, // Inside a safe
    PROPFLAG2_INTERACTCHECKLOS      = 0x00000800, // Don't activate if stan gap Bond/Object
    PROPFLAG2_PICKUP_THROUGH_WALLS  = 0x00001000, // Watch Magnet Attract through walls/objects.
    PROPFLAG2_REMOVE_WHEN_DESTROYED = 0x00002000, // Remove When Destroyed
    PROPFLAG2_GUNFIRE_IMMUNE        = 0x00004000, // Immune to Gunfire
    PROPFLAG2_SHOOTTHROUGH          = 0x00008000, // Can Shoot Through Object
    PROPFLAG2_DISABLE_ZBUFFER       = 0x00010000, // Always Visible (in front)
    PROPFLAG2_USESTANROOM           = 0x00020000, // Register only in the prop's stan room. Skip bounding box room detection. Used for Streets building models.
    PROPFLAG2_HIDE_FROM_INVENTORY   = 0x00040000, // Hide inventory item (for tagged armor/ammo/magazine)
    PROPFLAG2_ONLYEXPLOSIONDAMAGE   = 0x00080000, // Invincible Except to Explosions
    PROPFLAG2_BULLETPROOFGLASS      = 0x00100000, // Bulletproof Glass
    PROPFLAG2_EXPLOSION_IMMUNE      = 0x00200000, // Immune to Explosions
    PROPFLAG2_NOLOAD2P              = 0x00400000, // Don't load on 2P
    PROPFLAG2_NOLOAD3P              = 0x00800000, // Don't load on 3P
    PROPFLAG2_NOLOAD4P              = 0x01000000, // Don't load on 4P
    PROPFLAG2_THROWTHROUGH          = 0x02000000, // Rockets/Mines/Grenades Pass Through
    PROPFLAG2_FORCEONSCREEN         = 0x04000000, // Bypass the room-based scissor rectangle.
    PROPFLAG2_LOCKEDFRONT           = 0x08000000, // One-Way Lock (Front)
    PROPFLAG2_LOCKEDBACK            = 0x10000000, // One-Way Lock (Back)
    PROPFLAG2_LOCKEDTOAI            = 0x20000000, // Character AI Cannot Operate
    PROPFLAG2_AUTOGUN_STOP_SCAN     = 0x40000000, // Hold the current yaw and pitch during random scanning.
    PROPFLAG2_DOOR_INTERLOCK        = 0x40000000, // Close linked doors before opening this door.
    PROPFLAG2_DOOR_ALTCOORDSYSTEM   = 0x80000000, // Massive Explosion (08 Type)/Rotating Disabled Drone Gun (0D Type)
    PROPFLAG2_RANDOM_SCAN           = 0x80000000  // Turn towards a random yaw and pitch, used by the Surface communications dish.
} PROPFLAG2;

typedef enum PROPDEF_TYPE
{
    PROPDEF_NOTHING,
    PROPDEF_DOOR,
    PROPDEF_DOOR_SCALE,
    PROPDEF_PROP,
    PROPDEF_KEY,
    PROPDEF_ALARM,
    PROPDEF_CCTV,
    PROPDEF_MAGAZINE,
    PROPDEF_COLLECTABLE,
    PROPDEF_GUARD,
    PROPDEF_MONITOR,
    PROPDEF_MULTI_MONITOR,
    PROPDEF_RACK,
    PROPDEF_AUTOGUN,
    PROPDEF_LINK,
    PROPDEF_DEBRIS,
    PROPDEF_UNK16,
    PROPDEF_HAT,
    PROPDEF_GUARD_ATTRIBUTE,
    PROPDEF_SWITCH,
    PROPDEF_AMMO,
    PROPDEF_ARMOUR,
    PROPDEF_TAG,
    PROPDEF_OBJECTIVE_START,
    PROPDEF_OBJECTIVE_END,
    PROPDEF_OBJECTIVE_DESTROY_OBJECT,
    PROPDEF_OBJECTIVE_COMPLETE_CONDITION,
    PROPDEF_OBJECTIVE_FAIL_CONDITION,
    PROPDEF_OBJECTIVE_COLLECT_OBJECT,
    PROPDEF_OBJECTIVE_DEPOSIT_OBJECT,
    PROPDEF_OBJECTIVE_PHOTOGRAPH,
    PROPDEF_OBJECTIVE_NULL,
    PROPDEF_OBJECTIVE_ENTER_ROOM,
    PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM,
    PROPDEF_OBJECTIVE_COPY_ITEM,
    PROPDEF_WATCH_MENU_OBJECTIVE_TEXT,
    PROPDEF_GAS_RELEASING,
    PROPDEF_RENAME,
    PROPDEF_LOCK_DOOR,
    PROPDEF_VEHICLE,
    PROPDEF_AIRCRAFT,
    PROPDEF_UNK41,
    PROPDEF_GLASS,
    PROPDEF_SAFE,
    PROPDEF_SAFE_ITEM,
    PROPDEF_TANK,
    PROPDEF_CAMERAPOS,
    PROPDEF_TINTED_GLASS,
    PROPDEF_END,
    PROPDEF_MAX
} PROPDEF_TYPE;

#endif /* GUD_PROPCONSTANTS_H */
