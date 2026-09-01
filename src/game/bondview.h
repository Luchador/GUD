#ifndef _BOND_H_
#define _BOND_H_

#include <ultra64.h>
#include <bondtypes.h>
#include "chr.h"
#include "matrixmath.h"
#include "options.h"
#include "image.h"

/*cannonically these are both*/
#define BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH 0x97
#define BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH 0x65
/*these*/
#define MAXTALKMESSLEN 150
#define MAXMESSAGELEN 100

typedef struct invitem_weap
{
	s32 weapon;
} invitem_weap;

typedef struct invitem_prop
{
  PropRecord *prop;
} invitem_prop;

typedef struct invitem_dual
{
	s32 weapon_right;
	s32 weapon_left;
} invitem_dual;

/**
 * Player position, collision, facing, and camera-orientation state.
 * The player stores both the active state and a snapshot used by death replays.
 *
 * sizeof == 84 (0x54)
 */
struct PlayerSpatialState {

    /**
     * Stan tile containing collisionPosition.
     * Offset 0x00.
     */
    struct StandTile *collisionTile;

    /**
     * Offset 0x04.
     */
    coord3d collisionPosition;

    /**
     * Horizontal unit vector representing the player's facing direction.
     * Derived from vv_theta and used to transform forward/sideways movement
     * into world-space movement.
     * Offset 0x10.
     */
    struct coord3d facingDirection;

    /**
     * cameraPosition projected onto the Stan floor. Its X and Z match the
     * camera position, while Y is the floor height at cameraTile.
     * Offset 0x1c.
     */
    coord3d cameraGroundPosition;

    /**
     * Offset 0x28.
     */
    f32 collisionRadius;

    /**
     * Position from which the scene is rendered. Normally matches
     * collisionPosition, but can be displaced by head and death animations.
     * Offset 0x2c.
     */
    coord3d cameraPosition;

    /**
     * Camera forward/look direction calculated by bondviewMoveAnimationTick.
     * Offset 0x38.
     */
    struct coord3d cameraLookDirection;

    /**
     * Camera up direction calculated by bondviewMoveAnimationTick.
     * Offset 0x44.
     */
    struct coord3d cameraUp;

    /**
     * Stan tile containing cameraPosition. Used to select the camera's room
     * for portal visibility and rendering.
     * Offset 0x50.
     */
    StandTile *cameraTile;
};

typedef struct TopMessageLocals
{
  s32 textwidth;
  s32 textheight;
  s32 bottom;
  s32 y;
  s32 x;
} TopMessageLocals;

/**
 * first hand: 0x800c6fd0
*/
struct hand
{
  ITEM_IDS weaponnum;
  ITEM_IDS weaponnum_watchmenu;
  ITEM_IDS previous_weapon;

  s8 weapon_firing_status;
  s8 flashVisible;
  s8 weaponVisible;
  s8 handVisible;

  /**
   * Time holding weapon
  */
  s32 weapon_hold_time;
  s32 field_888;
  s32 field_88C;
  s32 field_890;
  s32 weapon_action_state;
  s32 weapon_current_animation;
  s32 weapon_ammo_in_magazine;
  s32 field_8A0;
  s32 numvisibleshells;
  s32 field_8A8;
  s32 weapon_next_weapon;
  s32 field_8B0;
  s32 weapon_animation_trigger;
  s32 field_8B8;
  f32 field_8BC;
  f32 field_8C0;
  f32 field_8C4;
  f32 field_8C8;
  f32 field_8CC;
  f32 field_8D0;
  f32 field_8D4;
  f32 field_8D8;
  f32 field_8DC;
  f32 field_8E0;
  f32 field_8E4;
  f32 field_8E8;
  Mtxf animMtx;
  s32 isAnimating;
  f32 sway_pos_x;
  f32 sway_pos_y;
  f32 sway_pos_z;
  f32 sway_look_x;
  f32 sway_look_y;
  f32 sway_look_z;
  f32 sway_up_x;
  f32 sway_up_y;
  f32 sway_up_z;
  f32 spring_pos_x;
  f32 spring_pos_y;
  f32 spring_pos_z;
  f32 spring_look_x;
  f32 spring_look_y;
  f32 spring_look_z;
  f32 spring_up_x;
  f32 spring_up_y;
  f32 spring_up_z;
  coord3d blendpos[4];
  coord3d blendlook[4];
  coord3d blendup[4];
  s32 curblendpos;
  f32 dampt;
  f32 blendscale1;
  s32 sideflag;
  f32 weapon_theta_displacement;
  f32 weapon_verta_displacement;
  f32 gunofs2_x;
  f32 gunofs2_y;
  f32 gunofs2_z;
  f32 field_A34;
  f32 field_A38;
  f32 field_A3C;
  f32 field_A40;
  ALSoundState * audioHandle;
  s32 field_A48;
  s32 field_A4C;
  s32 field_A50;
  BeamRecord weapon_beam;
  f32 noise;
  f32 triggerFingerRot;
  f32 slideRecoilOffset;
  s32 slideRecoiling;
  AttachedObj* rocket;
  s32 firedrocket;
  Mtxf gunmtx_camspace;
  Mtxf throw_item_pos_related;
  Mtxf throw_item_pos_related_prev;
  coord3d field_B58;
  f32 field_B64;
  Model gunmodel;
  u32 modeldatas[32];
  s32 volley;  // Number of bullets discharged in a row. For pistols, it's always 1 even if the fire button is held.
  coord3d item_related;
};

typedef struct InvItem {
    s32 type;

    union {
		struct invitem_weap type_weap;
		struct invitem_prop type_prop;
		struct invitem_dual type_dual;

	} type_inv_item;

    struct InvItem *next;
    struct InvItem *prev;

} InvItem;

typedef struct textoverride {

  /*0x00*/ u32 unk1;
	/*0x04*/ s32 objoffset;
	/*0x08*/ s32 weapon;
	/*0x0c*/ u32 titletext1; //1st title
	/*0x10*/ u32 titletext2;//2nd title
	/*0x14*/ u32 shorttext;
	/*0x18*/ u32 longtext; //longname
	/*0x1c*/ u32 pickuptext;
	/*0x20*/ struct textoverride *next;
	/*0x24*/ struct ObjectRecord *obj;
} textoverride;

typedef struct gunheld {
	s32 weapon1;
	s32 weapon2;
	s32 totaltime;
} gunheld;

struct player
{
  s32 frozencam;

  /**
   * Offset 0x0004.
   */
  coord3d pos;

  /**
   * Offset 0x0010.
   */
  coord3d pos2;

  /**
   * Offset 0x001c.
   */
  coord3d offset;

  /**
   * Offset 0x0028.
   */
  coord3d pos3;

  /**
   * Offset 0x0034.
   */
  StandTile *cameratile;

  /**
   * Offset 0x0038.
   */
  coord3d current_model_pos;

  /**
   * Offset 0x0044.
   */
  coord3d previous_model_pos;

  /**
   * Offset 0x0050.
   */
  coord3d current_room_pos;

  /**
   * Used as parameter to gbi macro.
   * Offset 0x005c.
   */
  s32 field_5C;

  /* 0x0060 */ s32 field_60;
  /* 0x0064 */ s32 field_64;
  /* 0x0068 */ s32 field_68;

  /**
   * Collision / clipping related.
   * Related to y offset.
   * Offset 0x006c.
   */
  /* 0x006c */ f32 field_6C;

  /**
   * Collision / clipping related.
   * Seems to hold save value as stanHeight.
   * Offset 0x0070.
   */
  /* 0x0070 */ f32 field_70;

  /* 0x0074 0x800c67d4 */ f32 stanHeight;

  /* 0x0078 */ f32 field_78;

  /**
   * Collision / clipping related.
   * Used when descending down stairs/ramp.
   * Offset 0x007c.
   */
  /* 0x007c */ f32 field_7C;

  /* 0x0080 */ f32 field_80;

  /**
   * Collision / clipping related.
   * Offset 0x0084.
   */
  /* 0x0084 */ f32 field_84;

  /**
   * Collision / clipping related.
   * Offset 0x0088.
   */
  /* 0x0088 */ f32 field_88;

  /**
   * Collision / clipping related.
   * Offset 0x008c.
   */
  /* 0x008c */ s32 field_8C;

  /**
   * Collision / clipping related.
   * When Bond falls off a ladder or similar, will "overshoot" vertical default.
   * Current offset is stored in this field.
   *
   * Offset 0x0090.
   */
  /* 0x0090 */ f32 vertical_bounce_adjust;
  /* 0x0094 */ s32 field_94;
  /* 0x0098 */ f32 field_98;

  /**
   * Flag: 0, 1, 2
  */
  /* 0x009c */ s32 crouchpos;

  /**
   * Varies from 0.0f to -100.0f
   * /
  /* 0x00a0 */ f32 crouchoffset;

  /**
   * Crouch related, only used while moving up or down into or
   * out of crouch position.
  */
  /* 0x00a4 */ f32 field_A4;
  /* 0x00a8 */ PropRecord* prop;
  /* 0x00ac */ s32 collisionEnabled;

  /**
   * Offset 0x00b0.
   */
  struct rect4f collision_bounds;

  /* 0x00d0 */ s32 field_D0;

  /**
   * bodyModel is the third person model - it's seen by the other players in MP, and
   * during intro/outros/death scenes in SP.
   */
  /* 0x00d4 */ struct Model *bodyModel;
  
  /* 0x00d8 */ enum BONDSTATE bondstate;
  /* 0x00dc */ f32 bondhealth;
  /* 0x00e0 */ f32 bondarmour;
  /* 0x00e4 */ f32 oldhealth;
  /* 0x00e8 */ f32 oldarmour;
  /* 0x00ec */ f32 apparenthealth;
  /* 0x00f0 */ f32 apparentarmour;

    /**
     * When a non-negative integer:
     * - hide ammo and aim sight
     * - hide any active speech text
     * - disable shoot and B press interact
     * Otherwise,
     * - undo the above.
     * 0x00f4
     */
    s32 damageshowtime;

    /**
     * When a non-negative integer:
     * - Show health and body armor overlay if Bond isn't dead.
     * Otherwise,
     * - undo the above.
     * 0x00f8
     */
    s32 healthshowtime;

  /* 0x00fc */ s32 healthshowmode;
  /* 0x0100 */ s32 field_100; // unused

  /**
  * The field names from this point up to bondbreathing are canonical names
  * with the exceptions of autoaim_target_y and autoaim_target_x.
  */

  /* 0x0104 */ bool docentreupdown;

  /**
   * Assigned to 0 but never read.
   * 0x0108
   */
  s32 lastupdown60;

  /**
   * When the player is doing manual pitch inputs, this is set to true so
   * the automatic look ahead pitch adjust is interrupted.
   * 0x010c
   */
  bool prevupdown;

  /**
  * If the player is moving forward quickly, and has made a manual pitch adjustment, suppress
  * the automatic look ahead pitch adjusting until the player has slowed down.
  * 0x0110
  */
  bool movecentrerelease;

  /**
   * If enabled, test the tiles ahead of the player for the look ahead functionality.
   * This is always enabled.
   * 0x0114
   */
  bool lookaheadcentreenabled;

  /**
   * Look Ahead Setting in the watch menu.
   * 0x0118
   */
  bool automovecentreenabled;

  /**
   * Always false.
   * 0x011c
   */
  bool fastmovecentreenabled;

  /**
   * True when Look Ahead is enabled and the player is giving strong
   * forwards or backwards input.
   * 0x0120
   */
  bool automovecentre;

  /**
   * 0: crosshair shown on screen
   * 1: crosshair hidden
   * Only applies in level, when in control of bond.
   * Address 0x800c6884
  */
  /* 0x0124 */ s32 isInAimMode;
  /* 0x0128 */ s32 autoyaimenabled;
  /* 0x012c */ f32 autoaimy;
  /* 0x0130 */ struct PropRecord *autoaim_target_y;
  /* 0x0134 */ s32 autoyaimtime60;
  /* 0x0138 */ s32 autoxaimenabled;
  /* 0x013c */ f32 autoaimx;
  /* 0x0140 */ struct PropRecord *autoaim_target_x;
  /* 0x0144 */ s32 autoxaimtime60;

  /* 0x0148 */ f32 vv_theta;
  /* 0x014c */ f32 speedtheta;

  /**
   * Computed value from vv_theta, used to calculate boost direction.
   * 0x0150
   **/
  f32 vv_costheta;

  /**
   * Computed value from vv_theta, used to calculate boost direction.
   * 0x0154
   **/
  f32 vv_sintheta;

  /**
   * Vertical look angle.
   * 0x0158
   **/
  f32 vv_verta;

  /**
   * Vertical look angle. Computed value from vv_verta, should always be between 0 and 360 degrees.
   * 0x015c
   **/
  f32 vv_verta360;

  /* 0x0160 */ f32 speedverta;

  /**
   * Computed value from vv_verta360, but otherwise unused?
   * 0x0164
   **/
  f32 vv_cosverta;

  /**
   * Computed value from vv_verta360, but otherwise unused?
   * 0x0168
   **/
  f32 vv_sinverta;

  /* 0x016c */ f32 speedsideways;
  /* 0x0170 */ f32 speedstrafe;
  /* 0x0174 */ f32 speedforwards;

  /**
   * Normal run speed: 1.0
   * After 3 seconds of running: 1.35.
   * Address 0x80c68d8
  */
  /* 0x0178 */ f32 speedboost;

  /**
   * How long Bond has been running
  */
  /* 0x017c */ s32 speedmaxtime60;

  coord3d bondshotspeed; //0x180

  // offset 0x18c
  f32 bondfadetime60;

  // offset 0x190
  f32 bondfadetimemax60;

  // offset 0x194
  f32 bondfadefracold;

  // offset 0x198
  f32 bondfadefracnew;

  // offset 0x19c
  f32 bondbreathing;

  s32 field_1A0;
  s32 unreferenced_1A4[7]; /* never referenced by name; layout preserved */

  /**
   * Current pause time.
   * Resets after every new pause.
   * Address 0x800c6920
  */
  s32 watch_pause_time;

  /* 0x01c4 */ s32 timer_1C4;
  /* 0x01c8 */ s32 watch_animation_state;

  /**
   * 1 = level is active
   * 0 = game is paused
   * Changes to zero only after fully entering watch.
   * Address 0x800c692c
  */
  s32 outside_watch_menu;
  s32 open_close_solo_watch_menu;
  f32 field_1D4;
  f32 field_1D8;
  f32 pause_watch_position;
  f32 field_1E0;
  f32 field_1E4;
  f32 field_1E8;
  f32 field_1EC;
  f32 field_1F0;
  f32 field_1F4;
  s32 unreferenced_1F8[2]; /* never referenced by name; layout preserved */
  /**
   * Pausing flag.
   * 0 = level is active
   * 1 = begin pause animation. Set as soon as moving arm begins, cleared when moving arm ends.
   * 0x800C6960
  */
  s32 pausing_flag; // 0x200
  f32 pause_starting_angle; // 0x204
  f32 pause_saved_verta; // 0x208, camera pitch to restore after pause
  f32 pause_target_verta; // 0x20c
  f32 pause_transition_time; // 0x210, elapsed time
  f32 pause_transition_duration; // 0x214, max/duration

  /**
   * Pausing states.
   * 0 = Unpaused
   * 1 = Entering pause.
   * 2 = Leaving pause.
   * 3 = Paused.
   */
  s32 pause_state; // 0x218
  s32 field_21C;
  s32 step_in_view_watch_animation;
  f32 pause_animation_counter;

  /**
   * Offset 0x0228.
   */
  f32 pause_watch_related;

  /**
   * Looks to be same as pause_watch_related, but adjusted by scale factor.
   * Offset 0x022c.
   */
  f32 pause_watch_related_scaled;

  s32 something_with_watch_object_instance;

  s32 field_234;
  s32 field_238;
  s32 field_23C;
  s32 field_240;
  f32 watch_scale_destination;
  s32 field_248;
  s32 field_24C;
  s32 field_250;
  s32 field_254;

  /**
   * Some kind of adjustment applied before scaling to set pause_watch_related_scaled.
   * Offset 0x0258.
   */
  f32 pause_watch_related_adjust;
  s32 unreferenced_25C[86]; /* never referenced by name; layout preserved */

  /**
   * Copy of buttons pressed.
   * Address 0x800C6B17
  */
  u16 buttons_pressed;
  u16 prev_buttons_pressed;

  struct coord3d field_3B8;

  f32 field_3C4;
  f32 field_3C8;
  f32 field_3CC;

  /**
   * Canonical names from here up through deathanimfinished.
   */
  s32 colourscreenred;
  s32 colourscreengreen;
  s32 colourscreenblue;
  f32 colourscreenfrac;
  f32 colourfadetime60;

  /**
   * Offset 0x3e4.
   */
  f32 colourfadetimemax60;
  s32 colourfaderedold;
  s32 colourfaderednew;
  s32 colourfadegreenold;
  s32 colourfadegreennew;
  s32 colourfadeblueold;
  s32 colourfadebluenew;
  f32 colourfadefracold;
  f32 colourfadefracnew;

  /*
  * Something with position, like previous x position.
  * Offset 0x408.
  */
  coord3d bondprevpos;

  f32 thetadie; //0x414
  f32 vertadie; //0x418
  s32 bondtype; //0x41c
  s32 startnewbonddie; //0x420

  /**
   * Multiplayer related.
   * Offset 0x424.
   */
  s32 redbloodfinished;

  /**
   * Multiplayer related.
   * Offset 0x428.
   */
  s32 deathanimfinished;
  s32 field_42c; 
  s32 controldef; //0x430 canonical name

  struct PlayerSpatialState previous_collision_info; // canonical "periminfo" ?
  struct PlayerSpatialState field_488;

  /**
   * Canonical names from here up to standcnt.
   */

  s32 resetheadpos; // bool
  s32 resetheadrot; // bool
  s32 resetheadtick; // bool 0x4e4

  s32 headanim; // index into array of pointers. Pointers are for animations.

  // has NTSC<->PAL difference, but not (5/6)
  f32 headdamp;
  s32 headwalkingtime60; //0x4f0
  f32 headamplitude; //0x4f4
  f32 sideamplitude; //0x4f8
  coord3d headpos;
  vec3d headlook;

  vec3d headup;
  coord3d headpossum;

  // headlooksum[2] has NTSC->PAL conversion rate (5/6)
  coord3d headlooksum;

  // headupsum[1] has NTSC->PAL conversion rate (5/6)
  vec3d headupsum;
  coord3d headbodyoffset; //0x544
  f32 standheight; // old name stationary_ground_offset

  // f32[4] ?? or 2 x f32[2] ??
  coord3d standbodyoffset; //0x554
  f32 standfrac; //0x560

  // offset 0x564
  vec3d standlook[2];

  // offset 0x57C
  vec3d standup[2];

  // offset 0x594
  s32 standcnt;
  Model *model;
  s32 unreferenced_59C[8]; /* never referenced by name; layout preserved */
  s8 animFlipFlag;
  s8 field_5BD;
  s8 field_5BE;
  s8 field_5BF;

  f32 field_5C0;

  s32 unreferenced_5C4[36]; /* never referenced by name; layout preserved */
  s32 field_654;
  s32 unreferenced_658[30]; /* never referenced by name; layout preserved */

  Mtxf bondheadmatrices[4];

  Vp viewports[2];

  /**
   * Offset 0x7f0.
   */
  s16 viewx;

  /**
   * Offset 0x7f2.
   */
  s16 viewy;

  /**
   * Offset 0x7f4.
   */
  s16 viewleft;

  /**
   * Offset 0x7f6.
   */
  s16 viewtop;

  s32 hand_invisible[2]; /* 0x7f8*/
  ITEM_IDS hand_item[2]; /* 0x800 */
  ModelFileHeader *ptr_hand_weapon_buffer[2]; /* 0x808 */

  /**
   * Offset 0x810.
   */
  ModelFileHeader copy_of_body_obj_header[2];

  struct texpool item_related[2]; /* 0x850 */

  /**
   * Offset 0x870.
   */
  struct hand hands[2];
  f32 gunposamplitude; // canonical name
  f32 gunxamplitude; // canonical name

  // Whether the trigger was released this frame.
  s32 trigger_released;

  /**
   * Whether player is currently holding the trigger button.
   * Offset 0xfcc.
   */
  s32 trigger_down;

  // Whether player was holding the trigger button on the previous frame.
  s32 prev_trigger_down;

  s32 z_trigger_timer;

  // Which hand will fire when the Z trigger is pressed.
  s32 current_trigger_hand;
  
  struct rgba_u8 tileColor;

  s32 resetshadecol; // canonical name

  // unused.
  s32 aimtype;

  /**
   * Crosshair or bullet angle related.
   * Offset 0xfe8.
  */
  coord2d crosshair_angle;
  f32 crosshair_x_pos;
  f32 crosshair_y_pos;
  f32 guncrossdamp; // canonical name
  coord2d field_FFC;
  f32 gun_azimuth_angle;
  f32 gun_azimuth_turning;
  f32 gunaimdamp; // canonical name
  coord3d field_1010;
  Mtxf field_101C;
  s32 last_z_trigger_timer;
  s32 copiedgoldeneye; // canonical name
  s32 gunammooff; // canonical name
  s32 field_1068;
  f32 gunsync; // canonical name
  f32 syncchange; // canonical name
  f32 synccount; // canonical name
  s32 syncoffset; // canonical name
  f32 field_107C;
  f32 field_1080;
  f32 sniper_zoom;
  f32 camera_zoom;
  s32 curRoomIndex;

  /**
   * Canonical names from here up through c_recipscaley.
   */

  /**
   * Offset 0x1090.
   */
  f32 c_screenwidth;

  /**
   * Offset 0x1094.
   */
  f32 c_screenheight;

  /**
   * Offset 0x1098.
   */
  f32 c_screenleft;

  /**
   * Offset 0x109c.
   */
  f32 c_screentop;

  /**
   * Offset 0x10a0.
   */
  f32 c_perspnear;

  /**
   * Offset 0x10a4.
   */
  f32 c_perspfovy;

  /**
   * Offset 0x10a8.
   */
  f32 c_perspaspect;

  /**
   * Offset 0x10ac.
   */
  f32 c_halfwidth;

  /**
   * Offset 0x10b0.
   */
  f32 c_halfheight;

  /**
   * Offset 0x10b4.
   */
  f32 c_scalex;

  /**
   * Offset 0x10b8.
   */
  f32 c_scaley;

  /**
   * Offset 0x10bc.
   */
  f32 c_recipscalex;
  f32 c_recipscaley;

  Mtx* field_10C4;
  Mtx* field_10C8;
  Mtxf* field_10CC;
  s32 field_10D0; // ptr

  /**
   * Offset 0x10d4.
   */
  Mtxf* viewtoworldmtxf;
  Mtx* projmatrix;
  Mtxf* projmatrixf;
  Mtx* projViewMtx;
  s32 field_10E4; // ptr
  Mtxf* field_10E8;
  Mtxf* field_10EC;
  f32 c_scalelod60; // canonical name
  f32 c_scalelod; // canonical name
  f32 c_lodscalez; // canonical name
  u32 c_lodscalezu32; // canonical name
  coord3d c_cameratopnorm; // canonical name
  coord3d c_cameraleftnorm; // canonical name

  /**
   * Offset 0x1118.
   */
  struct bbox2d screensize;

  /**
   * Used during level.
   * 0x2: no crosshair
   * 0x0: cross hair shown on screen.
   *
   * Offset 0x1128.
   */
  s32 gunsightmode;
  s32 field_112C;
  s32 ammoheldarr[30]; // canonical name
  u8 *bloodImgCur;
  u8 *bloodImgNxt;
  u8 *bloodImgBufPtrArray[2];
  s32 bloodImgIdx;

  /**
   * Canonical names from here up through bondmesscnt.
   */

  f32 zoomintime;
  f32 zoomintimemax;
  f32 zoominfovy;
  f32 zoominfovyold;
  f32 zoominfovynew;
  f32 fovy;
  f32 aspect;
  s32 hudmessoff;
  s32 bondmesscnt;

  InvItem *ptr_inventory_first_in_cycle;
  InvItem *p_itemcur;
  s32 equipmaxitems; // canonical name
  s32 equipallguns;  // canonical name
  s32 equipcuritem;  // canonical name
  textoverride *textoverrides;
  gunheld gunheldarr[10]; // canonical name

  // -1 for inactive, 0 or greater for active.
  s32 magnetattracttime;

  f32 swaytarget;
  f32 swayoffset0;
  f32 swayoffset2;
  f32 mpStrafeYawOffsetDegrees;
  s32 players_cur_animation;
  f32 field_1288;

  /**
   * This buffers button presses.
   * L_TRIGGER and R_TRIGGER do not add entry by themselves.
   * Entry seems to be added only on other button presses.
   * Offset 0x128c.
   */
  u16 cheatInputBuffer[20]; // canonically "buthist"

  /**
   * Offset 0x12b4.
   */
  /* 0x12B4 */ u8 cheatInputBufferIndex; // canonically "buthistindex"
  /* 0x12B5 */ u8 cheatInputCount; // canonically "buthistlen"
  /* 0x12B6 */ u8 cheatBondInvincible; // canonically "bondinvincible"
  /* 0x12B7 */ u8 field_12B7;
  /* 0x12B8 */ struct damage_display_parent armor_display_values[23];
  /* 0x1598 */ struct damage_display_parent health_display_values[23];

  /**
   * Offset 0x1878
  */
  struct WatchRectangle buffer_for_watch_greenbackdrop_vertices[WATCH_NUMBER_SCREENS];

  /**
   * Watch static, not "static vertices".
   * Offset 0x19b8.
  */
  struct WatchRectangle buffer_for_watch_static_vertices[1];

  /**
   * Offset 0x19f8
  */
  Gfx watch_body_armor_bar_gdl[230]; /* 0x730-byte gauge-bar DL staging (armor); written by buildGaugeBarDL, replayed via gSPDisplayList */


  /**
   * Offset 0x2128
  */
  Gfx watch_health_bar_gdl[230]; /* 0x730-byte gauge-bar DL staging (health); written by buildGaugeBarDL, replayed via gSPDisplayList */


  /**
   * Offset 0x2858
  */
  Gfx buffer_for_watch_greenbackdrop_DL[WATCH_NUMBER_SCREENS];

  s32 unreferenced_2880[70]; /* never referenced by name; layout preserved */

  /**
   * Watch static, not "static DL".
   * Offset 0x2998
  */
  Gfx buffer_for_watch_static_DL[1];

  s32 unreferenced_29A0[6]; /* never referenced by name; layout preserved */

  // Alt field_29C0 ?? Used in EU.
  s32 healthdamagetype; // canonical name

  /**
   * Related to player perspective.
   * Offset 0x29bc.
   */
  f32 eyeheight;
  f32 field_29C0;

  s32 mpmenuon;           // 29C4 canonincal name
  s32 mpmenumode;         // 29C8 canonincal name
  s32 mpquitconfirm;      // 29CC canonincal name
  s32 mpjoywascentre;     // 29D0 canonincal name
  s32 damagetype;         // 29D4 canonincal name
  s32 deathcount;         // 29D8 canonincal name
  s32 num_suicides;       // 29DC
  s32 field_29E0;         // 29E0
  s32 last_kill_time[4];  // 29E4

  /**
   * Holds mission offset timer value.
   * Offset 0x29f4.
   */
  s32 lifestarttime60; // canonical name

  s32 kills_this_life;
  s32 autocrouchpos;
  s32 healthdisplaytime; // canonically "healthdisplaytime60"

  /**
   * Current tile pointer -> room.
   *
   * Offset 0x2a04.
   */
  s16 registeredroom;
  f32 viewPitchRadians;
  f32 viewYawRadians;
  s32 ptr_text_first_mp_award;
  s32 ptr_text_second_mp_award;
  coord3d handMuzzlePos[2];
  s32 handMuzzleSampleTick[2];
  s32 cur_item_weapon_getname;
  f32 actual_health;
  f32 actual_armor;

  /**
   * Stores the requested model for each hand when a weapon switch is pending.
   */
  ITEM_IDS pendingHandModelItem[2];

  f32 speedgo;
  s32 lock_hand_model[2];
  s32 cur_player_control_type_0;
  s32 cur_player_control_type_1;
  f32 cur_player_control_type_2;
  s32 neg_vspacing_for_control_type_entry;
  u32 has_set_control_type_data;
};

struct firing_anim_struct {
    struct weapon_firing_animation_table * pointer;
    u32 anim;
    f32 x;
    f32 y;
    f32 z;
};

struct IntroAnim {
    s32 anonymous_0;
    f32 anonymous_1;
    f32 anonymous_2;
    f32 animSpeed;
};

typedef struct DamageType
{
    f32 field_0x0;
    f32 field_0x4;
    f32 field_0x8;
    f32 field_0xC;
    f32 flashStartFrame;
    f32 flashFullFrame;
    f32 flashEndFrame;
    f32 maxAlpha;
    u32 red;
    u32 green;
    u32 blue;
} DamageType;

typedef struct HealthDisplayDuration {
    s32 validStartFrame;          // When positive the health display does not show up at all.
    s32 updateToRealHealthFrame;  // Frame to switch from showing the health before taking damage, to showing the new health value.
    s32 hideHealthFrame;          // Frame to remove the health/armor gauge from the screen.
} HealthDisplayDuration;

/**
 * First person weapon animation keyframe. Used for special weapons like the Throwing Knife and Taser.
 */
typedef struct Weapon1PTransformKeyframe {
    u32 isFinalKey;
    coord3d pos; // local space (+right/-left, +up/-down, +backward/-forward)
    coord3d rot;
    f32 interpParam;
    f32 duration;
} Weapon1PTransformKeyframe;

struct move_bond_temp_struct {
    s32 unk00;
    s32 unk04;
};

extern s32 g_bondviewForceDisarm;
extern u8* g_HiResFrameBuffer;
extern s32 g_HiResModeActive;
extern s32 g_HiResEnterDelay;
extern s32 g_HiResExitDelay;
extern s32 g_CreditsRollTimer;
extern enum CREDITS_STATE g_CreditsState;
extern CreditsEntry *credits_pointer;
extern s32 g_SurroundBondWithExplosionsFlag;
extern s32 g_PlayerIsInTank;
extern struct PropRecord *g_WorldTankProp;
extern struct PropRecord *g_PlayerTankProp;
extern f32 g_PlayerTankYOffset;
extern ALSoundState * g_TankSfxState[2];
extern f32 g_TankTurnSpeed;
extern f32 g_TankOrientationAngle;
extern f32 g_TankTurretVerticalAngle;
extern f32 g_TankTurretVerticalAngleRelated;
extern f32 g_TankTurretOrientationAngleRad;
extern f32 g_TankTurretOrientationAngleDeg;
extern f32 tank_turret_turn_speed;
extern s32 g_BondCanEnterTank;
extern f32 g_TankTurretAngle;
extern f32 g_TankTurretTurn;
extern s32 g_ExplodeTankOnDeathFlag;
extern s32 g_TankDamagePenaltyTicks;
extern enum CAMERAMODE g_CameraMode;
extern enum CAMERAMODE g_CameraAfterCinema;
extern s32 camera_fade_active;
extern s32 stop_time_flag;
extern f32 camera_transition_timer;
extern s32 intro_camera_index;
extern struct SetupIntroSwirl *g_IntroSwirl;
extern s32 is_timer_active;

/**
 * Used to make the player invincible upon level completion.
 * This is separate from the invincibility cheat, stored in cheatBondInvincible.
 */
extern bool g_PlayerInvincible;
extern struct SetupIntroCamera* g_CurrentSetupIntroCamera;
extern s32 g_SetupIntroCameraCount;
extern struct SetupIntroCamera *ptr_random06cam_entry;
extern s32 g_VisibleToGuardsFlag;
extern s32 obj_collision_flag;
extern f32 g_ViewConversionScale;
extern f32 D_800364D0;
extern s32 g_bondviewBondDeathAnimations[];
extern s32 g_bondviewBondDeathAnimationsCount;
extern enum CAMERAMODE camera_mode;
extern s32 g_IntroAnimationIndex;
extern struct DamageType g_DamageTypes[];
extern struct coord3d g_DefaultFrozenPlayerPos;
extern struct coord3d g_DefaultFrozenPlayerPos2;
extern struct coord3d g_DefaultFrozenPlayerOffset;
extern struct coord3d g_DefaultFrozenMoveOffset;
extern coord3d ZeroCoord;
extern ModelRenderData D_8003683C;
extern coord3d ZeroCoordWatchPos;
extern coord3d ZeroCoordSpawnPos;
extern s32 status_bar_text_buffer_index;
extern s32 display_statusbar;
extern s32 copy_1stfonttable;
extern s32 copy_2ndfonttable;
extern s32 g_UpperTextTopSlot;
extern s32 g_UpperTextMsgQueued;
extern s32 g_UpperTextTimer;
extern s32 g_UpperTextSuppressFlags;
extern s32 startpadcount;
extern vec3d g_ForceBondMoveOffset;
extern s32 mission_timer;
extern s32 watch_time_0;
extern f32 watch_transition_time;
extern ITEM_IDS g_StartingWeapons[2];
extern PadRecord *g_Startpad[];
extern s32 startpadcount;

extern struct coord3d g_DefaultMoveBondOffset;
extern struct coord3d g_DefaultFrozenPlayerPos;
extern struct coord3d g_DefaultFrozenPlayerPos2;
extern struct coord3d g_DefaultFrozenPlayerOffset;
extern struct coord3d g_DefaultFrozenMoveOffset;
extern struct coord3d ZeroCoordShake;

PropRecord* getCurrentPlayerProp(void);

f32 currentPlayerGetHealth(void);
f32 currentPlayerGetArmor(void);

bool currentPlayerGetIsAiming(void);

f32 getPlayer_c_screenwidth(void);
f32 getPlayer_c_screenheight(void);
f32 getPlayer_c_screenleft(void);
f32 getPlayer_c_screentop(void);
f32 getPlayer_c_perspaspect(void);

void currentPlayerSetXAutoAimEnabled(bool enabled);
bool currentPlayerGetXAutoAimEnabled(void);
bool currentPlayerGetXAutoAimEnabledRedirect(void);
void currentPlayerSetYAutoAimEnabled(s32 enabled);
bool currentPlayerGetYAutoAimEnabled(void);
bool currentPlayerGetYAutoAimEnabledRedirect(void);
void currentPlayerSetLookAheadSetting(bool enabled);

u32 bondviewGetCameraMode(void);

void bondviewTriggerWatchZoom(f32 zoominfovy);

void watchChangeFOV(f32 final, f32 time);

void set_open_close_solo_watch_menu_to1(void);

void init_player_BONDdata(void);
void bviewPlayerBeginLife(void);


void bondviewMovePlayerUpdateViewport(s8 arg0, s8 arg1, u16 arg2);

#define HUDMESSAGEBOTTOM hudmsgBottomShow
void hudmsgBottomShow(char *string);
void setFontTables(s32 arg0, s32 arg1);

Gfx *bviewRenderCameraView(Gfx *arg0);
s32 bond_pressed_reload_activate(void);
Gfx *bondviewRenderPlayerView(Gfx *gdl);
bool camIsPosInScreen(coord3d *vec_scale, f32 norm_scale);
bool camIsPosInScreenBox(coord3d *, f32, struct bbox2d *);
s32 getMissiontimer(void);
void bviewLoadPlayerChr(void);
void bondviewUpdateYAutoAimTime(struct PropRecord *autoaim_target, f32 auto_aim_y);
void bondviewUpdateXAutoAimTime(struct PropRecord *autoaim_target, f32 auto_aim_x);
void bondviewSet3dCoord7F07CEB0(coord3d *arg0);
f32 bondviewYPositionRelated(struct StandTile *arg0, f32 arg1, f32 arg2);
f32 bondviewGetPlayerDuckingHeightRelated(struct player *player);
void bondviewGetCollisionRadius(PropRecord* arg0, f32 *collision_radius, f32 *height, f32 *always_30);
void bondviewUpdatePlayerY(s32 use_stanHeight, f32 stanHeight_offset);
void currentPlayerSetFadeColour(s32 r, s32 g, s32 b, f32 frac);
void currentPlayerSetFadeFrac(f32 maxfadetime, f32 frac);
f32 bviewGetPlayerStanHeight(struct player *player);
void record_damage_kills(f32, f32, f32, s32, s32);
void bondviewCallRecordDamageKills(f32 arg0, f32 rad, s32 arg2, s32 arg3);
int bondviewGetIfCurrentPlayerDamageShowTime(void);
int bondviewGetIfCurrentPlayerHealthShowTime(void);
u8 bondviewGetPlayerRoom(void);
coord3d *bondviewGetPlayerPosition(void);
coord3d *bondviewGetPlayerGroundPosition(void);
void bviewSetPlayerSolid(PropRecord *prop, s32 flag);
void bondviewGetPropHeightRelatedValues(PropRecord *arg0, struct rect4f **field_B0, s32 *arg2, f32 *height_related, f32 *collision);
void bondviewAddCurrentPlayerArmor(f32 arg0);
void bondviewResetIntroCameraMessageDialogs(void);
void hudmsgsSetOn(s32 flag);
void hudmsgsSetOff(s32 flags);
Gfx* bondviewGfxPlayerField5cMatrix(Gfx* gdl);
Mtxf *currentPlayerGetViewToWorldMtxf(void);
void bviewTransformManyPosToWorldMatrix(Mtxf *matrices, s32 count);
void currentPlayerStartChrFade(f32 duration60, f32 targetfrac);
s32 bondviewGetVisibleToGuardsFlag(void);
u8 get_bondata_invincible_flag(void);
void set_bondata_invincible_flag(u32 arg0);
void bondviewSetVisibleToGuardsFlag(s32 param_1);
Mtxf *currentPlayerGetMatrix10EC(void);
f32 bondviewGetPlayerYawRadians(void);
Mtxf *camGetWorldToScreenMtxf(void);
void transformAndNormalizeByLength2Dto3D(struct coord2d *in, coord3d *out, f32 value);
void bviewTransformManyPosToViewMatrix(RenderPosView *arg0, s32 arg1);
s32 sub_GAME_7F078474(void);
Mtx *camGetPlayerProjViewMtx(void);
Gfx *bondviewRenderProp(PropRecord *arg0, Gfx *arg1, s32 arg2);
f32 getPlayer_c_lodscalez(void);
f32 bondviewGetBondBreathing(void);
void bviewClearUpperTextDisplayFlag(PLAYERFLAG flag);
void bviewSetUpperTextDisplayFlag(PLAYERFLAG flag);
void bviewSetCameraMode(CAMERAMODE cameraMode);
bool isBondInTank(void);
void bviewShowUpperMessage(char* msg);
void SurroundWithExplosions(int delay);
s32 check_watch_page_transistion_running(void);
f32 bondviewWatchAnimationRelated(void);
struct coord3d *getCurrentPlayerPrevPos(void);
struct PropRecord *get_ptr_for_players_tank(void);
s32 bviewGetRandomSpawnPadIndex(void);
void change_player_pos_to_target(struct PlayerSpatialState *arg0, struct coord3d *arg1, struct StandTile *arg2);
void sub_GAME_7F089718(f32);
void bviewResetUpperTextDisplay(void);
Mtxf *currentPlayerGetProjectionMatrixF(void);
void transform3Dto2DCoords(coord3d *in, coord2d *out);
void bondviewRemovePlayerBody(void);
void currentPlayerAdjustFade(f32 maxfadetime, s32 r, s32 g, s32 b, f32 frac);
void bondviewSelectCuff(Model *model, ModelFileHeader *header, s32 switchindex);
void bviewSetConversionScale(f32 scale);

#endif
