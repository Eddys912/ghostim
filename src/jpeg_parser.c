/*
 * JPEG / EXIF parser — pure C, zero external dependencies.
 *
 * JPEG structure:
 *   SOI  (FF D8)
 *   APP0 (FF E0) — JFIF
 *   APP1 (FF E1) — EXIF  <── we target this
 *   ...
 *   EOI  (FF D9)
 *
 * Every segment begins with a 2-byte marker followed by a 2-byte
 * big-endian length that includes the length field itself.
 *
 * EXIF data inside APP1:
 *   6 bytes "Exif\0\0"
 *   TIFF header (byte order mark + offset to first IFD)
 *   IFD0 → may contain GPS SubIFD (tag 0x8825)
 */

#include "ghostim/jpeg_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* ── Endian helpers ──────────────────────────────────────────────────────── */

static unsigned short read_be16(const unsigned char *p) {
  return (unsigned short)((p[0] << 8) | p[1]);
}

static unsigned short read_le16(const unsigned char *p) {
  return (unsigned short)((p[1] << 8) | p[0]);
}

static unsigned int read_be32(const unsigned char *p) {
  return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
         ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

static unsigned int read_le32(const unsigned char *p) {
  return ((unsigned int)p[3] << 24) | ((unsigned int)p[2] << 16) |
         ((unsigned int)p[1] << 8) | (unsigned int)p[0];
}

/* ── JPEG markers ────────────────────────────────────────────────────────── */
#define MARKER_SOI 0xD8
#define MARKER_EOI 0xD9
#define MARKER_APP0 0xE0
#define MARKER_APP1 0xE1
/* APP2..APPE also carry metadata sometimes, we preserve them unless STRIP_ALL
 */
#define MARKER_APP2 0xE2
#define MARKER_APPE 0xEE
#define MARKER_SOS 0xDA /* Start of Scan: everything after is compressed data  \
                         */

/* ── TIFF / EXIF types ───────────────────────────────────────────────────── */
#define TIFF_TYPE_BYTE 1
#define TIFF_TYPE_ASCII 2
#define TIFF_TYPE_SHORT 3
#define TIFF_TYPE_LONG 4
#define TIFF_TYPE_RATIONAL 5

static unsigned int tiff_type_size(unsigned short type) {
  switch (type) {
  case TIFF_TYPE_BYTE:
  case TIFF_TYPE_ASCII:
    return 1;
  case TIFF_TYPE_SHORT:
    return 2;
  case TIFF_TYPE_LONG:
    return 4;
  case TIFF_TYPE_RATIONAL:
    return 8;
  default:
    return 1;
  }
}

/* EXIF tag IDs we care about for the info display */
#define TAG_IMAGE_WIDTH 0x0100
#define TAG_IMAGE_HEIGHT 0x0101
#define TAG_MAKE 0x010F
#define TAG_MODEL 0x0110
#define TAG_SOFTWARE 0x0131
#define TAG_DATETIME 0x0132
#define TAG_EXIF_IFD 0x8769
#define TAG_GPS_IFD 0x8825
/* GPS sub-tags */
#define GPS_LATITUDE_REF 0x0001
#define GPS_LATITUDE 0x0002
#define GPS_LONGITUDE_REF 0x0003
#define GPS_LONGITUDE 0x0004

/* ── Internal structures ─────────────────────────────────────────────────── */

typedef struct {
  unsigned char *data; /* raw EXIF block (after "Exif\0\0") */
  size_t size;
  int little_endian; /* 1 = LE (Intel), 0 = BE (Motorola) */
} TiffCtx;

static unsigned short tiff_u16(const TiffCtx *ctx, unsigned int offset) {
  if (offset + 2 > ctx->size)
    return 0;
  return ctx->little_endian ? read_le16(ctx->data + offset)
                            : read_be16(ctx->data + offset);
}

static unsigned int tiff_u32(const TiffCtx *ctx, unsigned int offset) {
  if (offset + 4 > ctx->size)
    return 0;
  return ctx->little_endian ? read_le32(ctx->data + offset)
                            : read_be32(ctx->data + offset);
}

/* Read a null-terminated ASCII tag value into buf (max buf_size bytes). */
static void tiff_read_ascii(const TiffCtx *ctx, unsigned int offset,
                            unsigned int count, char *buf, size_t buf_size) {
  size_t n = (count < buf_size) ? count : buf_size - 1;
  if (offset + n > ctx->size) {
    buf[0] = '\0';
    return;
  }
  memcpy(buf, ctx->data + offset, n);
  buf[n] = '\0';
}

/* Read rational (numerator/denominator) as double. */
static double tiff_rational(const TiffCtx *ctx, unsigned int offset) {
  unsigned int num = tiff_u32(ctx, offset);
  unsigned int den = tiff_u32(ctx, offset + 4);
  return (den == 0) ? 0.0 : (double)num / (double)den;
}

/* ── GPS coordinate helper ───────────────────────────────────────────────── */

static double gps_dms_to_decimal(const TiffCtx *ctx, unsigned int offset,
                                 unsigned int count) {
  if (count < 3)
    return 0.0;
  double deg = tiff_rational(ctx, offset);
  double min = tiff_rational(ctx, offset + 8);
  double sec = tiff_rational(ctx, offset + 16);
  return deg + min / 60.0 + sec / 3600.0;
}

/* ── IFD tag value resolution ────────────────────────────────────────────── */

/*
 * For a tag entry at `entry_offset` in the TIFF block, return the absolute
 * offset of the actual value data. For values ≤4 bytes it's inline.
 */
static unsigned int tag_value_offset(const TiffCtx *ctx,
                                     unsigned int entry_offset,
                                     unsigned short type, unsigned int count) {
  unsigned int value_size = tiff_type_size(type) * count;
  if (value_size <= 4) {
    return entry_offset + 8; /* value is stored inline in the entry */
  }
  return tiff_u32(ctx, entry_offset + 8); /* value is at this offset */
}

/* ── Info structures ─────────────────────────────────────────────────────── */

typedef struct {
  char make[128];
  char model[128];
  char datetime[32];
  char software[128];
  unsigned int width;
  unsigned int height;
  int has_gps;
  double gps_lat;
  double gps_lon;
  char gps_lat_ref[4];
  char gps_lon_ref[4];
} ExifInfo;

/* ── IFD walker ──────────────────────────────────────────────────────────── */

static void walk_gps_ifd(const TiffCtx *ctx, unsigned int ifd_offset,
                         ExifInfo *info) {
  if (ifd_offset + 2 > ctx->size)
    return;

  unsigned short entry_count = tiff_u16(ctx, ifd_offset);
  unsigned int entry_offset = ifd_offset + 2;

  for (unsigned short i = 0; i < entry_count; i++, entry_offset += 12) {
    if (entry_offset + 12 > ctx->size)
      break;

    unsigned short tag = tiff_u16(ctx, entry_offset);
    unsigned short type = tiff_u16(ctx, entry_offset + 2);
    unsigned int count = tiff_u32(ctx, entry_offset + 4);
    unsigned int voff = tag_value_offset(ctx, entry_offset, type, count);

    switch (tag) {
    case GPS_LATITUDE_REF:
      tiff_read_ascii(ctx, voff, count, info->gps_lat_ref,
                      sizeof(info->gps_lat_ref));
      break;
    case GPS_LATITUDE:
      info->gps_lat = gps_dms_to_decimal(ctx, voff, count);
      info->has_gps = 1;
      break;
    case GPS_LONGITUDE_REF:
      tiff_read_ascii(ctx, voff, count, info->gps_lon_ref,
                      sizeof(info->gps_lon_ref));
      break;
    case GPS_LONGITUDE:
      info->gps_lon = gps_dms_to_decimal(ctx, voff, count);
      break;
    }
  }
}

static void walk_ifd0(const TiffCtx *ctx, unsigned int ifd_offset,
                      ExifInfo *info) {
  if (ifd_offset + 2 > ctx->size)
    return;

  unsigned short entry_count = tiff_u16(ctx, ifd_offset);
  unsigned int entry_offset = ifd_offset + 2;

  for (unsigned short i = 0; i < entry_count; i++, entry_offset += 12) {
    if (entry_offset + 12 > ctx->size)
      break;

    unsigned short tag = tiff_u16(ctx, entry_offset);
    unsigned short type = tiff_u16(ctx, entry_offset + 2);
    unsigned int count = tiff_u32(ctx, entry_offset + 4);
    unsigned int voff = tag_value_offset(ctx, entry_offset, type, count);

    switch (tag) {
    case TAG_MAKE:
      tiff_read_ascii(ctx, voff, count, info->make, sizeof(info->make));
      break;
    case TAG_MODEL:
      tiff_read_ascii(ctx, voff, count, info->model, sizeof(info->model));
      break;
    case TAG_DATETIME:
      tiff_read_ascii(ctx, voff, count, info->datetime, sizeof(info->datetime));
      break;
    case TAG_SOFTWARE:
      tiff_read_ascii(ctx, voff, count, info->software, sizeof(info->software));
      break;
    case TAG_IMAGE_WIDTH:
      info->width =
          (type == TIFF_TYPE_SHORT) ? tiff_u16(ctx, voff) : tiff_u32(ctx, voff);
      break;
    case TAG_IMAGE_HEIGHT:
      info->height =
          (type == TIFF_TYPE_SHORT) ? tiff_u16(ctx, voff) : tiff_u32(ctx, voff);
      break;
    case TAG_GPS_IFD:
      walk_gps_ifd(ctx, tiff_u32(ctx, entry_offset + 8), info);
      break;
    }
  }
}

/* ── Parse EXIF APP1 segment ─────────────────────────────────────────────── */

static int parse_app1_exif(const unsigned char *seg_data, size_t seg_len,
                           ExifInfo *info) {
  /* APP1 segment starts with "Exif\0\0" (6 bytes) */
  if (seg_len < 6)
    return -1;
  if (memcmp(seg_data, "Exif\0\0", 6) != 0)
    return -1;

  TiffCtx ctx;
  ctx.data = (unsigned char *)seg_data + 6;
  ctx.size = seg_len - 6;

  if (ctx.size < 8)
    return -1;

  /* TIFF byte order: "II" = little-endian, "MM" = big-endian */
  if (ctx.data[0] == 'I' && ctx.data[1] == 'I') {
    ctx.little_endian = 1;
  } else if (ctx.data[0] == 'M' && ctx.data[1] == 'M') {
    ctx.little_endian = 0;
  } else {
    return -1; /* not valid TIFF */
  }

  /* Offset to IFD0 is at bytes 4-7 in the TIFF block */
  unsigned int ifd0_offset = tiff_u32(&ctx, 4);
  walk_ifd0(&ctx, ifd0_offset, info);
  return 0;
}

/* ── Load entire file into a heap buffer ─────────────────────────────────── */

static unsigned char *load_file(const char *path, size_t *out_size) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long len = ftell(f);
  if (len <= 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);

  unsigned char *buf = (unsigned char *)malloc((size_t)len);
  if (!buf) {
    fclose(f);
    return NULL;
  }

  if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
    free(buf);
    fclose(f);
    return NULL;
  }
  fclose(f);
  *out_size = (size_t)len;
  return buf;
}

/* ── Public: jpeg_print_info ─────────────────────────────────────────────── */

int jpeg_print_info(const char *path, int verbose) {
  size_t file_size = 0;
  unsigned char *buf = load_file(path, &file_size);
  if (!buf) {
    fprintf(stderr, "Error: cannot open '%s'.\n", path);
    return -1;
  }

  /* Verify SOI */
  if (file_size < 2 || buf[0] != 0xFF || buf[1] != MARKER_SOI) {
    fprintf(stderr, "Error: '%s' is not a valid JPEG file.\n", path);
    free(buf);
    return -1;
  }

  ExifInfo info;
  memset(&info, 0, sizeof(info));
  int found_exif = 0;
  int found_jfif = 0;
  unsigned int sof_width = 0, sof_height = 0;

  /* Walk markers */
  size_t pos = 2;
  while (pos + 3 < file_size) {
    if (buf[pos] != 0xFF)
      break;

    unsigned char marker = buf[pos + 1];

    if (marker == MARKER_SOI) {
      pos += 2;
      continue;
    }
    if (marker == MARKER_EOI)
      break;

    /* Pad bytes (FF FF ... before real marker) */
    if (marker == 0xFF) {
      pos++;
      continue;
    }

    unsigned short seg_len = read_be16(buf + pos + 2);
    if (seg_len < 2)
      break;

    unsigned char *seg_data = buf + pos + 4;
    size_t seg_body = (size_t)(seg_len - 2);

    /* APP0 = JFIF marker */
    if (marker == MARKER_APP0 && seg_body >= 5 &&
        memcmp(seg_data, "JFIF", 4) == 0) {
      found_jfif = 1;
    }

    if (marker == MARKER_APP1) {
      if (parse_app1_exif(seg_data, seg_body, &info) == 0)
        found_exif = 1;
    }

    /* SOF markers (C0..C3, C5..C7, C9..CB, CD..CF) carry image dimensions */
    if ((marker >= 0xC0 && marker <= 0xC3) ||
        (marker >= 0xC5 && marker <= 0xC7) ||
        (marker >= 0xC9 && marker <= 0xCB) ||
        (marker >= 0xCD && marker <= 0xCF)) {
      if (seg_body >= 5) {
        /* SOF: 1 byte precision, 2 bytes height, 2 bytes width */
        sof_height = read_be16(seg_data + 1);
        sof_width = read_be16(seg_data + 3);
      }
    }

    if (marker == MARKER_SOS)
      break;

    pos += 2 + (size_t)seg_len;
  }

  free(buf);

  /* ── Print report ── */
  double size_mb = (double)file_size / (1024.0 * 1024.0);
  printf("\n");
  printf("+==============================================+\n");
  printf("|          JPEG Image Report                  |\n");
  printf("+==============================================+\n");
  printf("| File   : %-34s|\n", path);
  printf("| Size   : %.2f MB                            |\n", size_mb);
  printf("| Format : %-34s|\n", found_jfif   ? "JFIF (standard JPEG)"
                                : found_exif ? "EXIF JPEG"
                                             : "JPEG");
  if (sof_width && sof_height)
    printf("| Image  : %u x %u px%*s|\n", sof_width, sof_height,
           (int)(27 - snprintf(NULL, 0, "%u x %u px", sof_width, sof_height)),
           " ");
  printf("+----------------------------------------------+\n");

  if (!found_exif) {
    printf("| Metadata : None — this image is already      |\n");
    printf("|            clean. No action needed.          |\n");
    printf("+----------------------------------------------+\n\n");
    return 0;
  }

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
    printf("|   Latitude  : %8.4f %s%*s|\n", info.gps_lat, info.gps_lat_ref,
           (int)(21 -
                 snprintf(NULL, 0, "%8.4f %s", info.gps_lat, info.gps_lat_ref)),
           " ");
    printf("|   Longitude : %8.4f %s%*s|\n", info.gps_lon, info.gps_lon_ref,
           (int)(21 -
                 snprintf(NULL, 0, "%8.4f %s", info.gps_lon, info.gps_lon_ref)),
           " ");
    printf("|   Run: ghostim clean <file> --strip gps     |\n");
  }

  printf("+----------------------------------------------+\n");
  printf("| Run: ghostim clean <file> to remove all     |\n");
  printf("+----------------------------------------------+\n\n");

  (void)verbose;
  return 0;
}

/* ── Segment filtering logic ─────────────────────────────────────────────── */

/*
 * JPEG marker reference for filtering decisions:
 *
 *  APP0  (E0) — JFIF header. Safe to remove in --optimize (no image data).
 *  APP1  (E1) — EXIF / XMP. Always removed (contains privacy metadata).
 *  APP2  (E2) — ICC color profile or FlashPix. KEEP — affects color rendering.
 *  APP3..APPE — Various vendor extensions. Removed in --optimize.
 *  APPF  (EF) — Usually empty or vendor. Removed in --optimize.
 *  COM   (FE) — Text comment. Removed in --optimize.
 *  DQT   (DB) — Quantization tables. KEEP — required for decoding.
 *  DHT   (C4) — Huffman tables. KEEP — required for decoding.
 *  SOF*  (C0-CF) — Start of Frame. KEEP — image dimensions/format.
 *  SOS   (DA) — Start of Scan. KEEP — compressed image data follows.
 *  EOI   (D9) — End of image. KEEP.
 *
 * clean always drops all non-image segments:
 * APP0, APP1, APP3-APPF, COM. Keeps APP2 (ICC), DQT, DHT, SOF*, SOS, EOI.
 *                 Keep APP2 (ICC profile), DQT, DHT, SOF*, SOS, EOI.
 */

#define MARKER_COM 0xFE /* Comment segment */

static int segment_should_drop(unsigned char marker) {
  if (marker == MARKER_APP1)
    return 1; /* EXIF / XMP       */
  if (marker == MARKER_APP0)
    return 1; /* JFIF header      */
  if (marker == MARKER_COM)
    return 1; /* Text comments    */
  if (marker >= 0xE3 && marker <= 0xEF)
    return 1; /* APP3..APPF       */
  return 0;
}

/*
 * Zero out the GPS SubIFD pointer inside an APP1/EXIF segment (--strip gps).
 * seg_data points to the raw segment body (after marker+length bytes).
 */
static void strip_gps_from_app1(unsigned char *seg_data, size_t seg_len) {
  if (seg_len < 6 || memcmp(seg_data, "Exif\0\0", 6) != 0)
    return;

  TiffCtx ctx;
  ctx.data = seg_data + 6;
  ctx.size = seg_len - 6;
  if (ctx.size < 8)
    return;

  ctx.little_endian = (ctx.data[0] == 'I');

  unsigned int ifd0_offset = tiff_u32(&ctx, 4);
  if (ifd0_offset + 2 > ctx.size)
    return;

  unsigned short entry_count = tiff_u16(&ctx, ifd0_offset);
  unsigned int entry_off = ifd0_offset + 2;

  for (unsigned short i = 0; i < entry_count; i++, entry_off += 12) {
    if (entry_off + 12 > ctx.size)
      break;
    unsigned short tag = tiff_u16(&ctx, entry_off);
    if (tag == TAG_GPS_IFD) {
      ctx.data[entry_off] = 0x00;
      ctx.data[entry_off + 1] = 0x00;
    }
  }
}

/* ── Public: jpeg_clean ──────────────────────────────────────────────────── */

int jpeg_clean(const char *src, const char *dst, StripMode strip_mode,
               int dry_run, int verbose) {
  size_t file_size = 0;
  unsigned char *buf = load_file(src, &file_size);
  if (!buf) {
    fprintf(stderr, "Error: cannot open '%s'.\n", src);
    return -1;
  }

  if (file_size < 2 || buf[0] != 0xFF || buf[1] != MARKER_SOI) {
    fprintf(stderr, "Error: '%s' is not a valid JPEG.\n", src);
    free(buf);
    return -1;
  }

  /* Build output buffer — starts at file_size capacity, shrinks as we drop */
  size_t out_cap = file_size;
  size_t out_size = 0;
  unsigned char *out = (unsigned char *)malloc(out_cap);
  if (!out) {
    free(buf);
    return -1;
  }

#define APPEND(ptr, n)                                                         \
  do {                                                                         \
    size_t _n = (n);                                                           \
    if (out_size + _n > out_cap) {                                             \
      out_cap = (out_size + _n) * 2;                                           \
      unsigned char *_r = (unsigned char *)realloc(out, out_cap);              \
      if (!_r) {                                                               \
        free(buf);                                                             \
        free(out);                                                             \
        return -1;                                                             \
      }                                                                        \
      out = _r;                                                                \
    }                                                                          \
    memcpy(out + out_size, (ptr), _n);                                         \
    out_size += _n;                                                            \
  } while (0)

  /* Always keep SOI */
  APPEND(buf, 2);

  long removed_bytes = 0;
  int removed_count = 0;

  size_t pos = 2;
  while (pos + 3 < file_size) {
    if (buf[pos] != 0xFF) {
      /* Lost sync or raw entropy data outside SOS — copy remainder */
      APPEND(buf + pos, file_size - pos);
      break;
    }

    unsigned char marker = buf[pos + 1];

    /* Markers with no payload */
    if (marker == MARKER_SOI) {
      APPEND(buf + pos, 2);
      pos += 2;
      continue;
    }
    if (marker == MARKER_EOI) {
      APPEND(buf + pos, 2);
      break;
    }
    /* Skip pad bytes (0xFF stuffing) */
    if (marker == 0xFF) {
      pos++;
      continue;
    }

    if (pos + 3 >= file_size)
      break;
    unsigned short seg_len = read_be16(buf + pos + 2);
    if (seg_len < 2)
      break;

    int drop = segment_should_drop(marker);

    /* --strip gps: keep APP1 but patch GPS pointer out */
    if (marker == MARKER_APP1 && strip_mode == STRIP_GPS) {
      drop = 0; /* override: keep the segment, just patch it */
    }

    if (drop) {
      if (verbose)
        printf("  [remove] marker=FF%02X  size=%u bytes\n", marker,
               (unsigned)(2 + seg_len));
      removed_bytes += 2 + seg_len;
      removed_count++;
    } else {
      if (marker == MARKER_APP1 && strip_mode == STRIP_GPS) {
        /* Copy marker+length, then body, then patch GPS in-place */
        APPEND(buf + pos, 4);
        size_t body_start = out_size;
        size_t body_len = (size_t)(seg_len - 2);
        APPEND(buf + pos + 4, body_len);
        strip_gps_from_app1(out + body_start, body_len);
        if (verbose)
          printf("  [patch]  GPS removed from APP1\n");
      } else {
        APPEND(buf + pos, 2 + (size_t)seg_len);
      }
    }

    /* SOS: everything after is compressed scan data — copy verbatim */
    if (marker == MARKER_SOS) {
      pos += 2 + (size_t)seg_len;
      APPEND(buf + pos, file_size - pos);
      break;
    }

    pos += 2 + (size_t)seg_len;
  }

#undef APPEND

  free(buf);

  /* ── Report ── */
  if (dry_run || verbose) {
    double saved_kb = (double)removed_bytes / 1024.0;
    double pct = (file_size > 0)
                     ? (double)removed_bytes / (double)file_size * 100.0
                     : 0.0;
    if (dry_run) {
      printf("[dry-run] %s\n", src);
      printf("          Would remove %d segment(s), saving %.1f KB (%.1f%%)\n",
             removed_count, saved_kb, pct);
      free(out);
      return 0;
    } else {
      printf("  Removed %d segment(s), saved %.1f KB (%.1f%%)\n", removed_count,
             saved_kb, pct);
    }
  }

  /* Write atomically: temp file → rename */
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
  if (!MoveFileExA(tmp_path, dst, MOVEFILE_REPLACE_EXISTING)) {
    fprintf(stderr, "Error: cannot replace '%s'.\n", dst);
    return -1;
  }
#else
  if (rename(tmp_path, dst) != 0) {
    FILE *in = fopen(tmp_path, "rb");
    FILE *o = fopen(dst, "wb");
    char rb[65536];
    size_t n;
    while (in && o && (n = fread(rb, 1, sizeof(rb), in)) > 0)
      fwrite(rb, 1, n, o);
    if (in)
      fclose(in);
    if (o)
      fclose(o);
    remove(tmp_path);
  }
#endif

  return 0;
}
