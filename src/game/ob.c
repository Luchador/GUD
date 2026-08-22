#include <ultra64.h>
#include "macro.h"
#include "ob.h"
#include <deb.h>
#include <memp.h>
#include <assets/obseg/obseg.h>
#include "decompress.h"
#include "assets/obseg/file_resource_id_enums.h"

//bss
//800888b0

 resource_lookup_data_entry resource_lookup_data_array[OBJ_INDEX_MAX];


// data
//D:80046050
s32 ob_c_debug_notice_list_entry = 0;


#include <assets/obseg/file_resource_table.inc.c>
/* struct fileentry file_resource_table[] =
   {
       blah;
    };
 */


//D:800482D4
s32 file_entry_max = OBJ_INDEX_END;


void load_resource(u8 *ptrdata, s32 bytes,  fileentry *srcfile,  resource_lookup_data_entry *lookupdata)
{
    u8 *source;
    u8  buffer[0x2100];

    if (bytes == 0)
    {
        romCopy(ptrdata, srcfile->hw_address, lookupdata->rom_size);
        return;
    }

    source = (ptrdata + bytes) - ((lookupdata->rom_size + 7) & -8);

    if ((u32) (source - ptrdata) < 8U)
    {
        lookupdata->poolRemaining = 0;
    }
    else
    {
        romCopy(source, srcfile->hw_address, lookupdata->rom_size);
        lookupdata->poolRemaining = decompressdata(source, ptrdata, buffer);
    }
}


void obInit(void)
{
    s32 size;
    s32 i;
    
    for (i = 1; i < file_entry_max - 1; i++)
    {
        size = (file_resource_table[i + 1].hw_address - file_resource_table[i].hw_address);

        resource_lookup_data_array[i].rom_size = size;
        resource_lookup_data_array[i].poolRemaining = 0;
        resource_lookup_data_array[i].pc_size = 0;
        resource_lookup_data_array[i].rom_remaining = 0;
    }
}


void *_fileIndexLoadToBank(s32 index, FILELOADMETHOD param_2, s32 size, u8 bank)
{
    return fileIndexLoadToBank(index, param_2, size, bank);
}


void *_fileIndexLoadToAddr(int index, FILELOADMETHOD param_2, u8 *ptrdata, int size)
{
    return fileIndexLoadToAddr(index, param_2, ptrdata, size);
}


void *_fileNameLoadToBank(char *filename, FILELOADMETHOD loadMethod, s32 size, u8 bank)
{
    return fileIndexLoadToBank(fileGetIndex(filename), loadMethod, size, bank);
}


void * _fileNameLoadToAddr(char *filename, FILELOADMETHOD loadMethod, u8 *ptrdata, s32 size)
{
    return fileIndexLoadToAddr(fileGetIndex(filename), loadMethod, ptrdata, size);
}


/**
 * Loads data stored at an offset of a bg file.
 */
void obLoadBGFileBytesAtOffset(u8 *bgname, u8 *target, s32 offset, s32 len)
{
    s32 index;
    fileentry *fileentry;

    index = fileGetIndex(bgname);
    fileentry = &file_resource_table[index];

    if (resource_lookup_data_array[index].rom_size != 0)
    {
        //if the size of offset data would exceed file size, loop forever
        if ((resource_lookup_data_array[index].rom_size + 0xF) < (offset + len))
        {
            while (1);
        }
        romCopy(target, &fileentry->hw_address[offset], len, fileentry);
    }
}


void *fileIndexLoadToBank(s32 index, FILELOADMETHOD loadMethod, s32 size, u8 bank)
{
    resource_lookup_data_entry *info = &resource_lookup_data_array[index];
    s32                         bytes;
    void                       *ptrdata = NULL;

    if (loadMethod == FILELOADMETHOD_EXTRAMEM || loadMethod == FILELOADMETHOD_DEFAULT || loadMethod == 2)
    {
        // bytes = info->poolRemaining;
        if (info->poolRemaining == 0)
        { // verify pool remaining is 0
            info->poolRemaining = mempGetBankSizeLeft(bank);
            // info->poolRemaining = bytes;
        }
        // bytes = info->poolRemaining;
        ptrdata             = mempAllocBytesInBank(info->poolRemaining, bank); // get pointer to allocated space in bank
        info->rom_remaining = info->poolRemaining;

        load_resource(ptrdata, info->poolRemaining, &file_resource_table[index], info);

        if (loadMethod != FILELOADMETHOD_EXTRAMEM)
        {
            mempAddEntryOfSizeToBank(ptrdata, info->poolRemaining, bank);
        }
    }
    else // skipped in PD
    {
        if (info->poolRemaining == 0)
        {
            if (info->rom_size != 0)
            {
                info->poolRemaining = info->rom_size;
            }
            else
            {
                info->poolRemaining = info->pc_size;
            }
        }

        ptrdata = mempAllocBytesInBank(info->poolRemaining, bank);
        info->rom_remaining = info->poolRemaining;

        load_resource(ptrdata, 0, &file_resource_table[index], info);

        if (size == 0)
        {
            info->loaded_bank = bank;
        }
    }

    return ptrdata;
}


void *fileIndexLoadToAddr(s32 index, FILELOADMETHOD loadMethod, void *ptrdata, s32 bytes) //#match https://decomp.me/scratch/YExRh
{
    resource_lookup_data_entry *info = &resource_lookup_data_array[index];

    if (!info->poolRemaining)
    {
        if (info->rom_size)
        {
            info->poolRemaining = info->rom_size;
        }
        else
        {
            info->poolRemaining = info->pc_size;
        }
    }

    if (loadMethod == FILELOADMETHOD_EXTRAMEM || loadMethod == FILELOADMETHOD_DEFAULT || loadMethod == 2)
    {
        info->rom_remaining = bytes;
        load_resource(ptrdata, bytes, &file_resource_table[index], &resource_lookup_data_array[index]);
    }
    else
    {
        load_resource(ptrdata, 0, &file_resource_table[index], &resource_lookup_data_array[index]);
    }

    return ptrdata;
}


s32 get_pc_remaining_buffer_for_index(s32 index)
{
    return resource_lookup_data_array[index].poolRemaining;
}


s32 get_rom_remaining_buffer_for_index(s32 index)
{
    return resource_lookup_data_array[index].rom_remaining;
}


void fileSetSize(s32 filenum, u8* ptr, u32 size, s32 reallocate)
{
    resource_lookup_data_array[filenum].poolRemaining = size;
    resource_lookup_data_array[filenum].rom_remaining = size;

    if (reallocate != 0)
    {
        mempAddEntryOfSizeToBank(ptr, resource_lookup_data_array[filenum].poolRemaining, MEMPOOL_STAGE);
    }
}


s32 get_pc_buffer_remaining_value(u8 *name)
{
    s32 index;

    index = fileGetIndex(name);

    return resource_lookup_data_array[index].poolRemaining;
}


void obBlankResourcesLoadedInBank(u8 bank)
{
    s32 i;

    for (i = 1; i < file_entry_max; i++) 
    {
        if (resource_lookup_data_array[i].loaded_bank <= bank) 
        {
            resource_lookup_data_array[i].loaded_bank = '\0';
        }

        if (bank == 4) {
            resource_lookup_data_array[i].poolRemaining = 0;
        }
    }
}


void obBlankResourcesInBank5(void) 
{
  obBlankResourcesLoadedInBank(MEMPOOL_ME);
}


s32 fileGetIndex(u8 *resname)
{
    s32 i;
    s32 stack;
    s32 size;
    struct resource_lookup_data_entry *lookup;

    for (i = 1; i < file_entry_max; i++)
    {
        if (file_resource_table[i].filename != NULL)
        {
            if (strcmp(resname, file_resource_table[i].filename) == 0)
            {
                return i;
            }
        }
    }
    
    i = file_entry_max;
    
    //too many files exist
    if (i >= OBJ_INDEX_MAX)
    {
        return 0;
    }
    
    file_entry_max++;
    file_resource_table[i].index = i; // offset 0
    file_resource_table[i].filename = resname;  // offset 4

    lookup = &resource_lookup_data_array[i];
    lookup->unk_11 = 0;
    file_resource_table[i].hw_address = 0;  // offset 8
    lookup->rom_size = 0;
    lookup->poolRemaining = 0;
    lookup->pc_size = ALIGN16_a(size);
    lookup->rom_remaining = 0;
    lookup->loaded_bank = 0;

    return i;
}
