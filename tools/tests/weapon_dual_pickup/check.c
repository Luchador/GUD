#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <propconstants.h>
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;
typedef uint8_t u8;
typedef uint32_t u32;
typedef float f32;
#define TRUE 1
#define FALSE 0
#include <bondaicommands.h>
#include "declarations.inc"

/* Only platform and unrelated gameplay services are stubbed. Inventory list
 * insertion, linked-weapon handling, and the complete pickup gate run verbatim. */
typedef struct { float x, y, z; } Coord;
typedef struct StandTile { int room; } StandTile;
typedef struct { int refreshrate, unk90; } Projectile;
#define OBJECT_FIELDS int type; u32 flags, flags2, runtime_bitflags; Coord position; Projectile *projectile;
typedef struct ObjectRecord { OBJECT_FIELDS } ObjectRecord;
typedef struct WeaponObjRecord {
    OBJECT_FIELDS
    s8 weaponnum, LinkedWeaponType;
    int timer;
    struct WeaponObjRecord *dualweapon;
} WeaponObjRecord;
typedef struct PropRecord { int type; ObjectRecord *obj; Coord pos; StandTile *stan; } PropRecord;
typedef struct InvItem {
    int type;
    struct InvItem *next, *prev;
    union {
        struct { ITEM_IDS weapon; } type_weap;
        struct { ITEM_IDS weapon_right, weapon_left; } type_dual;
    } type_inv_item;
} InvItem;
typedef struct {
    int equipmaxitems, equipallguns, equipcuritem, magnetattracttime;
    InvItem *p_itemcur, *ptr_inventory_first_in_cycle;
    ITEM_IDS equipped[2];
} Player;
static Player player, *g_CurrentPlayer = &player;
static InvItem items[16];
static PropRecord playerProp;
static int ammo, pickups;
#define AMMOTYPE_GLOBAL_MAX 1
typedef struct AmmoCrateRecord { OBJECT_FIELDS int ammoType; } AmmoCrateRecord;
typedef struct MultiAmmoCrateRecord { OBJECT_FIELDS struct { int quantity; } slots[AMMOTYPE_GLOBAL_MAX]; } MultiAmmoCrateRecord;
typedef struct BodyArmourRecord { OBJECT_FIELDS float amount; } BodyArmourRecord;
static u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 mask)
{
    assert(mask == WEAPONSTATBITFLAG_CAN_DUAL_WIELD);
    return item == ITEM_WPPK || item == ITEM_WPPKSIL || item == ITEM_TT33;
}
static int objIsCollectable(ObjectRecord *obj) { return obj->type == PROPDEF_COLLECTABLE; }
static PropRecord *getCurrentPlayerProp(void) { return &playerProp; }
static float bondviewGetPlayerPitchRadians(void) { return 0; }
static int objCanPickupFromSafe(ObjectRecord *obj) { return 1; }
static int gunGetAmmoType(ITEM_IDS item) { return 1; }
static int get_ammo_count_for_weapon(ITEM_IDS item) { return ammo; }
static int get_max_ammo_for_weapon(ITEM_IDS item) { return 800; }
static int check_cur_player_ammo_amount_in_inventory(int type) { return ammo; }
static int get_max_ammo_for_type(int type) { return 800; }
static int objGetDestroyedLevel(ObjectRecord *obj) { return 0; }
static float currentPlayerGetArmor(void) { return 0; }
static int getPlayerCount(void) { return 1; }
static int get_scenario(void) { return 0; }
static int bondinvIsAliveWithFlag(void) { return 0; }
static int bondinvHasGoldenGun(void) { return 0; }
static int stanTestLineUnobstructed(StandTile **stan, float x, float z, float x2,
    float z2, int type, float a, float b, float c, float d) { return 1; }
static TICKOP propPickupByPlayer(PropRecord *prop, bool showstring);
#include "production.inc"

static TICKOP propPickupByPlayer(PropRecord *prop, bool showstring)
{
    pickups++;
    bondinvAddWeaponByProp(prop);
    return TICKOP_FREE;
}

static void Reset(void)
{
    memset(&player, 0, sizeof(player));
    memset(items, 0, sizeof(items));
    memset(&playerProp, 0, sizeof(playerProp));
    for (int i = 0; i < 16; i++) { items[i].type = -1; }
    player.equipmaxitems = 16;
    player.p_itemcur = items;
    bondinvAddInvItem(ITEM_UNARMED);
    bondinvAddInvItem(ITEM_WPPKSIL);
    player.equipcuritem = 1;
    player.equipped[0] = ITEM_WPPKSIL;
    player.equipped[1] = ITEM_UNARMED;
    ammo = 800;
    pickups = 0;
}

static WeaponObjRecord Weapon(ITEM_IDS item, u32 flags)
{
    WeaponObjRecord weapon = {0};
    weapon.type = PROPDEF_COLLECTABLE;
    weapon.flags = flags;
    weapon.weaponnum = item;
    weapon.LinkedWeaponType = -1;
    weapon.timer = -1;
    return weapon;
}

static TICKOP PickUp(WeaponObjRecord *weapon)
{
    PropRecord prop = {PROP_TYPE_WEAPON, (ObjectRecord *)weapon, {0, 0, 0}, NULL};
    TICKOP result = objTickPlayer(&prop);
    assert(player.equipcuritem == 1);
    assert(player.equipped[0] == ITEM_WPPKSIL && player.equipped[1] == ITEM_UNARMED);
    return result;
}

static int PairCount(void)
{
    int count = 0;
    for (int i = 0; i < 16; i++) { count += items[i].type == INV_ITEM_DUAL; }
    return count;
}

int main(void)
{
    WeaponObjRecord weapon, other;
    const u32 flag = PROPFLAG_WEAPON_GRANTS_DUAL;
    int spawn = -1;
    /* The real AI macros must encode the normal (not silenced) PP7 and the
     * big-endian flag, despite this generated setup's reversed literal syntax. */
    for (unsigned i = 0; i + 8 < sizeof(ai_16); i++) {
        if (ai_16[i] == guard_try_spawning_item_ID && ai_16[i + 1] == 0
            && ai_16[i + 2] == 0xbf && ai_16[i + 3] == ITEM_WPPK) {
            assert(spawn == -1);
            spawn = (int)i;
            u32 encoded = (u32)ai_16[i + 4] << 24 | (u32)ai_16[i + 5] << 16
                | (u32)ai_16[i + 6] << 8 | ai_16[i + 7];
            assert(encoded == flag && ai_16[i + 8] == 0x2c);
        }
    }
    assert(spawn >= 0);

    /* Boris's pickup works whether or not Bond already owns a regular PP7,
     * with full or partial ammo, and for either weapon-hand flag. */
    for (int owned = 0; owned < 2; owned++) {
        for (int full = 0; full < 2; full++) {
            for (int left = 0; left < 2; left++) {
                Reset();
                if (owned) { bondinvAddInvItem(ITEM_WPPK); }
                ammo = full ? 800 : 20;
                weapon = Weapon(ITEM_WPPK, flag | (left ? PROPFLAG_WEAPON_LEFTHANDED : 0));
                assert(PickUp(&weapon) == TICKOP_FREE && pickups == 1);
                assert(bondinvHasInvItem(ITEM_WPPK));
                assert(bondinvHasDualWeapon(ITEM_WPPK, ITEM_WPPK) && PairCount() == 1);
                assert(!bondinvHasDualWeapon(ITEM_WPPK, ITEM_WPPKSIL));
                assert(PickUp(&weapon) == (full ? TICKOP_NONE : TICKOP_FREE));
                assert(PairCount() == 1);
            }
        }
    }

    Reset();
    weapon = Weapon(ITEM_WPPK, 0);
    assert(PickUp(&weapon) == TICKOP_FREE && PairCount() == 0);
    assert(PickUp(&weapon) == TICKOP_NONE);
    weapon.flags = PROPFLAG_IS_DOUBLE;
    ammo = 20;
    assert(PickUp(&weapon) == TICKOP_FREE && PairCount() == 0);

    /* Existing two-prop pairs still unlock only after the second pickup. */
    Reset();
    weapon = Weapon(ITEM_WPPK, 0);
    other = Weapon(ITEM_WPPK, PROPFLAG_WEAPON_LEFTHANDED);
    propweaponSetDual(&weapon, &other);
    assert(PickUp(&weapon) == TICKOP_FREE && PairCount() == 0);
    assert(other.dualweapon == NULL && other.LinkedWeaponType == ITEM_WPPK);
    assert(PickUp(&other) == TICKOP_FREE && PairCount() == 1);

    /* A marked member of a linked pair grants its matching pair immediately
     * while leaving the other member's normal mixed-pair pickup intact. */
    Reset();
    weapon = Weapon(ITEM_WPPK, flag);
    other = Weapon(ITEM_TT33, PROPFLAG_WEAPON_LEFTHANDED);
    propweaponSetDual(&weapon, &other);
    assert(PickUp(&weapon) == TICKOP_FREE && PairCount() == 1);
    assert(PickUp(&other) == TICKOP_FREE && PairCount() == 2);
    assert(bondinvHasDualWeapon(ITEM_WPPK, ITEM_TT33));

    Reset();
    weapon = Weapon(ITEM_GRENADE, flag);
    assert(!bondinvWeaponGrantsDual(&weapon));
    assert(PickUp(&weapon) == TICKOP_FREE && PairCount() == 0);
    assert(PickUp(&weapon) == TICKOP_NONE);
    weapon = Weapon(ITEM_KEYCARD, flag);
    assert(!bondinvWeaponGrantsDual(&weapon));
    weapon = Weapon(ITEM_WPPK, flag | PROPFLAG_UNCOLLECTABLE);
    assert(PickUp(&weapon) == TICKOP_NONE);
    weapon.flags = flag;
    weapon.position.x = 1000;
    assert(PickUp(&weapon) == TICKOP_NONE);
    puts("PASS: Boris AI bytes; single-pickup dual grants; full-ammo and repeat pickups; unchanged equipment; ordinary, linked and unsupported weapons.");
    return 0;
}
