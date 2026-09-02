#include <ultra64.h>
#include <bondconstants.h>
#include <music.h>
#include "frametiming.h"


/**
 * Stored the number of nominal game-frame intervals that elapsed since the previous rendered frame i.e. the number of skipped frames when the game lags.
 * At full 60 FPS it equals 1. At 30 FPS it becomes 2, etc...
 */
s32 g_FrameDelta = 1;

u32 copy_of_osgetcount_value_0 = 0;
u32 copy_of_osgetcount_value_1 = 0;
s32 frameDelay = 1;


/**
 * Stores the current OS count in the two global variables.
 */
void store_osgetcount(void)
{
    copy_of_osgetcount_value_1 = osGetCount();
    copy_of_osgetcount_value_0 = copy_of_osgetcount_value_1;
}


/**
 * Updates the timing-related counters and frame information based on the given argument.
 *
 * @param deltaFrames The number of frames to add to the current frame counter.
 */
void updateFrameCounters(s32 deltaFrames)
{
    copy_of_osgetcount_value_0 = (s32) copy_of_osgetcount_value_1;
    copy_of_osgetcount_value_1 = osGetCount();

    g_FrameDelta = deltaFrames;

    musicTrack1LoopWatchdog();
}


/**
 * Waits until the appropriate time has passed before updating the frame counters.
 * This function effectively controls the frame rate by waiting for the next tick.
 */
void waitForNextFrame(void)
{
    u32 nextFrameTime; //next frame time?

    do 
    {
        nextFrameTime = ((osGetCount() - copy_of_osgetcount_value_1) + 387937) / 775875; //current time + 1/5
    } 
    while (nextFrameTime < frameDelay);

    frameDelay = 1;
    updateFrameCounters(nextFrameTime);
}


/**
 * Fix the Silo X track not looping.
 */
void musicTrack1LoopWatchdog(void)
{
    if (g_musicXTrack1CurrentTrackNum == M_SILOX && alCSPGetState(g_musicXTrack1SeqPlayer) == AL_STOPPED)
    {
        musicTrack1Play(M_SILOX);
    }
}
