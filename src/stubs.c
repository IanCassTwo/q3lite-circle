#include <stddef.h>
#include <string.h>

int mkfifo(const char *pathname, unsigned int mode) {
    (void)pathname; (void)mode;
    return -1; // Not supported on bare-metal
}
