#include "protocol.h"

#include <string.h>
#include <stddef.h>
#include <arpa/inet.h>

static uint32_t crc_for_packet(const pkt_hdr_t *net_hdr, const uint8_t *payload, uint16_t len) {
    uint8_t tmp[PKT_HDR_LEN + MAX_PAYLOAD];
    size_t total = PKT_HDR_LEN + len;

    memcpy(tmp, net_hdr, PKT_HDR_LEN);
    tmp[offsetof(pkt_hdr_t, crc32) + 0] = 0;
    tmp[offsetof(pkt_hdr_t, crc32) + 1] = 0;
    tmp[offsetof(pkt_hdr_t, crc32) + 2] = 0;
    tmp[offsetof(pkt_hdr_t, crc32) + 3] = 0;

    if (payload && len > 0) {
        memcpy(tmp + PKT_HDR_LEN, payload, len);
    }
    return crc32_ieee(tmp, total);
}

static size_t build_common(uint8_t *buf, size_t buf_cap, uint8_t type,
                           uint32_t seq, uint32_t ack,
                           const uint8_t *payload, uint16_t len) {
    if (len > MAX_PAYLOAD || buf_cap < PKT_HDR_LEN + len) {
        return 0;
    }

    pkt_hdr_t hdr;
    hdr.magic = htons(MAGIC_CONST);
    hdr.type = type;
    hdr.flags = 0;
    hdr.seq = htonl(seq);
    hdr.ack = htonl(ack);
    hdr.len = htons(len);
    hdr.crc32 = 0;

    uint32_t crc = crc_for_packet(&hdr, payload, len);
    hdr.crc32 = htonl(crc);

    memcpy(buf, &hdr, PKT_HDR_LEN);
    if (len > 0 && payload) {
        memcpy(buf + PKT_HDR_LEN, payload, len);
    }

    return PKT_HDR_LEN + len;
}

size_t pkt_build_data(uint8_t *buf, size_t buf_cap, uint32_t seq,
                      const uint8_t *payload, uint16_t len) {
    return build_common(buf, buf_cap, PKT_TYPE_DATA, seq, 0, payload, len);
}

size_t pkt_build_ack(uint8_t *buf, size_t buf_cap, uint32_t ack) {
    return build_common(buf, buf_cap, PKT_TYPE_ACK, 0, ack, NULL, 0);
}

size_t pkt_build_fin(uint8_t *buf, size_t buf_cap, uint32_t seq) {
    return build_common(buf, buf_cap, PKT_TYPE_FIN, seq, 0, NULL, 0);
}

size_t pkt_build_finack(uint8_t *buf, size_t buf_cap, uint32_t ack) {
    return build_common(buf, buf_cap, PKT_TYPE_FINACK, 0, ack, NULL, 0);
}

int pkt_parse(const uint8_t *buf, size_t len, pkt_hdr_t *hdr,
              const uint8_t **payload, uint16_t *payload_len) {
    if (len < PKT_HDR_LEN) {
        return -1;
    }

    pkt_hdr_t net_hdr;
    memcpy(&net_hdr, buf, PKT_HDR_LEN);

    uint16_t magic = ntohs(net_hdr.magic);
    if (magic != MAGIC_CONST) {
        return -2;
    }

    uint16_t plen = ntohs(net_hdr.len);
    if (len < PKT_HDR_LEN + plen) {
        return -3;
    }

    uint32_t recv_crc = ntohl(net_hdr.crc32);
    uint32_t calc_crc = crc_for_packet(&net_hdr, buf + PKT_HDR_LEN, plen);
    if (recv_crc != calc_crc) {
        return -4;
    }

    if (hdr) {
        hdr->magic = magic;
        hdr->type = net_hdr.type;
        hdr->flags = net_hdr.flags;
        hdr->seq = ntohl(net_hdr.seq);
        hdr->ack = ntohl(net_hdr.ack);
        hdr->len = plen;
        hdr->crc32 = recv_crc;
    }

    if (payload) {
        *payload = buf + PKT_HDR_LEN;
    }
    if (payload_len) {
        *payload_len = plen;
    }

    return 0;
}
