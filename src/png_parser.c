/*
 * PNG metadata parser — pure C, zero external dependencies.
 *
 * PNG structure:
 *   8-byte signature: 89 50 4E 47 0D 0A 1A 0A
 *   Sequence of chunks, each:
 *     4 bytes  length  (big-endian, excludes type and CRC)
 *     4 bytes  type    (ASCII letters, e.g. "IHDR", "tEXt", "eXIf")
 *     N bytes  data
 *     4 bytes  CRC32   (of type + data)
 *
 * Metadata chunks we target:
 *   tEXt  — Latin-1 keyword/value pairs
 *   iTXt  — UTF-8 keyword/value pairs
 *   zTXt  — compressed text (we drop when STRIP_ALL)
 *   eXIf  — EXIF data (same TIFF structure as JPEG APP1)
 *   gAMA  — gamma (not private, keep)
 *   tIME  — last-modification time
 */

#include "ghostim/png_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── PNG signature ───────────────────────────────────────────────────────── */
static const unsigned char PNG_SIG[8] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};

/* ── Endian helper ───────────────────────────────────────────────────────── */
static unsigned int read_be32_raw(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] <<  8) |  (unsigned int)p[3];
}

/* ── Chunk type comparison ───────────────────────────────────────────────── */
static int chunk_is(const unsigned char *type_bytes, const char *name) {
    return memcmp(type_bytes, name, 4) == 0;
}

/* ── Load file ───────────────────────────────────────────────────────────── */
static unsigned char *load_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long len = ftell(f);
    if (len <= 0) { fclose(f); return NULL; }
    rewind(f);
    unsigned char *buf = (unsigned char *)malloc((size_t)len);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    *out_size = (size_t)len;
    return buf;
}

/* ── Public: png_print_info ──────────────────────────────────────────────── */

int png_print_info(const char *path, int verbose) {
    size_t file_size = 0;
    unsigned char *buf = load_file(path, &file_size);
    if (!buf) {
        fprintf(stderr, "Error: cannot open '%s'.\n", path);
        return -1;
    }

    if (file_size < 8 || memcmp(buf, PNG_SIG, 8) != 0) {
        fprintf(stderr, "Error: '%s' is not a valid PNG file.\n", path);
        free(buf);
        return -1;
    }

    double size_mb = (double)file_size / (1024.0 * 1024.0);
    printf("\n");
    printf("+==============================================+\n");
    printf("|         PNG Metadata Report                 |\n");
    printf("+==============================================+\n");
    printf("| File : %-36s|\n", path);
    printf("| Size : %-5.2f MB                             |\n", size_mb);
    printf("+----------------------------------------------+\n");

    int found_meta = 0;
    int has_exif   = 0;

    size_t pos = 8; /* skip signature */
    while (pos + 12 <= file_size) {
        unsigned int  chunk_len  = read_be32_raw(buf + pos);
        unsigned char *type_ptr  = buf + pos + 4;

        if (pos + 12 + chunk_len > file_size) break; /* truncated */

        unsigned char *data_ptr = buf + pos + 8;

        if (chunk_is(type_ptr, "tEXt") || chunk_is(type_ptr, "iTXt")) {
            /* Keyword is null-terminated within data */
            /* Portable null-search bounded by chunk_len */
            size_t kw_len = 0;
            while (kw_len < chunk_len && data_ptr[kw_len] != '\0') kw_len++;
            char keyword[80];
            size_t kn = kw_len < 79 ? kw_len : 79;
            memcpy(keyword, data_ptr, kn);
            keyword[kn] = '\0';

            /* Value starts after the null terminator (tEXt: Latin-1 direct) */
            char value[256] = "";
            if (chunk_is(type_ptr, "tEXt") && kw_len + 1 < chunk_len) {
                size_t val_len = chunk_len - kw_len - 1;
                size_t vn = val_len < 255 ? val_len : 255;
                memcpy(value, data_ptr + kw_len + 1, vn);
                value[vn] = '\0';
            }

            printf("| %-12s : %-29s|\n", keyword, value);
            found_meta = 1;
        } else if (chunk_is(type_ptr, "eXIf")) {
            has_exif = 1;
            found_meta = 1;
            printf("| eXIf chunk found (%u bytes)%*s|\n",
                   chunk_len, (int)(19 - snprintf(NULL,0,"%u bytes",chunk_len)), " ");
        } else if (chunk_is(type_ptr, "tIME") && chunk_len >= 7) {
            unsigned short year  = (unsigned short)((data_ptr[0] << 8) | data_ptr[1]);
            printf("| Last modified : %04u-%02u-%02u %02u:%02u:%02u      |\n",
                   year, data_ptr[2], data_ptr[3],
                   data_ptr[4], data_ptr[5], data_ptr[6]);
            found_meta = 1;
        } else if (chunk_is(type_ptr, "IHDR") && chunk_len >= 8) {
            unsigned int w = read_be32_raw(data_ptr);
            unsigned int h = read_be32_raw(data_ptr + 4);
            printf("| Dimensions : %u x %u px%*s|\n",
                   w, h,
                   (int)(22 - snprintf(NULL,0,"%u x %u px",w,h)), " ");
        }

        if (chunk_is(type_ptr, "IEND")) break;
        pos += 12 + chunk_len;
    }

    if (!found_meta) {
        printf("| No metadata chunks found.                    |\n");
    }

    if (has_exif) {
        printf("+----------------------------------------------+\n");
        printf("| !! Embedded EXIF data found !!               |\n");
        printf("| Run 'clean' to remove it                     |\n");
    }

    printf("+----------------------------------------------+\n\n");

    free(buf);
    (void)verbose;
    return 0;
}

/* ── Chunk filtering ─────────────────────────────────────────────────────── */

/*
 * CLEAN_LITE     : drop only eXIf (EXIF embedded in PNG).
 * CLEAN_OPTIMIZE : drop eXIf + tEXt + iTXt + zTXt + tIME.
 *                  Always keep IHDR, PLTE, IDAT, IEND, cHRM, gAMA, sRGB,
 *                  iCCP (color management) — these affect rendering.
 * STRIP_GPS      : regardless of clean_mode, only drop eXIf.
 */
static int should_drop_chunk(const unsigned char *type, StripMode strip_mode) {
    /* --strip gps: only drop the EXIF chunk */
    if (strip_mode == STRIP_GPS)
        return chunk_is(type, "eXIf");

    /* Default: remove all non-image metadata chunks */
    return chunk_is(type, "eXIf") || chunk_is(type, "tEXt") ||
           chunk_is(type, "iTXt") || chunk_is(type, "zTXt") ||
           chunk_is(type, "tIME");
}

/* ── Public: png_clean ───────────────────────────────────────────────────── */

int png_clean(const char *src, const char *dst,
              StripMode strip_mode, int dry_run, int verbose) {
    size_t file_size = 0;
    unsigned char *buf = load_file(src, &file_size);
    if (!buf) {
        fprintf(stderr, "Error: cannot open '%s'.\n", src);
        return -1;
    }

    if (file_size < 8 || memcmp(buf, PNG_SIG, 8) != 0) {
        fprintf(stderr, "Error: '%s' is not a valid PNG.\n", src);
        free(buf);
        return -1;
    }

    size_t out_cap  = file_size;
    size_t out_size = 0;
    unsigned char *out = (unsigned char *)malloc(out_cap);
    if (!out) { free(buf); return -1; }

#define APPEND(ptr, n) do {                               \
    size_t _n = (n);                                      \
    if (out_size + _n > out_cap) {                        \
        out_cap = (out_size + _n) * 2;                    \
        unsigned char *_r = (unsigned char*)realloc(out, out_cap); \
        if (!_r) { free(buf); free(out); return -1; }     \
        out = _r;                                         \
    }                                                     \
    memcpy(out + out_size, (ptr), _n);                    \
    out_size += _n;                                       \
} while(0)

    /* Copy PNG signature */
    APPEND(buf, 8);

    long removed_bytes = 0;
    int  removed_count = 0;
    size_t pos = 8;

    while (pos + 12 <= file_size) {
        unsigned int   chunk_len = read_be32_raw(buf + pos);
        unsigned char *type_ptr  = buf + pos + 4;

        if (pos + 12 + chunk_len > file_size) {
            APPEND(buf + pos, file_size - pos);
            break;
        }

        if (should_drop_chunk(type_ptr, strip_mode)) {
            if (verbose)
                printf("  [remove] chunk='%.4s'  size=%u bytes\n",
                       (char *)type_ptr, chunk_len);
            removed_bytes += 12 + (long)chunk_len;
            removed_count++;
        } else {
            APPEND(buf + pos, 12 + chunk_len);
        }

        if (chunk_is(type_ptr, "IEND")) break;
        pos += 12 + chunk_len;
    }

#undef APPEND

    free(buf);

    if (dry_run || verbose) {
        double saved_kb = (double)removed_bytes / 1024.0;
        double pct = (file_size > 0)
                     ? (double)removed_bytes / (double)file_size * 100.0
                     : 0.0;
        if (dry_run) {
            printf("[dry-run] %s\n", src);
            printf("          Would remove %d chunk(s), saving %.1f KB (%.1f%%)\n",
                   removed_count, saved_kb, pct);
            free(out);
            return 0;
        } else {
            printf("  Removed %d chunk(s), saved %.1f KB (%.1f%%)\n",
                   removed_count, saved_kb, pct);
        }
    }

    char tmp_path[4096];
    snprintf(tmp_path, sizeof(tmp_path), "%s.ghostim_tmp", dst);

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        fprintf(stderr, "Error: cannot write '%s'.\n", tmp_path);
        free(out);
        return -1;
    }
    fwrite(out, 1, out_size, f);
    fclose(f);
    free(out);

    #ifdef _WIN32
        #include <windows.h>
        if (!MoveFileExA(tmp_path, dst, MOVEFILE_REPLACE_EXISTING)) {
            fprintf(stderr, "Error: cannot replace '%s'.\n", dst);
            return -1;
        }
    #else
        if (rename(tmp_path, dst) != 0) {
            FILE *in  = fopen(tmp_path, "rb");
            FILE *o   = fopen(dst, "wb");
            char  rb[65536]; size_t n;
            while (in && o && (n = fread(rb,1,sizeof(rb),in)) > 0)
                fwrite(rb,1,n,o);
            if (in) fclose(in);
            if (o)  fclose(o);
            remove(tmp_path);
        }
    #endif

    if (verbose)
        printf("  Cleaned '%s' → '%s' (-%ld bytes)\n", src, dst, removed_bytes);

    return 0;
}
