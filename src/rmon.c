/*
 * THIS FILE IS AN OS FILE (PR\libultra\rmon\rmonMain.c)
 */
#include <stdarg.h>
#include <ultra64.h>
#include <PR/os.h>
#include "libultra/libc/xstdio.h"
#include "rmon.h" /*<PR/rmon.h>*/

/************************************************************************
Function: rmonMain
Args: none
Type: void
Purpose: This is the main loop of the rmon debugger. It is mostly a protocol
	parser/dispatcher.
************************************************************************/
void rmonMain(void) {
}

/**
 * rmonGetToken
 * returns true if this if rmon is disabled
 */
s32 rmonGetToken(void) {
    #if defined(DEBUG)
        //flesh out sending a token from pc app
        return FALSE;
    #else
        return TRUE;
    #endif
}


/**
 * Removed
 * reimpliment osWriteHost
 * target rmon
*/
void osWriteHost(void * buffer, u32 size)
{
    if (buffer);
	if (size);
}


/**
 * Removed
 * reimpliment osReadHost
 * target rmon
*/
void osReadHost(void * buffer, u32 size)
{
    if (buffer);
	if (size);
}


/**
 * Removed
 * rmonPrintf
 */
void rmonPrintf(void)
{
}

/*
 * -----------------------------------------------------------------
 * This is a new file (PR/libultra/libc/syncprintf.c)
*/



void *proutSyncPrintf(void *str, const char *buf, size_t n)
{
    return (void *)1;
}


/**
 * Print formatted string to Debugger
 */
void osSyncPrintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    _Printf(&proutSyncPrintf, NULL, fmt, args);
    va_end(args);
}
