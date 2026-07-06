#ifndef XPLANE_CONNECT_H
#define XPLANE_CONNECT_H

#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XPC_IP "127.0.0.1"
#define XPC_DEFAULT_PORT 49009

typedef struct XPCSocket {
    SOCKET sock;
    unsigned short xp_port;
    char xp_ip[16];
} XPCSocket;

XPCSocket openUDP(unsigned short xp_port);
void closeUDP(XPCSocket sock);
int getDREFs(XPCSocket sock, const char *drefs[], float *values[],
    unsigned char count, int sizes[]);

#ifdef __cplusplus
}
#endif

#endif
