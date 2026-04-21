#pragma once

#include <stdint.h>
#include <stddef.h>

#define SFS_MAGIC       "\x53\x46\x53\x01"
#define SFS_MAGIC_LEN   4
#define SFS_HEADER_SIZE 24

#define SFS_WINDOW_BITS 15
#define SFS_WINDOW_SIZE (1 << SFS_WINDOW_BITS)
#define SFS_WINDOW_MASK (SFS_WINDOW_SIZE - 1)
#define SFS_MAX_MATCH   258
#define SFS_MIN_MATCH   3

uint8_t  *sfs_compress(const uint8_t *src, size_t src_len, size_t *out_len);
uint8_t  *sfs_decompress(const uint8_t *src, size_t src_len, size_t orig_len, size_t *out_len);
uint32_t  crc32_calc(const uint8_t *data, size_t len);
