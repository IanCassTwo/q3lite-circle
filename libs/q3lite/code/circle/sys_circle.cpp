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
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <string>
#include <ctype.h>

// Circle C++ headers MUST stay outside extern "C"
#include <circle/bcmrandom.h>
#include <circle/sched/scheduler.h>
#include <circle/timer.h>

// Declarations for GNU Linker Symbol Wrapping
extern "C" {
    FILE *__real_fopen( const char *path, const char *mode );
    FILE *__wrap_fopen( const char *path, const char *mode );
    int __real_fclose( FILE *fp );
    int __wrap_fclose( FILE *fp );
}

// Simple in-memory file structure
struct CachedFile {
    char *data;
    size_t size;
};

// Global cache table mapping path -> file buffer
static std::unordered_map<std::string, CachedFile> g_ramCache;

struct RamCookie {
    const char *data;
    size_t size;
    size_t offset;
};

static ssize_t ram_read( void *cookie, char *buf, size_t size ) {
    RamCookie *rc = (RamCookie *)cookie;
    if ( rc->offset >= rc->size ) {
        return 0; // EOF
    }
    size_t avail = rc->size - rc->offset;
    size_t to_read = ( size < avail ) ? size : avail;
    memcpy( buf, rc->data + rc->offset, to_read );
    rc->offset += to_read;
    return to_read;
}

static int ram_seek( void *cookie, off_t *offset, int whence ) {
    RamCookie *rc = (RamCookie *)cookie;
    off_t new_pos = rc->offset;

    switch ( whence ) {
        case SEEK_SET: new_pos = *offset; break;
        case SEEK_CUR: new_pos += *offset; break;
        case SEEK_END: new_pos = rc->size + *offset; break;
        default: return -1;
    }

    if ( new_pos < 0 || new_pos > (off_t)rc->size ) {
        return -1;
    }

    rc->offset = (size_t)new_pos;
    *offset = new_pos;
    return 0;
}

static int ram_close( void *cookie ) {
    delete (RamCookie *)cookie;
    return 0;
}

static cookie_io_functions_t ram_funcs = {
    .read  = ram_read,
    .write = NULL,
    .seek  = ram_seek,
    .close = ram_close
};

// Helper function to check if a file path ends in .pk3 (case-insensitive)
static bool IsPK3File( const char *path ) {
    if ( !path ) return false;
    size_t len = strlen( path );
    if ( len < 4 ) return false;

    const char *ext = path + len - 4;
    return ( tolower( (unsigned char)ext[0] ) == '.' &&
             tolower( (unsigned char)ext[1] ) == 'p' &&
             tolower( (unsigned char)ext[2] ) == 'k' &&
             tolower( (unsigned char)ext[3] ) == '3' );
}

extern "C" {

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

/*
==================
__wrap_fopen
==================
Intercepts standard calls to fopen() at link time.
Handles path normalization, missing-file checks, and .pk3 RAM-caching.
*/
FILE *__wrap_fopen( const char *path, const char *mode ) {
    if ( !path ) return NULL;

    // Sanitize leading double slashes (e.g. "//baseq3/pak0.pk3" -> "/baseq3/pak0.pk3")
    const char *clean_path = path;
    while ( clean_path[0] == '/' && clean_path[1] == '/' ) {
        clean_path++;
    }

    // Attempt to open standard file from disk using real C library symbol
    FILE *disk_f = __real_fopen( clean_path, mode );
    if ( !disk_f ) {
        return NULL;
    }

    // Intercept read operations for .pk3 archives
    if ( ( mode[0] == 'r' || mode[0] == 'b' ) && IsPK3File( clean_path ) ) {
        std::string key( clean_path );

        // Cache HIT: Return RAM-backed FILE* cookie
        auto it = g_ramCache.find( key );
        if ( it != g_ramCache.end() ) {
            __real_fclose( disk_f );
            RamCookie *rc = new RamCookie{ it->second.data, it->second.size, 0 };
            return fopencookie( rc, "rb", ram_funcs );
        }

        // Cache MISS: Load .pk3 file contents into RAM
        fseek( disk_f, 0, SEEK_END );
        long file_size = ftell( disk_f );
        
        // Always reset file pointer to the beginning before attempting allocation/reads
        fseek( disk_f, 0, SEEK_SET );

        if ( file_size > 0 ) {
            char *cached_data = (char *)malloc( file_size );
            if ( cached_data ) {

                // Expand the stream's internal buffer to 1MB to maximize SD card burst speed
                setvbuf( disk_f, NULL, _IOFBF, 1024 * 1024 );

                // Read in large blocks to prevent C-library / FAT overhead
                constexpr size_t BLOCK_SIZE = 1024 * 1024; // 1MB block size
                size_t total_bytes_read = 0;
                bool read_error = false;

                while ( total_bytes_read < (size_t)file_size ) {
                    size_t bytes_to_read = BLOCK_SIZE;
                    if ( total_bytes_read + bytes_to_read > (size_t)file_size ) {
                        bytes_to_read = (size_t)file_size - total_bytes_read;
                    }

                    size_t read_count = fread( cached_data + total_bytes_read, 1, bytes_to_read, disk_f );
                    total_bytes_read += read_count;

                    // Handle early EOF or disk read errors
                    if ( read_count == 0 && ferror( disk_f ) ) {
                        read_error = true;
                        break;
                    }
                }

                if ( !read_error && total_bytes_read == (size_t)file_size ) {
                    __real_fclose( disk_f );

                    g_ramCache[key] = CachedFile{ cached_data, (size_t)file_size };

                    RamCookie *rc = new RamCookie{ cached_data, (size_t)file_size, 0 };
                    return fopencookie( rc, "rb", ram_funcs );
                }

                // If read failed, clean up allocated buffer and fall back to disk stream
                free( cached_data );
                fseek( disk_f, 0, SEEK_SET );
            }
        }
    }

    // Default stream buffer for non-PK3 disk handles or un-cached PK3 fallbacks
    if ( mode[0] == 'r' || mode[0] == 'b' ) {
        setvbuf( disk_f, NULL, _IOFBF, 64 * 1024 );
    }
    return disk_f;
}

/*
==================
__wrap_fclose
==================
Ensures close calls map cleanly to real fclose.
*/
int __wrap_fclose( FILE *fp ) {
    if ( !fp ) return 0;
    return __real_fclose( fp );
}

static CBcmRandomNumberGenerator *g_pHwRandom = nullptr;
static bool g_RngSeeded = false;
static uint32_t g_PrngState = 0;

void *test_fopencookie = (void *)fopencookie;

qboolean stdinIsATTY;

// Used to determine where to store user-specific files
static char homePath[ MAX_OSPATH ] = { 0 };

// Used to store the Steam Quake 3 installation path
static char steamPath[ MAX_OSPATH ] = { 0 };

// Used to store the GOG Quake 3 installation path
static char gogPath[ MAX_OSPATH ] = { 0 };

static const char *timestamp(void)
{
    static char buf[32];
    unsigned long seconds = (unsigned long)(CTimer::GetClockTicks64() / 1000000);
    Com_sprintf(buf, sizeof(buf), "Uptime: %lu s", seconds);
    return buf;
}

/*
==================
Sys_DefaultHomePath
==================
*/
char *Sys_DefaultHomePath(void)
{
	return "";
}

/*
================
Sys_SteamPath
================
*/
char *Sys_SteamPath( void )
{
	return steamPath;
}

/*
================
Sys_GogPath
================
*/
char *Sys_GogPath( void )
{
	return gogPath;
}

/*
================
Sys_Milliseconds
================
*/
int Sys_Milliseconds (void)
{
	return (int)(CTimer::GetClockTicks() / 1000);
}

// Fast xorshift32 PRNG
static uint32_t FastRandom32(void)
{
    if (!g_RngSeeded)
    {
        if (!g_pHwRandom) {
            g_pHwRandom = new CBcmRandomNumberGenerator();
        }
        g_PrngState = g_pHwRandom->GetNumber();
        if (g_PrngState == 0) {
            g_PrngState = 0xA5A5A5A5;
        }
        g_RngSeeded = true;
    }

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

    while (i + 4 <= len)
    {
        uint32_t nNumber = FastRandom32();
        
        string[i + 0] = (byte)(nNumber & 0xFF);
        string[i + 1] = (byte)((nNumber >> 8) & 0xFF);
        string[i + 2] = (byte)((nNumber >> 16) & 0xFF);
        string[i + 3] = (byte)((nNumber >> 24) & 0xFF);

        i += 4;
    }

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

/*
==================
Sys_LowPhysicalMemory
==================
*/
qboolean Sys_LowPhysicalMemory( void )
{
	// No Raspberry Pi has low memory, so we can safely return false here.
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
Simple pass-through wrapper to fopen.
*/
FILE *Sys_FOpen( const char *ospath, const char *mode ) {
	// Seems Quake3 doesn't always call this method to open files, 
	// so we just pass through to fopen() directly and implement our
	// ram caching in __wrap_fopen() instead.
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
		if (!Q_stricmp(d->d_name, ".") || !Q_stricmp(d->d_name, ".."))
			continue;

		bool isDir = false;
	#ifdef DT_DIR
		if (d->d_type == DT_DIR) {
			isDir = true;
		} else if (d->d_type == DT_UNKNOWN) {
	#endif
			Com_sprintf(filename, sizeof(filename), "%s/%s", search, d->d_name);
			if (stat(filename, &st) == 0 && (st.st_mode & S_IFDIR)) {
				isDir = true;
			}
	#ifdef DT_DIR
		}
	#endif

		if (isDir) {
			if (strlen(subdirs)) {
				Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s/%s", subdirs, d->d_name);
			} else {
				Com_sprintf(newsubdirs, sizeof(newsubdirs), "%s", d->d_name);
			}
			Sys_ListFilteredFiles(basedir, newsubdirs, filter, list, numfiles);
		} else {
			if (*numfiles >= MAX_FOUND_FILES - 1)
				break;

			Com_sprintf(filename, sizeof(filename), "%s/%s", subdirs, d->d_name);
			if (Com_FilterPath(filter, filename, qfalse)) {
				list[*numfiles] = CopyString(filename);
				(*numfiles)++;
			}
		}
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
				continue;
			}
		}

		if ( nfiles == MAX_FOUND_FILES - 1 )
			break;
		list[ nfiles ] = CopyString( d->d_name );
		nfiles++;
	}

	list[ nfiles ] = NULL;

	closedir(fdir);

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
==================
*/
void Sys_Sleep( int msec )
{
	if ( msec < 0 )
        return;

    if ( msec == 0 )
    {
        CScheduler::Get()->Yield();
        return;
    }

	CScheduler::Get ()->MsSleep (msec);
}

/*
==============
Sys_ErrorDialog
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
Sys_CrashLog
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

	Sys_Print( va( "\n===================== %s =====================", timestamp() ) );
	Sys_Print( va( "\n%s\n", error ) );

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

static void Sys_ClearExecBuffer( void ) {}
static void Sys_AppendToExecBuffer( const char *text ) {}

static int Sys_Exec( void )
{
	Com_Printf( "Sys_Exec: Cannot execute external commands on bare-metal target.\n" );
	return -1;
}

static void Sys_ZenityCommand( dialogType_t type, const char *message, const char *title ) {}
static void Sys_KdialogCommand( dialogType_t type, const char *message, const char *title ) {}
static void Sys_XmessageCommand( dialogType_t type, const char *message, const char *title ) {}

dialogResult_t Sys_Dialog( dialogType_t type, const char *message, const char *title )
{
    Com_Printf( "[DIALOG - %s]: %s\n", title ? title : "Notice", message ? message : "" );

    if ( type == DT_YES_NO ) {
        return DR_YES;
    }
    
    return DR_OK;
}

void Sys_GLimpSafeInit( void ) {}
void Sys_GLimpInit( void ) {}

void Sys_SetFloatEnv(void)
{
	// Ensure floating point operations use round-to-nearest mode
	fesetround(FE_TONEAREST);

	// Enable flush-to-zero and denormals-are-zero for ARM architectures so 
	// that denormalized floating point numbers don't cause performance issues.
#if defined(__arm__) || defined(__aarch64__)
    uint32_t fpscr;
    __asm__ volatile("mrc p10, 7, %0, cr1, cr0, 0" : "=r"(fpscr));
    fpscr |= (1 << 24);
    __asm__ volatile("mcr p10, 7, %0, cr1, cr0, 0" :: "r"(fpscr));
#endif
}

void Sys_PlatformInit( void )
{
	Sys_SetFloatEnv();
	stdinIsATTY = qfalse;
}

void Sys_PlatformExit( void ) {}
void Sys_SetEnv(const char *name, const char *value) {}

int Sys_PID( void )
{
	return 1;
}

qboolean Sys_PIDIsRunning( int pid )
{
	return qtrue;
}

qboolean Sys_DllExtension( const char *name ) {
	const char *p;
	char c = 0;

	if ( COM_CompareExtension( name, DLL_EXT ) ) {
		return qtrue;
	}

	p = strstr( name, DLL_EXT "." );

	if ( p ) {
		p += strlen( DLL_EXT );

		while ( *p ) {
			c = *p;

			if ( !isdigit( c ) && c != '.' ) {
				return qfalse;
			}

			p++;
		}

		if ( c != '.' ) {
			return qtrue;
		}
	}

	return qfalse;
}

}