#include "ghostim/platform.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(p, m) _mkdir(p)
#endif

static int read_magic(const char *path, unsigned char *hdr, size_t n) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;
  size_t r = fread(hdr, 1, n, f);
  fclose(f);
  return (r == n) ? 0 : -1;
}

ImageType platform_detect(const char *path) {
  unsigned char h[12] = {0};
  if (read_magic(path, h, 12) != 0)
    return IMG_UNKNOWN;
  if (h[0] == 0xFF && h[1] == 0xD8)
    return IMG_JPEG;
  if (memcmp(h, "\x89PNG\r\n\x1a\n", 8) == 0)
    return IMG_PNG;
  if (memcmp(h, "RIFF", 4) == 0 && memcmp(h + 8, "WEBP", 4) == 0)
    return IMG_WEBP;
  return IMG_UNKNOWN;
}

int platform_mkdir(const char *dir) {
#ifdef _WIN32
  return _mkdir(dir);
#else
  return mkdir(dir, 0755);
#endif
}

int platform_build_dst(const char *src, const char *dir, char *dst, size_t sz) {
  const char *base = strrchr(src, '/');
#ifdef _WIN32
  const char *base2 = strrchr(src, '\\');
  if (!base || (base2 && base2 > base))
    base = base2;
#endif
  base = base ? base + 1 : src;
  int r = snprintf(dst, sz, "%s/%s", dir, base);
  return (r > 0 && (size_t)r < sz) ? 0 : -1;
}
