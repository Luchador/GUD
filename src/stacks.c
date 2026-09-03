/**
 * Thread stack storage.
 * 
 * MIPS grows stacks downwards so each thread receives a stack pointer near the end of its corresponding buffer.
 * 
 * sp_boot - Temporary stack used by _start before the main thread is created.
 *           _start sets $sp to sp_rmon, which immediately follows sp_boot and
 *           therefore acts as the top of the boot stack.
 * sp_rmon - Remote monitor/Indy debugging thread stack.
 * sp_idle - Idle thread stack.
 * sp_shed - Scheduler thread stack ("shed" is the original abbreviated name).
 * sp_main - Main game thread stack.
 * sp_audi - Audio manager thread stack.
 *
 * The linker places these buffers together in the dedicated .stacks section.
 */

#include <ultra64.h>


u8 sp_boot[0x10];
u8 sp_rmon[0x300];
u8 sp_idle[0x40];
u8 sp_shed[0x200];
u8 sp_main[0x8000];
u8 sp_audi[0x1000];
