/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
Q3lite Source Code - Stubbed Cinematic Manager
===========================================================================
*/

#include "client.h"

// --- Public Cinematic API Stubs ---

e_status CIN_StopCinematic(int handle) {
    (void)handle;
    return FMV_EOF;
}

e_status CIN_RunCinematic(int handle) {
    (void)handle;
    return FMV_EOF;
}

int CIN_PlayCinematic(const char *arg, int x, int y, int w, int h, int systemBits) {
    (void)arg; (void)x; (void)y; (void)w; (void)h; (void)systemBits;
    return -1;
}

void CIN_SetExtents(int handle, int x, int y, int w, int h) {
    (void)handle; (void)x; (void)y; (void)w; (void)h;
}

void CIN_SetLooping(int handle, qboolean loop) {
    (void)handle; (void)loop;
}

void CIN_CloseAllVideos(void) {
}

void CIN_DrawCinematic(int handle) {
    (void)handle;
}

void CIN_UploadCinematic(int handle) {
    (void)handle;
}

void CL_PlayCinematic_f(void) {
}

void SCR_DrawCinematic(void) {
}

void SCR_RunCinematic(void) {
}

void SCR_StopCinematic(void) {
}
