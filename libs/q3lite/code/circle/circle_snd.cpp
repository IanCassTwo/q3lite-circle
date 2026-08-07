#include <circle/sched/scheduler.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/types.h>
#include <circle/alloc.h>
#include <circle/util.h>
#include "kernel.h"

extern "C" {
#include "../client/client.h"
#include "../client/snd_local.h"
}

typedef struct
{
    byte *ptr;
    size_t len;
} Span;

static boolean s_soundInitialised = FALSE;
static int s_dmaPos = 0;

#define QUEUE_SIZE_FRAMES 16384 // Ring Buffer Size

static Span ringReadableSpan(
    byte *buffer,
    size_t bufferSize,
    size_t readPos)
   
{
    Span s;
    s.ptr = buffer + readPos;
    s.len = bufferSize - readPos;
    return s;
}
/*
===============================================================================
SNDDMA_Init
===============================================================================
*/
extern "C" qboolean SNDDMA_Init(void)
{
    if (s_soundInitialised)
        return qtrue;

    CSoundBaseDevice *pSound = CKernel::Get()->GetSoundDevice();
    if (!pSound)
        return qfalse;

    // Initialize the hardware sound device with the desired format
    pSound->SetWriteFormat(SoundFormatSigned16, 2);
    if (!pSound->AllocateQueueFrames(4096)) { // ~0.74s buffer space
        Com_Printf("Failed to allocate sound queue");
        return qfalse;
    }

    // Start the sound driver
    if (!pSound->Start()) {
        Com_Printf("Could not start the sound device!");
        return qfalse;
    }

    // Initialize the DMA buffer structure
    memset(&dma, 0, sizeof(dma));

    // Set up the DMA buffer structure
    dma.channels = 2;
    dma.samplebits = 16;
    dma.speed = 44100; // khz
    dma.samples = QUEUE_SIZE_FRAMES * dma.channels;
    dma.fullsamples = QUEUE_SIZE_FRAMES;
    dma.submission_chunk = 1; // Standard submission chunk size

    // Get total buffer capacity IN BYTES
    size_t bufferSizeBytes = dma.samples * (dma.samplebits / 8);

    // Allocate memory for the 16bit samples
    dma.buffer = (byte *) malloc(bufferSizeBytes);
    if (!dma.buffer)
        return qfalse;

    memset(dma.buffer, 0, bufferSizeBytes);

    s_dmaPos = 0;

    s_soundInitialised = TRUE;
    Com_Printf("Circle Sound DMA initialized: %d Hz, %d channels, %d bits\n",
               dma.speed, dma.channels, dma.samplebits);

    return qtrue;
}

/*
===============================================================================
SNDDMA_GetDMAPos
===============================================================================
*/
extern "C" int SNDDMA_GetDMAPos(void)
{
    if (!s_soundInitialised)
        return 0;

    CSoundBaseDevice *pSound = CKernel::Get()->GetSoundDevice();
    if (!pSound || !pSound->IsActive())
        return 0;

    // Get total buffer capacity IN BYTES
    size_t bytesPerSample = dma.samplebits / 8;
    size_t bufferSizeBytes = dma.samples * bytesPerSample;
    if (bufferSizeBytes == 0)
        return 0;

    // Per Circle docs: GetQueueFramesAvail() is the number of frames 
    // currently waiting in the queue to be sent to hardware!
    unsigned queuedFrames = pSound->GetQueueFramesAvail();
    unsigned queuedBytes = queuedFrames * dma.channels * bytesPerSample;

    // Subtract queuedBytes with wrap protection:
    // Adding bufferSizeBytes before subtracting guarantees s_dmaPos - queuedBytes 
    // won't underflow below 0 when s_dmaPos has wrapped around to the start.
    size_t playedBytes = (s_dmaPos + bufferSizeBytes - (queuedBytes % bufferSizeBytes)) % bufferSizeBytes;

    // Convert byte position back to Quake mono samples
    int samplePos = (int)(playedBytes / bytesPerSample);
    return samplePos % dma.samples;
}

/*
===============================================================================
SNDDMA_Submit
===============================================================================
*/
extern "C" void SNDDMA_Submit(void)
{
    if (!s_soundInitialised)
        return;

    CSoundBaseDevice *pSound = CKernel::Get()->GetSoundDevice();
    if (!pSound || !pSound->IsActive())
        return;

    // Get total buffer capacity IN BYTES
    size_t bufferSizeBytes = dma.samples * (dma.samplebits / 8);

    // Get a readable span of the ring buffer starting from s_dmaPos
    Span s = ringReadableSpan(dma.buffer, bufferSizeBytes, s_dmaPos);

    // Attempt to send
    int bytesWritten = pSound->Write(s.ptr, s.len);
    if (bytesWritten <= 0)
        return;

    // Position our DMA Pointer
    s_dmaPos = (s_dmaPos + bytesWritten) % bufferSizeBytes;
}

/*
===============================================================================
SNDDMA_Shutdown
===============================================================================
*/
extern "C" void SNDDMA_Shutdown(void)
{
    if (!s_soundInitialised)
        return;

    CSoundBaseDevice *pSound = CKernel::Get()->GetSoundDevice();
    if (!pSound || !pSound->IsActive())
        return;

    pSound->Cancel();
    // Wait for DMA to finish current transfer (with timeout)
    unsigned timeout_ms = 0;
    while (pSound->IsActive() && timeout_ms < 500) {
        CTimer::Get()->MsDelay(5);
        timeout_ms += 5;
    }

    if (dma.buffer) {
        free(dma.buffer);
        dma.buffer = NULL;
    }

    s_soundInitialised = FALSE;
}

extern "C" void SNDDMA_BeginPainting(void)
{
    // No-op
}