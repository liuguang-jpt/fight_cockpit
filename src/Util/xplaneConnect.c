#include "xplaneConnect.h"

#include <stdio.h>
#include <string.h>

static LONG g_wsa_refs = 0;

static int ensure_winsock(void) {
    WSADATA wsa_data;

    if (InterlockedIncrement(&g_wsa_refs) == 1) {
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            InterlockedDecrement(&g_wsa_refs);
            return -1;
        }
    }

    return 0;
}

static void release_winsock(void) {
    if (InterlockedDecrement(&g_wsa_refs) == 0) {
        WSACleanup();
    }
}

static int send_udp(XPCSocket sock, const char *buffer, int len) {
    struct sockaddr_in dst;
    int sent = 0;

    if (len <= 0) {
        return -1;
    }

    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(sock.xp_port);

    if (inet_pton(AF_INET, sock.xp_ip, &dst.sin_addr.s_addr) != 1) {
        return -2;
    }

    sent = sendto(sock.sock, buffer, len, 0, (const struct sockaddr *)&dst, sizeof(dst));
    if (sent != len) {
        return -3;
    }

    return sent;
}

static int read_udp(XPCSocket sock, char *buffer, int len) {
    fd_set read_fds;
    fd_set except_fds;
    struct timeval timeout;
    int status = 0;

    FD_ZERO(&read_fds);
    FD_ZERO(&except_fds);
    FD_SET(sock.sock, &read_fds);
    FD_SET(sock.sock, &except_fds);

    timeout.tv_sec = 0;
    timeout.tv_usec = 50000;

    status = select((int)(sock.sock + 1), &read_fds, NULL, &except_fds, &timeout);
    if (status <= 0) {
        return status;
    }

    return recv(sock.sock, buffer, len, 0);
}

static int send_dref_request(XPCSocket sock, const char *drefs[], unsigned char count) {
    char buffer[65536] = "GETD";
    int len = 6;
    unsigned char i = 0;

    buffer[5] = (char)count;

    for (i = 0; i < count; ++i) {
        size_t dref_len = strnlen(drefs[i], 255);
        if (dref_len == 0 || dref_len > 255) {
            return -1;
        }

        buffer[len++] = (char)dref_len;
        memcpy(buffer + len, drefs[i], dref_len);
        len += (int)dref_len;
    }

    return send_udp(sock, buffer, len) < 0 ? -2 : 0;
}

static int read_dref_response(XPCSocket sock, float *values[], unsigned char count, int sizes[]) {
    char buffer[65536];
    int result = read_udp(sock, buffer, (int)sizeof(buffer));
    int cursor = 6;
    unsigned char i = 0;

    if (result <= 0) {
        return -1;
    }
    if (result < 6 || buffer[5] != (char)count) {
        return -2;
    }

    for (i = 0; i < count; ++i) {
        unsigned char row_size = (unsigned char)buffer[cursor++];
        int copy_count = row_size;

        if (copy_count > sizes[i]) {
            copy_count = sizes[i];
        }

        memcpy(values[i], buffer + cursor, copy_count * (int)sizeof(float));
        sizes[i] = copy_count;
        cursor += row_size * (int)sizeof(float);

        if (cursor > result) {
            return -3;
        }
    }

    return 0;
}

XPCSocket openUDP(unsigned short xp_port) {
    XPCSocket sock;
    struct sockaddr_in recv_addr;
    DWORD timeout = 1;

    memset(&sock, 0, sizeof(sock));
    sock.sock = INVALID_SOCKET;
    sock.xp_port = xp_port == 0 ? XPC_DEFAULT_PORT : xp_port;
    strncpy(sock.xp_ip, XPC_IP, sizeof(sock.xp_ip) - 1);

    if (ensure_winsock() != 0) {
        return sock;
    }

    sock.sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock.sock == INVALID_SOCKET) {
        release_winsock();
        return sock;
    }

    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_addr.s_addr = INADDR_ANY;
    recv_addr.sin_port = htons(0);

    if (bind(sock.sock, (const struct sockaddr *)&recv_addr, sizeof(recv_addr)) == SOCKET_ERROR) {
        closesocket(sock.sock);
        sock.sock = INVALID_SOCKET;
        release_winsock();
        return sock;
    }

    setsockopt(sock.sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    return sock;
}

void closeUDP(XPCSocket sock) {
    if (sock.sock != INVALID_SOCKET) {
        closesocket(sock.sock);
        release_winsock();
    }
}

int getDREFs(XPCSocket sock, const char *drefs[], float *values[],
        unsigned char count, int sizes[]) {
    if (sock.sock == INVALID_SOCKET) {
        return -1;
    }
    if (send_dref_request(sock, drefs, count) != 0) {
        return -2;
    }
    return read_dref_response(sock, values, count, sizes);
}
