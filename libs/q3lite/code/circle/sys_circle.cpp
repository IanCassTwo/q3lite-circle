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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/time.h>
#include <libgen.h>
#include <fcntl.h>
#include <fenv.h>

// Circle C++ headers MUST stay outside extern "C"
#include <circle/bcmrandom.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>

extern "C" {

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

static CBcmRandomNumberGenerator *g_pHwRandom = nullptr;
static bool g_RngSeeded = false;
static uint32_t g_PrngState = 0;

qboolean stdinIsATTY;

// Used to determine where to store user-specific files
static char homePath[ MAX_OSPATH ] = { 0 };

// Used to store the Steam Quake 3 installation path
static char steamPath[ MAX_OSPATH ] = { 0 };

// Used to store the GOG Quake 3 installation path
static char gogPath[ MAX_OSPATH ] = { 0 };

static const char *timestamp(void)
{
    time_t t = time(NULL);
    char *retval = asctime(localtime(&t));
    if (retval) {
        char *ptr;
        for (ptr = retval; *ptr; ptr++) {
            if ((*ptr == '\r') || (*ptr == '\n')) {
                *ptr = '\0';
                break;
            }
        }
    }
    return retval ? retval : "[date unknown]";
}

/*
==================
Sys_DefaultHomePath
==================
*/
char *Sys_DefaultHomePath(void)
{
	return "/";
}

/*
================
Sys_SteamPath
================
*/
char *Sys_SteamPath( void )
{
	// Disabled since Steam doesn't let you install Quake 3 on Circle
	return steamPath;
}

/*
================
Sys_GogPath
================
*/
char *Sys_GogPath( void )
{
	// GOG also doesn't let you install Quake 3 on Circle
	return gogPath;
}

/*
================
Sys_Milliseconds
================
*/
int Sys_Milliseconds (void)
{
	return (int)(CTimer::GetClockTicks64() / 1000);
}

// Fast xorshift32 PRNG (takes ~1 CPU cycle per byte)
static uint32_t FastRandom32(void)
{
    // Seed on first run using hardware RNG
    if (!g_RngSeeded)
    {
	if (!g_pHwRandom) {
            g_pHwRandom = new CBcmRandomNumberGenerator();
        }
        g_PrngState = g_pHwRandom->GetNumber();
        // Ensure state is non-zero
        if (g_PrngState == 0) {
            g_PrngState = 0xA5A5A5A5;
        }
        g_RngSeeded = true;
    }

    // Xorshift algorithm
    uint32_t x = g_PrngState;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_PrngState = x;
    return x;
}

/*
==================
Sys_RandomBytes
==================
*/
qboolean Sys_RandomBytes( byte *string, int len )
{
if (!string || len <= 0)
    {
        return qfalse;
    }

    int i = 0;

    // Fill buffer using PRNG
    while (i + 4 <= len)
    {
        uint32_t nNumber = FastRandom32();
        
        string[i + 0] = (byte)(nNumber & 0xFF);
        string[i + 1] = (byte)((nNumber >> 8) & 0xFF);
        string[i + 2] = (byte)((nNumber >> 16) & 0xFF);
        string[i + 3] = (byte)((nNumber >> 24) & 0xFF);

        i += 4;
    }

    // Trailing bytes
    if (i < len)
    {
        uint32_t nNumber = FastRandom32();
        while (i < len)
        {
            string[i] = (byte)(nNumber & 0xFF);
            nNumber >>= 8;
            i++;
        }
    }

    return qtrue;
}

/*
==================
Sys_GetCurrentUser
==================
*/
char *Sys_GetCurrentUser( void )
{
	return "player";
}

#define MEM_THRESHOLD 96*1024*1024

/*
==================
Sys_LowPhysicalMemory
==================
*/
qboolean Sys_LowPhysicalMemory( void )
{
	return qfalse;
}

/*
==================
Sys_Basename
==================
*/
const char *Sys_Basename( char *path )
{
    if (!path || !*path) return ".";
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}

/*
==================
Sys_Dirname
==================
*/
const char *Sys_Dirname( char *path )
{
    static char dot[] = ".";
    if ( !path || !*path ) return dot;
    char *p = strrchr( path, '/' );
    if ( !p ) return dot;
    if ( p == path ) return (char *)"/";
    *p = '\0';
    return path;
}

/*
==============
Sys_FOpen
==============
*/
FILE *Sys_FOpen( const char *ospath, const char *mode ) {
	struct stat buf;

	// check if path exists and is a directory
	if ( !stat( ospath, &buf ) && S_ISDIR( buf.st_mode ) )
		return NULL;

	return fopen( ospath, mode );
}

/*
==================
Sys_Mkdir
==================
*/
qboolean Sys_Mkdir( const char *path )
{
	int result = mkdir( path, 0750 );

	if( result != 0 )
		return (errno == EEXIST) ? qtrue : qfalse;

	return qtrue;
}

/*
==================
Sys_Mkfifo
==================
*/
FILE *Sys_Mkfifo( const char *ospath )
{
	FILE	*fifo;
	int	result;
	int	fn;
	struct	stat buf;

	// if file already exists AND is a pipefile, remove it
	if( !stat( ospath, &buf ) && S_ISFIFO( buf.st_mode ) )
		FS_Remove( ospath );

	result = mkfifo( ospath, 0600 );
	if( result != 0 )
		return NULL;

	fifo = fopen( ospath, "w+" );
	if( fifo )
	{
		fn = fileno( fifo );
		fcntl( fn, F_SETFL, O_NONBLOCK );
	}

	return fifo;
}

/*
==================
Sys_Cwd
==================
*/
char *Sys_Cwd( void )
{
	static char cwd[MAX_OSPATH];

	char *result = getcwd( cwd, sizeof( cwd ) - 1 );
	if( result != cwd )
		return NULL;

	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

/*
==================
Sys_ListFilteredFiles
==================
*/
void Sys_ListFilteredFiles( const char *basedir, char *subdirs, char *filter, char **list, int *numfiles )
{
	char          search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char          filename[MAX_OSPATH];
	DIR           *fdir;
	struct dirent *d;
	struct stat   st;

	if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
		return;
	}

	if (strlen(subdirs)) {
		Com_sprintf( search, sizeof(search), "%s/%s", basedir, subdirs );
	}
	else {
		Com_sprintf( search, sizeof(search), "%s", basedir );
	}

	if ((fdir = opendir(search)) == NULL) {
		return;
	}

	while ((d = readdir(fdir)) != NULL) {
		Com_sprintf(filename, sizeof(filename), "%s/%s", search, d->d_name);
		if (stat(filename, &st) == -1)
			continue;

		if (st.st_mode & S_IFDIR) {
			if (Q_stricmp(d->d_name, ".") && Q_stricmp(d->d_name, "..")) {
				if (strlen(subdirs)) {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s/%s", subdirs, d->d_name);
				}
				else {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s", d->d_name);
				}
				Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
			}
		}
		if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
			break;
		}
		Com_sprintf( filename, sizeof(filename), "%s/%s", subdirs, d->d_name );
		if (!Com_FilterPath( filter, filename, qfalse ))
			continue;
		list[ *numfiles ] = CopyString( filename );
		(*numfiles)++;
	}

	closedir(fdir);
}

/*
==================
Sys_ListFiles
==================
*/
char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs )
{
	struct dirent *d;
	DIR           *fdir;
	qboolean      dironly = wantsubs;
	char          search[MAX_OSPATH];
	int           nfiles;
	char          **listCopy;
	char          *list[MAX_FOUND_FILES];
	int           i;
	struct stat   st;

	int           extLen;

	if (filter) {

		nfiles = 0;
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

		list[ nfiles ] = NULL;
		*numfiles = nfiles;

		if (!nfiles)
			return NULL;

		listCopy = (char **)Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0 ; i < nfiles ; i++ ) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;

		return listCopy;
	}

	if ( !extension)
		extension = "";

	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		dironly = qtrue;
	}

	extLen = strlen( extension );

	// search
	nfiles = 0;

	if ((fdir = opendir(directory)) == NULL) {
		*numfiles = 0;
		return NULL;
	}

	while ((d = readdir(fdir)) != NULL) {
		Com_sprintf(search, sizeof(search), "%s/%s", directory, d->d_name);
		if (stat(search, &st) == -1)
			continue;
		if ((dironly && !(st.st_mode & S_IFDIR)) ||
			(!dironly && (st.st_mode & S_IFDIR)))
			continue;

		if (*extension) {
			if ( strlen( d->d_name ) < extLen ||
				Q_stricmp(
					d->d_name + strlen( d->d_name ) - extLen,
					extension ) ) {
				continue; // didn't match
			}
		}

		if ( nfiles == MAX_FOUND_FILES - 1 )
			break;
		list[ nfiles ] = CopyString( d->d_name );
		nfiles++;
	}

	list[ nfiles ] = NULL;

	closedir(fdir);

	// return a copy of the list
	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = (char **)Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}

/*
==================
Sys_FreeFileList
==================
*/
void Sys_FreeFileList( char **list )
{
	int i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}

/*
==================
Sys_Sleep

Block execution for msec or until input is received.
==================
*/
void Sys_Sleep( int msec )
{
	CScheduler::Get ()->MsSleep (msec);
}

/*
==============
Sys_ErrorDialog

Display an error message
==============
*/
void Sys_ErrorDialog( const char *error )
{
	char buffer[ 1024 ];
	unsigned int size;
	int f = -1;
	const char *homepath = Cvar_VariableString( "fs_homepath" );
	const char *gamedir = Cvar_VariableString( "fs_game" );
	const char *fileName = "crashlog.txt";
	char *dirpath = FS_BuildOSPath( homepath, gamedir, "");
	char *ospath = FS_BuildOSPath( homepath, gamedir, fileName );

	Sys_Print( va( "%s\n", error ) );

	// Make sure the write path for the crashlog exists...

	if(!Sys_Mkdir(homepath))
	{
		Com_Printf("ERROR: couldn't create path '%s' for crash log.\n", homepath);
		return;
	}

	if(!Sys_Mkdir(dirpath))
	{
		Com_Printf("ERROR: couldn't create path '%s' for crash log.\n", dirpath);
		return;
	}

	// We might be crashing because we maxed out the Quake MAX_FILE_HANDLES,
	// which will come through here, so we don't want to recurse forever by
	// calling FS_FOpenFileWrite()...use the Unix system APIs instead.
	f = open( ospath, O_CREAT | O_TRUNC | O_WRONLY, 0640 );
	if( f == -1 )
	{
		Com_Printf( "ERROR: couldn't open %s\n", fileName );
		return;
	}

	// We're crashing, so we don't care much if write() or close() fails.
	while( ( size = CON_LogRead( buffer, sizeof( buffer ) ) ) > 0 ) {
		if( write( f, buffer, size ) != size ) {
			Com_Printf( "ERROR: couldn't fully write to %s\n", fileName );
			break;
		}
	}

	close( f );
}

/*
==============
Sys_CrashLog

Send error messages to crashlog.txt. This can be used
for errors that happen before cvars are initialized.
==============
*/
void Sys_CrashLog( const char *error )
{
	char buffer[ 1024 ];
	unsigned int size;
	int f = -1;
	char homepath[MAX_OSPATH];
    Com_sprintf( homepath, sizeof( homepath ), "/.q3a" );
	const char *gamedir = "baseq3";
	const char *fileName = "crashlog.txt";
	char *dirpath = FS_BuildOSPath( homepath, gamedir, "");
	char *ospath = FS_BuildOSPath( homepath, gamedir, fileName );

	// Print crash log header/timestamp.
	Sys_Print( va( "\n===================== %s =====================", timestamp() ) );

	// Print error message.
	Sys_Print( va( "\n%s\n", error ) );

	// Make sure the write path for the crashlog exists...
	if(!Sys_Mkdir(homepath))
	{
		Com_Printf("ERROR: couldn't create path '%s' for crash log.\n", homepath);
		return;
	}

	if(!Sys_Mkdir(dirpath))
	{
		Com_Printf("ERROR: couldn't create path '%s' for crash log.\n", dirpath);
		return;
	}

	// Use the Unix system APIs to write the crash log.
	f = open( ospath, O_CREAT | O_TRUNC | O_WRONLY, 0640 );
	if( f == -1 )
	{
		Com_Printf( "ERROR: couldn't open %s\n", fileName );
		return;
	}

	while( ( size = CON_LogRead( buffer, sizeof( buffer ) ) ) > 0 ) {
		if( write( f, buffer, size ) != size ) {
			Com_Printf( "ERROR: couldn't fully write to %s\n", fileName );
			break;
		}
	}

	close( f );
}

/*
==============
Sys_ClearExecBuffer
==============
*/
static void Sys_ClearExecBuffer( void )
{

}

/*
==============
Sys_AppendToExecBuffer
==============
*/
static void Sys_AppendToExecBuffer( const char *text )
{
}

/*
==============
Sys_Exec
==============
*/
static int Sys_Exec( void )
{
	Com_Printf( "Sys_Exec: Cannot execute external commands on bare-metal target.\n" );
	return -1;
}

/*
==============
Sys_ZenityCommand
==============
*/
static void Sys_ZenityCommand( dialogType_t type, const char *message, const char *title )
{
}

/*
==============
Sys_KdialogCommand
==============
*/
static void Sys_KdialogCommand( dialogType_t type, const char *message, const char *title )
{
}

/*
==============
Sys_XmessageCommand
==============
*/
static void Sys_XmessageCommand( dialogType_t type, const char *message, const char *title )
{
}

/*
==============
Sys_Dialog

Display a *nix dialog box
==============
*/
dialogResult_t Sys_Dialog( dialogType_t type, const char *message, const char *title )
{
	// Log the dialog prompt out to UART / Circle log
    Com_Printf( "[DIALOG - %s]: %s\n", title ? title : "Notice", message ? message : "" );

    // Auto-acknowledge so the engine proceeds safely
    if ( type == DT_YES_NO ) {
        return DR_YES;
    }
    
    return DR_OK;
}

/*
==============
Sys_GLimpSafeInit

Unix specific "safe" GL implementation initialisation
==============
*/
void Sys_GLimpSafeInit( void )
{
	// NOP
}

/*
==============
Sys_GLimpInit

Unix specific GL implementation initialisation
==============
*/
void Sys_GLimpInit( void )
{
	// NOP
}

void Sys_SetFloatEnv(void)
{
	// rounding toward nearest
	fesetround(FE_TONEAREST);
}

/*
==============
Sys_PlatformInit

Circle specific initialisation
==============
*/
void Sys_PlatformInit( void )
{
	Sys_SetFloatEnv();
	stdinIsATTY = qfalse;
}

/*
==============
Sys_PlatformExit

Unix specific deinitialisation
==============
*/
void Sys_PlatformExit( void )
{
}

/*
==============
Sys_SetEnv

set/unset environment variables (empty value removes it)
==============
*/

void Sys_SetEnv(const char *name, const char *value)
{
	// Circle does not support environment variables, so this is a no-op
}

/*
==============
Sys_PID
==============
*/
int Sys_PID( void )
{
	// Circle only has one process, so return 1
	return 1;
}

/*
==============
Sys_PIDIsRunning
==============
*/
qboolean Sys_PIDIsRunning( int pid )
{
	return qtrue; // Circle only has one process, so it's always running
}

/*
=================
Sys_DllExtension

Check if filename should be allowed to be loaded as a DLL.
=================
*/
qboolean Sys_DllExtension( const char *name ) {
	const char *p;
	char c = 0;

	if ( COM_CompareExtension( name, DLL_EXT ) ) {
		return qtrue;
	}

	// Check for format of filename.so.1.2.3
	p = strstr( name, DLL_EXT "." );

	if ( p ) {
		p += strlen( DLL_EXT );

		// Check if .so is only followed for periods and numbers.
		while ( *p ) {
			c = *p;

			if ( !isdigit( c ) && c != '.' ) {
				return qfalse;
			}

			p++;
		}

		// Don't allow filename to end in a period. file.so., file.so.0., etc
		if ( c != '.' ) {
			return qtrue;
		}
	}

	return qfalse;
}

}
