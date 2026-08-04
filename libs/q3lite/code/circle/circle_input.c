/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

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

In addition, Q3lite Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.
If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
Bare-metal Raspberry Pi Circle Input Implementation (Stubbed)
===========================================================================
*/

#include "../client/client.h"
#include "../sys/sys_local.h"
#include "input_queue.h"

#ifndef CTRL
#define CTRL(a) ((a)-'a'+1)
#endif

// Shift-state helper to map ASCII keys when Shift is down
static int ApplyShiftToChar(int ch) {
    // Letters: lowercase to uppercase
    if (ch >= 'a' && ch <= 'z') {
        return ch - ('a' - 'A');
    }

    // Number row & symbol translation table
    switch (ch) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case '\\': return '|';
        case ';': return ':';
        case '\'': return '"';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        case '`': return '~';
        default:  return ch;
    }
}

/*
===============
IN_Init
===============
*/
void IN_Init( void *windowData )
{
    Com_Printf( "\n------- Input Initialization (Bare-Metal Stubbed) -------\n" );
    // Custom USB / Keyboard / Mouse hardware drivers from Circle 
    // can be hooked up here in the future.
    Com_Printf( "---------------------------------------------------------\n" );
}

/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void )
{
}

/*
===============
IN_Restart
===============
*/
void IN_Restart( void )
{
}

/*
===============
IN_Frame
===============
*/
void IN_Frame( void )
{
    int eventTime = Sys_Milliseconds();
    static qboolean shiftPressed = qfalse;

    // 1. Drain input events from ring buffer
    while (g_queueTail != g_queueHead) {
        rawInputEvent_t ev = g_inputQueue[g_queueTail];
        g_queueTail = (g_queueTail + 1) % INPUT_QUEUE_SIZE;

        // Track Shift modifier state internally
        if (ev.key == K_SHIFT) {
            shiftPressed = ev.down ? qtrue : qfalse;
        }

        // Check for console key
        if (ev.key == '`' || ev.key == '~' || ev.key == K_CONSOLE) {
            if (ev.down) {
                Com_QueueEvent( eventTime, SE_KEY, K_CONSOLE, qtrue, 0, NULL );
                Com_QueueEvent( eventTime, SE_KEY, K_CONSOLE, qfalse, 0, NULL );
            }
            continue;
        }

        // Standard Key Event (always uses base keycode, e.g., 'a' or '-')
        Com_QueueEvent( eventTime, SE_KEY, ev.key, ev.down, 0, NULL );

        // Backspace Special Handling
        if (ev.down && ev.key == K_BACKSPACE) {
            Com_QueueEvent( eventTime, SE_CHAR, 8, 0, 0, NULL ); // ASCII 8 (Ctrl+H)
        }
        // Printable Character Handling
        else if (ev.down && ev.key >= 32 && ev.key <= 126) {
            int charToQueue = ev.key;
            
            // Translate character if Shift is active
            if (shiftPressed) {
                charToQueue = ApplyShiftToChar(charToQueue);
            }

            Com_QueueEvent( eventTime, SE_CHAR, charToQueue, 0, 0, NULL );
        }
    }

    // 2. Poll mouse movement
    if (g_mouseDeltaX != 0 || g_mouseDeltaY != 0) {
        int dx = g_mouseDeltaX;
        int dy = g_mouseDeltaY;
        
        g_mouseDeltaX = 0;
        g_mouseDeltaY = 0;

        Com_QueueEvent( eventTime, SE_MOUSE, dx, dy, 0, NULL );
    }
}