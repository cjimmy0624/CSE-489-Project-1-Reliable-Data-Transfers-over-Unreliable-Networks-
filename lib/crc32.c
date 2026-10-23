#include <stdint.h>
#include <stddef.h>

static uint32_t crc_table[256];
static int crc_table_ready = 0;

static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) {
                c = 0xEDB88320U ^ (c >> 1);
            } else {
                c >>= 1;
            }
        }
        crc_table[i] = c;
    }
    crc_table_ready = 1;
}

uint32_t crc32_ieee(const uint8_t *data, size_t len) {
    if (!crc_table_ready) {
        crc32_init();
    }
    uint32_t c = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; i++) {
        c = crc_table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFU;
}
