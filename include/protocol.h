#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define MAGIC_CONST 0xCCAA
#define MAX_PAYLOAD 1000

#define PKT_TYPE_DATA   0
#define PKT_TYPE_ACK    1
#define PKT_TYPE_FIN    2
#define PKT_TYPE_FINACK 3

#define SR_MAX_WINDOW 512

#pragma pack(push, 1)
typedef struct {
    uint16_t magic;
    uint8_t  type;
    uint8_t  flags;
    uint32_t seq;
    uint32_t ack;
    uint16_t len;
    uint32_t crc32;
} pkt_hdr_t;
#pragma pack(pop)

#define PKT_HDR_LEN ((size_t)sizeof(pkt_hdr_t))

uint32_t crc32_ieee(const uint8_t *data, size_t len);

size_t pkt_build_data(uint8_t *buf, size_t buf_cap, uint32_t seq,
                      const uint8_t *payload, uint16_t len);
size_t pkt_build_ack(uint8_t *buf, size_t buf_cap, uint32_t ack);
size_t pkt_build_fin(uint8_t *buf, size_t buf_cap, uint32_t seq);
size_t pkt_build_finack(uint8_t *buf, size_t buf_cap, uint32_t ack);

int pkt_parse(const uint8_t *buf, size_t len, pkt_hdr_t *hdr,
              const uint8_t **payload, uint16_t *payload_len);

#endif
