//
// kernel.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2019  R. Stange <rsta2@o2online.de>
// Copyright (C) 2026 Ian Cass
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
#include "keycodes.h"

#define ROOTDRIVE "0:"


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
    // --- CURSOR ARROWS (FIXED ORDER) ---
    /* 0x4F */ K_RIGHTARROW,
    /* 0x50 */ K_LEFTARROW,
    /* 0x51 */ K_DOWNARROW,
    /* 0x52 */ K_UPARROW,

    // --- NUMERIC KEYPAD ---
    /* 0x53 */ 0,          // Num Lock
    /* 0x54 */ '/',        // KP /
    /* 0x55 */ '*',        // KP *
    /* 0x56 */ '-',        // KP -
    /* 0x57 */ '+',        // KP +
    /* 0x58 */ K_ENTER,    // KP Enter
    /* 0x59 */ K_END,      // KP 1 / End
    /* 0x5A */ K_DOWNARROW,// KP 2 / Down
    /* 0x5B */ K_PGDN,     // KP 3 / PgDn
    /* 0x5C */ K_LEFTARROW,// KP 4 / Left
    /* 0x5D */ '5',        // KP 5
    /* 0x5E */ K_RIGHTARROW,//KP 6 / Right
    /* 0x5F */ K_HOME,     // KP 7 / Home
    /* 0x60 */ K_UPARROW,  // KP 8 / Up
    /* 0x61 */ K_PGUP,     // KP 9 / PgUp
    /* 0x62 */ K_INS,      // KP 0 / Ins
    /* 0x63 */ K_DEL,       // KP . / Del
    // --- NON-US / ISO EXTRA KEYS ---
    /* 0x64 */ '\\',        // Non-US \ and | (ISO key next to Left Shift)
    /* 0x65 */ 0            // Application / Menu key
};

// USB HID Modifier Bitmask Offsets
#define LCTRL_BIT  (1 << 0)
#define LSHIFT_BIT (1 << 1)
#define LALT_BIT   (1 << 2)
#define RCTRL_BIT  (1 << 4)
#define RSHIFT_BIT (1 << 5)
#define RALT_BIT   (1 << 6)

// Quake 3 Mouse Keycodes
#define K_MOUSE1      178
#define K_MOUSE2      179
#define K_MOUSE3      180
#define K_MOUSE4      181
#define K_MOUSE5      182
#define K_MWHEELUP    183
#define K_MWHEELDOWN  184

#define SOUND_CHUNK_SIZE      (384 * 10)
#define SUPPLICANT_CONFIG_FILE ROOTDRIVE "/wpa_supplicant.conf"
#define HOSTNAME "q3lite"
#define FIRMWARE_PATH "/firmware/"

// Initialize the shared ring buffer globals
rawInputEvent_t g_inputQueue[INPUT_QUEUE_SIZE];
volatile int g_queueHead = 0;
volatile int g_queueTail = 0;

// Global mouse motion counters
volatile int g_mouseDeltaX = 0;
volatile int g_mouseDeltaY = 0;



LOGMODULE("kernel");

extern "C" int _main (void);

static int MapUsbScancodeToQ3(unsigned char usbScancode) {
    return s_UsbToQ3KeyMap[usbScancode];
}

static void PushKeyEvent(int key, int isDown)
{
    if (key == 0) return;

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
:       m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
        m_Timer (&m_Interrupt),
        m_Logger (m_Options.GetLogLevel (), &m_Timer),
        m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
        m_VCHIQ (CMemorySystem::Get (), &m_Interrupt),
        m_WLAN (FIRMWARE_PATH),
        m_Net (0, 0, 0, 0, HOSTNAME, NetDeviceTypeWLAN),
        m_WPASupplicant (SUPPLICANT_CONFIG_FILE),
        m_USBHCI (&m_Interrupt, &m_Timer, TRUE),
        m_pKeyboard (0),
        m_pMouse (0),
        m_pHDMISound (0)

{
	s_pThis = this;
	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
	delete m_pHDMISound;
    	m_pHDMISound = 0;
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
		bOK = m_VCHIQ.Initialize ();
		LOGNOTE("Initialized VCHIQ");
	}

	if (bOK)
	{
		//m_pHDMISound = new CHDMISoundBaseDevice (&m_Interrupt, 44100, SOUND_CHUNK_SIZE);
		m_pHDMISound = new CVCHIQSoundBaseDevice (&m_VCHIQ, 44100, SOUND_CHUNK_SIZE, VCHIQSoundDestinationAuto);
		//m_pHDMISound = new CPWMSoundBaseDevice (&m_Interrupt, 44100, SOUND_CHUNK_SIZE);

	    	LOGNOTE("Initialized HDMI Sound");
	}

	if (bOK)
	{
		bOK = m_EMMC.Initialize ();
		LOGNOTE("Initialized SD Card");
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
		bOK = m_USBHCI.Initialize ();
		LOGNOTE("Initialized USB Controller");
	}

	if (bOK)
	{
		if (!m_WLAN.Initialize())
		{
		    LOGWARN("WLAN not available - continuing without network");
		    m_bNetworkAvailable = FALSE;
		}
		else
		{
		    LOGNOTE("Initialized WLAN");
		    m_bNetworkAvailable = TRUE;
		}
	}

	if (bOK && m_bNetworkAvailable)
	{
		if (!m_Net.Initialize(FALSE))
		{
		    LOGWARN("Network initialization failed - continuing without network");
		    m_bNetworkAvailable = FALSE;
		}
		else
		{
		    LOGNOTE("Initialized network");
		}
	}


	if (bOK && m_bNetworkAvailable)
	{
		if (!m_WPASupplicant.Initialize())
		{
		    LOGWARN("WPA supplicant initialization failed - continuing without network");
		    m_bNetworkAvailable = FALSE;
		}
		else
		{
		    LOGNOTE("Initialized WPA supplicant");
		}
	}

	// Set CPU speed
	//CCPUThrottle::Get()->SetSpeed(CPUSpeedMaximum);

	return bOK;
}

TShutdownMode CKernel::Run (void)
{

	LOGNOTE("====== Welcome to q3Lite ======");
	LOGNOTE("Compile time: " __DATE__ " " __TIME__);
	LOGNOTE("Memory Size: %u", CMemorySystem::Get()->GetMemSize());
	LOGNOTE("CPU Speed %u", CCPUThrottle::Get()->GetClockRate());
	LOGNOTE("===============================");

	// Search for USB devices
	boolean bUpdated = m_USBHCI.UpdatePlugAndPlay ();

	// Register the USB keyboard callbacks
	if (bUpdated && m_pKeyboard == 0) {
		m_pKeyboard = (CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd1", FALSE);
		if (m_pKeyboard != 0) {
			LOGNOTE("USB Keyboard found");
			m_pKeyboard->RegisterRemovedHandler (KeyboardRemovedHandler);
			m_pKeyboard->RegisterKeyStatusHandlerRaw (KeyStatusHandlerRaw);
		} else {
			LOGNOTE("USB Keyboard not found");
		}
	}

	// Register the USB mouse callbacks
	if (bUpdated && m_pMouse == 0) {
		m_pMouse = (CMouseDevice *) m_DeviceNameService.GetDevice ("mouse1", FALSE);
		if (m_pMouse != 0) {
			LOGNOTE("USB Mouse found");
			m_pMouse->RegisterRemovedHandler (MouseRemovedHandler);
			m_pMouse->RegisterStatusHandler (MouseStatusHandler);
		} else {
			LOGNOTE("USB Mouse not found");
		}
	}

	// Run Quake
	LOGNOTE("Initialization complete");
	_main ();

	return ShutdownReboot;
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

void CKernel::MouseStatusHandler(unsigned nButtons, int nDisplacementX, int nDisplacementY, int nWheelMove)
{
	assert(s_pThis != 0);

	static unsigned s_nPrevButtons = 0; 

	// Accumulate Mouse Movement Deltas for IN_Frame()
	// (Note: Inverting Y displacement is typical for Quake 3 screen space)
	g_mouseDeltaX += nDisplacementX;
	g_mouseDeltaY += nDisplacementY;

	// Process Mouse Button Deltas
	unsigned buttonDiff = nButtons ^ s_nPrevButtons;
	if (buttonDiff != 0)
	{
		if (buttonDiff & MOUSE_BUTTON_LEFT)
			PushKeyEvent(K_MOUSE1, (nButtons & MOUSE_BUTTON_LEFT) ? 1 : 0);
		if (buttonDiff & MOUSE_BUTTON_RIGHT)
			PushKeyEvent(K_MOUSE2, (nButtons & MOUSE_BUTTON_RIGHT) ? 1 : 0);
		if (buttonDiff & MOUSE_BUTTON_MIDDLE)
			PushKeyEvent(K_MOUSE3, (nButtons & MOUSE_BUTTON_MIDDLE) ? 1 : 0);
		if (buttonDiff & MOUSE_BUTTON_SIDE1)
			PushKeyEvent(K_MOUSE4, (nButtons & MOUSE_BUTTON_SIDE1) ? 1 : 0);
		if (buttonDiff & MOUSE_BUTTON_SIDE2)
			PushKeyEvent(K_MOUSE5, (nButtons & MOUSE_BUTTON_SIDE2) ? 1 : 0);
	}

	// Process Mouse Wheel (Wheel moves produce a press + immediate release)
	if (nWheelMove > 0)
	{
		PushKeyEvent(K_MWHEELUP, 1);
		PushKeyEvent(K_MWHEELUP, 0);
	}
	else if (nWheelMove < 0)
	{
		PushKeyEvent(K_MWHEELDOWN, 1);
		PushKeyEvent(K_MWHEELDOWN, 0);
	}

	s_nPrevButtons = nButtons;
}

void CKernel::KeyboardRemovedHandler (CDevice *pDevice, void *pContext)
{
	assert (s_pThis != 0);
	LOGNOTE("Keyboard removed");
	s_pThis->m_pKeyboard = 0;
}

void CKernel::MouseRemovedHandler (CDevice *pDevice, void *pContext)
{
	assert (s_pThis != 0);
	LOGNOTE("Mouse removed");
	s_pThis->m_pMouse = 0;
}

