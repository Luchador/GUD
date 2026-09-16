#ifndef TEST_SETUP_COMPARE_H
#define TEST_SETUP_COMPARE_H
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"

/* Commit/save/load may remove abandoned bytes and relocate native pointers.
 * Compare compact native copies; this still checks every retained scalar,
 * record, script byte, index and string. Raw no-op cases remain byte exact. */
static void SetupAssertNativeEqual(const SetupFile *a, const SetupFile *b)
{
    unsigned char *left, *right;
    DWORD lsize, rsize;
    const char *why;
    if (a->size == b->size && !memcmp(a->data,b->data,a->size)) { return; }
    assert(SetupCompactNative(a->data,a->size,&left,&lsize,&why));
    assert(SetupCompactNative(b->data,b->size,&right,&rsize,&why));
    assert(lsize == rsize && !memcmp(left,right,lsize));
    free(left); free(right);
}
#endif
