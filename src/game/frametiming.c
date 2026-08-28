#include <ultra64.h>
#include <bondconstants.h>
#include <music.h>
#include "frametiming.h"


s32 lastFrameCounter = -1;
s32 currentFrameCounter = 0;

/**
 * Appears to be rendered framerate, or some kind of counter since the last frame update.
 */
s32 speedgraphframes = 1;

s32 previousFrameCounter = -1;
s32 halfFrameCounter = 0; // half of currentFrameCounter
s32 isFrameCounterOdd = 0; // is currentFrameCounter Odd
s32 halfMinusPreviousCounter = 0; // half - previousFrameCounter
u32 copy_of_osgetcount_value_0 = 0;
u32 copy_of_osgetcount_value_1 = 0;
s32 frameDelay = 1; //usually 1


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

    lastFrameCounter = currentFrameCounter;
    currentFrameCounter = (s32) (currentFrameCounter + deltaFrames);
    speedgraphframes = deltaFrames;

    previousFrameCounter = (s32) halfFrameCounter;
    halfFrameCounter = (s32) (currentFrameCounter / 2);
    isFrameCounterOdd = (s32) (currentFrameCounter & 1);
    halfMinusPreviousCounter = (s32) (halfFrameCounter - previousFrameCounter);

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


void setFrameDelay(s32 arg0)
{
    #ifdef LEFTOVERDEBUG
    frameDelay = arg0;
    #endif
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
