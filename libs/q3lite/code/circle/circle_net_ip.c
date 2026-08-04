/*
===========================================================================
Q3lite / Circle Bare-Metal Loopback Stub (net_ip.c)
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"

// Bare-metal replacement for ntohs (Network to Host Short)
#ifndef ntohs
#define ntohs(x) ((unsigned short)((((unsigned short)(x) & 0x00ff) << 8) | (((unsigned short)(x) & 0xff00) >> 8)))
#endif

// Static networking flag to satisfy engine checks
static qboolean networkingEnabled = qtrue;

/*
====================
NET_ErrorString
====================
*/
char *NET_ErrorString( void ) {
    return "No Error (Bare-Metal Stub)";
}

/*
====================
Sys_StringToAdr
====================
*/
qboolean Sys_StringToAdr( const char *s, netadr_t *a, netadrtype_t family ) {
    if ( !s || !a ) {
        return qfalse;
    }

    // Default to bad address
    memset( a, 0, sizeof( netadr_t ) );

    if ( !Q_stricmp( s, "localhost" ) || !Q_stricmp( s, "127.0.0.1" ) || !Q_stricmp( s, "loopback" ) ) {
        a->type = NA_LOOPBACK;
        return qtrue;
    }

    // Hardcoded fallback for IPv4 parsing (e.g. "127.0.0.1")
    int ip[4];
    if ( sscanf( s, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3] ) == 4 ) {
        a->type = NA_IP;
        a->ip[0] = (byte)ip[0];
        a->ip[1] = (byte)ip[1];
        a->ip[2] = (byte)ip[2];
        a->ip[3] = (byte)ip[3];
        return qtrue;
    }

    return qfalse;
}

/*
===================
NET_CompareBaseAdrMask
===================
*/
qboolean NET_CompareBaseAdrMask( netadr_t a, netadr_t b, int netmask ) {
    if ( a.type != b.type ) {
        return qfalse;
    }

    if ( a.type == NA_LOOPBACK ) {
        return qtrue;
    }

    if ( a.type == NA_IP ) {
        byte *addra = (byte *)&a.ip;
        byte *addrb = (byte *)&b.ip;

        if ( netmask < 0 || netmask > 32 ) netmask = 32;

        int curbyte = netmask >> 3;
        if ( curbyte && memcmp( addra, addrb, curbyte ) ) {
            return qfalse;
        }

        netmask &= 0x07;
        if ( netmask ) {
            byte cmpmask = ( 1 << netmask ) - 1;
            cmpmask <<= ( 8 - netmask );
            if ( ( addra[curbyte] & cmpmask ) == ( addrb[curbyte] & cmpmask ) ) {
                return qtrue;
            }
        } else {
            return qtrue;
        }
    }

    return qfalse;
}

/*
===================
NET_CompareBaseAdr
===================
*/
qboolean NET_CompareBaseAdr( netadr_t a, netadr_t b ) {
    return NET_CompareBaseAdrMask( a, b, -1 );
}

/*
===================
NET_CompareAdr
===================
*/
qboolean NET_CompareAdr( netadr_t a, netadr_t b ) {
    if ( !NET_CompareBaseAdr( a, b ) ) {
        return qfalse;
    }

    if ( a.type == NA_IP || a.type == NA_IP6 ) {
        return ( a.port == b.port );
    }

    return qtrue;
}

/*
===================
NET_AdrToString / NET_AdrToStringwPort
===================
*/
const char *NET_AdrToString( netadr_t a ) {
    static char s[NET_ADDRSTRMAXLEN];

    if ( a.type == NA_LOOPBACK ) {
        Com_sprintf( s, sizeof( s ), "loopback" );
    } else if ( a.type == NA_BOT ) {
        Com_sprintf( s, sizeof( s ), "bot" );
    } else if ( a.type == NA_IP ) {
        Com_sprintf( s, sizeof( s ), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3] );
    } else {
        Com_sprintf( s, sizeof( s ), "unknown" );
    }

    return s;
}

const char *NET_AdrToStringwPort( netadr_t a ) {
    static char s[NET_ADDRSTRMAXLEN];

    if ( a.type == NA_LOOPBACK ) {
        Com_sprintf( s, sizeof( s ), "loopback" );
    } else if ( a.type == NA_IP ) {
        Com_sprintf( s, sizeof( s ), "%s:%hu", NET_AdrToString( a ), ntohs( a.port ) );
    } else {
        Com_sprintf( s, sizeof( s ), "%s", NET_AdrToString( a ) );
    }

    return s;
}

/*
===================
NET_IsLocalAddress / Sys_IsLANAddress
===================
*/
qboolean NET_IsLocalAddress( netadr_t adr ) {
    return ( adr.type == NA_LOOPBACK );
}

qboolean Sys_IsLANAddress( netadr_t adr ) {
    if ( adr.type == NA_LOOPBACK ) {
        return qtrue;
    }

    if ( adr.type == NA_IP ) {
        if ( adr.ip[0] == 127 || adr.ip[0] == 10 ) return qtrue;
        if ( adr.ip[0] == 172 && ( adr.ip[1] & 0xf0 ) == 16 ) return qtrue;
        if ( adr.ip[0] == 192 && adr.ip[1] == 168 ) return qtrue;
    }

    return qfalse;
}

/*
===================
Sys_ShowIP
===================
*/
void Sys_ShowIP( void ) {
    Com_Printf( "IP: 127.0.0.1 (Loopback Bare-Metal Mode)\n" );
}

/*
===================
Sys_SendPacket
===================
*/
void Sys_SendPacket( int length, const void *data, netadr_t to ) {
    // Bare-metal stub: hardware sockets do not exist.
    // Loopback transmission in Quake 3 bypasses Sys_SendPacket entirely.
}

/*
===================
NET_GetPacket
===================
*/
qboolean NET_GetPacket( netadr_t *net_from, msg_t *net_message, void *fdr ) {
    // Loopback receiving is handled directly via NET_GetLoopPacket() in qcommon.
    return qfalse;
}

/*
===================
NET_Sleep
===================
*/
void NET_Sleep( int msec ) {
    // No-op for bare metal: we do not block on select() or OS sockets
}

/*
===================
NET_JoinMulticast6 / NET_LeaveMulticast6
===================
*/
void NET_JoinMulticast6( void ) {}
void NET_LeaveMulticast6( void ) {}

/*
===================
NET_Config / NET_Init / NET_Shutdown / NET_Restart_f
===================
*/
void NET_Config( qboolean enableNetworking ) {
    networkingEnabled = qtrue;
}

void NET_Init( void ) {
    Cmd_AddCommand( "net_restart", NET_Restart_f );
    Com_Printf( "NET_Init: Bare-metal loopback initialized.\n" );
}

void NET_Shutdown( void ) {}

void NET_Restart_f( void ) {
    Com_Printf( "NET_Restart: Loopback reset.\n" );
}