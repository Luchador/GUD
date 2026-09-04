#include <ultra64.h>
#include <PR/os.h>
#include <bondconstants.h>
#include "sched.h"
#include "audi.h"
#include "thread_config.h"
#include "bondgame.h"


// 0x5622 = 22050
#define OUTPUT_RATE                    0x5622
#define FRAMES_PER_FIELD_AS_POW2            1
#define AUDIO_FRAME_MESSAGE_QUEUE_SIZE      8
#define AUDIO_REPLY_MESSAGE_QUEUE_SIZE      8
#define AUDIO_DMA_IO_QUEUE_SIZE            64
#define AUDIO_DMA_MAX_BUFFER_LENGTH     0x200

#define NUMBER_OUTPUT_BUFFERS               3
#define NUMBER_ACMD_LISTS                   2
#define MAX_ACMD_SIZE                    3000
#define NUMBER_DMA_BUFFERS                 64
#define EXTRA_SAMPLES                    0x25
#define DMA_BUFFER_FRAME_LAG                 1

#define MAIN_QUIT_MESSAGE                  10

extern long long int rspbootTextStart[];
extern long long int gsp3DTextStart[];
extern long long int aspMainTextStart[];
extern long long int aspMainDataStart[];
extern u8 sp_audi[];

typedef struct DMABuffer_s {
    ALLink node;
    int startAddr;
    u32 lastFrame;
    u32 physicalAddress;
} DMABuffer;

typedef struct DMAState_s {
    u8 initialized;
    DMABuffer *firstUsed;
    DMABuffer *firstFree;
} DMAState;

typedef struct AudioInfo_s {
    s16 *data;
    s16 *physicalData;

    /**
     * # of samples synthesized in this frame
     */
    s16 frameSamples;
    OSScTask task;
} AudioInfo;

static u32 g_AudioFrameCount;
static u32 g_NextDmaIndex;

/**
 * This macro is used/defined in both libultra and libnaudio
 */
#define ms *(((s32)((f32)44.1)) & ~0x7)

#define CUSTOM_FX_SECTION_COUNT   6
#define CUSTOM_FX_SECTION_SIZE    8
/*
* Following the libultra and libnaudio naming convention ...
*/
static s32 g_CustomFxParams[CUSTOM_FX_SECTION_COUNT * CUSTOM_FX_SECTION_SIZE + 2] = {

    /* sections	   length */
             6,     160 ms,

    /*                                         chorus  chorus   filter
    input    output  fbcoef  ffcoef    gain     rate   depth    coef  */
        0,     4 ms,   9830,  -9830,      0,        0,     0,       0,
     4 ms,     8 ms,   9830,  -9830, 0x2B84,        0,     0,  0x2500,
    20 ms,    64 ms,  16384, -16384, 0x11EB,        0,     0,  0x3000,
    80 ms,   140 ms,  16384, -16384, 0x11EB,        0,     0,  0x3500,
    84 ms,   120 ms,   8192,  -8192,      0,        0,     0,  0x4000,
        0,   148 ms,  13000, -13000,      0,   0x017C,   0xA,  0x4500
};

/**
 * sizeof(struct AudioManager_s) == 0x288 (648)
 */
struct AudioManager_s {
    Acmd *cmdList[NUMBER_ACMD_LISTS];
    AudioInfo *audioInfo[NUMBER_OUTPUT_BUFFERS];
    OSThread audioThread;
    OSMesgQueue frameMessageQueue;
    OSMesg frameMessageBuffer[AUDIO_FRAME_MESSAGE_QUEUE_SIZE];
    OSMesgQueue replyMessageQueue;
    OSMesg replyMessageBuffer[AUDIO_REPLY_MESSAGE_QUEUE_SIZE];
    ALGlobals g;
};

static struct AudioManager_s g_AudioManager;

/*
 * osScAddClient stores the retrace cadence in g_AudioClient[1].next, and the
 * scheduler reads that word when deciding whether to notify this client.
 */
static OSScClient g_AudioClient[2];
static DMAState g_DmaState;

static DMABuffer g_DmaBuffers[NUMBER_DMA_BUFFERS];

static u32 g_MinFrameSize;
static u32 g_FrameSize;

static OSIoMesg g_DmaIOMessageBuffer[AUDIO_DMA_IO_QUEUE_SIZE];
static OSMesgQueue g_DmaMessageQueue;
static OSMesg g_DmaMessageBuffer[AUDIO_DMA_IO_QUEUE_SIZE];


// Begin forward declarations.

s32 amDmaCallback(s32 addr, s32 len, void* state);
void amClearDmaBuffers(void);
void amHandleFrameMessage(AudioInfo *info, AudioInfo *lastInfo, Acmd *commandList);
void amMain(void* arg);
ALDMAproc amDmaNew(DMAState** state);

// End forward declarations.

/**
 * @param alconf hw setup/config.
 */
void amCreateAudioManager(ALSynConfig* alconf)
{
    u32 j;
    u32 maxFrameSize;
    f32 fsize;
    AudioInfo *audioInfo;

    alconf->dmaproc = &amDmaNew;
    alconf->outputRate = osAiSetFrequency(OUTPUT_RATE);

    fsize = (f32) ((alconf->outputRate << FRAMES_PER_FIELD_AS_POW2) / (f32)VI_REFRESH_RATE);

    g_FrameSize = (u32) fsize;

    if (g_FrameSize < fsize)
    {
        g_FrameSize++;
    }

    // This rounds up to the next multiple of 16.
    if (g_FrameSize & 0xf)
    {
        g_FrameSize = (g_FrameSize & ~0xf) + 0x10;
    }

    g_MinFrameSize = (u32)(g_FrameSize - 0x10);
    maxFrameSize = (u32)(g_FrameSize + EXTRA_SAMPLES + 0x10);

    if (alconf->fxType == AL_FX_CUSTOM)
    {
        alconf->params = g_CustomFxParams;
    }

    alInit(&g_AudioManager.g, alconf);

    for (j = 0; j < NUMBER_OUTPUT_BUFFERS; j++)
    {
        audioInfo = (AudioInfo *)alHeapAlloc(alconf->heap, 1, sizeof(AudioInfo));
        audioInfo->data = (s16 *)alHeapAlloc(alconf->heap, 1, maxFrameSize * 4);
        audioInfo->physicalData = (s16 *)osVirtualToPhysical(audioInfo->data);

        /* These task fields never change after initialization. */
        audioInfo->task.flags = OS_SC_NEEDS_RSP;
        audioInfo->task.msgQ = &g_AudioManager.replyMessageQueue;
        audioInfo->task.msg = audioInfo;
        audioInfo->task.list.t.type = M_AUDTASK;
        audioInfo->task.list.t.flags = 0;
        audioInfo->task.list.t.ucode_boot = (u64 *)rspbootTextStart;
        audioInfo->task.list.t.ucode_boot_size = (s32)gsp3DTextStart - (s32)rspbootTextStart;
        audioInfo->task.list.t.ucode = (u64 *)aspMainTextStart;
        audioInfo->task.list.t.ucode_data = (u64 *)aspMainDataStart;
        audioInfo->task.list.t.ucode_data_size = SP_UCODE_DATA_SIZE;
        audioInfo->task.list.t.yield_data_ptr = NULL;
        audioInfo->task.list.t.yield_data_size = 0;

        g_AudioManager.audioInfo[j] = audioInfo;
    }

    osCreateMesgQueue(&g_AudioManager.replyMessageQueue, g_AudioManager.replyMessageBuffer, AUDIO_REPLY_MESSAGE_QUEUE_SIZE);
    osCreateMesgQueue(&g_AudioManager.frameMessageQueue, g_AudioManager.frameMessageBuffer, AUDIO_FRAME_MESSAGE_QUEUE_SIZE);
    osCreateMesgQueue(&g_DmaMessageQueue, g_DmaMessageBuffer, AUDIO_DMA_IO_QUEUE_SIZE);

    /* The free DMA buffers only need a singly linked list. */
    for (j = 0; j < NUMBER_DMA_BUFFERS; j++)
    {
        g_DmaBuffers[j].node.next = (ALLink *)&g_DmaBuffers[j + 1];
        g_DmaBuffers[j].node.prev = NULL;
        g_DmaBuffers[j].physicalAddress = osVirtualToPhysical(alHeapAlloc(alconf->heap, 1, AUDIO_DMA_MAX_BUFFER_LENGTH));
    }
    g_DmaBuffers[NUMBER_DMA_BUFFERS - 1].node.next = NULL;

    for (j = 0; j < NUMBER_ACMD_LISTS; j++)
    {
        g_AudioManager.cmdList[j] = (Acmd *)alHeapAlloc(alconf->heap, 1, MAX_ACMD_SIZE * sizeof(Acmd));
    }

    osCreateThread(&g_AudioManager.audioThread, AUDI_THREAD_ID, &amMain, 0, (void*)setSPToEnd((u8*)(&sp_audi), sizeof(sp_audi)), AUDI_THREAD_PRIORITY);
}


void amStartAudioThread(void)
{
    osStartThread(&g_AudioManager.audioThread);
}

/**
 * @param arg unused.
 */
void amMain(void* arg)
{
    AudioInfo **nextAudioInfo = g_AudioManager.audioInfo;
    Acmd **nextCommandList = g_AudioManager.cmdList;
    OSScMsg *message = NULL;
    AudioInfo *completedInfo = NULL;

    osScAddClient(&os_scheduler, &g_AudioClient[0], &g_AudioManager.frameMessageQueue, (OSScClient *)1);

    for (;;)
    {
        osRecvMesg(&g_AudioManager.frameMessageQueue, (OSMesg *)&message, OS_MESG_BLOCK);

        switch (message->type)
        {
            case OS_SC_RETRACE_MSG:
                amHandleFrameMessage(*nextAudioInfo, completedInfo, *nextCommandList);
                nextAudioInfo++;
                if (nextAudioInfo == &g_AudioManager.audioInfo[NUMBER_OUTPUT_BUFFERS])
                {
                    nextAudioInfo = g_AudioManager.audioInfo;
                }
                nextCommandList++;
                if (nextCommandList == &g_AudioManager.cmdList[NUMBER_ACMD_LISTS])
                {
                    nextCommandList = g_AudioManager.cmdList;
                }

                osRecvMesg(&g_AudioManager.replyMessageQueue, (OSMesg *)&completedInfo, OS_MESG_BLOCK);
                break;
            case OS_SC_PRE_NMI_MSG:
            case MAIN_QUIT_MESSAGE:
                alClose(&g_AudioManager.g);
                return;
        }
    }
}

/**
 * Original documentation:
 * First, clear the past audio dma's, then calculate
 * the number of samples you will need for this frame. This value varies
 * due to the fact that audio is synchronised off of the video interupt
 * which can have a small amount of jitter in it. Varying the number of
 * samples slightly will allow you to stay in synch with the video. This
 * is an advantageous thing to do, since if you are in synch with the
 * video, you will have fewer graphics yields. After you've calculated
 * the number of frames needed, call alAudioFrame, which will call all
 * of the synthesizer's players (sequence player and sound player) to
 * generate the audio task list. If you get a valid task list back, put
 * it in a task structure and send a message to the scheduler to let it
 * know that the next frame of audio is ready for processing.
 *
 * @param info audio info.
 * @param lastInfo last info.
 * @param commandList command buffer to populate for this frame.
 */
void amHandleFrameMessage(AudioInfo *info, AudioInfo *lastInfo, Acmd *commandList)
{
    Acmd *commandListEnd;
    s32 commandLength;

    /* call once a frame, before doing alAudioFrame */
    amClearDmaBuffers();

    if (lastInfo)
    {
        osAiSetNextBuffer(lastInfo->data, lastInfo->frameSamples * 4);
    }

    /* calculate how many samples needed for this frame to keep the DAC full */
    /* this will vary slightly frame to frame, must recalculate every frame */
    /* divide by four, to convert bytes */
    /* to stereo 16 bit samples */
    info->frameSamples = (u16)(((g_FrameSize - (osAiGetLength() >> 2)) + 16 + EXTRA_SAMPLES) & ~0xf);

    if (info->frameSamples < (s16)g_MinFrameSize)
    {
        info->frameSamples = (s16)g_MinFrameSize;
    }

    commandListEnd = (Acmd *)alAudioFrame(commandList, &commandLength, info->physicalData, info->frameSamples);

    info->task.list.t.data_ptr = (u64 *)commandList;
    info->task.list.t.data_size = (s32)commandListEnd - (s32)commandList;

    osSendMesg(&os_scheduler.cmdQ, (OSMesg)&info->task, OS_MESG_NOBLOCK);
}

/**
 * Original documentation:
 * This routine handles the dma'ing of samples from rom to ram.
 * First it checks the current buffers to see if the samples needed are
 * already in place. Because buffers are linked sequentially by the
 * addresses where the samples are on rom, it doesn't need to check all
 * of them, only up to the address that it needs. If it finds one, it
 * returns the address of that buffer. If it doesn't find the samples
 * that it needs, it will initiate a DMA of the samples that it needs.
 * In either case, it updates the lastFrame variable, to indicate that
 * this buffer was last used in this frame. This is important for the
 * __clearAudioDMA routine.
 *
 * @param addr ?.
 * @param len ?.
 * @param state unused.
 * @return Physical address of the requested sample data.
 */
s32 amDmaCallback(s32 addr, s32 len, void* state)
{
    void *freeBuffer;
    s32 delta;
    DMABuffer *dmaPtr;
    DMABuffer *nextDmaPtr;
    s32 addrEnd;
    s32 buffEnd;
    DMABuffer *lastDmaPtr;

    lastDmaPtr = NULL;
    dmaPtr = g_DmaState.firstUsed;
    delta = addr & 0x1;
    addrEnd = addr + len;

    /* first check to see if a currently existing buffer contains the
       sample that you need.  */
    while (dmaPtr)
    {
        buffEnd = dmaPtr->startAddr + AUDIO_DMA_MAX_BUFFER_LENGTH;

        /* since buffers are ordered */
        /* abort if past possible */
        if ((u32)dmaPtr->startAddr > (u32)addr)
        {
            break;
        }
        /* yes, found a buffer with samples */
        else if (addrEnd <= buffEnd)
        {
            /* mark it used */
            dmaPtr->lastFrame = g_AudioFrameCount;
            return dmaPtr->physicalAddress + addr - dmaPtr->startAddr;
        }

        lastDmaPtr = dmaPtr;
        dmaPtr = (DMABuffer*)dmaPtr->node.next;
    }

    /* get here, and you didn't find a buffer, so dma a new one */
    /* get a buffer from the free list */
    dmaPtr = g_DmaState.firstFree;

    /*
     * if you get here and dmaPtr is null, send back a bogus
     * pointer, it's better than nothing
     */
    if (!dmaPtr)
    {
        if (!lastDmaPtr)
        {
            lastDmaPtr = g_DmaState.firstUsed;
        }

        return lastDmaPtr->physicalAddress + delta;
    }

    g_DmaState.firstFree = (DMABuffer*)dmaPtr->node.next;

    /* Insert the buffer into the ROM-address-sorted used list. */
    if (lastDmaPtr)
    {
        nextDmaPtr = (DMABuffer *)lastDmaPtr->node.next;
        dmaPtr->node.next = (ALLink *)nextDmaPtr;
        dmaPtr->node.prev = (ALLink *)lastDmaPtr;
        lastDmaPtr->node.next = (ALLink *)dmaPtr;

        if (nextDmaPtr)
        {
            nextDmaPtr->node.prev = (ALLink *)dmaPtr;
        }
    }
    else
    {
        nextDmaPtr = g_DmaState.firstUsed;
        g_DmaState.firstUsed = dmaPtr;
        dmaPtr->node.next = (ALLink *)nextDmaPtr;
        dmaPtr->node.prev = NULL;

        if (nextDmaPtr)
        {
            nextDmaPtr->node.prev = (ALLink *)dmaPtr;
        }
    }

    addr -= delta;
    dmaPtr->startAddr = addr;
    dmaPtr->lastFrame = g_AudioFrameCount;
    freeBuffer = OS_PHYSICAL_TO_K0(dmaPtr->physicalAddress);

    osPiStartDma(&g_DmaIOMessageBuffer[g_NextDmaIndex++], OS_MESG_PRI_HIGH, OS_READ, (u32)addr, freeBuffer, AUDIO_DMA_MAX_BUFFER_LENGTH, &g_DmaMessageQueue);
    return dmaPtr->physicalAddress + delta;
}

/**
 * Original documentation:
 * Initialize the dma buffers and return the address of the
 * procedure that will be used to dma the samples from rom to ram. This
 * routine will be called once for each physical voice that is created.
 * In this case, because we know where all the buffers are, and since
 * they are not attached to a specific voice, we will only really do any
 * initialization the first time. After that we just return the address
 * to the dma routine.
 *
 * @param state will point to g_DmaState after call.
 * @return Address of dma callback function.
 */
ALDMAproc amDmaNew(DMAState** state)
{
    if (g_DmaState.initialized == 0)
    {
        g_DmaState.firstUsed = NULL;
        g_DmaState.firstFree = g_DmaBuffers;
        g_DmaState.initialized = TRUE;
    }

    *state = &g_DmaState;
    return &amDmaCallback;
}

/**
 * Original documentation:
 * Routine to move dma buffers back to the unused list.
 * First clear out your dma messageQ. Then check each buffer to see when
 * it was last used. If that was more than FRAME_LAG frames ago, move it
 * back to the unused list.
 */
void amClearDmaBuffers(void)
{
    u32 i;
    DMABuffer *previousPtr;
    DMABuffer *dmaPtr, *nextPtr;

    /* Consume the completion messages produced by the preceding audio frame. */
    for (i = 0; i < g_NextDmaIndex; i++)
    {
        osRecvMesg(&g_DmaMessageQueue, NULL, OS_MESG_NOBLOCK);
    }

    dmaPtr = g_DmaState.firstUsed;
    while (dmaPtr)
    {
        nextPtr = (DMABuffer*)dmaPtr->node.next;

        if (dmaPtr->lastFrame + DMA_BUFFER_FRAME_LAG < g_AudioFrameCount)
        {
            previousPtr = (DMABuffer *)dmaPtr->node.prev;

            if (previousPtr)
            {
                previousPtr->node.next = (ALLink *)nextPtr;
            }
            else
            {
                g_DmaState.firstUsed = nextPtr;
            }

            if (nextPtr)
            {
                nextPtr->node.prev = (ALLink *)previousPtr;
            }

            dmaPtr->node.next = (ALLink *)g_DmaState.firstFree;
            dmaPtr->node.prev = NULL;
            g_DmaState.firstFree = dmaPtr;
        }
        dmaPtr = nextPtr;
    }

    g_NextDmaIndex = 0;
    g_AudioFrameCount++;
}
