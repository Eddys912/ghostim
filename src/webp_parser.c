/*
 * webp_parser.c — WebP metadata removal and optimization.
 *
 * WebP uses a RIFF container:
 *   "RIFF" <size32> "WEBP" <chunks...>
 *
 * Each chunk: 4-byte FourCC + 4-byte LE size + data (+ 1 pad byte if odd).
 *
 * Lossless: rebuild RIFF without EXIF/XMP/ICCP chunks.
 *   VP8 / VP8L / VP8X bitstream copied verbatim.
 *
 * Lossy: libwebp WebPDecodeRGB → WebPEncodeRGB (or lossless at q=100).
 */

#include "ghostim/webp_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>
#include <webp/mux.h>

/* ── Endian helpers ──────────────────────────────────────────────────────── */
static unsigned int read_le32(const unsigned char *p) {
  return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
         ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}
static void write_le32(unsigned char *p, unsigned int v) {
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
  p[2] = (unsigned char)(v >> 16);
  p[3] = (unsigned char)(v >> 24);
}

/* ── Metadata chunk FourCCs to drop ─────────────────────────────────────── */
static int chunk_is_meta(const unsigned char *fourcc) {
  return memcmp(fourcc, "EXIF", 4) == 0 || memcmp(fourcc, "XMP ", 4) == 0 ||
         memcmp(fourcc, "ICCP", 4) == 0;
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

/* ── Validate RIFF/WEBP header ───────────────────────────────────────────── */
static int is_webp(const unsigned char *buf, size_t sz) {
  return sz >= 12 && memcmp(buf, "RIFF", 4) == 0 &&
         memcmp(buf + 8, "WEBP", 4) == 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * PUBLIC: webp_print_info
 * ════════════════════════════════════════════════════════════════════════════
 */
int webp_print_info(const char *path, int verbose) {
  size_t file_size = 0;
  unsigned char *buf = load_file(path, &file_size);
  if (!buf) {
    fprintf(stderr, "Error: cannot open '%s'.\n", path);
    return -1;
  }
  if (!is_webp(buf, file_size)) {
    fprintf(stderr, "Error: '%s' is not a valid WebP.\n", path);
    free(buf);
    return -1;
  }

  int has_exif = 0, has_xmp = 0, has_icc = 0;
  const char *subtype = "VP8 (lossy)";
  int width = 0, height = 0;

  WebPBitstreamFeatures feat;
  if (WebPGetFeatures(buf, (size_t)file_size, &feat) == VP8_STATUS_OK) {
    width = feat.width;
    height = feat.height;
    if (feat.has_alpha)
      subtype = "VP8X (with alpha)";
  }

  size_t pos = 12;
  while (pos + 8 <= file_size) {
    unsigned int clen = read_le32(buf + pos + 4);
    if (memcmp(buf + pos, "EXIF", 4) == 0)
      has_exif = 1;
    if (memcmp(buf + pos, "XMP ", 4) == 0)
      has_xmp = 1;
    if (memcmp(buf + pos, "ICCP", 4) == 0)
      has_icc = 1;
    if (memcmp(buf + pos, "VP8L", 4) == 0)
      subtype = "VP8L (lossless)";
    pos += 8 + clen + (clen & 1);
  }
  free(buf);

  double mb = (double)file_size / 1048576.0;
  printf("\n+==============================================+\n");
  printf("|          WebP Image Report                  |\n");
  printf("+==============================================+\n");
  printf("| File    : %-33s|\n", path);
  printf("| Size    : %.2f MB                           |\n", mb);
  printf("| Format  : %-33s|\n", subtype);
  if (width && height)
    printf("| Image   : %d x %d px%*s|\n", width, height,
           (int)(27 - snprintf(NULL, 0, "%d x %d px", width, height)), " ");
  printf("+----------------------------------------------+\n");
  if (!has_exif && !has_xmp && !has_icc) {
    printf("| Metadata : None — already clean.             |\n");
  } else {
    printf("| Metadata found:                              |\n");
    if (has_exif)
      printf("|   EXIF data                                  |\n");
    if (has_xmp)
      printf("|   XMP metadata                               |\n");
    if (has_icc)
      printf("|   ICC color profile                          |\n");
    printf("+----------------------------------------------+\n");
    printf("| Run: ghostim clean <file> to remove all     |\n");
  }
  printf("+----------------------------------------------+\n\n");
  (void)verbose;
  return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Lossless path — RIFF chunk walk, drop metadata
 * ════════════════════════════════════════════════════════════════════════════
 */
static int webp_lossless_strip(const char *src, const char *dst, int dry_run,
                               int verbose) {
  size_t file_size = 0;
  unsigned char *buf = load_file(src, &file_size);
  if (!buf) {
    fprintf(stderr, "Error: cannot open '%s'.\n", src);
    return -1;
  }
  if (!is_webp(buf, file_size)) {
    fprintf(stderr, "Error: '%s' is not a valid WebP.\n", src);
    free(buf);
    return -1;
  }

  /* worst case same size */
  unsigned char *out = (unsigned char *)malloc(file_size);
  if (!out) {
    free(buf);
    return -1;
  }
  size_t out_size = 0;

  /* RIFF header placeholder */
  memcpy(out, "RIFF", 4);
  out_size += 8;
  memcpy(out + 8, "WEBP", 4);
  out_size += 4;

  long removed = 0;
  int removed_n = 0;
  size_t pos = 12;
  while (pos + 8 <= file_size) {
    unsigned int clen = read_le32(buf + pos + 4);
    size_t padded = clen + (clen & 1);
    if (chunk_is_meta(buf + pos)) {
      if (verbose)
        printf("  [remove] %.4s  %u bytes\n", buf + pos, 8 + (unsigned)padded);
      removed += 8 + (long)padded;
      removed_n++;
    } else {
      size_t total = 8 + padded;
      if (out_size + total > file_size)
        break;
      memcpy(out + out_size, buf + pos, total);
      out_size += total;
    }
    pos += 8 + padded;
  }
  free(buf);

  /* patch RIFF size */
  write_le32(out + 4, (unsigned int)(out_size - 8));

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
 * Lossy path — decode → re-encode at target quality
 * ════════════════════════════════════════════════════════════════════════════
 */
static int webp_reencode(const char *src, const char *dst, int quality,
                         int dry_run, int verbose) {
  size_t file_size = 0;
  unsigned char *buf = load_file(src, &file_size);
  if (!buf) {
    fprintf(stderr, "Error: cannot open '%s'.\n", src);
    return -1;
  }

  int width = 0, height = 0;
  unsigned char *pixels =
      WebPDecodeRGB(buf, (size_t)file_size, &width, &height);
  free(buf);
  if (!pixels) {
    fprintf(stderr, "Error: cannot decode '%s'.\n", src);
    return -1;
  }

  if (dry_run) {
    printf("[dry-run] lossy  %s  (quality=%d)\n", src, quality);
    printf("          Would re-encode %dx%d WebP\n", width, height);
    WebPFree(pixels);
    return 0;
  }

  uint8_t *out_buf = NULL;
  size_t out_size = 0;
  if (quality >= 100)
    out_size =
        WebPEncodeLosslessRGB(pixels, width, height, width * 3, &out_buf);
  else
    out_size = (size_t)WebPEncodeRGB(pixels, width, height, width * 3,
                                     (float)quality, &out_buf);
  WebPFree(pixels);

  if (!out_buf || out_size == 0) {
    fprintf(stderr, "Error: WebP encode failed for '%s'.\n", src);
    return -1;
  }

  double before_kb = (double)file_size / 1024.0;
  double after_kb = (double)out_size / 1024.0;
  double pct = before_kb > 0 ? (before_kb - after_kb) / before_kb * 100.0 : 0.0;
  if (verbose)
    printf("  WebP re-encode quality=%d: %.1f KB → %.1f KB (%.1f%% smaller)\n",
           quality, before_kb, after_kb, pct);

  int r = atomic_write(dst, out_buf, out_size);
  WebPFree(out_buf);
  return r;
}

/* ════════════════════════════════════════════════════════════════════════════
 * PUBLIC: webp_clean — dispatcher
 * ════════════════════════════════════════════════════════════════════════════
 */
int webp_clean(const char *src, const char *dst, OptMode opt_mode, int quality,
               int dry_run, int verbose) {
  if (opt_mode == OPT_LOSSY)
    return webp_reencode(src, dst, quality, dry_run, verbose);
  return webp_lossless_strip(src, dst, dry_run, verbose);
}
