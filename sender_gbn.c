#include "netif.h"
#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

//ADDED CODE
#include <stdbool.h>

static void usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s --listen PORT --peer_ip IP --peer_port PORT --in FILE --win N --timeout MS\n",
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
    const char *in_path = NULL;
    int win = -1;
    int rto_ms = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
            listen_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--peer_ip") == 0 && i + 1 < argc) {
            peer_ip = argv[++i];
        } else if (strcmp(argv[i], "--peer_port") == 0 && i + 1 < argc) {
            peer_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            in_path = argv[++i];
        } else if (strcmp(argv[i], "--win") == 0 && i + 1 < argc) {
            win = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            rto_ms = atoi(argv[++i]);
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (listen_port <= 0 || !peer_ip || peer_port <= 0 || !in_path || win <= 0 || rto_ms <= 0) {
        usage(argv[0]);
        return 1;
    }

    //ADDED CODE
    if (rto_ms < 500) {
        rto_ms = 500;
    }

    FILE *in = fopen(in_path, "rb");
    if (!in) {
        perror("fopen");
        return 1;
    }

    struct stat st;
    if (fstat(fileno(in), &st) != 0) {
        perror("fstat");
        fclose(in);
        return 1;
    }
    uint64_t file_size = (uint64_t)st.st_size;

    // Create a UDP socket through the netif wrapper (emulator-aware).
    int sock = netif_socket();
    if (sock < 0) {
        fclose(in);
        return 1;
    }

    // Bind local port for this sender.
    if (netif_bind(sock, listen_port) != 0) {
        fclose(in);
        close(sock);
        return 1;
    }
    // Tell emulator which peer port to forward to.
    //  if (netif_connect(sock, peer_ip, peer_port) != 0) {
    //      fclose(in);
    //      close(sock);
    //      return 1;
    //  }
    // Register with the emulator multiple times to handle startup races.
    for (int attempt = 0; attempt < 10; attempt++) {
        if (netif_connect(sock, peer_ip, peer_port) != 0) {
             fclose(in);
             close(sock);
             return 1;
         }
         struct timespec delay = {0, 100000000L};
         nanosleep(&delay, NULL); 
     }

    uint8_t buf[PKT_HDR_LEN + MAX_PAYLOAD];
    uint8_t recvbuf[PKT_HDR_LEN + MAX_PAYLOAD];
    uint32_t seq = 0;
    uint64_t data_sent = 0;
    uint64_t data_retx = 0;
    uint64_t ack_rcvd = 0;
    uint64_t start_ms = 0;
    uint64_t end_ms = 0;

    //ADDED CODE ->window buffer variables
    uint8_t win_buf[win][sizeof(buf)];
    size_t pkt_lengths[win];
    size_t index = 0;
    uint32_t base = 0;
    bool breakOr = true;
    uint64_t timer_ms = 0;


    // Basic sender: send each data packet once, then send FIN once.
    // TODO(student): implement GBN/SR reliability here:
    //   - keep a send window and buffer unacked packets
    //   - start/restart timers and retransmit on timeout
    //   - process ACKs to slide the window and compute RTT/RTO
    while (1) {

        //ADDED CODE
        if (base == seq && !breakOr){
                break;
            }
        while(seq < base + win && breakOr){
            size_t nread = fread(buf + PKT_HDR_LEN, 1, MAX_PAYLOAD, in);

            if (nread == 0) {
                if (ferror(in)) {
                    perror("fread");
                    fclose(in);
                    close(sock);
                    return 1;
                    }

                breakOr = false;
                break;
            }

            // Build a DATA packet: header + payload.
            size_t pktlen = pkt_build_data(buf, sizeof(buf), seq, buf + PKT_HDR_LEN, (uint16_t)nread);
            if (pktlen == 0) {
                fprintf(stderr, "packet build failed\n");
                fclose(in);
                close(sock);
                return 1;
            }

            //ADDED CODED -> initalized window buffer variables
            index = seq % win;
            pkt_lengths[index] = pktlen;
            memcpy(win_buf[index],buf,pktlen);

            // Example data send call (goes through emulator).
            // TODO(student): in your reliable sender, send from a window buffer.
            if (netif_send(sock, buf, pktlen) < 0) {
                perror("sendto");
                fclose(in);
                close(sock);
                return 1;
                }

                if (start_ms == 0) {
                start_ms = now_ms();
            }
            data_sent += 1;

            //ADDED CODE
            if (base == seq){
                timer_ms = now_ms();
            }
            seq += 1;
            }
        
            // Example ACK receive path (non-blocking). We only count ACKs here.
            // TODO(student): use ACKs to slide the window and retransmit on timeout.
            ssize_t rn = netif_recv(sock, recvbuf, sizeof(recvbuf), 10);
            if (rn > 0) {
                pkt_hdr_t hdr;
                if (pkt_parse(recvbuf, (size_t)rn, &hdr, NULL, NULL) == 0) {
                    if (hdr.type == PKT_TYPE_ACK) {
                        ack_rcvd++;
                        //ADDED CODE
                        if (hdr.ack > base && hdr.ack <= seq) {
                            base = hdr.ack;
                            if (base < seq) {
                                timer_ms = now_ms();
                            } else {
                                timer_ms = 0;
                            }
                        }
                    }
                }
            }

            //ADDED CODE
            if (base < seq && now_ms() - timer_ms >= (uint64_t)rto_ms){
                for (uint32_t resending_pkt = base; resending_pkt < seq; resending_pkt++){
                    index = resending_pkt % win;
                    size_t pktlen = pkt_lengths[index];

                    if (netif_send(sock, win_buf[index], pktlen) < 0) {
                        perror("sendto");
                        fclose(in);
                        close(sock);
                        return 1;
                    }
                    data_retx += 1;

                }
                timer_ms = now_ms();
            }

    }

    // Basic FIN send (no retransmission).
    // Build and send FIN to mark end of file.
    size_t fin_len = pkt_build_fin(buf, sizeof(buf), seq);

    //ADDED CODE
    if (fin_len == 0) {
        fprintf(stderr, "FIN packet build failed\n");
        fclose(in);
        close(sock);
        return 1;
    }

    bool finack_rev = false;
    int finack_tries = 0;
    const int FINACK_RETRIES = 10;

    while (!finack_rev && finack_tries <= FINACK_RETRIES) {
        if (netif_send(sock, buf, fin_len) < 0) {
            perror("FIN send");
            fclose(in);
            close(sock);
            return 1;
        }
        // Basic wait for FINACK (no retries).
        uint64_t wait_ms = now_ms();
        while (now_ms() - wait_ms < (uint64_t)rto_ms) {
            ssize_t n = netif_recv(sock, recvbuf, sizeof(recvbuf), 50);
            if (n > 0) {
                pkt_hdr_t hdr;
                if (pkt_parse(recvbuf, (size_t)n, &hdr, NULL, NULL) == 0) {
                    if (hdr.type == PKT_TYPE_FINACK) {
                        finack_rev = true;
                        end_ms = now_ms();
                        break;
                    }
                    if (hdr.type == PKT_TYPE_ACK) {
                        ack_rcvd++;
                    }
                }
            }        
        }
        if (!finack_rev) {
                finack_tries++;
            }
    }
    // Fail if FINACK never arrives.
    if (!finack_rev) {
        fprintf(stderr, "FINACK not received after retries\n");
        fclose(in);
        close(sock);
        return 1;
    }
    if (!end_ms) {
        end_ms = now_ms();
    }

    double elapsed_ms = (start_ms && end_ms && end_ms > start_ms) ? (double)(end_ms - start_ms) : 1.0;
    double goodput_kbps = (file_size * 8.0) / (elapsed_ms);

    printf("FILE_BYTES=%llu\n", (unsigned long long)file_size);
    printf("CHUNK_BYTES=%d\n", MAX_PAYLOAD);
    printf("WIN=%d\n", win);
    printf("DATA_SENT_PKTS=%llu\n", (unsigned long long)data_sent);
    printf("DATA_RETX_PKTS=%llu\n", (unsigned long long)data_retx);
    printf("ACK_RCVD_PKTS=%llu\n", (unsigned long long)ack_rcvd);
    printf("ELAPSED_MS=%.0f\n", elapsed_ms);
    printf("GOODPUT_KBPS=%.2f\n", goodput_kbps);

    fclose(in);
    close(sock);
    return 0;
}