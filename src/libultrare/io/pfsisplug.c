#include <PR/os.h>
#include <io/controller.h>
#include <ultra64.h>

extern u8 __osMaxControllers;

OSPifRam __osPfsPifRam;


void __osPfsRequestData(u8 cmd) {
    u8 *ptr;
    __OSContRequesFormat requestformat;
    int i;

    __osContLastCmd = cmd;

    for (i = 0; i < ARRLEN(__osPfsPifRam.ramarray) + 1; i++) { // also clear pifstatus
        __osPfsPifRam.ramarray[i] = 0;
    }

    __osPfsPifRam.pifstatus = CONT_CMD_EXE;

    ptr = (u8 *)&__osPfsPifRam;
    requestformat.dummy = CONT_CMD_NOP;
    requestformat.txsize = CONT_CMD_REQUEST_STATUS_TX;
    requestformat.rxsize = CONT_CMD_REQUEST_STATUS_RX;
    requestformat.cmd = cmd;
    requestformat.typeh = CONT_CMD_NOP;
    requestformat.typel = CONT_CMD_NOP;
    requestformat.status = CONT_CMD_NOP;
    requestformat.dummy1 = CONT_CMD_NOP;
    for (i = 0; i < __osMaxControllers; i++) {
        *(__OSContRequesFormat *)ptr = requestformat;
        ptr += sizeof(__OSContRequesFormat);
    }
    *ptr = CONT_CMD_END;
}

void __osPfsGetInitData(u8 *pattern, OSContStatus *data) {
    u8 *ptr;
    __OSContRequesFormat requestformat;
    int i;
    u8 bits;
    bits = 0;
    ptr = (u8 *)&__osPfsPifRam;
    for (i = 0; i < __osMaxControllers; i++, ptr += sizeof(__OSContRequesFormat)) {
        requestformat = *(__OSContRequesFormat *)ptr;
        data->errno = CHNL_ERR(requestformat);
        if (data->errno == 0) {
            data->type = (requestformat.typel << 8) | (requestformat.typeh);
            data->status = requestformat.status;
            bits |= 1 << i;
        }
        data++;
    }
    *pattern = bits;
}
