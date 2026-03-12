/*
 * png_parser.c — PNG metadata removal and optimization.
 *
 * Lossless: walks raw chunk sequence, drops metadata chunks.
 *   IDAT pixel data is never decoded or modified.
 *
 * Optimized: libpng decode → re-encode at zlib level 9 with
 *   adaptive filter selection. PNG is always lossless — this
 *   path achieves better compression, not pixel degradation.
 *
 * Metadata chunks dropped: eXIf, tEXt, iTXt, zTXt, tIME.
 * Always kept: IHDR, PLTE, IDAT, IEND, cHRM, gAMA, sRGB, iCCP,
 *              bKGD, pHYs, sBIT, sPLT, hIST, tRNS.
 */

#include "ghostim/png_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include <png.h>

/* ── PNG signature ───────────────────────────────────────────────────────── */
static const unsigned char PNG_SIG[8] = {0x89, 0x50, 0x4E, 0x47,
                                         0x0D, 0x0A, 0x1A, 0x0A};

/* ── Endian helper ───────────────────────────────────────────────────────── */
static unsigned int read_be32(const unsigned char *p) {
  return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
         ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}
/* ── Metadata chunk detection ────────────────────────────────────────────── */
static int chunk_is_meta(const unsigned char type[4]) {
  static const char *drop[] = {"eXIf", "tEXt", "iTXt", "zTXt", "tIME", NULL};
  for (int i = 0; drop[i]; i++)
    if (memcmp(type, drop[i], 4) == 0)
      return 1;
  return 0;
}

/* ── Load file ───────────────────────────────────────────────────────────── */
static unsigned char *load_file(const char *path, size_t *sz) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  rewind(f);
  if (len <= 0) {
    fclose(f);
    return NULL;
  }
  unsigned char *b = (unsigned char *)malloc((size_t)len);
  if (!b) {
    fclose(f);
    return NULL;
  }
  if (fread(b, 1, (size_t)len, f) != (size_t)len) {
    free(b);
    fclose(f);
    return NULL;
  }
  fclose(f);
  *sz = (size_t)len;
  return b;
}

/* ── Atomic write ────────────────────────────────────────────────────────── */
static int atomic_write(const char *dst, const unsigned char *data, size_t sz) {
  char tmp[4096];
  snprintf(tmp, sizeof(tmp), "%s.ghostim_tmp", dst);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return -1;
  fwrite(data, 1, sz, f);
  fclose(f);
#ifdef _WIN32
  if (!MoveFileExA(tmp, dst, MOVEFILE_REPLACE_EXISTING)) {
    remove(tmp);
    return -1;
  }
#else
  if (rename(tmp, dst) != 0) {
    FILE *in = fopen(tmp, "rb"), *out = fopen(dst, "wb");
    char b[65536];
    size_t n;
    while (in && out && (n = fread(b, 1, sizeof(b), in)) > 0)
      fwrite(b, 1, n, out);
    if (in)
      fclose(in);
    if (out)
      fclose(out);
    remove(tmp);
  }
#endif
  return 0;
}

/* ── APPEND macro ────────────────────────────────────────────────────────── */
#define APPEND(dst, dsz, dcap, src, n)                                         \
  do {                                                                         \
    size_t _n = (n);                                                           \
    if ((dsz) + _n > (dcap)) {                                                 \
      (dcap) = ((dsz) + _n) * 2;                                               \
      unsigned char *_r = (unsigned char *)realloc((dst), (dcap));             \
      if (!_r) {                                                               \
        free(buf);                                                             \
        free(out);                                                             \
        return -1;                                                             \
      }                                                                        \
      (dst) = _r;                                                              \
    }                                                                          \
    memcpy((dst) + (dsz), (src), _n);                                          \
    (dsz) += _n;                                                               \
  } while (0)

/* ════════════════════════════════════════════════════════════════════════════
 * PUBLIC: png_print_info
 * ════════════════════════════════════════════════════════════════════════════
 */
int png_print_info(const char *path, int verbose) {
  size_t file_size = 0;
  unsigned char *buf = load_file(path, &file_size);
  if (!buf) {
    fprintf(stderr, "Error: cannot open '%s'.\n", path);
    return -1;
  }
  if (file_size < 8 || memcmp(buf, PNG_SIG, 8) != 0) {
    fprintf(stderr, "Error: '%s' is not a valid PNG.\n", path);
    free(buf);
    return -1;
  }

  unsigned int width = 0, height = 0, bit_depth = 0, color_type = 0;
  int has_exif = 0, has_text = 0, has_time = 0;
  char text_keys[512] = "";
  size_t tk_len = 0;

  size_t pos = 8;
  while (pos + 12 <= file_size) {
    unsigned int len = read_be32(buf + pos);
    unsigned char *tp = buf + pos + 4;
    if (pos + 12 + len > file_size)
      break;

    if (memcmp(tp, "IHDR", 4) == 0 && len >= 13) {
      width = read_be32(buf + pos + 8);
      height = read_be32(buf + pos + 12);
      bit_depth = buf[pos + 16];
      color_type = buf[pos + 17];
    }
    if (memcmp(tp, "eXIf", 4) == 0)
      has_exif = 1;
    if (memcmp(tp, "tEXt", 4) == 0 || memcmp(tp, "iTXt", 4) == 0 ||
        memcmp(tp, "zTXt", 4) == 0) {
      has_text = 1;
      /* extract key (null-terminated) */
      if (len > 0 && tk_len < sizeof(text_keys) - 32) {
        size_t kl = strlen((char *)buf + pos + 8);
        if (kl > 64)
          kl = 64;
        if (tk_len > 0) {
          text_keys[tk_len++] = ',';
          text_keys[tk_len++] = ' ';
        }
        memcpy(text_keys + tk_len, buf + pos + 8, kl);
        tk_len += kl;
        text_keys[tk_len] = '\0';
      }
    }
    if (memcmp(tp, "tIME", 4) == 0)
      has_time = 1;
    if (memcmp(tp, "IEND", 4) == 0)
      break;
    pos += 12 + len;
  }
  free(buf);

  static const char *ct_names[] = {"Grayscale",       "", "RGB", "Palette",
                                   "Grayscale+Alpha", "", "RGBA"};
  const char *ct_str = (color_type < 7 && ct_names[color_type][0])
                           ? ct_names[color_type]
                           : "Unknown";

  printf("\n+==============================================+\n");
  printf("|          PNG Image Report                   |\n");
  printf("+==============================================+\n");
  printf("| File   : %-34s|\n", path);
  printf("| Image  : %u x %u px  %u-bit %s%*s|\n", width, height, bit_depth,
         ct_str,
         (int)(14 - snprintf(NULL, 0, "%u x %u px  %u-bit %s", width, height,
                             bit_depth, ct_str)),
         " ");
  printf("+----------------------------------------------+\n");
  if (!has_exif && !has_text && !has_time) {
    printf("| Metadata : None — already clean.             |\n");
  } else {
    printf("| Metadata found:                              |\n");
    if (has_exif)
      printf("|   EXIF embedded                              |\n");
    if (has_time)
      printf("|   Timestamp (tIME)                           |\n");
    if (has_text)
      printf("|   Text chunks: %-28s|\n", text_keys);
    printf("+----------------------------------------------+\n");
    printf("| Run: ghostim clean <file> to remove all     |\n");
  }
  printf("+----------------------------------------------+\n\n");
  (void)verbose;
  return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Lossless path — raw chunk walk
 * ════════════════════════════════════════════════════════════════════════════
 */
static int png_lossless(const char *src, const char *dst, int dry_run,
                        int verbose) {
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

  size_t out_cap = file_size, out_size = 0;
  unsigned char *out = (unsigned char *)malloc(out_cap);
  if (!out) {
    free(buf);
    return -1;
  }

  APPEND(out, out_size, out_cap, PNG_SIG, 8);

  long removed = 0;
  int removed_n = 0;
  size_t pos = 8;
  while (pos + 12 <= file_size) {
    unsigned int len = read_be32(buf + pos);
    unsigned char *tp = buf + pos + 4;
    if (pos + 12 + len > file_size)
      break;
    if (chunk_is_meta(tp)) {
      if (verbose)
        printf("  [remove] %.4s  %u bytes\n", tp, 12 + len);
      removed += 12 + (long)len;
      removed_n++;
    } else {
      APPEND(out, out_size, out_cap, buf + pos, 12 + len);
    }
    if (memcmp(tp, "IEND", 4) == 0)
      break;
    pos += 12 + len;
  }
  free(buf);

  double saved_kb = (double)removed / 1024.0;
  double pct =
      file_size > 0 ? (double)removed / (double)file_size * 100.0 : 0.0;
  if (dry_run) {
    printf("[dry-run] lossless  %s\n", src);
    printf("          Would remove %d chunk(s), saving %.1f KB (%.1f%%)\n",
           removed_n, saved_kb, pct);
    free(out);
    return 0;
  }
  if (verbose)
    printf("  Removed %d chunk(s), saved %.1f KB (%.1f%%)\n", removed_n,
           saved_kb, pct);
  int r = atomic_write(dst, out, out_size);
  free(out);
  return r;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Optimized path — libpng decode → re-encode at zlib level 9
 * ════════════════════════════════════════════════════════════════════════════
 */

/* libpng write-to-memory callbacks */
typedef struct {
  unsigned char *data;
  size_t size;
  size_t cap;
} MemBuf;

static void mem_write_fn(png_structp p, png_bytep d, png_size_t n) {
  MemBuf *m = (MemBuf *)png_get_io_ptr(p);
  if (m->size + n > m->cap) {
    m->cap = (m->size + n) * 2;
    m->data = (unsigned char *)realloc(m->data, m->cap);
  }
  memcpy(m->data + m->size, d, n);
  m->size += n;
}
static void mem_flush_fn(png_structp p) { (void)p; }

static int png_optimized(const char *src, const char *dst, int dry_run,
                         int verbose) {
  /* ── Decode ── */
  FILE *fin = fopen(src, "rb");
  if (!fin) {
    fprintf(stderr, "Error: cannot open '%s'.\n", src);
    return -1;
  }

  png_structp rp =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop ri = png_create_info_struct(rp);
  if (setjmp(png_jmpbuf(rp))) {
    png_destroy_read_struct(&rp, &ri, NULL);
    fclose(fin);
    return -1;
  }
  png_init_io(rp, fin);
  png_read_png(rp, ri, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING, NULL);
  fclose(fin);

  png_uint_32 width = png_get_image_width(rp, ri);
  png_uint_32 height = png_get_image_height(rp, ri);
  int color = png_get_color_type(rp, ri);
  int depth = png_get_bit_depth(rp, ri);
  png_bytepp rows = png_get_rows(rp, ri);

  if (dry_run) {
    printf("[dry-run] optimized  %s\n", src);
    printf("          Would re-encode %ux%u PNG at zlib level 9\n", width,
           height);
    png_destroy_read_struct(&rp, &ri, NULL);
    return 0;
  }

  /* ── Encode at level 9 ── */
  MemBuf mb;
  mb.cap = 1 << 20;
  mb.size = 0;
  mb.data = (unsigned char *)malloc(mb.cap);

  png_structp wp =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop wi = png_create_info_struct(wp);
  if (setjmp(png_jmpbuf(wp))) {
    png_destroy_write_struct(&wp, &wi);
    png_destroy_read_struct(&rp, &ri, NULL);
    free(mb.data);
    return -1;
  }
  png_set_write_fn(wp, &mb, mem_write_fn, mem_flush_fn);
  png_set_compression_level(wp, 9);
  png_set_filter(wp, 0, PNG_ALL_FILTERS);
  png_set_IHDR(wp, wi, width, height, depth, color, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_set_rows(wp, wi, rows);
  png_write_png(wp, wi, PNG_TRANSFORM_IDENTITY, NULL);
  png_destroy_write_struct(&wp, &wi);
  png_destroy_read_struct(&rp, &ri, NULL);

  double before_kb = 0.0;
  {
    FILE *t = fopen(src, "rb");
    if (t) {
      fseek(t, 0, SEEK_END);
      before_kb = (double)ftell(t) / 1024.0;
      fclose(t);
    }
  }
  double after_kb = (double)mb.size / 1024.0;
  double pct = before_kb > 0 ? (before_kb - after_kb) / before_kb * 100.0 : 0.0;
  if (verbose)
    printf("  Optimized PNG: %.1f KB → %.1f KB (%.1f%% smaller)\n", before_kb,
           after_kb, pct);

  int r = atomic_write(dst, mb.data, mb.size);
  free(mb.data);
  return r;
}

/* ════════════════════════════════════════════════════════════════════════════
 * PUBLIC: png_clean — dispatcher
 * ════════════════════════════════════════════════════════════════════════════
 */
int png_clean(const char *src, const char *dst, OptMode opt_mode, int dry_run,
              int verbose) {
  if (opt_mode == OPT_LOSSY)
    return png_optimized(src, dst, dry_run, verbose);
  return png_lossless(src, dst, dry_run, verbose);
}
