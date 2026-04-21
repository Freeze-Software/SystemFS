#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../include/sfs.h"

static void write_le64(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; i++, v >>= 8) p[i] = v & 0xffu;
}

static void write_le32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++, v >>= 8) p[i] = v & 0xffu;
}

static uint64_t read_le64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
    return v;
}

static uint32_t read_le32(const uint8_t *p)
{
    uint32_t v = 0;
    for (int i = 3; i >= 0; i--) v = (v << 8) | p[i];
    return v;
}

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    *len = (size_t)sz;
    uint8_t *buf = malloc(*len + 1);
    if (!buf) { fclose(f); return NULL; }
    if (*len && fread(buf, 1, *len, f) != *len) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    return buf;
}

static int dump(const char *path, const uint8_t *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int ok = (!len || fwrite(data, 1, len, f) == len);
    fclose(f);
    return ok ? 0 : -1;
}

static char *append_ext(const char *path, const char *ext)
{
    size_t n = strlen(path) + strlen(ext) + 1;
    char *s = malloc(n);
    snprintf(s, n, "%s%s", path, ext);
    return s;
}

static char *drop_ext(const char *path, const char *ext)
{
    size_t pl = strlen(path), el = strlen(ext);
    if (pl <= el || strcmp(path + pl - el, ext) != 0) return NULL;
    char *s = malloc(pl - el + 1);
    memcpy(s, path, pl - el);
    s[pl - el] = '\0';
    return s;
}

static void human(double bytes, char *buf, size_t bufsz)
{
    const char *units[] = { "B", "KiB", "MiB", "GiB" };
    int u = 0;
    while (bytes >= 1024.0 && u < 3) { bytes /= 1024.0; u++; }
    snprintf(buf, bufsz, "%.1f %s", bytes, units[u]);
}

static int compress_file(const char *in, int keep, int verbose)
{
    size_t in_len;
    uint8_t *src = slurp(in, &in_len);
    if (!src) {
        fprintf(stderr, "sfs: %s: %s\n", in, strerror(errno));
        return 1;
    }

    size_t   comp_len;
    uint8_t *comp = sfs_compress(src, in_len, &comp_len);
    uint32_t crc  = crc32_calc(src, in_len);
    free(src);

    size_t   total = SFS_HEADER_SIZE + comp_len;
    uint8_t *out   = malloc(total);
    memcpy(out, SFS_MAGIC, SFS_MAGIC_LEN);
    write_le64(out + 4,  (uint64_t)in_len);
    write_le32(out + 12, crc);
    write_le64(out + 16, (uint64_t)comp_len);
    memcpy(out + SFS_HEADER_SIZE, comp, comp_len);
    free(comp);

    char *outpath = append_ext(in, ".sfs");
    if (dump(outpath, out, total) != 0) {
        fprintf(stderr, "sfs: %s: %s\n", outpath, strerror(errno));
        free(out); free(outpath); return 1;
    }
    free(out);

    if (verbose) {
        char hs[32], cs[32];
        human((double)in_len,   hs, sizeof hs);
        human((double)comp_len, cs, sizeof cs);
        double ratio = in_len ? (1.0 - (double)comp_len / in_len) * 100.0 : 0.0;
        printf("%-40s  %s -> %s  (%.1f%% saved)\n", outpath, hs, cs, ratio);
    }

    free(outpath);
    if (!keep) remove(in);
    return 0;
}

static int decompress_file(const char *in, int keep, int verbose)
{
    size_t   file_len;
    uint8_t *file = slurp(in, &file_len);
    if (!file) {
        fprintf(stderr, "sfs: %s: %s\n", in, strerror(errno));
        return 1;
    }

    if (file_len < SFS_HEADER_SIZE || memcmp(file, SFS_MAGIC, SFS_MAGIC_LEN) != 0) {
        fprintf(stderr, "sfs: %s: not an .sfs file\n", in);
        free(file); return 1;
    }

    uint64_t orig_len  = read_le64(file + 4);
    uint32_t want_crc  = read_le32(file + 12);
    uint64_t comp_len  = read_le64(file + 16);

    if (SFS_HEADER_SIZE + comp_len > file_len) {
        fprintf(stderr, "sfs: %s: truncated\n", in);
        free(file); return 1;
    }

    size_t   got_len;
    uint8_t *out = sfs_decompress(file + SFS_HEADER_SIZE,
                                   (size_t)comp_len,
                                   (size_t)orig_len,
                                   &got_len);
    free(file);

    if (!out) {
        fprintf(stderr, "sfs: %s: decompression failed\n", in);
        return 1;
    }

    if (crc32_calc(out, got_len) != want_crc) {
        fprintf(stderr, "sfs: %s: CRC mismatch – data corrupt\n", in);
        free(out); return 1;
    }

    char *outpath = drop_ext(in, ".sfs");
    if (!outpath) outpath = append_ext(in, ".out");

    if (dump(outpath, out, got_len) != 0) {
        fprintf(stderr, "sfs: %s: %s\n", outpath, strerror(errno));
        free(out); free(outpath); return 1;
    }
    free(out);

    if (verbose) printf("%s -> %s\n", in, outpath);
    free(outpath);
    if (!keep) remove(in);
    return 0;
}

static int test_file(const char *in, int verbose)
{
    size_t   file_len;
    uint8_t *file = slurp(in, &file_len);
    if (!file) {
        fprintf(stderr, "sfs: %s: %s\n", in, strerror(errno));
        return 1;
    }

    if (file_len < SFS_HEADER_SIZE || memcmp(file, SFS_MAGIC, SFS_MAGIC_LEN) != 0) {
        fprintf(stderr, "sfs: %s: not an .sfs file\n", in);
        free(file); return 1;
    }

    uint64_t orig_len = read_le64(file + 4);
    uint32_t want_crc = read_le32(file + 12);
    uint64_t comp_len = read_le64(file + 16);

    if (SFS_HEADER_SIZE + comp_len > file_len) {
        fprintf(stderr, "sfs: %s: truncated\n", in);
        free(file); return 1;
    }

    size_t   got_len;
    uint8_t *out = sfs_decompress(file + SFS_HEADER_SIZE,
                                   (size_t)comp_len,
                                   (size_t)orig_len,
                                   &got_len);
    free(file);

    if (!out || crc32_calc(out, got_len) != want_crc) {
        free(out);
        fprintf(stderr, "sfs: %s: FAILED\n", in);
        return 1;
    }
    free(out);

    if (verbose) printf("%s: OK\n", in);
    return 0;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTION]... FILE...\n"
        "\n"
        "  (no flag)   compress, output FILE.sfs\n"
        "  -d          decompress\n"
        "  -t          test integrity, no output\n"
        "  -k          keep original file\n"
        "  -v          verbose\n"
        "\n"
        "Examples:\n"
        "  sfs archive.tar          -> archive.tar.sfs\n"
        "  sfs -d archive.tar.sfs   -> archive.tar\n"
        "  sfs -tv archive.tar.sfs  -> integrity check\n",
        prog);
    exit(1);
}

int main(int argc, char **argv)
{
    int decomp = 0, test = 0, keep = 0, verbose = 0;
    int i = 1;

    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        for (const char *c = argv[i] + 1; *c; c++) {
            switch (*c) {
            case 'd': decomp  = 1; break;
            case 't': test    = 1; break;
            case 'k': keep    = 1; break;
            case 'v': verbose = 1; break;
            case '-': break;
            default:
                fprintf(stderr, "sfs: unknown option -%c\n", *c);
                usage(argv[0]);
            }
        }
    }

    if (i >= argc) usage(argv[0]);

    int ret = 0;
    for (; i < argc; i++) {
        if (test)
            ret |= test_file(argv[i], verbose);
        else if (decomp)
            ret |= decompress_file(argv[i], keep, verbose);
        else
            ret |= compress_file(argv[i], keep, verbose);
    }
    return ret;
}
