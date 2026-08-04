//
// kernel.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2019  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "kernel.h"
#include <circle/memory.h>
#include <circle/logger.h>
#include "input_queue.h"

#define ROOTDRIVE "0:"

// Quake 3 keycodes fallback defines (in case keycodes.h is not included directly)
#ifndef K_TAB
#define K_TAB         9
#define K_ENTER       13
#define K_ESCAPE      27
#define K_SPACE       32
#define K_BACKSPACE   127
#define K_UPARROW     128
#define K_DOWNARROW   129
#define K_LEFTARROW   130
#define K_RIGHTARROW  131
#define K_ALT         132
#define K_CTRL        133
#define K_SHIFT       134
#define K_F1          135
#define K_F2          136
#define K_F3          137
#define K_F4          138
#define K_F5          139
#define K_F6          140
#define K_F7          141
#define K_F8          142
#define K_F9          143
#define K_F10         144
#define K_F11         145
#define K_F12         146
#define K_INS         147
#define K_DEL         148
#define K_PGDN        149
#define K_PGUP        150
#define K_HOME        151
#define K_END         152
#define K_PAUSE       153
#define K_CAPSLOCK    154
#endif

// USB HID Scancode (0x00 - 0x52) -> Quake 3 Keycode Lookup
static const int s_UsbToQ3KeyMap[256] = {
    /* 0x00 - 0x03 */ 0, 0, 0, 0,
    /* 0x04 - 0x07 */ 'a', 'b', 'c', 'd',
    /* 0x08 - 0x0B */ 'e', 'f', 'g', 'h',
    /* 0x0C - 0x0F */ 'i', 'j', 'k', 'l',
    /* 0x10 - 0x13 */ 'm', 'n', 'o', 'p',
    /* 0x14 - 0x17 */ 'q', 'r', 's', 't',
    /* 0x18 - 0x1B */ 'u', 'v', 'w', 'x',
    /* 0x1C - 0x1F */ 'y', 'z', '1', '2',
    /* 0x20 - 0x23 */ '3', '4', '5', '6',
    /* 0x24 - 0x27 */ '7', '8', '9', '0',
    /* 0x28 */ K_ENTER,
    /* 0x29 */ K_ESCAPE,
    /* 0x2A */ K_BACKSPACE,
    /* 0x2B */ K_TAB,
    /* 0x2C */ K_SPACE,
    /* 0x2D */ '-',
    /* 0x2E */ '=',
    /* 0x2F */ '[',
    /* 0x30 */ ']',
    /* 0x31 */ '\\',
    /* 0x32 */ '#',
    /* 0x33 */ ';',
    /* 0x34 */ '\'',
    /* 0x35 */ '`', // Console key / Tilde
    /* 0x36 */ ',',
    /* 0x37 */ '.',
    /* 0x38 */ '/',
    /* 0x39 */ K_CAPSLOCK,
    /* 0x3A - 0x43 (F1 - F10) */ K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10,
    /* 0x44 - 0x45 (F11 - F12) */ K_F11, K_F12,
    /* 0x46 - 0x4E */ 0, 0, K_PAUSE, K_INS, K_HOME, K_PGUP, K_DEL, K_END, K_PGDN,
    /* 0x4F - 0x52 */ K_RIGHTARROW, K_LEFTARROW, K_DOWNARROW, K_UPARROW
};

// USB HID Modifier Bitmask Offsets
#define LCTRL_BIT  (1 << 0)
#define LSHIFT_BIT (1 << 1)
#define LALT_BIT   (1 << 2)
#define RCTRL_BIT  (1 << 4)
#define RSHIFT_BIT (1 << 5)
#define RALT_BIT   (1 << 6)

// Initialize the shared ring buffer globals
rawInputEvent_t g_inputQueue[INPUT_QUEUE_SIZE];
volatile int g_queueHead = 0;
volatile int g_queueTail = 0;

LOGMODULE("kernel");

extern "C" int _main (void);

static int MapUsbScancodeToQ3(unsigned char usbScancode) {
    return s_UsbToQ3KeyMap[usbScancode];
}

static void PushKeyEvent(int key, int isDown)
{
    if (key == 0) return;

    LOGNOTE("[PushKeyEvent] Key: %d, Down: %d", key, isDown);
    int next = (g_queueHead + 1) % INPUT_QUEUE_SIZE;
    if (next != g_queueTail)
    {
        g_inputQueue[g_queueHead].key = key;
        g_inputQueue[g_queueHead].down = isDown;

        // Compiler barrier: prevents instruction reordering
        asm volatile("" ::: "memory");

        g_queueHead = next;
    }
}

CKernel *CKernel::s_pThis = 0;

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),	
	m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),	
	m_VCHIQ (CMemorySystem::Get (), &m_Interrupt),
	m_USBHCI (&m_Interrupt, &m_Timer, TRUE),		// TRUE: enable plug-and-play
	m_pKeyboard (0)
{
	s_pThis = this;
	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
	// Unreachable
	s_pThis = 0;
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Serial.Initialize (115200);
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		bOK = m_Logger.Initialize (pTarget);
		LOGNOTE("Initialized logger");
	}

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
		LOGNOTE("Initialized interrupts");
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
		LOGNOTE("Initialized timer");
	}

	if (bOK)
	{
		bOK = m_EMMC.Initialize ();
		LOGNOTE("Initialized EMMC");
	}

	if (bOK)
	{
		if (f_mount(&m_RootFileSystem, ROOTDRIVE, 1) != FR_OK)
		{
		    LOGERR("Cannot mount drive: %s", ROOTDRIVE);
		    bOK = FALSE;
		}
		LOGNOTE("Initialized filesystem");
	}

	if (bOK)
	{
		bOK = m_VCHIQ.Initialize ();
		LOGNOTE("Initialized VCHIQ");
	}

	if (bOK)
	{
		bOK = m_USBHCI.Initialize ();
	}

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	LOGNOTE("Compile time: " __DATE__ " " __TIME__);

	boolean bUpdated = m_USBHCI.UpdatePlugAndPlay ();
	if (bUpdated && m_pKeyboard == 0) {
		m_pKeyboard = (CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd1", FALSE);
		if (m_pKeyboard != 0) {
			m_pKeyboard->RegisterRemovedHandler (KeyboardRemovedHandler);
			m_pKeyboard->RegisterKeyStatusHandlerRaw (KeyStatusHandlerRaw);
		}
	}

	// Run Quake
	_main ();

	// Unreachable
	while (1)
	{
		m_Scheduler.Yield ();
	}

	return ShutdownHalt;
}

void CKernel::KeyStatusHandlerRaw (unsigned char ucModifiers, const unsigned char RawKeys[6])
{
	assert (s_pThis != 0);

	static unsigned char s_PrevKeys[6] = {0};
	static unsigned char s_PrevModifiers = 0;

	// Short-circuit on USB heartbeat packets
	if (ucModifiers == s_PrevModifiers &&
	    memcmp(RawKeys, s_PrevKeys, sizeof(s_PrevKeys)) == 0)
	{
		return;
	}

	// Process Modifier Key Deltas (Ctrl, Shift, Alt)
	unsigned char modDiff = ucModifiers ^ s_PrevModifiers;
	if (modDiff != 0)
	{
		if (modDiff & (LCTRL_BIT | RCTRL_BIT))
			PushKeyEvent(K_CTRL, (ucModifiers & (LCTRL_BIT | RCTRL_BIT)) ? 1 : 0);
		if (modDiff & (LSHIFT_BIT | RSHIFT_BIT))
			PushKeyEvent(K_SHIFT, (ucModifiers & (LSHIFT_BIT | RSHIFT_BIT)) ? 1 : 0);
		if (modDiff & (LALT_BIT | RALT_BIT))
			PushKeyEvent(K_ALT, (ucModifiers & (LALT_BIT | RALT_BIT)) ? 1 : 0);
	}

	// Process Standard Key Presses
	for (unsigned i = 0; i < 6; i++) {
		if (RawKeys[i] != 0) {
			bool wasPressed = false;
			for (unsigned j = 0; j < 6; j++) {
				if (RawKeys[i] == s_PrevKeys[j]) {
					wasPressed = true;
					break;
				}
			}
			if (!wasPressed) {
				int q3Key = MapUsbScancodeToQ3(RawKeys[i]);
				PushKeyEvent(q3Key, 1);
			}
		}
	}

	// Process Standard Key Releases
	for (unsigned i = 0; i < 6; i++) {
		if (s_PrevKeys[i] != 0) {
			bool isStillPressed = false;
			for (unsigned j = 0; j < 6; j++) {
				if (s_PrevKeys[i] == RawKeys[j]) {
					isStillPressed = true;
					break;
				}
			}
			if (!isStillPressed) {
				int q3Key = MapUsbScancodeToQ3(s_PrevKeys[i]);
				PushKeyEvent(q3Key, 0);
			}
		}
	}

	// Save state for next IRQ comparison
	s_PrevModifiers = ucModifiers;
	memcpy(s_PrevKeys, RawKeys, sizeof(s_PrevKeys));
}

void CKernel::KeyboardRemovedHandler (CDevice *pDevice, void *pContext)
{
	assert (s_pThis != 0);
	LOGNOTE("Keyboard removed");
	s_pThis->m_pKeyboard = 0;
}
