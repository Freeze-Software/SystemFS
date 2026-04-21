#include <stdint.h>
#include <stddef.h>
#include "../include/sfs.h"

static uint32_t table[256];
static int      table_ready = 0;

static void build_table(void)
{
    for (int i = 0; i < 256; i++) {
        uint32_t c = (uint32_t)i;
        for (int j = 0; j < 8; j++)
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        table[i] = c;
    }
    table_ready = 1;
}

uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    if (!table_ready) build_table();

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
