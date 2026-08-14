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

#include <stdio.h>
#include <circle/logger.h>

extern "C" {
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

/*
==================
CON_Shutdown
==================
*/
void CON_Shutdown( void )
{
}

/*
==================
CON_Init
==================
*/
void CON_Init( void )
{
}

/*
==================
CON_Input
==================
*/
char *CON_Input( void )
{
	return NULL;
}

/*
==================
CON_Print
==================
*/
/*
void CON_Print( const char *msg )
{
    // Direct stdout output respects newlines exactly as Quake prints them
    fputs(msg, stdout);
    fflush(stdout);
}
*/
/*
void CON_Print( const char *msg )
{
    if (CLogger::Get()) {
        CLogger::Get()->Write("quake3", LogNotice, "%s", msg);
    }
}
*/
void CON_Print(const char *msg)
{
    if (CLogger::Get()) {
        CString clean(msg);
        clean.Replace("\n", "");
        clean.Replace("\r", "");

        CLogger::Get()->Write("quake3", LogNotice, "%s", clean.c_str());
    }
}
}