#ifndef DIAG_TEST_RCP_H
#define DIAG_TEST_RCP_H
#include <ultra64.h>
enum {
    SP_STATUS_REG, DPC_STATUS_REG, PI_STATUS_REG, SP_PC_REG,
    DPC_CURRENT_REG, DPC_END_REG, DPC_START_REG, VI_CONTROL_REG, VI_ORIGIN_REG,
    VI_WIDTH_REG, VI_BURST_REG, VI_V_SYNC_REG, VI_H_SYNC_REG, VI_LEAP_REG,
    VI_H_START_REG, VI_V_START_REG, VI_V_BURST_REG, VI_INTR_REG,
    VI_X_SCALE_REG, VI_Y_SCALE_REG, TEST_REG_COUNT
};
#define SP_SET_HALT 2
#define SP_CLR_HALT 1
#define SP_CLR_BROKE 4
#define SP_CLR_SSTEP 0x20
#define SP_CLR_INTR_BREAK 0x80
#define SP_STATUS_HALT 1
#define SP_STATUS_BROKE 2
#define SP_STATUS_DMA_BUSY 4
#define SP_STATUS_DMA_FULL 8
#define SP_IMEM_START 0x04001000
#define SP_DMEM_START 0x04000000
#define DPC_SET_FREEZE 8
#define DPC_STATUS_XBUS_DMEM_DMA 1
extern u16 *testFramebuffer;
u32 testRead(u32 reg);
void testWrite(u32 reg, u32 value);
#define IO_READ(reg) testRead(reg)
#define IO_WRITE(reg, value) testWrite(reg, value)
#define K0_TO_PHYS(p) ((u32)(uintptr_t)(p) & 0x1fffffff)
#define PHYS_TO_K1(p) ((void *)testFramebuffer)
#endif
