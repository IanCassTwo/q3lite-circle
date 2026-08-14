/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
Q3lite Source Code - Adapted for RSTA2 Circle Bare-Metal Networking API.
===========================================================================
*/

// Circle C++ Bare-Metal Headers FIRST
#include <circle/net/netsubsystem.h>
#include <circle/net/ipaddress.h>
#include <circle/net/socket.h>
#include <circle/net/in.h>
#include "kernel.h"

// Quake 3 C Headers SECOND (Must be extern "C")
extern "C" {
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
}

static qboolean networkingEnabled = qfalse;

static cvar_t *net_enabled;
static cvar_t *net_ip;
static cvar_t *net_port;
static cvar_t *net_dropsim;

// Circle UDP Socket pointer
static CSocket *g_pUDPSocket = NULL;
static CNetSubSystem *g_pNetSubsystem = nullptr;

#ifndef htons
#define htons(n) (((((u16)(n)) & 0xFF00) >> 8) | ((((u16)(n)) & 0x00FF) << 8))
#endif

#ifndef ntohs
#define ntohs(n) htons(n)
#endif

// Keep C linkage for all functions called by or exported to Quake 3
extern "C" {

/*
====================
NET_ErrorString
====================
*/
char *NET_ErrorString( void ) {
    return (char *)"Circle Net Error";
}

/*
=============
Sys_StringToAdr
=============
*/
qboolean Sys_StringToAdr( const char *s, netadr_t *a, netadrtype_t family ) {
    int ip1, ip2, ip3, ip4;

    if (sscanf(s, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) != 4) {
        return qfalse;
    }

    u8 bytes[4] = { (u8)ip1, (u8)ip2, (u8)ip3, (u8)ip4 };

    CIPAddress ip;
    ip.Set(bytes);

    if (!ip.IsSet()) {
        return qfalse;
    }

    a->type = NA_IP;
    *(u32 *)&a->ip = (u32)ip; 

    return qtrue;
}

/*
===================
NET_CompareBaseAdrMask
===================
*/
qboolean NET_CompareBaseAdrMask(netadr_t a, netadr_t b, int netmask) {
    byte cmpmask, *addra, *addrb;
    int curbyte;

    if (a.type != b.type)
        return qfalse;

    if (a.type == NA_LOOPBACK)
        return qtrue;

    if (a.type == NA_IP) {
        addra = (byte *)&a.ip;
        addrb = (byte *)&b.ip;
        if (netmask < 0 || netmask > 32)
            netmask = 32;
    } else {
        return qfalse;
    }

    curbyte = netmask >> 3;
    if (curbyte && memcmp(addra, addrb, curbyte))
        return qfalse;

    netmask &= 0x07;
    if (netmask) {
        cmpmask = (1 << netmask) - 1;
        cmpmask <<= 8 - netmask;
        if ((addra[curbyte] & cmpmask) == (addrb[curbyte] & cmpmask))
            return qtrue;
    } else {
        return qtrue;
    }

    return qfalse;
}

/*
===================
NET_CompareBaseAdr
===================
*/
qboolean NET_CompareBaseAdr (netadr_t a, netadr_t b) {
    return NET_CompareBaseAdrMask(a, b, -1);
}

const char *NET_AdrToString (netadr_t a) {
    static char s[NET_ADDRSTRMAXLEN];

    if (a.type == NA_LOOPBACK)
        Com_sprintf (s, sizeof(s), "loopback");
    else if (a.type == NA_BOT)
        Com_sprintf (s, sizeof(s), "bot");
    else if (a.type == NA_IP) {
        Com_sprintf (s, sizeof(s), "%i.%i.%i.%i",
                     a.ip[0], a.ip[1], a.ip[2], a.ip[3]);
    }

    return s;
}

const char *NET_AdrToStringwPort (netadr_t a) {
    static char s[NET_ADDRSTRMAXLEN];

    if (a.type == NA_LOOPBACK)
        Com_sprintf (s, sizeof(s), "loopback");
    else if (a.type == NA_BOT)
        Com_sprintf (s, sizeof(s), "bot");
    else if (a.type == NA_IP) {
        Com_sprintf(s, sizeof(s), "%s:%hu", NET_AdrToString(a), ntohs(a.port));
    }

    return s;
}

qboolean NET_CompareAdr (netadr_t a, netadr_t b) {
    if(!NET_CompareBaseAdr(a, b))
        return qfalse;

    if (a.type == NA_IP) {
        if (a.port == b.port)
            return qtrue;
    } else {
        return qtrue;
    }

    return qfalse;
}

qboolean NET_IsLocalAddress( netadr_t adr ) {
    return adr.type == NA_LOOPBACK;
}

/*
====================
NET_CheckDeferredInit
====================
*/
static void NET_CheckDeferredInit( void ) {
    // If networking is enabled but socket isn't opened yet, retry
    if (!g_pUDPSocket) {
        if (g_pNetSubsystem && g_pNetSubsystem->IsRunning()) {
            const CIPAddress *pIP = g_pNetSubsystem->GetConfig()->GetIPAddress();
            if (pIP && pIP->IsSet()) {
                Com_Printf("Network connected! Initializing network socket...\n");
                NET_Restart_f();
            }
        }
    }
}

/*
==================
NET_GetPacket
==================
*/
qboolean NET_GetPacket( netadr_t *net_from, msg_t *net_message, void *unused ) {

    if (!g_pUDPSocket) {
        return qfalse;
    }

    CIPAddress fromIP;
    u16 fromPort = 0;
    
    int ret = g_pUDPSocket->ReceiveFrom(net_message->data, net_message->maxsize, MSG_DONTWAIT, &fromIP, &fromPort);
    if (ret <= 0) {
        return qfalse;
    }

    net_from->type = NA_IP;
    *(u32 *)&net_from->ip = (u32)fromIP;
    net_from->port = htons(fromPort);
    net_message->readcount = 0;

    if (ret >= net_message->maxsize) {
        Com_Printf("Oversize packet from %s\n", NET_AdrToString(*net_from));
        return qfalse;
    }

    net_message->cursize = ret;
    return qtrue;
}

/*
==================
Sys_SendPacket
==================
*/
void Sys_SendPacket( int length, const void *data, netadr_t to ) {
    if (!g_pUDPSocket) {
        Com_Printf("Sys_SendPacket: UDP socket not initialized, restarting\n");
        NET_Restart_f();
        return;
    }

    if (to.type != NA_BROADCAST && to.type != NA_IP) {
        Com_Error( ERR_FATAL, "Sys_SendPacket: bad address type" );
        return;
    }

    CIPAddress destIP;
    if (to.type == NA_BROADCAST) {
        destIP.SetBroadcast();
    } else {
        destIP.Set(*(u32 *)&to.ip);
    }

    u16 destPort = ntohs(to.port);

    g_pUDPSocket->SendTo(data, length, MSG_DONTWAIT, destIP, destPort);

}

/*
==================
Sys_IsLANAddress
==================
*/
qboolean Sys_IsLANAddress( netadr_t adr ) {
    if (adr.type == NA_LOOPBACK) {
        return qtrue;
    }

    if (adr.type == NA_IP) {
        if (adr.ip[0] == 10)
            return qtrue;
        if (adr.ip[0] == 172 && (adr.ip[1] & 0xf0) == 16)
            return qtrue;
        if (adr.ip[0] == 192 && adr.ip[1] == 168)
            return qtrue;
        if (adr.ip[0] == 127)
            return qtrue;
    }

    return qfalse;
}

/*
==================
Sys_ShowIP
==================
*/
void Sys_ShowIP(void) {
    if (g_pNetSubsystem && g_pNetSubsystem->IsRunning()) {
        const CIPAddress *pMyIP = g_pNetSubsystem->GetConfig()->GetIPAddress();
        if (pMyIP && pMyIP->IsSet()) {
            CString strIP;
            pMyIP->Format(&strIP);
            Com_Printf("IP: %s\n", (const char *)strIP);
        }
    }
}



/*
====================
NET_OpenIP
====================
*/
void NET_OpenIP( void ) {

    if (!g_pNetSubsystem || !g_pNetSubsystem->IsRunning()) {
        Com_Printf("WARNING: Network is not yet available.\n");
        return;
    }

    int port = net_port->integer;

    g_pUDPSocket = new CSocket(g_pNetSubsystem, IPPROTO_UDP);
    if (!g_pUDPSocket) {
        Com_Printf("WARNING: NET_OpenIP: Failed to allocate CSocket\n");
        return;
    }

    // TODO I don't think we need to bind to the port for UDP
    /*
    if (g_pUDPSocket->Bind(port) < 0) {
        Com_Printf("WARNING: NET_OpenIP: Couldn't bind to port %i\n", port);
        delete g_pUDPSocket;
        g_pUDPSocket = nullptr;
        return;
    }
    */

    Com_Printf("Network is enabled");
    //CScheduler::Get()->Yield();
    //Sys_ShowIP();
}

/*
====================
NET_GetCvars
====================
*/
static qboolean NET_GetCvars( void ) {
    int modified;

    net_enabled = Cvar_Get( (char*)"net_enabled", (char*)"1", CVAR_LATCH | CVAR_ARCHIVE );
    modified = net_enabled->modified;
    net_enabled->modified = qfalse;

    net_ip = Cvar_Get( (char*)"net_ip", (char*)"0.0.0.0", CVAR_LATCH );
    modified += net_ip->modified;
    net_ip->modified = qfalse;

    net_port = Cvar_Get( (char*)"net_port", (char*)va( (char*)"%i", PORT_SERVER ), CVAR_LATCH );
    modified += net_port->modified;
    net_port->modified = qfalse;

    net_dropsim = Cvar_Get((char*)"net_dropsim", (char*)"", CVAR_TEMP);

    return modified ? qtrue : qfalse;
}

/*
====================
NET_Config
====================
*/
void NET_Config( qboolean enableNetworking ) {

    if (!g_pNetSubsystem || !g_pNetSubsystem->IsRunning()) {
        Com_Printf("WARNING: Circle Network is not yet available.\n");
        return;
    }

    Com_Printf("NET_Config: %s networking\n", enableNetworking ? "Enabling" : "Disabling");
    qboolean modified = NET_GetCvars();

    if (!net_enabled->integer) {
        enableNetworking = qfalse;
    }

    if (enableNetworking == networkingEnabled && !modified) {
        Com_Printf("NET_Config: No changes to networking state, returning.\n");
        return;
    }

    if (g_pUDPSocket) {
        delete g_pUDPSocket;
        g_pUDPSocket = NULL;
    }

    networkingEnabled = enableNetworking;

    if (networkingEnabled) {
        NET_OpenIP();
    }
}

/*
====================
NET_Restart_f
====================
*/
void NET_Restart_f(void) {
    Com_Printf("Restarting network subsystem...\n");
    NET_Config(qfalse);
    NET_Config(qtrue);
}

/*
====================
NET_Init
====================
*/
void NET_Init( void ) {
    Com_Printf("Initializing Circle Network Subsystem...\n");
    g_pNetSubsystem = CKernel::Get()->GetNetwork();
    NET_GetCvars();
    NET_Config( qtrue );
    Cmd_AddCommand ("net_restart", NET_Restart_f);
}

/*
====================
NET_Shutdown
====================
*/
void NET_Shutdown( void ) {
    if (!networkingEnabled) {
        return;
    }

    NET_Config( qfalse );
}

/*
====================
NET_Event
====================
*/
qboolean NET_Event( void ) {
    byte bufData[ MAX_MSGLEN_BUF ];
    netadr_t from;
    msg_t netmsg;

    qboolean packetReceived = qfalse;
    //int count = 0;

    // Force Circle's network stack to pull pending RX hardware packets
    // into the socket queues immediately before we check for packets
    /*
    if ( g_pNetSubsystem ) {
        g_pNetSubsystem->Process();
    }
    */

    while ( 1 ) {
        MSG_Init( &netmsg, bufData, MAX_MSGLEN );

        if ( NET_GetPacket( &from, &netmsg, NULL ) ) {
            packetReceived = qtrue;
            //count++;
            if ( net_dropsim->value > 0.0f && net_dropsim->value <= 100.0f ) {
                if ( rand() < (int) (((double) RAND_MAX) / 100.0 * (double) net_dropsim->value) )
                    continue; 
            }

            if (com_sv_running->integer)
                Com_RunAndTimeServerPacket(&from, &netmsg);
            else
                CL_PacketEvent(from, &netmsg);
        } else {
            break;
        }
    }

    //if ( count > 1 ) {
    //    Com_Printf( "NET_Event: Drained %d packets at once!\n", count );
    //}

    return packetReceived;
}

/*
====================
NET_Sleep
====================
*/
void NET_Sleep(int msec) {

    int start = Sys_Milliseconds();

    if (msec < 0) {
        msec = 0;
    }

    NET_CheckDeferredInit();

    while ( 1 ) {
        if (NET_Event()) {
            return;
        }

        //CScheduler::Get()->Yield();

        // Check if our sleep duration has expired
        if ( msec == 0 || ( Sys_Milliseconds() - start ) >= msec ) {
            return;
        }

    }
}

/*
====================
IPv6 Multicast Stubs
====================
*/
void NET_JoinMulticast6(void) {}
void NET_LeaveMulticast6(void) {}

} // extern "C"