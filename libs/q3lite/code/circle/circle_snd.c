/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
Copyright (C) 2026 Ian Cass

This file is part of Q3lite Source Code.

Q3lite Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Q3lite Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Q3lite Source Code.  If not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

/*
===========================================================================
Bare-metal Raspberry Pi Circle Audio Implementation (Stubbed Dummy Driver)
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../client/snd_local.h"
#include "../client/client.h"

static qboolean snd_inited = qfalse;

// Dummy stereo 16-bit buffer for the software mixer.
static byte dummy_dma_buffer[65536];
static int dummy_samplepos = 0;

/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init(void)
{
	if (snd_inited)
		return qtrue;

    Com_Printf("Sound initializing (Circle stub DMA)\n");
	Com_Memset(&dma, 0, sizeof(dma));

	dma.channels = 2;
	dma.samplebits = 16;
	dma.speed = 22050;
	dma.samples = 16384;
	dma.fullsamples = dma.samples / dma.channels;
	dma.submission_chunk = 1;
	dma.isfloat = 0;
	dma.buffer = dummy_dma_buffer;

	Com_Memset(dummy_dma_buffer, 0, sizeof(dummy_dma_buffer));
	dummy_samplepos = 0;
	snd_inited = qtrue;

	Com_Printf("Sound initialized (Circle stub DMA)\n");
	return qtrue;
}

/*
===============
SNDDMA_GetDMAPos
===============
*/
int SNDDMA_GetDMAPos(void)
{
	if (!snd_inited || dma.samples <= 0)
		return 0;

	// Advance a little each poll so the software mixer makes forward progress.
	dummy_samplepos += (dma.speed / 100);
	if (dummy_samplepos >= dma.samples)
		dummy_samplepos %= dma.samples;

	return dummy_samplepos;
}

/*
===============
SNDDMA_Shutdown
===============
*/
void SNDDMA_Shutdown(void)
{
    snd_inited = qfalse;
    dummy_samplepos = 0;
    Com_Memset(&dma, 0, sizeof(dma));
}

/*
===============
SNDDMA_Submit
===============
*/
void SNDDMA_Submit(void)
{
	// No-op for stub backend.
}

/*
===============
SNDDMA_BeginPainting
===============
*/
void SNDDMA_BeginPainting(void)
{
	// No-op for stub backend.
}

#ifdef USE_VOIP
void SNDDMA_StartCapture(void)
{
}

int SNDDMA_AvailableCaptureSamples(void)
{
    return 0;
}

void SNDDMA_Capture(int samples, byte *data)
{
	if (data && samples > 0)
	{
		Com_Memset(data, 0, samples * 2);
	}
}

void SNDDMA_StopCapture(void)
{
}

void SNDDMA_MasterGain(float val)
{
}
#endif

// --- Sound System Stubs ---
void S_CodecInit(void) {}
void S_CodecShutdown(void) {}
void S_DisplayFreeMemory(void) {}
void S_CodecCloseStream(void *stream) { (void)stream; }
void *S_CodecOpenStream(const char *filename) { (void)filename; return NULL; }
int  S_CodecReadStream(void *stream, int bytes, void *buffer) { (void)stream; (void)bytes; (void)buffer; return 0; }
void SND_setup(void) {}
void SND_shutdown(void) {}
sndBuffer *SND_malloc() {}
void SND_free(sndBuffer *ptr) { (void)ptr; }
qboolean S_LoadSound(sfx_t *sfx) {return qfalse;}
void S_PaintChannels(int endtime) { (void)endtime; }
