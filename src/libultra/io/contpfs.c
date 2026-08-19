#include <os_internal.h>
#include "controller.h"

s32 __osIdCheckSum(u16 *ptr, u16 *csum, u16 *icsum)
{
    u16 data;
    u32 j;
    data = 0;
    *icsum = 0;
    *csum = *icsum;
    for (j = 0; j < 28; j += 2)
    {
        //feels like this should be a compiler optimization not manual..
        //but it doesn't match and I'm pretty sure this is just -O1
        data = *(u16 *)((u8 *)ptr + j);
        //data = ptr[j]
        *csum += data;
        *icsum += ~data;
    }
    return 0;
}
s32 __osRepairPackId(OSPfs *pfs, __OSPackId *badid, __OSPackId *newid)
{

    s32 ret;
    u8 temp[32];
    u8 comp[32];
    u8 mask;
    int i;
    int j;
    u16 index[4];

    ret = 0;
    mask = 0;
    SET_ACTIVEBANK_TO_ZERO;
    newid->repaired = -1;
    newid->random = osGetCount();
    newid->serial_mid = badid->serial_mid;
    newid->serial_low = badid->serial_low;
    for (j = 0; j < PFS_MAX_BANKS;)
    {
        pfs->activebank = j;
        ERRCK(__osPfsSelectBank(pfs))
        ERRCK(__osContRamRead(pfs->queue, pfs->channel, 0, (u8*)&temp)); //TODO: fix magic number
        temp[0] = j | 0x80;
        for (i = 1; i < ARRLEN(temp); i++)
        {

            temp[i] = ~temp[i];
        }

        ERRCK(__osContRamWrite(pfs->queue, pfs->channel, 0, (u8*)temp, FALSE)); //oddr 0, don't force
        ERRCK(__osContRamRead(pfs->queue, pfs->channel, 0, (u8*)&comp));

        for (i = 0; i < ARRLEN(temp); i++)
        {
            if (comp[i] != temp[i])
                break;
        }
        if (i != ARRLEN(temp))
            break;
        if (j > 0)
        {
            pfs->activebank = 0;
            ERRCK(__osPfsSelectBank(pfs));
            ERRCK(__osContRamRead(pfs->queue, pfs->channel, 0, (u8*)temp));
            if (temp[0] != 128)
                break; //TODO: remove magic constant
        }
        j++;
    }
    pfs->activebank = 0;
    ERRCK(__osPfsSelectBank(pfs));
    if (j > 0)
        mask = 1;
    else
        mask = 0;
    newid->deviceid = (badid->deviceid & (u16)~1) | mask;
    newid->banks = j;
    newid->version = badid->version;
    __osIdCheckSum((u16*)newid, &newid->checksum, &newid->inverted_checksum);
    index[0] = 1;
    index[1] = 3;
    index[2] = 4;
    index[3] = 6;
    for (i = 0; i < ARRLEN(index); i++)
    {
        ERRCK(__osContRamWrite(pfs->queue, pfs->channel, index[i], (u8*)newid, TRUE));
    }
    ERRCK(__osContRamRead(pfs->queue, pfs->channel, 1, (u8*)temp));
    for (i = 0; i < ARRLEN(temp); i++)
    {
        if (temp[i] != ((u8 *)newid)[i])
            return PFS_ERR_ID_FATAL;
    }
    return 0;
}

s32 __osCheckPackId(OSPfs *pfs, __OSPackId *temp)
{
    u16 index[4];
    s32 ret;
    u16 sum;
    u16 isum;
    int i;
    int j;

    ret = 0;
    SET_ACTIVEBANK_TO_ZERO;
    index[0] = 1;
    index[1] = 3;
    index[2] = 4;
    index[3] = 6;
    for (i = 1; i < ARRLEN(index); i++)
    {
        ERRCK(__osContRamRead(pfs->queue, pfs->channel, index[i], (u8*)temp));
        __osIdCheckSum((u16 *)temp, &sum, &isum);
        if (temp->checksum == sum && temp->inverted_checksum == isum)
            break;
    }
    if (i == ARRLEN(index))
        return PFS_ERR_ID_FATAL;

    for (j = 0; j < ARRLEN(index); j++)
    {
        if (j != i)
        {
            ERRCK(__osContRamWrite(pfs->queue, pfs->channel, index[j], (u8*)temp, TRUE));
        }
    }
    return 0;
}

s32 __osGetId(OSPfs *pfs)
{
    int k;
    u16 sum;
    u16 isum;
    u8 temp[32];
    __OSPackId newid;
    s32 ret;
    __OSPackId *id;

    SET_ACTIVEBANK_TO_ZERO;
    ERRCK(__osContRamRead(pfs->queue, pfs->channel, 1, (u8*)temp));
    __osIdCheckSum((u16*)temp, &sum, &isum);
    id = (__OSPackId*)temp;
    if (id->checksum != sum || id->inverted_checksum != isum)
    {
        ret = __osCheckPackId(pfs, id);
        if (ret == PFS_ERR_ID_FATAL)
        {
            ERRCK(__osRepairPackId(pfs, id, &newid));
            id = &newid;
        }
        else if (ret != 0)
        {
            return ret;
        }
    }
    if ((id->deviceid & 1) == 0) //TODO: remove magic constant
    {
        ERRCK(__osRepairPackId(pfs, id, &newid));
        id = &newid;
        if ((id->deviceid & 1) == 0)
            return PFS_ERR_DEVICE;
    }
    for (k = 0; k < ARRLEN(pfs->id); k++)
    {
        pfs->id[k] = ((u8 *)id)[k];
    }
    pfs->version = id->version;
    pfs->banks = id->banks;
    pfs->inode_start_page = pfs->banks * 2 + 3; //TODO: loads of magic constants..
    pfs->dir_size = 16;
    pfs->inode_table = 8;
    pfs->minode_table = pfs->banks * PFS_ONE_PAGE + 8;
    pfs->dir_table = pfs->minode_table + pfs->banks * PFS_ONE_PAGE;
    ERRCK(__osContRamRead(pfs->queue, pfs->channel, 7, pfs->label));
    return 0;
}



s32 __osPfsSelectBank(OSPfs *pfs)
{
    u8 temp[BLOCKSIZE];
    int i;
    s32 ret;
    ret = 0;
    for (i = 0; i < ARRLEN(temp); i++)
    {
        temp[i] = pfs->activebank;
    }
    ret = __osContRamWrite(pfs->queue, pfs->channel, 1024, (u8*)temp, FALSE);
    return ret;
}
