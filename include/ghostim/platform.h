#ifndef GHOSTIM_PLATFORM_H
#define GHOSTIM_PLATFORM_H

#include <stddef.h>

typedef enum { IMG_UNKNOWN = 0, IMG_JPEG, IMG_PNG, IMG_WEBP } ImageType;

ImageType platform_detect(const char *path);
int platform_is_dir(const char *path);
int platform_mkdir(const char *dir);
int platform_build_dst(const char *src, const char *dir, char *dst, size_t sz);

/*
 * Load an entire file into a heap-allocated buffer.
 * Caller must free the returned pointer. On error returns NULL.
 */
unsigned char *platform_load_file(const char *path, size_t *sz);

/*
 * Write `sz` bytes to `dst` atomically via a temp file + rename.
 * Returns 0 on success, -1 on error.
 */
int platform_atomic_write(const char *dst, const unsigned char *data, size_t sz);

/*
 * Recursively collect all image files (JPEG/PNG/WebP) under `dir`.
 * Results written into a dynamically allocated array of strings.
 * Caller must free each string and the array itself.
 * Returns number of files found, -1 on error.
 */
int platform_collect_images(const char *dir, char ***out_files);

#endif /* GHOSTIM_PLATFORM_H */
