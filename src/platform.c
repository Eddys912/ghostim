#include "ghostim/platform.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Platform-specific includes ─────────────────────────────────────────── */
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <unistd.h>
#define PATH_SEP '/'
#endif

/* ── Magic bytes ─────────────────────────────────────────────────────────── */
#define JPEG_MAGIC_0 0xFF
#define JPEG_MAGIC_1 0xD8
#define PNG_MAGIC_0 0x89
#define PNG_MAGIC_1 0x50 /* 'P' */
#define PNG_MAGIC_2 0x4E /* 'N' */
#define PNG_MAGIC_3 0x47 /* 'G' */

ImageType platform_detect_image_type(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return IMAGE_UNKNOWN;

  unsigned char buf[4] = {0};
  size_t n = fread(buf, 1, sizeof(buf), f);
  fclose(f);

  if (n < 2)
    return IMAGE_UNKNOWN;

  /* JPEG: starts with FF D8 */
  if (buf[0] == JPEG_MAGIC_0 && buf[1] == JPEG_MAGIC_1)
    return IMAGE_JPEG;

  /* PNG: starts with 89 50 4E 47 */
  if (n >= 4 && buf[0] == PNG_MAGIC_0 && buf[1] == PNG_MAGIC_1 &&
      buf[2] == PNG_MAGIC_2 && buf[3] == PNG_MAGIC_3)
    return IMAGE_PNG;

  return IMAGE_UNKNOWN;
}

/* ── Path helpers ────────────────────────────────────────────────────────── */

/* Return pointer to the basename portion of path (no allocation). */
static const char *basename_ptr(const char *path) {
  const char *last = path;
  for (const char *p = path; *p; p++) {
    if (*p == '/' || *p == '\\')
      last = p + 1;
  }
  return last;
}

int platform_build_output_path(char *dst, size_t dst_size, const char *src_path,
                               const char *output_dir) {
  if (!output_dir) {
    /* overwrite in place */
    size_t len = strlen(src_path);
    if (len + 1 > dst_size)
      return -1;
    memcpy(dst, src_path, len + 1);
    return 0;
  }

  const char *base = basename_ptr(src_path);
  int n = snprintf(dst, dst_size, "%s%c%s", output_dir, PATH_SEP, base);
  if (n < 0 || (size_t)n >= dst_size)
    return -1;
  return 0;
}

/* ── Directory creation ──────────────────────────────────────────────────── */

#ifdef _WIN32
int platform_mkdir_p(const char *path) {
  /* CreateDirectory returns non-zero on success or if already exists */
  if (CreateDirectoryA(path, NULL))
    return 0;
  if (GetLastError() == ERROR_ALREADY_EXISTS)
    return 0;
  return -1;
}
#else
int platform_mkdir_p(const char *path) {
  /* Walk the path, creating each component */
  char tmp[4096];
  size_t len = strlen(path);
  if (len + 1 > sizeof(tmp))
    return -1;
  memcpy(tmp, path, len + 1);

  /* Strip trailing slash */
  if (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\'))
    tmp[--len] = '\0';

  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
      *p = '/';
    }
  }
  if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
    return -1;
  return 0;
}
#endif

/* ── File replace ────────────────────────────────────────────────────────── */

int platform_replace_file(const char *dst, const char *src) {
#ifdef _WIN32
  /* MoveFileExA with REPLACE_EXISTING is atomic on the same volume */
  if (MoveFileExA(src, dst, MOVEFILE_REPLACE_EXISTING))
    return 0;
  return -1;
#else
  /* POSIX rename is atomic on the same filesystem */
  if (rename(src, dst) == 0)
    return 0;

  /* Cross-device: fall back to copy + unlink */
  FILE *in = fopen(src, "rb");
  FILE *out = fopen(dst, "wb");
  if (!in || !out) {
    if (in)
      fclose(in);
    if (out)
      fclose(out);
    return -1;
  }

  char buf[65536];
  size_t n;
  int ok = 1;
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) {
      ok = 0;
      break;
    }
  }
  fclose(in);
  fclose(out);

  if (!ok)
    return -1;
  unlink(src);
  return 0;
#endif
}
