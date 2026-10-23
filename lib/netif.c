#include "netif.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define EMU_DEFAULT_IP "127.0.0.1"
#define EMU_DEFAULT_PORT 11000

static int get_emu_port(void) {
    const char *env = getenv("RELIABLE_EMU_PORT");
    if (env && *env) {
        int port = atoi(env);
        if (port > 0) {
            return port;
        }
    }
    return EMU_DEFAULT_PORT;
}

static const char *get_emu_ip(void) {
    const char *env = getenv("RELIABLE_EMU_IP");
    if (env && *env) {
        return env;
    }
    return EMU_DEFAULT_IP;
}

int netif_socket(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    return sock;
}

int netif_bind(int sock, int local_port) {
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)local_port);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return -1;
    }

    return 0;
}

int netif_connect(int sock, const char *peer_ip, int peer_port) {
    (void)peer_ip;
    if (peer_port <= 0) {
        return -1;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "HELLO %d", peer_port);
    return (netif_sendto(sock, get_emu_ip(), get_emu_port(), msg, strlen(msg)) < 0) ? -1 : 0;
}

ssize_t netif_send(int sock, const void *buf, size_t len) {
    return netif_sendto(sock, get_emu_ip(), get_emu_port(), buf, len);
}

ssize_t netif_recv(int sock, void *buf, size_t maxlen, int timeout_ms) {
    return netif_recvfrom(sock, buf, maxlen, timeout_ms, NULL, NULL);
}

ssize_t netif_sendto(int sock, const char *ip, int port,
                     const void *buf, size_t len) {
    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &dst.sin_addr) != 1) {
        fprintf(stderr, "inet_pton failed for %s\n", ip);
        return -1;
    }

    return sendto(sock, buf, len, 0, (struct sockaddr *)&dst, sizeof(dst));
}

ssize_t netif_recvfrom(int sock, void *buf, size_t maxlen,
                       int timeout_ms, char *src_ip, int *src_port) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);

    struct timeval tv;
    struct timeval *tvp = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tvp = &tv;
    }

    int ret = select(sock + 1, &rfds, NULL, NULL, tvp);
    if (ret < 0) {
        if (errno == EINTR) {
            return 0;
        }
        perror("select");
        return -1;
    }
    if (ret == 0) {
        return 0;
    }

    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    ssize_t n = recvfrom(sock, buf, maxlen, 0, (struct sockaddr *)&src, &srclen);
    if (n < 0) {
        return -1;
    }

    if (src_ip) {
        inet_ntop(AF_INET, &src.sin_addr, src_ip, INET_ADDRSTRLEN);
    }
    if (src_port) {
        *src_port = ntohs(src.sin_port);
    }

    return n;
}
