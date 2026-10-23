#ifndef NETIF_H
#define NETIF_H

#include <stddef.h>
#include <sys/types.h>

int netif_socket(void);
int netif_bind(int sock, int local_port);
int netif_connect(int sock, const char *peer_ip, int peer_port);
ssize_t netif_send(int sock, const void *buf, size_t len);
ssize_t netif_recv(int sock, void *buf, size_t maxlen, int timeout_ms);
ssize_t netif_sendto(int sock, const char *ip, int port,
                     const void *buf, size_t len);
ssize_t netif_recvfrom(int sock, void *buf, size_t maxlen,
                       int timeout_ms, char *src_ip, int *src_port);

#endif
