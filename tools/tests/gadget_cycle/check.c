#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int8_t s8;
typedef uint8_t u8;
typedef int32_t s32;
typedef uint16_t u16;
typedef uint32_t u32;
typedef float f32;
#include "declarations.inc"

#define TRUE 1
#define FALSE 0
#define MAX_PLAYER_COUNT 4
#define A_BUTTON 0x8000
#define B_BUTTON 0x4000
#define Z_TRIG 0x2000
#define START_BUTTON 0x1000
#define U_JPAD 0x0800
#define D_JPAD 0x0400
#define L_JPAD 0x0200
#define R_JPAD 0x0100
#define L_TRIG 0x0020
#define R_TRIG 0x0010
#define U_CBUTTONS 0x0008
#define D_CBUTTONS 0x0004
#define L_CBUTTONS 0x0002
#define R_CBUTTONS 0x0001
#define ANY_BUTTON 0xffff
#define WEAPONSTATBITFLAG_DISABLE_CROUCH 1

struct TestHand {
    ITEM_IDS weaponnum, weapon_next_weapon;
    s32 weapon_action_state, weapon_current_animation, weapon_animation_trigger;
    s32 field_890, field_8B0, field_8B8;
};
static struct TestPlayer {
    struct TestHand hands[2];
    s32 bondstate, watch_animation_state, open_close_solo_watch_menu, isInAimMode;
    u16 prev_buttons_pressed, oldButtons;
    bool owned[ITEM_IDS_MAX];
} players[MAX_PLAYER_COUNT], *g_CurrentPlayer = players;
static s32 playerNum, playerCount = 1, controlType, aimToggle;
static s32 g_bondviewForceDisarm, g_stopPlayFlag, g_gameOverFlag, g_PlayerIsInTank;
static bool locked, menuOpen, hasFlag;
static s32 scenario;
static u16 secondaryButtons;

static s32 get_cur_playernum(void) { return playerNum; }
static s32 getPlayerCount(void) { return playerCount; }
static bool lvGetControlsLockedFlag(void) { return locked; }
static bool disablePlayerActionsWhenPausedOrInMpMenu(void) { return !menuOpen; }
static bool bondinvIsAliveWithFlag(void) { return hasFlag; }
static s32 get_scenario(void) { return scenario; }
static bool bondinvHasInvItem(ITEM_IDS item)
{ assert(item >= ITEM_UNARMED && item < ITEM_IDS_MAX); return g_CurrentPlayer->owned[item]; }
static s32 get_cur_player_look_vertical_inverted(void) { return 0; }
static CONTROLLER_CONFIG cur_player_get_control_type(void) { return controlType; }
static s32 cur_player_get_aim_control(void) { return aimToggle; }
static s8 joyGetStickX(s32 port) { return 0; }
static s8 joyGetStickY(s32 port) { return 0; }
static u16 joyGetButtons(s32 port, u32 mask) { return secondaryButtons & mask; }
static bool bondwalkItemCheckBitflags(ITEM_IDS item, u32 mask) { return FALSE; }
static f32 bviewGetAimInputSpeed(f32 speed) { return speed / 60.0f; }
static void trigger_solo_watch_menu(s32 unused) { g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x1; }

#include "production.inc"

static const ITEM_IDS gadgets[] = {
    ITEM_PLASTIQUE, ITEM_DOORDECODER, ITEM_BOMBDEFUSER, ITEM_CAMERA,
    ITEM_KEYANALYSERCASE, ITEM_BUG, ITEM_DATATHIEF, ITEM_WATCHMAGNETATTRACT,
    ITEM_EXPLOSIVEFLOPPY
};

static struct MoveData frame(u16 primary, u16 secondary)
{
    secondaryButtons = secondary;
    struct MoveData result = decodeInput(0, 0, primary, g_CurrentPlayer->oldButtons);
    g_CurrentPlayer->oldButtons = primary;
    return result;
}

static void reset(void)
{
    memset(players, 0, sizeof(players));
    playerCount = 1;
    locked = menuOpen = hasFlag = FALSE;
    scenario = aimToggle = g_bondviewForceDisarm = g_stopPlayFlag = g_gameOverFlag = g_PlayerIsInTank = 0;
    controlType = CONTROLLER_CONFIG_PLENTY;
    /* Release both raw controllers for every player, including the production latches. */
    for (playerNum = 0; playerNum < MAX_PLAYER_COUNT; playerNum++) {
        g_CurrentPlayer = &players[playerNum];
        g_CurrentPlayer->hands[GUNRIGHT].weaponnum = ITEM_WPPK;
        frame(0, 0);
    }
    playerNum = 0;
    g_CurrentPlayer = &players[0];
    controlType = CONTROLLER_CONFIG_HONEY;
}

static void assertConsumed(struct MoveData move)
{
    assert(!move.triggerOn && !move.btap && !move.detonating);
    assert(!move.weaponBackOffset && !move.weaponForwardOffset);
}

static void assertRequested(ITEM_IDS item)
{
    assert(g_CurrentPlayer->hands[GUNRIGHT].weapon_next_weapon == item);
    assert(g_CurrentPlayer->hands[GUNRIGHT].weapon_current_animation == 5);
    assert(g_CurrentPlayer->hands[GUNRIGHT].weapon_animation_trigger == 1);
    assert(g_CurrentPlayer->hands[GUNLEFT].weapon_next_weapon == ITEM_UNARMED);
}

static void testSelection(void)
{
    reset();
    /* Unlisted items, including the remote detonator and watch laser, are excluded. */
    g_CurrentPlayer->owned[ITEM_WATCHLASER] = TRUE;
    g_CurrentPlayer->owned[ITEM_TRIGGER] = TRUE;
    g_CurrentPlayer->owned[ITEM_GOLDENEYEKEY] = TRUE;
    assert(bondinvGetNextGadget(ITEM_WPPK) == ITEM_NOTHING);
    gunCycleGadget();
    assert(!g_CurrentPlayer->hands[GUNRIGHT].weapon_animation_trigger);

    /* Every owned subset: missing/removed items, wraparound, zero and one gadget. */
    for (s32 subset = 0; subset < (1 << 9); subset++) {
        for (s32 i = 0; i < 9; i++) g_CurrentPlayer->owned[gadgets[i]] = (subset >> i) & 1;
        for (s32 start = -1; start < 9; start++) {
            ITEM_IDS expected = ITEM_NOTHING;
            for (s32 offset = 1; offset <= 9; offset++) {
                s32 i = (start + offset) % 9;
                if (subset & (1 << i)) { expected = gadgets[i]; break; }
            }
            assert(bondinvGetNextGadget(start < 0 ? ITEM_WPPK : gadgets[start]) == expected);
        }
    }

    reset();
    g_CurrentPlayer->owned[ITEM_CAMERA] = TRUE;
    g_CurrentPlayer->owned[ITEM_BUG] = TRUE;
    g_CurrentPlayer->hands[GUNLEFT].weaponnum = ITEM_WPPK;
    gunCycleGadget();
    assertRequested(ITEM_CAMERA);
    assert(g_CurrentPlayer->hands[GUNLEFT].weapon_current_animation == 5);

    /* A rapid second tap advances from the pending gadget while the pistol lowers. */
    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_SWITCH_LOWER;
    gunCycleGadget();
    assertRequested(ITEM_BUG);
    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_SWITCH_SWAP;
    gunCycleGadget();
    assertRequested(ITEM_CAMERA);

    /* B starts reloading before Z arrives: multiple taps still advance the queued switch. */
    reset();
    g_CurrentPlayer->owned[ITEM_CAMERA] = TRUE;
    g_CurrentPlayer->owned[ITEM_BUG] = TRUE;
    g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_RELOAD_START;
    gunCycleGadget();
    assertRequested(ITEM_CAMERA);
    gunCycleGadget();
    assertRequested(ITEM_BUG);

    /* A single already-equipped gadget should not restart its animation. */
    reset();
    g_CurrentPlayer->owned[ITEM_CAMERA] = TRUE;
    g_CurrentPlayer->hands[GUNRIGHT].weaponnum = ITEM_CAMERA;
    gunCycleGadget();
    assert(!g_CurrentPlayer->hands[GUNRIGHT].weapon_animation_trigger);
}

static void testInput(void)
{
    for (s32 config = CONTROLLER_CONFIG_HONEY; config <= CONTROLLER_CONFIG_GOODHEAD; config++) {
        s32 ports = config >= CONTROLLER_CONFIG_PLENTY ? 2 : 1;
        for (s32 port = 0; port < ports; port++) {
            reset();
            controlType = config;
            aimToggle = 1; /* The shortcut must not toggle Z-aim layouts. */
            g_CurrentPlayer->owned[ITEM_CAMERA] = TRUE;
            g_CurrentPlayer->owned[ITEM_BUG] = TRUE;
            frame(port == 0 ? B_BUTTON : 0, port == 1 ? B_BUTTON : 0);
            assertConsumed(frame(port == 0 ? B_BUTTON | Z_TRIG : 0, port == 1 ? B_BUTTON | Z_TRIG : 0));
            assertRequested(ITEM_CAMERA);
            assert(!g_CurrentPlayer->isInAimMode);
            g_CurrentPlayer->hands[GUNRIGHT].weapon_action_state = GUN_ANIM_STATE_SWITCH_LOWER;
            for (s32 i = 0; i < 60; i++) {
                assertConsumed(frame(port == 0 ? B_BUTTON | Z_TRIG : 0, port == 1 ? B_BUTTON | Z_TRIG : 0));
                assertRequested(ITEM_CAMERA);
            }
            assertConsumed(frame(port == 0 ? Z_TRIG : 0, port == 1 ? Z_TRIG : 0));
            assertRequested(ITEM_CAMERA);
            frame(0, 0);
            assertConsumed(frame(port == 0 ? B_BUTTON | Z_TRIG : 0, port == 1 ? B_BUTTON | Z_TRIG : 0));
            assertRequested(ITEM_BUG);
        }
    }

    reset();
    /* Empty inventory still consumes B + Z instead of firing the gun. */
    assertConsumed(frame(B_BUTTON | Z_TRIG, 0));
    assert(!g_CurrentPlayer->hands[GUNRIGHT].weapon_animation_trigger);
    frame(0, 0);
    assert(frame(Z_TRIG, 0).triggerOn); /* Normal Z resumes after release. */
    assert(frame(B_BUTTON | Z_TRIG, 0).triggerOn); /* B added to an existing Z hold is not cycling. */
    frame(0, 0);
    assert(frame(B_BUTTON, 0).btap);
    frame(0, 0);
    assert(frame(A_BUTTON, 0).weaponForwardOffset);
    assert(frame(A_BUTTON | Z_TRIG, 0).weaponBackOffset);
    frame(0, 0);
    g_CurrentPlayer->hands[GUNRIGHT].weaponnum = ITEM_REMOTEMINE;
    assert(frame(A_BUTTON | B_BUTTON, 0).detonating);
    frame(0, 0);
    assertConsumed(frame(A_BUTTON | B_BUTTON | Z_TRIG, 0));

    /* A latched shortcut on one player must not suppress another player's trigger. */
    reset();
    playerCount = 2;
    assertConsumed(frame(B_BUTTON | Z_TRIG, 0));
    playerNum = 1;
    g_CurrentPlayer = &players[1];
    assert(frame(Z_TRIG, 0).triggerOn);
}

static void testGates(void)
{
    for (s32 gate = 0; gate < 8; gate++) {
        reset();
        g_CurrentPlayer->owned[ITEM_CAMERA] = TRUE;
        switch (gate) {
            case 0: g_CurrentPlayer->watch_animation_state = WATCH_ANIMATION_0x5; break;
            case 1: g_CurrentPlayer->bondstate = BONDSTATE_DEAD; break;
            case 2: locked = TRUE; break;
            case 3: menuOpen = TRUE; break;
            case 4: g_bondviewForceDisarm = 1; break;
            case 5: playerCount = 2; g_stopPlayFlag = 1; break;
            case 6: playerCount = 2; g_gameOverFlag = 1; break;
            case 7: playerCount = 2; scenario = 2; hasFlag = TRUE; break;
        }
        frame(B_BUTTON | Z_TRIG, 0);
        assert(!g_CurrentPlayer->hands[GUNRIGHT].weapon_animation_trigger);
    }
    reset();
    g_CurrentPlayer->owned[ITEM_CAMERA] = TRUE;
    frame(START_BUTTON | B_BUTTON | Z_TRIG, 0);
    assert(!g_CurrentPlayer->hands[GUNRIGHT].weapon_animation_trigger);
}

int main(void)
{
    testSelection();
    testInput();
    testGates();
    puts("PASS: gadget ownership/order/wrap, pending switches, all control styles, input consumption and gameplay gates.");
    return 0;
}
