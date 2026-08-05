// circle_snd.cpp
extern "C" {
#include "../client/snd_local.h"
}

#include "kernel.h"
#include <cstdlib>
#include <cstring>

static qboolean s_soundInitialised = qfalse;

// Positions in MONO samples (0 .. dma.samples - 1).
// s_ringWrite tracks what has been handed to Circle's queue from dma.buffer.
// s_dmaPos tracks the effective playback head seen by Quake's mixer.
static int s_ringWrite = 0;
static int s_dmaPos = 0;
static unsigned s_framesSubmitted = 0;

static int SndWrapMonoSamplePos(unsigned nFrames)
{
    return (int) ((nFrames % (unsigned) dma.fullsamples) * (unsigned) dma.channels);
}

static void SndUpdateDMAPos(CSoundBaseDevice *pSound)
{
    if (!pSound || dma.fullsamples <= 0 || dma.channels <= 0)
        return;

    unsigned queuedFrames = pSound->GetQueueFramesAvail();
    unsigned playedFrames = s_framesSubmitted > queuedFrames
        ? (s_framesSubmitted - queuedFrames)
        : 0;

    s_dmaPos = SndWrapMonoSamplePos(playedFrames);
}

extern "C" qboolean SNDDMA_Init(void)
{
    CKernel *pKernel = CKernel::Get();
    if (!pKernel) {
        Com_Printf("SNDDMA_Init: CKernel not initialized yet\n");
        return qfalse;
    }

    CSoundBaseDevice *pSound = pKernel->GetSoundDevice();
    if (!pSound) {
        Com_Printf("SNDDMA_Init: sound device not initialized yet\n");
        return qfalse;
    }

    if (!pSound->IsActive()) {
        Com_Printf("SNDDMA_Init: sound device is not active\n");
        return qfalse;
    }

    dma.channels = 2;
    dma.samplebits = 16;
    dma.isfloat = 0;
    dma.speed = 44100;

    // Match the engine ring size to the device queue depth where possible.
    unsigned queueFrames = pSound->GetQueueSizeFrames();
    if (queueFrames > 0)
        dma.samples = (int) (queueFrames * (unsigned) dma.channels);
    else
        dma.samples = dma.speed / 5 * dma.channels; // ~200 ms fallback

    dma.samples -= dma.samples % dma.channels;
    if (dma.samples <= 0)
        dma.samples = 4096;

    dma.fullsamples = dma.samples / dma.channels;
    dma.submission_chunk = 1;

    int dmasize = dma.samples * (dma.samplebits / 8);
    dma.buffer = (unsigned char *) calloc(1, dmasize);
    if (!dma.buffer) {
        Com_Printf("SNDDMA_Init: failed to allocate %d bytes for dma.buffer\n", dmasize);
        return qfalse;
    }

    s_ringWrite = 0;
    s_dmaPos = 0;
    s_framesSubmitted = 0;
    s_soundInitialised = qtrue;
    Com_Printf("SNDDMA_Init: initialized (%d Hz, %d ch, %d samples)\n",
        dma.speed, dma.channels, dma.samples);
    return qtrue;
}

extern "C" int SNDDMA_GetDMAPos(void)
{
    if (!s_soundInitialised) {
        return 0;
    }

    CKernel *pKernel = CKernel::Get();
    if (!pKernel)
        return s_dmaPos;

    CSoundBaseDevice *pSound = pKernel->GetSoundDevice();
    if (pSound && pSound->IsActive())
        SndUpdateDMAPos(pSound);

    return s_dmaPos;
}

extern "C" void SNDDMA_Submit(void)
{
    if (!s_soundInitialised)
        return;

    CKernel *pKernel = CKernel::Get();
    if (!pKernel)
        return;

    CSoundBaseDevice *pSound = pKernel->GetSoundDevice();
    if (!pSound || !pSound->IsActive())
        return;

    s16 *samples = (s16 *) dma.buffer;

    SndUpdateDMAPos(pSound);

    // Keep device queue near full by pushing data from dma.buffer in order.
    unsigned queueSizeFrames = pSound->GetQueueSizeFrames();
    unsigned queuedFrames = pSound->GetQueueFramesAvail();
    if (queuedFrames >= queueSizeFrames)
        return;

    unsigned freeFrames = queueSizeFrames - queuedFrames;

    while (freeFrames > 0)
    {
        int startFrame = s_ringWrite / dma.channels;
        int framesToWrap = dma.fullsamples - startFrame;
        if (framesToWrap <= 0)
            break;

        unsigned writeFrames = freeFrames < (unsigned) framesToWrap
            ? freeFrames
            : (unsigned) framesToWrap;

        unsigned bytes = writeFrames * (unsigned) dma.channels * sizeof(*samples);

        int written = pSound->Write(
            &samples[startFrame * dma.channels],
            bytes);

        if (written <= 0)
            break;

        // Only advance whole frames.
        written -= written % (dma.channels * sizeof(short));

        if (written == 0)
            break;

        int framesWritten = written / (dma.channels * (int) sizeof(short));
        int monoSamples = framesWritten * dma.channels;

        s_ringWrite = (s_ringWrite + monoSamples) % dma.samples;
        s_framesSubmitted += (unsigned) framesWritten;

        // Clear consumed region to keep startup and starvation behavior sane.
        memset(&samples[startFrame * dma.channels], 0, (size_t) written);

        freeFrames -= (unsigned) framesWritten;

        // partial write: Circle FIFO is full
        if ((unsigned)written < bytes)
            break;
    }

    SndUpdateDMAPos(pSound);
}

extern "C" void SNDDMA_Shutdown(void)
{
    if (s_soundInitialised) {
        CKernel *pKernel = CKernel::Get();
        if (pKernel && pKernel->GetSoundDevice()) {
            pKernel->GetSoundDevice()->Cancel();
        }
        
        if (dma.buffer) {
            free(dma.buffer);
            dma.buffer = NULL;
        }
        s_ringWrite = 0;
        s_dmaPos = 0;
        s_framesSubmitted = 0;
        s_soundInitialised = qfalse;
    }
}

extern "C" void SNDDMA_BeginPainting(void)
{
    // No-op
}