#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../include/sfs.h"

#define HASH_SIZE  65536
#define HASH_MASK  (HASH_SIZE - 1)
#define MAX_CHAIN  192

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
    uint32_t accum;
    int      bits;
} BitWriter;

static void bw_init(BitWriter *w)
{
    w->cap   = 65536;
    w->data  = malloc(w->cap);
    w->len   = 0;
    w->accum = 0;
    w->bits  = 0;
}

static void bw_put(BitWriter *w, uint32_t val, int n)
{
    w->accum = (w->accum << n) | (val & ((1u << n) - 1));
    w->bits += n;
    while (w->bits >= 8) {
        w->bits -= 8;
        if (w->len + 1 >= w->cap) {
            w->cap *= 2;
            w->data = realloc(w->data, w->cap);
        }
        w->data[w->len++] = (w->accum >> w->bits) & 0xffu;
    }
}

static void bw_flush(BitWriter *w)
{
    if (w->bits > 0)
        bw_put(w, 0, 8 - w->bits);
}

static uint32_t hash3(const uint8_t *p)
{
    return ((uint32_t)p[0] * 2654435761u
          ^ (uint32_t)p[1] * 40503u
          ^ (uint32_t)p[2]) & HASH_MASK;
}

uint8_t *sfs_compress(const uint8_t *src, size_t src_len, size_t *out_len)
{
    if (!src_len) {
        *out_len = 0;
        return calloc(1, 1);
    }

    int32_t *head = malloc(HASH_SIZE * sizeof(int32_t));
    int32_t *prev = malloc(SFS_WINDOW_SIZE * sizeof(int32_t));
    for (int i = 0; i < HASH_SIZE; i++)      head[i] = -1;
    for (int i = 0; i < SFS_WINDOW_SIZE; i++) prev[i] = -1;

    BitWriter w;
    bw_init(&w);

    size_t i = 0;
    while (i < src_len) {
        int best_len = 0, best_off = 0;

        if (i + SFS_MIN_MATCH <= src_len) {
            uint32_t h   = hash3(src + i);
            int32_t  pos = head[h];
            int      steps = MAX_CHAIN;

            while (pos >= 0 && steps-- > 0) {
                int32_t off = (int32_t)i - pos;
                if (off > SFS_WINDOW_SIZE) break;

                int lim = (int)(src_len - i);
                if (lim > SFS_MAX_MATCH) lim = SFS_MAX_MATCH;

                int ml = 0;
                const uint8_t *a = src + i, *b = src + pos;
                while (ml < lim && a[ml] == b[ml]) ml++;

                if (ml > best_len) {
                    best_len = ml;
                    best_off = (int)off;
                    if (ml == SFS_MAX_MATCH) break;
                }
                pos = prev[pos & SFS_WINDOW_MASK];
            }

            prev[i & SFS_WINDOW_MASK] = head[h];
            head[h] = (int32_t)i;
        }

        if (best_len >= SFS_MIN_MATCH) {
            bw_put(&w, 1, 1);
            bw_put(&w, (uint32_t)(best_off - 1), SFS_WINDOW_BITS);
            bw_put(&w, (uint32_t)(best_len - SFS_MIN_MATCH), 8);

            for (int k = 1; k < best_len; k++) {
                size_t j = i + k;
                if (j + SFS_MIN_MATCH <= src_len) {
                    uint32_t h2 = hash3(src + j);
                    prev[j & SFS_WINDOW_MASK] = head[h2];
                    head[h2] = (int32_t)j;
                }
            }
            i += (size_t)best_len;
        } else {
            bw_put(&w, 0, 1);
            bw_put(&w, src[i], 8);
            i++;
        }
    }

    bw_flush(&w);
    free(head);
    free(prev);

    *out_len = w.len;
    return w.data;
}

static uint32_t br_read(const uint8_t *src, size_t src_len,
                         size_t *bp, uint32_t *acc, int *bits, int n)
{
    while (*bits < n && *bp < src_len) {
        *acc  = (*acc << 8) | src[(*bp)++];
        *bits += 8;
    }
    *bits -= n;
    return (*acc >> *bits) & ((1u << n) - 1);
}

uint8_t *sfs_decompress(const uint8_t *src, size_t src_len,
                         size_t orig_len, size_t *out_len)
{
    if (!orig_len) {
        *out_len = 0;
        return calloc(1, 1);
    }

    uint8_t *out    = malloc(orig_len);
    size_t   op     = 0;
    size_t   bp     = 0;
    uint32_t acc    = 0;
    int      bits   = 0;

    while (op < orig_len) {
        int flag = (int)br_read(src, src_len, &bp, &acc, &bits, 1);
        if (flag) {
            uint32_t off = br_read(src, src_len, &bp, &acc, &bits, SFS_WINDOW_BITS) + 1;
            uint32_t len = br_read(src, src_len, &bp, &acc, &bits, 8) + SFS_MIN_MATCH;

            if (op < off) {
                free(out);
                *out_len = 0;
                return NULL;
            }

            size_t start = op - off;
            for (uint32_t k = 0; k < len && op < orig_len; k++)
                out[op++] = out[start + k];
        } else {
            out[op++] = (uint8_t)br_read(src, src_len, &bp, &acc, &bits, 8);
        }
    }

    *out_len = op;
    return out;
}
