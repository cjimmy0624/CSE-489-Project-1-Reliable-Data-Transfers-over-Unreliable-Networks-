#include "netif.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --listen PORT --peer_ip IP --peer_port PORT --out FILE\n",
            prog);
}

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

int main(int argc, char **argv) {
    int listen_port = -1;
    const char *peer_ip = NULL;
    int peer_port = -1;
    const char *out_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
            listen_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--peer_ip") == 0 && i + 1 < argc) {
            peer_ip = argv[++i];
        } else if (strcmp(argv[i], "--peer_port") == 0 && i + 1 < argc) {
            peer_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (listen_port <= 0 || !peer_ip || peer_port <= 0 || !out_path) {
        usage(argv[0]);
        return 1;
    }

    FILE *out = fopen(out_path, "wb");
    if (!out) {
        perror("fopen");
        return 1;
    }

    // Create a UDP socket through the netif wrapper (emulator-aware).
    int sock = netif_socket();
    if (sock < 0) {
        fclose(out);
        return 1;
    }

    // Bind local port for this receiver.
    if (netif_bind(sock, listen_port) != 0) {
        fclose(out);
        close(sock);
        return 1;
    }
    // Tell emulator which peer port to forward to.
    if (netif_connect(sock, peer_ip, peer_port) != 0) {
        fclose(out);
        close(sock);
        return 1;
    }

    uint8_t recvbuf[PKT_HDR_LEN + MAX_PAYLOAD];

    uint32_t expected = 0;
    int done = 0;
    int fin_seen = 0;
    uint64_t fin_deadline_ms = 0;

    // Basic receiver: accept in-order packets and send cumulative ACKs.
    // TODO(student): implement GBN/SR receiver logic here:
    //   - GBN: discard out-of-order, ACK last in-order
    //   - SR: buffer out-of-order, ACK each packet
    while (!done) {
        int timeout_ms = -1;
        if (fin_seen) {
            uint64_t now = now_ms();
            if (now >= fin_deadline_ms) {
                done = 1;
                break;
            }
            timeout_ms = 200;
        }

        // Receive a packet with optional timeout.
        ssize_t n = netif_recv(sock, recvbuf, sizeof(recvbuf), timeout_ms);
        if (n < 0) {
            perror("recv");
            break;
        }
        if (n == 0) {
            continue;
        }

        pkt_hdr_t hdr;
        const uint8_t *payload = NULL;
        uint16_t payload_len = 0;
        // Parse header and validate CRC.
        if (pkt_parse(recvbuf, (size_t)n, &hdr, &payload, &payload_len) != 0) {
            continue;
        }
        if (hdr.type == PKT_TYPE_DATA) {
            // We received an DATA packet, write it to the output file
			if (payload_len > 0) {
				fwrite(payload, 1, payload_len, out);
			}

            // After we receive an DATA packet, we send an ACK
			// Here we implement an example ACK send call 
            // TODO(student): change ACK policy according to GBN or SR

            uint8_t ackbuf[PKT_HDR_LEN + MAX_PAYLOAD];
            uint32_t ack_no = hdr.seq + 1;
            size_t pktlen = pkt_build_ack(ackbuf, sizeof(ackbuf), ack_no);
            if (pktlen > 0) {
                netif_send(sock, ackbuf, pktlen);
            }
        } else if (hdr.type == PKT_TYPE_FIN) {
			// We receive an FIN packet
            // FIN marks end of file; reply with FINACK.
            uint8_t finbuf[PKT_HDR_LEN + MAX_PAYLOAD];
            size_t pktlen = pkt_build_finack(finbuf, sizeof(finbuf), expected);
            if (pktlen > 0) {
                netif_send(sock, finbuf, pktlen);
            }
            fin_seen = 1;
            fin_deadline_ms = now_ms() + 1000;
        }
    }

    fclose(out);
    close(sock);
    return done ? 0 : 1;
}
