/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
Copyright (C) 2026 Ian Cass
Bare-metal Raspberry Pi Circle System & Main Loop Implementation
===========================================================================
*/

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/logger.h>
#include <circle/types.h>
#include <circle/alloc.h>
#include <circle/sched/scheduler.h>

extern "C" {
#include "sys_local.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

// Forward reference for C-linkage engine entry points
void Sys_In_Restart_f( void );
void Sys_Init( void );
void Sys_Quit( void );
}

//static char binaryPath[ MAX_OSPATH ] = "/sd";
//static char installPath[ MAX_OSPATH ] = "/sd";
static char binaryPath[ MAX_OSPATH ] = "/";
static char installPath[ MAX_OSPATH ] = "/";

/*
===========================================================================
CIRCLE HARDWARE APPLICATION CONTAINER
===========================================================================
*/
class CQuakeKernel
{
public:
    CQuakeKernel(void);
    ~CQuakeKernel(void);

    boolean Initialize(void);
    void Run(void);

private:
};

CQuakeKernel::CQuakeKernel(void)
{
}

CQuakeKernel::~CQuakeKernel(void)
{
}

boolean CQuakeKernel::Initialize(void)
{
    return TRUE;
}

void CQuakeKernel::Run(void)
{
    // Define initial engine variables passed into Com_Init
    // (e.g. Memory allocation sizes, base game path settings)
    char commandLine[ MAX_STRING_CHARS ] = "+set com_hunkMegs 64 +set r_mode -1 +set r_customwidth 1920 +set r_customheight 1080";

    // Set initial timing base
    Sys_Milliseconds();

    // Core Quake 3 initialization pipeline
    CON_Init();
    Com_Init( commandLine );
    NET_Init();

    // Bare-metal main tick loop replacing standard OS thread loop
    while ( 1 )
    {
        Com_Frame();

        // Let other tasks have CPU
        if (CScheduler::Get()) {
            CScheduler::Get()->Yield();
        }
    }
}

/*
===========================================================================
SYSTEM STUBS AND INTERFACES (C-Linkage for Engine Core)
===========================================================================
*/
extern "C" {

void Sys_SetBinaryPath(const char *path) { Q_strncpyz(binaryPath, path, sizeof(binaryPath)); }
char *Sys_BinaryPath(void) { return binaryPath; }
void Sys_SetDefaultInstallPath(const char *path) { Q_strncpyz(installPath, path, sizeof(installPath)); }
char *Sys_DefaultInstallPath(void) { return installPath; }
char *Sys_DefaultAppPath(void) { return Sys_BinaryPath(); }

void Sys_In_Restart_f( void )
{
    IN_Restart();
}

char *Sys_ConsoleInput(void)
{
    return CON_Input();
}

char *Sys_GetClipboardData(void)
{
    return NULL;
}

void Sys_RemovePIDFile( const char *gamedir ) {}
void Sys_InitPIDFile( const char *gamedir ) {}

void Sys_Quit( void )
{
    CON_Shutdown();
    NET_Shutdown();
    
    // Halt engine loop execution gracefully on bare metal
    while(1) {
        // Spin lock / Halt CPU
    }
}

cpuFeatures_t Sys_GetProcessorFeatures( void )
{
    // Return standard ARM features (NEON/VFP dynamically available depending on target Pi)
    return (cpuFeatures_t)0;
}

void Sys_Init(void)
{
    Cmd_AddCommand( "in_restart", Sys_In_Restart_f );
    Cvar_Set( "arch", "arm baremetal" );
    Cvar_Set( "username", "baremetal" );
}

void Sys_AnsiColorPrint( const char *msg )
{
    CON_Print( msg );
}

void Sys_Print( const char *msg )
{
    //CON_LogWrite( msg );
    CON_Print( msg );
}

void Sys_Error( const char *error, ... )
{
    va_list argptr;
    char    string[1024];

    va_start(argptr, error);
    Q_vsnprintf(string, sizeof(string), error, argptr);
    va_end(argptr);

    Com_Printf("\nFATAL ERROR: %s\n", string);
    Sys_Quit();
}

int Sys_FileTime( char *path )
{
    return -1;
}

// We're runnning interpreted VM, so these are to keep the compiler happy
void Sys_UnloadDll( void *dllHandle ) {}
void *Sys_LoadDll(const char *name, qboolean useSystemLib) { return NULL; }
void *Sys_LoadGameDll(const char *name, intptr_t (QDECL **entryPoint)(int, ...), intptr_t (*systemcalls)(intptr_t, ...)) { return NULL;}

/*
===========================================================================
MAIN BARE-METAL KERNEL ENTRY POINT
===========================================================================
*/
int _main( void )
{
    CQuakeKernel Kernel;

    Kernel.Run();

    return 0;
}

} // extern "C"
