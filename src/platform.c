#include "ghostim/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#endif

/* ── Magic byte detection ────────────────────────────────────────────────── */
static int read_magic(const char *path, unsigned char *hdr, size_t n) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;
  size_t r = fread(hdr, 1, n, f);
  fclose(f);
  return (r == n) ? 0 : -1;
}

/* ── Load entire file into heap buffer ───────────────────────────────────── */
unsigned char *platform_load_file(const char *path, size_t *sz) {
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
  *sz = (size_t)len;

  return buf;
}

/* ── Atomic write via temp file + rename ─────────────────────────────────── */
int platform_atomic_write(const char *dst, const unsigned char *data,
                          size_t sz) {
  char tmp[4096];
  snprintf(tmp, sizeof(tmp), "%s.ghostim_tmp", dst);
  FILE *f = fopen(tmp, "wb");
  if (!f)
    return -1;
  if (fwrite(data, 1, sz, f) != sz) {
    fclose(f);
    remove(tmp);
    return -1;
  }
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

/* ── Directory check ─────────────────────────────────────────────────────── */
int platform_is_dir(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
  return S_ISDIR(st.st_mode);
}

/* ── mkdir ───────────────────────────────────────────────────────────────── */
int platform_mkdir(const char *dir) {
#ifdef _WIN32
  return _mkdir(dir);
#else
  return mkdir(dir, 0755);
#endif
}

/* ── Build destination path ──────────────────────────────────────────────── */
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

/* ── Dynamic array helpers ───────────────────────────────────────────────── */
typedef struct {
  char **files;
  int count;
  int cap;
} FileList;

static int fl_push(FileList *fl, const char *path) {
  if (fl->count >= fl->cap) {
    int newcap = fl->cap ? fl->cap * 2 : 64;
    char **tmp = (char **)realloc(fl->files, (size_t)newcap * sizeof(char *));
    if (!tmp)
      return -1;
    fl->files = tmp;
    fl->cap = newcap;
  }
  fl->files[fl->count] = (char *)malloc(strlen(path) + 1);
  if (!fl->files[fl->count])
    return -1;
  strcpy(fl->files[fl->count], path);
  fl->count++;
  return 0;
}

/* ── Recursive collect ───────────────────────────────────────────────────── */
#ifdef _WIN32

static int collect_win(const char *dir, FileList *fl) {
  char pattern[4096];
  snprintf(pattern, sizeof(pattern), "%s\\*", dir);

  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return -1;

  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
      continue;

    char full[4096];
    snprintf(full, sizeof(full), "%s\\%s", dir, fd.cFileName);

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      collect_win(full, fl);
    } else {
      if (platform_detect(full) != IMG_UNKNOWN)
        fl_push(fl, full);
    }
  } while (FindNextFileA(h, &fd));

  FindClose(h);
  return 0;
}

#else

static int collect_posix(const char *dir, FileList *fl) {
  DIR *d = opendir(dir);
  if (!d)
    return -1;

  struct dirent *entry;
  while ((entry = readdir(d)) != NULL) {
    if (entry->d_name[0] == '.')
      continue; /* skip . .. .hidden */

    char full[4096];
    snprintf(full, sizeof(full), "%s/%s", dir, entry->d_name);

    struct stat st;
    if (stat(full, &st) != 0)
      continue;

    if (S_ISDIR(st.st_mode)) {
      collect_posix(full, fl);
    } else if (S_ISREG(st.st_mode)) {
      if (platform_detect(full) != IMG_UNKNOWN)
        fl_push(fl, full);
    }
  }

  closedir(d);
  return 0;
}

#endif

int platform_collect_images(const char *dir, char ***out_files) {
  FileList fl = {NULL, 0, 0};
#ifdef _WIN32
  collect_win(dir, &fl);
#else
  collect_posix(dir, &fl);
#endif
  *out_files = fl.files;
  return fl.count;
}
