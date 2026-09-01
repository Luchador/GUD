#include <ultra64.h>
#include "compiletime.h"

#ifdef BYTEMATCH
    char *get_compile_time(void) {
        return "Jun 29 1997 20:46:05";
    }
#else
char *get_compile_time(void) {
    return __DATE__" " __TIME__;
}
#endif

