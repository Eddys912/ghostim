/*
 * jpeg_parser.c — JPEG metadata removal and optimization.
 *
 * Lossless path: walks raw JPEG markers, drops non-image segments.
 *   No decode/encode — pixel data copied byte-for-byte.
 *
 * Lossy path: libjpeg-turbo decode → strip metadata → re-encode
 *   at target quality. Achieves maximum compression.
 */

#include "ghostim/jpeg_parser.h"
#include "ghostim/endian.h"
#include "ghostim/io.h"
#include "ghostim/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jerror.h>
#include <jpeglib.h>

/* ── JPEG marker constants ───────────────────────────────────────────────── */
#define MARKER_SOI 0xD8
#define MARKER_EOI 0xD9
#define MARKER_APP0 0xE0
#define MARKER_APP1 0xE1
#define MARKER_APP2 0xE2 /* ICC profile — always keep */
#define MARKER_COM 0xFE
#define MARKER_SOS 0xDA

/* ── TIFF/EXIF constants ─────────────────────────────────────────────────── */
#define TAG_GPS_IFD 0x8825

/* ── TIFF context ────────────────────────────────────────────────────────── */
typedef struct {
  unsigned char *data;
  size_t size;
  int le;
} TiffCtx;

static unsigned short tu16(const TiffCtx *c, unsigned int o) {
  if (o + 2 > c->size)
    return 0;
  return c->le ? read_le16(c->data + o) : read_be16(c->data + o);
}
static unsigned int tu32(const TiffCtx *c, unsigned int o) {
  if (o + 4 > c->size)
    return 0;
  return c->le ? read_le32(c->data + o) : read_be32(c->data + o);
}

/* ── EXIF info struct ────────────────────────────────────────────────────── */
typedef struct {
  char make[128], model[128], datetime[32], software[128];
  unsigned int width, height;
  int has_gps;
  double gps_lat, gps_lon;
  char gps_lat_ref[4], gps_lon_ref[4];
} ExifInfo;

/* ── Read null-terminated ASCII tag ─────────────────────────────────────── */
static void tiff_ascii(const TiffCtx *c, unsigned int off, unsigned int cnt,
                       char *buf, size_t sz) {
  size_t n = cnt < sz ? cnt : sz - 1;
  if (off + n > c->size) {
    buf[0] = '\0';
    return;
  }
  memcpy(buf, c->data + off, n);
  buf[n] = '\0';
}

/* ── GPS DMS → decimal ───────────────────────────────────────────────────── */
static double gps_decimal(const TiffCtx *c, unsigned int off,
                          unsigned int cnt) {
  if (cnt < 3)
    return 0.0;
  double deg = (double)tu32(c, off) / (double)tu32(c, off + 4);
  double min = (double)tu32(c, off + 8) / (double)tu32(c, off + 12);
  double sec = (double)tu32(c, off + 16) / (double)tu32(c, off + 20);
  return deg + min / 60.0 + sec / 3600.0;
}

/* ── Tag value offset ────────────────────────────────────────────────────── */
static unsigned int tag_voff(const TiffCtx *c, unsigned int eoff,
                             unsigned short type, unsigned int cnt) {
  static const unsigned int tsz[] = {0, 1, 1, 2, 4, 8};
  unsigned int sz = (type < 6 ? tsz[type] : 1) * cnt;
  return sz <= 4 ? eoff + 8 : tu32(c, eoff + 8);
}

/* ── Walk GPS IFD ────────────────────────────────────────────────────────── */
static void walk_gps(const TiffCtx *c, unsigned int off, ExifInfo *info) {
  if (off + 2 > c->size)
    return;
  unsigned short n = tu16(c, off);
  off += 2;
  for (unsigned short i = 0; i < n; i++, off += 12) {
    if (off + 12 > c->size)
      break;
    unsigned short tag = tu16(c, off);
    unsigned short type = tu16(c, off + 2);
    unsigned int cnt = tu32(c, off + 4);
    unsigned int vo = tag_voff(c, off, type, cnt);
    switch (tag) {
    case 0x0001:
      tiff_ascii(c, vo, cnt, info->gps_lat_ref, sizeof(info->gps_lat_ref));
      break;
    case 0x0002:
      info->gps_lat = gps_decimal(c, vo, cnt);
      info->has_gps = 1;
      break;
    case 0x0003:
      tiff_ascii(c, vo, cnt, info->gps_lon_ref, sizeof(info->gps_lon_ref));
      break;
    case 0x0004:
      info->gps_lon = gps_decimal(c, vo, cnt);
      break;
    }
  }
}

/* ── Walk IFD0 ───────────────────────────────────────────────────────────── */
static void walk_ifd0(const TiffCtx *c, unsigned int off, ExifInfo *info) {
  if (off + 2 > c->size)
    return;
  unsigned short n = tu16(c, off);
  off += 2;
  for (unsigned short i = 0; i < n; i++, off += 12) {
    if (off + 12 > c->size)
      break;
    unsigned short tag = tu16(c, off);
    unsigned short type = tu16(c, off + 2);
    unsigned int cnt = tu32(c, off + 4);
    unsigned int vo = tag_voff(c, off, type, cnt);
    switch (tag) {
    case 0x010F:
      tiff_ascii(c, vo, cnt, info->make, sizeof(info->make));
      break;
    case 0x0110:
      tiff_ascii(c, vo, cnt, info->model, sizeof(info->model));
      break;
    case 0x0132:
      tiff_ascii(c, vo, cnt, info->datetime, sizeof(info->datetime));
      break;
    case 0x0131:
      tiff_ascii(c, vo, cnt, info->software, sizeof(info->software));
      break;
    case 0x0100:
      info->width = (type == 3) ? tu16(c, vo) : tu32(c, vo);
      break;
    case 0x0101:
      info->height = (type == 3) ? tu16(c, vo) : tu32(c, vo);
      break;
    case TAG_GPS_IFD:
      walk_gps(c, tu32(c, off + 8), info);
      break;
    }
  }
}

/* ── Parse APP1 EXIF segment ─────────────────────────────────────────────── */
static int parse_exif(const unsigned char *seg, size_t len, ExifInfo *info) {
  if (len < 6 || memcmp(seg, "Exif\0\0", 6) != 0)
    return -1;
  TiffCtx c;
  c.data = (unsigned char *)seg + 6;
  c.size = len - 6;
  if (c.size < 8)
    return -1;
  if (c.data[0] == 'I' && c.data[1] == 'I')
    c.le = 1;
  else if (c.data[0] == 'M' && c.data[1] == 'M')
    c.le = 0;
  else
    return -1;
  walk_ifd0(&c, tu32(&c, 4), info);
  return 0;
}

/* ── Zero out GPS SubIFD pointer inside APP1 (--strip gps) ──────────────── */
static void strip_gps(unsigned char *seg, size_t len) {
  if (len < 6 || memcmp(seg, "Exif\0\0", 6) != 0)
    return;
  TiffCtx c;
  c.data = seg + 6;
  c.size = len - 6;
  if (c.size < 8)
    return;
  c.le = (c.data[0] == 'I');
  unsigned int off = tu32(&c, 4);
  if (off + 2 > c.size)
    return;
  unsigned short n = tu16(&c, off);
  off += 2;
  for (unsigned short i = 0; i < n; i++, off += 12) {
    if (off + 12 > c.size)
      break;
    if (tu16(&c, off) == TAG_GPS_IFD) {
      c.data[off] = 0x00;
      c.data[off + 1] = 0x00;
    }
  }
}

/* ── Segment drop logic ──────────────────────────────────────────────────── */
static int seg_drop(unsigned char marker) {
  if (marker == MARKER_APP1)
    return 1; /* EXIF/XMP       */
  if (marker == MARKER_APP0)
    return 1; /* JFIF header    */
  if (marker == MARKER_COM)
    return 1; /* Comments       */
  if (marker >= 0xE3 && marker <= 0xEF)
    return 1; /* APP3-APPF */
  return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * PUBLIC: jpeg_print_info
 * ════════════════════════════════════════════════════════════════════════════
 */
int jpeg_print_info(const char *path, int verbose) {
  size_t file_size = 0;
  unsigned char *buf = platform_load_file(path, &file_size);
  if (!buf) {
    fprintf(stderr, "Error: cannot open '%s'.\n", path);
    return -1;
  }
  if (file_size < 2 || buf[0] != 0xFF || buf[1] != MARKER_SOI) {
    fprintf(stderr, "Error: '%s' is not a valid JPEG.\n", path);
    free(buf);
    return -1;
  }

  ExifInfo info;
  memset(&info, 0, sizeof(info));
  int found_exif = 0, found_jfif = 0;
  unsigned int sof_w = 0, sof_h = 0;

  size_t pos = 2;
  while (pos + 3 < file_size) {
    if (buf[pos] != 0xFF)
      break;
    unsigned char m = buf[pos + 1];
    if (m == MARKER_SOI) {
      pos += 2;
      continue;
    }
    if (m == MARKER_EOI)
      break;
    if (m == 0xFF) {
      pos++;
      continue;
    }
    unsigned short slen = read_be16(buf + pos + 2);
    if (slen < 2)
      break;
    unsigned char *sd = buf + pos + 4;
    size_t sb = (size_t)(slen - 2);
    if (m == MARKER_APP0 && sb >= 4 && memcmp(sd, "JFIF", 4) == 0)
      found_jfif = 1;
    if (m == MARKER_APP1 && parse_exif(sd, sb, &info) == 0)
      found_exif = 1;
    if ((m >= 0xC0 && m <= 0xC3) || (m >= 0xC5 && m <= 0xC7) ||
        (m >= 0xC9 && m <= 0xCB) || (m >= 0xCD && m <= 0xCF)) {
      if (sb >= 5) {
        sof_h = read_be16(sd + 1);
        sof_w = read_be16(sd + 3);
      }
    }
    if (m == MARKER_SOS)
      break;
    pos += 2 + (size_t)slen;
  }
  free(buf);

  double mb = (double)file_size / 1048576.0;
  printf("\n+==============================================+\n");
  printf("|          JPEG Image Report                  |\n");
  printf("+==============================================+\n");
  printf("| File   : %-34s|\n", path);
  printf("| Size   : %.2f MB                            |\n", mb);
  printf("| Format : %-34s|\n", found_jfif   ? "JFIF (standard JPEG)"
                                : found_exif ? "EXIF JPEG"
                                             : "JPEG");
  if (sof_w && sof_h)
    printf("| Image  : %u x %u px%*s|\n", sof_w, sof_h,
           (int)(27 - snprintf(NULL, 0, "%u x %u px", sof_w, sof_h)), " ");
  printf("+----------------------------------------------+\n");
  if (!found_exif) {
    printf("| Metadata : None — already clean.             |\n");
  } else {
    printf("| Metadata found:                              |\n");
    if (info.make[0])
      printf("|   Make     : %-30s|\n", info.make);
    if (info.model[0])
      printf("|   Model    : %-30s|\n", info.model);
    if (info.datetime[0])
      printf("|   DateTime : %-30s|\n", info.datetime);
    if (info.software[0])
      printf("|   Software : %-30s|\n", info.software);
    if (info.has_gps) {
      printf("+----------------------------------------------+\n");
      printf("| !! GPS LOCATION DATA FOUND !!                |\n");
      printf("|   Lat : %8.4f %s  Lon : %8.4f %s%*s|\n", info.gps_lat,
             info.gps_lat_ref, info.gps_lon, info.gps_lon_ref,
             (int)(2 - snprintf(NULL, 0, "%s", info.gps_lon_ref)), " ");
    }
    printf("+----------------------------------------------+\n");
    printf("| Run: ghostim clean <file> to remove all     |\n");
  }
  printf("+----------------------------------------------+\n\n");
  (void)verbose;
  return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * PUBLIC: jpeg_clean — lossless path
 * ════════════════════════════════════════════════════════════════════════════
 */
static int jpeg_lossless(const char *src, const char *dst, StripMode strip_mode,
                         int dry_run, int verbose) {
  size_t file_size = 0;
  unsigned char *buf = platform_load_file(src, &file_size);
  if (!buf) {
    fprintf(stderr, "Error: cannot open '%s'.\n", src);
    return -1;
  }
  if (file_size < 2 || buf[0] != 0xFF || buf[1] != MARKER_SOI) {
    fprintf(stderr, "Error: '%s' is not a valid JPEG.\n", src);
    free(buf);
    return -1;
  }

  size_t out_cap = file_size, out_size = 0;
  unsigned char *out = (unsigned char *)malloc(out_cap);
  if (!out) {
    free(buf);
    return -1;
  }

  APPEND(out, out_size, out_cap, buf, 2); /* SOI */

  long removed = 0;
  int removed_n = 0;
  size_t pos = 2;
  while (pos + 3 < file_size) {
    if (buf[pos] != 0xFF) {
      APPEND(out, out_size, out_cap, buf + pos, file_size - pos);
      break;
    }
    unsigned char m = buf[pos + 1];
    if (m == MARKER_EOI) {
      APPEND(out, out_size, out_cap, buf + pos, 2);
      break;
    }
    if (m == MARKER_SOI) {
      APPEND(out, out_size, out_cap, buf + pos, 2);
      pos += 2;
      continue;
    }
    if (m == 0xFF) {
      pos++;
      continue;
    }
    if (pos + 3 >= file_size)
      break;
    unsigned short slen = read_be16(buf + pos + 2);
    if (slen < 2)
      break;

    int drop = seg_drop(m);
    if (m == MARKER_APP1 && strip_mode == STRIP_GPS)
      drop = 0;

    if (drop) {
      if (verbose)
        printf("  [remove] FF%02X  %u bytes\n", m, (unsigned)(2 + slen));
      removed += 2 + slen;
      removed_n++;
    } else if (m == MARKER_APP1 && strip_mode == STRIP_GPS) {
      APPEND(out, out_size, out_cap, buf + pos, 4);
      size_t bs = out_size;
      size_t bl = (size_t)(slen - 2);
      APPEND(out, out_size, out_cap, buf + pos + 4, bl);
      strip_gps(out + bs, bl);
      if (verbose)
        printf("  [patch]  GPS removed from APP1\n");
    } else {
      APPEND(out, out_size, out_cap, buf + pos, 2 + (size_t)slen);
    }
    if (m == MARKER_SOS) {
      pos += 2 + (size_t)slen;
      APPEND(out, out_size, out_cap, buf + pos, file_size - pos);
      break;
    }
    pos += 2 + (size_t)slen;
  }
  free(buf);

  double saved_kb = (double)removed / 1024.0;
  double pct =
      file_size > 0 ? (double)removed / (double)file_size * 100.0 : 0.0;
  if (dry_run) {
    printf("[dry-run] lossless  %s\n", src);
    printf("          Would remove %d segment(s), saving %.1f KB (%.1f%%)\n",
           removed_n, saved_kb, pct);
    free(out);
    return 0;
  }
  if (verbose)
    printf("  Removed %d segment(s), saved %.1f KB (%.1f%%)\n", removed_n,
           saved_kb, pct);

  int r = platform_atomic_write(dst, out, out_size);
  free(out);
  return r;
}

/* ════════════════════════════════════════════════════════════════════════════
 * PUBLIC: jpeg_clean — lossy path (libjpeg-turbo)
 * ════════════════════════════════════════════════════════════════════════════
 */
static int jpeg_lossy(const char *src, const char *dst, int quality,
                      int dry_run, int verbose) {
  /* ── Decode ── */
  struct jpeg_decompress_struct dec;
  struct jpeg_error_mgr jerr_dec;
  dec.err = jpeg_std_error(&jerr_dec);
  jpeg_create_decompress(&dec);

  FILE *fin = fopen(src, "rb");
  if (!fin) {
    fprintf(stderr, "Error: cannot open '%s'.\n", src);
    return -1;
  }
  jpeg_stdio_src(&dec, fin);
  jpeg_read_header(&dec, TRUE);

  dec.out_color_space = JCS_RGB;
  jpeg_start_decompress(&dec);

  int width = (int)dec.output_width;
  int height = (int)dec.output_height;
  int components = dec.output_components; /* 3 for RGB */
  size_t row_stride = (size_t)(width * components);
  size_t img_size = row_stride * (size_t)height;

  unsigned char *pixels = (unsigned char *)malloc(img_size);
  if (!pixels) {
    jpeg_destroy_decompress(&dec);
    fclose(fin);
    return -1;
  }

  while (dec.output_scanline < dec.output_height) {
    unsigned char *row = pixels + dec.output_scanline * row_stride;
    jpeg_read_scanlines(&dec, &row, 1);
  }
  jpeg_finish_decompress(&dec);
  jpeg_destroy_decompress(&dec);
  fclose(fin);

  if (dry_run) {
    printf("[dry-run] lossy  %s  (quality=%d)\n", src, quality);
    printf("          Would re-encode %dx%d image — actual size depends on "
           "content\n",
           width, height);
    free(pixels);
    return 0;
  }

  /* ── Encode ── */
  struct jpeg_compress_struct enc;
  struct jpeg_error_mgr jerr_enc;
  enc.err = jpeg_std_error(&jerr_enc);
  jpeg_create_compress(&enc);

  unsigned char *out_buf = NULL;
  unsigned long out_size = 0;
  jpeg_mem_dest(&enc, &out_buf, &out_size);

  enc.image_width = (JDIMENSION)width;
  enc.image_height = (JDIMENSION)height;
  enc.input_components = components;
  enc.in_color_space = JCS_RGB;
  jpeg_set_defaults(&enc);
  jpeg_set_quality(&enc, quality, TRUE);

  /* 4:2:0 below quality 90, 4:4:4 above — better detail at high quality */
  if (quality >= 90) {
    enc.comp_info[0].h_samp_factor = 1;
    enc.comp_info[0].v_samp_factor = 1;
  }

  jpeg_start_compress(&enc, TRUE);
  while (enc.next_scanline < enc.image_height) {
    unsigned char *row = pixels + enc.next_scanline * row_stride;
    jpeg_write_scanlines(&enc, &row, 1);
  }
  jpeg_finish_compress(&enc);
  jpeg_destroy_compress(&enc);
  free(pixels);

  double before_kb = 0.0;
  { /* get original size for report */
    FILE *tmp = fopen(src, "rb");
    if (tmp) {
      fseek(tmp, 0, SEEK_END);
      before_kb = (double)ftell(tmp) / 1024.0;
      fclose(tmp);
    }
  }
  double after_kb = (double)out_size / 1024.0;
  double saved_pct =
      before_kb > 0 ? (before_kb - after_kb) / before_kb * 100.0 : 0.0;

  if (verbose)
    printf("  Lossy re-encode quality=%d: %.1f KB → %.1f KB (%.1f%% smaller)\n",
           quality, before_kb, after_kb, saved_pct);

  int r = platform_atomic_write(dst, out_buf, (size_t)out_size);
  free(out_buf);
  return r;
}

/* ════════════════════════════════════════════════════════════════════════════
 * PUBLIC: jpeg_clean — dispatcher
 * ════════════════════════════════════════════════════════════════════════════
 */
int jpeg_clean(const char *src, const char *dst, StripMode strip_mode,
               OptMode opt_mode, int quality, int dry_run, int verbose) {
  if (opt_mode == OPT_LOSSY)
    return jpeg_lossy(src, dst, quality, dry_run, verbose);
  return jpeg_lossless(src, dst, strip_mode, dry_run, verbose);
}
