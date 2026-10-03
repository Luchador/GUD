/* Real player and queue code; OS scheduling and synthesis are controlled stubs.
 * This verifies playback commands, not rendered audio or N64 scheduling timing. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include "../../../src/snd.c"
#include "../../../src/libultra/audio/event.c"
#include "../../../src/libultra/audio/sl.c"
#include "bondconstants.h"
#include "declarations.inc"

ALSndPlayer g_sndPlayer;
s16 *g_sndSfxSlotVolume;
u16 *g_sndSfxSlotNaturalVolume;
ALBank *g_musicSfxBufferPtr;
static s16 slotVolumes[SFX_SLOT_COUNT];
static ALSoundState states[64], pending;
static ALEventListItem events[64];
static ALSynth synth;
static int peakVolume, startedVoices;
static OSIntMask interruptMask = OS_IM_ALL;
static void (*onInterruptEnable)(void);

OSIntMask osSetIntMask(OSIntMask mask) {
    OSIntMask old = interruptMask;
    interruptMask = mask;
    if (old == OS_IM_NONE && mask != OS_IM_NONE && onInterruptEnable) {
        void (*callback)(void) = onInterruptEnable;
        onInterruptEnable = NULL;
        callback();
    }
    return old;
}
void alCopy(void *src, void *dst, s32 len) { memcpy(dst, src, len); }
f32 alCents2Ratio(s32 cents) { return powf(2.0f, cents / 1200.0f); }
s32 alSynAllocVoice(ALSynth *s, ALVoice *v, ALVoiceConfig *c) { return 1; }
void alSynStartVoice(ALSynth *s, ALVoice *v, ALWaveTable *w) { startedVoices++; }
void alSynStopVoice(ALSynth *s, ALVoice *v) {}
void alSynFreeVoice(ALSynth *s, ALVoice *v) {}
void alSynSetPan(ALSynth *s, ALVoice *v, ALPan pan) {}
void alSynSetPitch(ALSynth *s, ALVoice *v, f32 pitch) {}
void alSynSetFXMix(ALSynth *s, ALVoice *v, u8 mix) {}
void alSynSetVol(ALSynth *s, ALVoice *v, s16 volume, ALMicroTime t) {
    if (volume > peakVolume) peakVolume = volume;
}

typedef struct { f32 x, y, z; } coord3d;
typedef struct { int type; coord3d pos; } PropRecord;
static struct Player { PropRecord *prop; } players[2];
static struct Player *g_playerPointers[] = {&players[0], &players[1]};
static PropRecord playerProps[2];
static int playerCount = 1;
static int getPlayerCount(void) { return playerCount; }
#define NUM_IMPACT_SFX_STATES 4
static ALSoundState *g_ImpactSfxStates[NUM_IMPACT_SFX_STATES];
static struct RicochetSoundsSmall ricochet_sounds_small;
static struct PunchSounds punch_sounds;
static struct BulletFleshSounds bullet_flesh_sounds;
static struct LaserRichochetSounds laser_ricochet_sounds;
static struct RicochetSoundsLarge ricochet_sounds_large;
static struct image_sound { u16 *sfx; int sfx_len; } material;
static struct image_sound *g_HitTypeSounds[] = {&material};
static struct { int hitSound; } g_Textures[1];
static int g_ClockTimer = 1;
static u32 randomGetNext(void) { return 0; }
#include "production.inc"

/* Flesh-hit bank parameters taken from SFX 69. Other test sounds use the same
 * envelope so expected voice volumes reflect attenuation alone. */
static ALEnvelope envelope = {0, 59723, 2000, 127, 127};
static ALKeyMap keymaps[262];
static ALSound sounds[262];
static struct { s32 a, b, c; ALSound *sounds[262]; } instrument;
static struct ALBankAlt_s bank;

static int countLinks(ALLink *link) {
    int count = 0;
    for (; link; link = link->next) count++;
    return count;
}

static void reset(int occupied) {
    memset(&g_sndPlayer, 0, sizeof(g_sndPlayer));
    memset(&D_800243E4, 0, sizeof(D_800243E4));
    memset(states, 0, sizeof(states));
    memset(events, 0, sizeof(events));
    memset(&pending, 0, sizeof(pending));
    memset(g_ImpactSfxStates, 0, sizeof(g_ImpactSfxStates));
    D_800243E4.g_sndPlayerSoundStatePtr = states;
    for (int i = 1; i < 64; i++) alLink(&states[i].link, &states[i - 1].link);
    g_sndAllocatedVoicesCount = 0;
    g_sndPlayer.drvr = &synth;
    g_sndPlayer.maxSounds = 8;
    g_sndSfxSlotVolume = slotVolumes;
    for (int i = 0; i < SFX_SLOT_COUNT; i++) slotVolumes[i] = 32767;
    alEvtqNew(&g_sndPlayer.evtq, events, 64);
    for (int i = 0; i < occupied; i++) {
        ALEvent event = {0};
        event.type = AL_SNDP_API_EVT;
        alEvtqPostEvent(&g_sndPlayer.evtq, &event, 1000000);
    }
    for (int i = 0; i < 262; i++) {
        keymaps[i] = (ALKeyMap){0, 0, 0, 0, 45, 0};
        sounds[i] = (ALSound){.envelope = &envelope, .keyMap = &keymaps[i],
            .samplePan = 64, .sampleVolume = 100};
        instrument.sounds[i] = &sounds[i];
    }
    bank.instArray[0] = (struct ALInstrumentAlt_s *)&instrument;
    g_musicSfxBufferPtr = (ALBank *)&bank;
    for (int i = 0; i < 20; i++) ricochet_sounds_small.arr[i] = HIT_BULLET_METAL_A_SFX;
    for (int i = 0; i < 36; i++) ricochet_sounds_large.arr[i] = HIT_BULLET_METAL_A_SFX;
    for (int i = 0; i < 3; i++) punch_sounds.arr[i] = PUNCH1_SFX;
    for (int i = 0; i < 2; i++) {
        bullet_flesh_sounds.arr[i] = HIT_BULLET_FLESH_SFX;
        laser_ricochet_sounds.arr[i] = RICO_LASER1_SFX;
    }
    memset(playerProps, 0, sizeof(playerProps));
    for (int i = 0; i < 2; i++) players[i].prop = &playerProps[i];
    playerCount = 1;
    material.sfx = NULL;
    material.sfx_len = 0;
    g_HitTypeSounds[0] = &material;
    peakVolume = startedVoices = 0;
    interruptMask = OS_IM_ALL;
    onInterruptEnable = NULL;
}

static void dispatch(void) {
    ALEvent event;
    alEvtqNextEvent(&g_sndPlayer.evtq, &event);
    assert(event.type != AL_SNDP_API_EVT && event.type != (s16)-1);
    sndHandleEvent(&g_sndPlayer, (ALSndpEvent *)&event);
}

static void interruptDuringStart(void) {
    /* At the first scheduling opportunity, the handle and whole sequence must
     * already be valid, and the very first voice command must be attenuated. */
    ALSoundState *state = (ALSoundState *)pending.link.next;
    assert(state && state->vol == 10000);
    assert(state->unk3e & SOUND_FLAG_FINAL_IN_SEQUENCE);
    dispatch();
    assert(startedVoices == 1 && peakVolume == 7873);
}

static void testPlayback(void) {
    reset(0);
    onInterruptEnable = interruptDuringStart;
    ALSoundState *state = sndPlaySfxAtVolume(&bank, 69, &pending, 10000);
    assert(state && !onInterruptEnable && startedVoices == 1);
    dispatch(); /* Decay also uses the initial volume. */
    assert(peakVolume == 7873);

    reset(63);
    state = sndPlaySfxAtVolume(&bank, 69, &pending, 10000);
    assert(state && state->vol == 10000);
    assert(countLinks(g_sndPlayer.evtq.allocList.next) == 64);
    dispatch();
    dispatch();
    assert(peakVolume == 7873); /* No second event is needed to set volume. */

    reset(64);
    assert(!sndPlaySfxAtVolume(&bank, 69, &pending, 10000));
    assert(!pending.link.next && !D_800243E4.node.next);
    assert(countLinks((ALLink *)D_800243E4.g_sndPlayerSoundStatePtr) == 64);
    assert(interruptMask == OS_IM_ALL);

    reset(0);
    keymaps[69].velocityMin = 70;
    keymaps[70].velocityMax = 15; /* Delayed second component. */
    state = sndPlaySfxAtVolume(&bank, 69, &pending, 10000);
    assert(countLinks(D_800243E4.node.next) == 2);
    for (ALLink *link = D_800243E4.node.next; link; link = link->next)
        assert(((ALSoundState *)link)->vol == 10000);
    while (g_sndPlayer.evtq.allocList.next) dispatch();
    assert(startedVoices == 2 && peakVolume == 7873 && !pending.link.next);

    reset(0);
    keymaps[69].velocityMin = 70;
    D_800243E4.g_sndPlayerSoundStatePtr->link.next = NULL; /* One state left. */
    state = sndPlaySfxAtVolume(&bank, 69, &pending, 10000);
    assert(state && state->vol == 10000 && (state->unk3e & SOUND_FLAG_FINAL_IN_SEQUENCE));
    while (g_sndPlayer.evtq.allocList.next) dispatch();
    assert(!pending.link.next && startedVoices == 1 && peakVolume == 7873);

    reset(0);
    keymaps[69].keyMax = SOUND_FLAG_RETRIGGER;
    keymaps[69].velocityMax = 1;
    state = sndPlaySfxAtVolume(&bank, 69, &pending, 10000);
    ALSndpEvent repeat = {0};
    repeat.playSfx.type = AL_SNDP_PLAY_SFX_EVT;
    repeat.playSfx.state = state;
    repeat.playSfx.soundIndex = 69;
    repeat.playSfx.soundBank = &bank;
    sndHandleEvent(&g_sndPlayer, &repeat);
    assert((ALSoundState *)pending.link.next != state);
    assert(((ALSoundState *)pending.link.next)->vol == 10000);

    reset(0);
    assert(sndPlaySfxAtVolume(&bank, 69, NULL, -20)->vol == 0);
    dispatch();
    assert(peakVolume == 0);
    assert(sndPlaySfxAtVolume(&bank, 69, NULL, 40000)->vol == 32767);
    assert(sndPlaySfx(&bank, 69, NULL)->vol == 32767);
}

static void testAttenuation(void) {
    const float meters[] = {0, 19.99f, 20, 20.01f, 40, 60, 80, 99.99f, 100, 100.01f, 1000};
    const int expected[] = {32767, 32767, 32767, 32765, 28671, 24575, 20479, 16386, 16384, 16384, 16384};
    reset(0);
    for (unsigned i = 0; i < sizeof(meters) / sizeof(*meters); i++) {
        coord3d pos = {meters[i] * 100, 0, 0};
        int volume = sndCalculateFleshHitVolumeAtPosition(&pos);
        assert(volume == expected[i]);
    }
    int previous = 32767;
    for (int cm = 0; cm <= 20000; cm++) {
        coord3d pos = {cm, 0, 0};
        int volume = sndCalculateFleshHitVolumeAtPosition(&pos);
        assert(volume >= 16384 && volume <= previous);
        previous = volume;
        assert(sndCalculateVolumeAtPosition(&pos, 5000, 6000)
            == sndCalculateVolumeFromDistance(cm, 5000, 6000));
    }
    coord3d diagonal = {3600, 4800, 0}; /* 60 m Euclidean distance. */
    assert(sndCalculateFleshHitVolumeAtPosition(&diagonal) == 24575);
    playerCount = 2;
    playerProps[1].pos = diagonal;
    assert(sndCalculateFleshHitVolumeAtPosition(&diagonal) == 32767);
}

static void testImpactCallers(void) {
    PropRecord target = {PROP_TYPE_CHR, {6000, 0, 0}};
    const int weapons[] = {ITEM_WPPK, ITEM_KNIFE, ITEM_FIST};
    const int volumes[] = {24575, 0, 0};
    const int soundIds[] = {HIT_BULLET_FLESH_SFX, HIT_BULLET_SNOW_SFX, PUNCH1_SFX};
    for (int i = 0; i < 3; i++) {
        reset(0);
        gunfirePlaySfxBulletImpact(weapons[i], &target, -1);
        assert(g_ImpactSfxStates[0]);
        assert(g_ImpactSfxStates[0]->sound == &sounds[soundIds[i]]);
        assert(g_ImpactSfxStates[0]->vol == volumes[i]);
        assert(countLinks(g_sndPlayer.evtq.allocList.next) == 1);
    }
    reset(63);
    target.pos.x = 15000;
    gunfirePlaySfxBulletImpact(ITEM_WPPK, &target, -1);
    assert(g_ImpactSfxStates[0]->vol == 16384);
    dispatch();
    assert(peakVolume == 12899);

    reset(0);
    target.type = PROP_TYPE_OBJ;
    u16 fleshMaterial[] = {HIT_BULLET_FLESH_SFX};
    material.sfx = fleshMaterial;
    material.sfx_len = 1;
    gunfirePlaySfxBulletImpact(ITEM_WPPK, &target, 0);
    assert(g_ImpactSfxStates[0]->vol == 0);
    assert(g_ImpactSfxStates[1]->vol == 16384);
    reset(0);
    gunfirePlaySfxBulletThroughGlass(&target.pos);
    assert(g_ImpactSfxStates[0]->vol == 0);
    reset(0);
    g_HitTypeSounds[0] = NULL;
    gunfirePlaySfxRicochetSounds(ITEM_WPPK, &target.pos, 0);
    assert(g_ImpactSfxStates[0]->vol == 0);
    reset(0);
    ALSoundState *yelp = chrobjSndPlayAtPosition(GET_HIT_MALE0_SFX, &pending, &target.pos);
    assert(yelp->vol == 0);
}

int main(void) {
    assert(sizeof(ALEvent) == sizeof(ALSndpEvent));
    testPlayback();
    testAttenuation();
    testImpactCallers();
    puts("PASS: initial-volume scheduling, queue pressure, sequence/delay/retrigger handling,");
    puts("      flesh-hit 20-100 m curve, nearest-player distance, and impact caller isolation.");
}
