#ifndef GEDITOR_BGCOMMANDS_H
#define GEDITOR_BGCOMMANDS_H
#include "bgload.h"

/* Read-only view of the native eight-byte visibility cells. Data cells belong
 * to the preceding instruction; 0x64 is a polygon address, 0x65 a value. */
#define BG_VIS_NO_TARGET ((DWORD)-1)
#define BG_VIS_UNKNOWN 1u
#define BG_VIS_LENGTH 2u
#define BG_VIS_OPERAND 4u
#define BG_VIS_ROOM 8u
#define BG_VIS_PORTAL 16u
#define BG_VIS_FLOW 32u
#define BG_VIS_NESTING_LIMIT 256u
#define BG_VIS_CELL_LIMIT 65536u

typedef enum BgVisArgument { BG_VIS_NONE, BG_VIS_VALUE, BG_VIS_ONE_ROOM, BG_VIS_ROOM_RANGE, BG_VIS_ONE_PORTAL } BgVisArgument;
typedef struct BgVisInstruction {
    DWORD offset, bytes, depth, warnings;
    DWORD arg[2], portal, matches, room1, room2;
    unsigned char opcode, length, operandType[2];
    BgVisArgument argument;
} BgVisInstruction;
typedef struct BgVisProgram {
    unsigned char *data;
    BgVisInstruction *instructions;
    DWORD offset, size, count, warnings;
    BOOL present, complete, singleDisplayList;
    char problem[192];
} BgVisProgram;
BOOL BgVisDecode(const BgFile *bg,const BgPortalFile *portals,DWORD roomcount,
    BgVisProgram *out,const char **why);
void BgVisFree(BgVisProgram *program);
const char *BgVisName(unsigned int opcode);
const char *BgVisExplanation(unsigned int opcode);
void BgVisSummary(const BgVisInstruction *instruction,char *out,size_t size);
void BgVisWarnings(const BgVisInstruction *instruction,char *out,size_t size);
/* Includes readable instructions, original cell bytes, and any diagnostics. */
char *BgVisReport(const BgVisProgram *program,const char *name);
#endif
