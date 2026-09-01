#include <ultra64.h>
#include <PR/gbi.h>

OSMesg vi_c_debug_MSG[0x8];
OSMesgQueue vi_c_debug_MQ;


void viInit(void)
{
    osCreateMesgQueue(&vi_c_debug_MQ, &vi_c_debug_MSG, 8);
}
