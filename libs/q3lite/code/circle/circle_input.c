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
    // Called once per frame by the client engine loop to poll input hardware.
    // When you attach Circle keyboard/mouse input drivers, you will queue 
    // engine events here using Com_QueueEvent().
}