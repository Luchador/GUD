# assembler directives
.set noat      # allow manual use of $at
.set noreorder # don't insert nops after branches
.set gp=64

.include "macros.inc"

.section .data
.balign 8
glabel g_tlbRandomSeed
.word 0xAB8D9F77
.word 0x81280783

.section .text, "ax"

#########################################################################
# u64 tlbRandomGetNext(void)
#
# Returns next 64-bit pseudorandom value and updates global seed.
#########################################################################

glabel tlbRandomGetNext
	# load 64-bit seed
	lui	$a0, %hi(g_tlbRandomSeed)
	ld	$a0, %lo(g_tlbRandomSeed)($a0)
	lui	$at, %hi(g_tlbRandomSeed)
	dsll32	$a2, $a0, 0x1f  # a2 = seed << 63 (upper 32 bits)
	dsll	$a1, $a0, 0x1f   # a1 = seed << 31 (full 64-bit)
	dsrl	$a2, $a2, 0x1f   # a2 = (seed >> 32) & 1
	dsrl32	$a1, $a1, 0     # a1 = (seed << 31) >> 32 = top 32 bits of (seed << 31)
	# Extract upper 32 bits of (seed << 12) and Combine the shifted parts
	dsll32	$a0, $a0, 0xc   # a0 = seed << 44
	or	$a2, $a2, $a1    # a2 = (bit31) | (upper32(seed<<31))
	dsrl32	$a0, $a0, 0     # a0 = (seed << 12) >> 32
	xor	$a2, $a2, $a0    # a2 ^= upper32(seed<<12)
	dsrl	$a0, $a2, 0x14
	andi	$a0, $a0, 0xfff  # mask to 12 bits
	xor	$a0, $a0, $a2
	# Store/Return new seed
	dsll32	$v0, $a0, 0
	sd	$a0, %lo(g_tlbRandomSeed)($at)
	jr	$ra
	 dsra32	$v0, $v0, 0
