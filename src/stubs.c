#include <stddef.h>
#include <string.h>

// --- Bot System Stubs ---
//int bot_enable = 0;

//void SV_BotInitCvars(void) {}
//void SV_BotInitBotLib(void) {}
//void SV_BotFrame(int time) { (void)time; }
//void SV_BotFreeClient(int clientNum) { (void)clientNum; }
//int  SV_BotAllocateClient(void) { return -1; }
//void SV_BotGetConsoleMessage(int clientNum, char *buf, int size) { (void)clientNum; (void)buf; (void)size; }
//int  SV_BotGetSnapshotEntity(int clientNum, int sequence) { (void)clientNum; (void)sequence; return -1; }
//void SV_BotLibSetup(void) {}
//void SV_BotLibShutdown(void) {}
//
//void BotDrawDebugPolygons(void (*drawPoly)(int color, int numPoints, float *points), int debugLevel) {
//    (void)drawPoly; (void)debugLevel;
//}
//void BotImport_DebugPolygonCreate(void) {}
//void BotImport_DebugPolygonDelete(void) {}

int mkfifo(const char *pathname, unsigned int mode) {
    (void)pathname; (void)mode;
    return -1; // Not supported on bare-metal
}
