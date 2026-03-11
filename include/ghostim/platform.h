#ifndef GHOSTIM_PLATFORM_H
#define GHOSTIM_PLATFORM_H

#include <stddef.h>

/* Detected image format */
typedef enum { IMAGE_UNKNOWN = 0, IMAGE_JPEG, IMAGE_PNG } ImageType;

/*
 * Detect image type by reading the file magic bytes.
 * Returns IMAGE_UNKNOWN if the file cannot be opened or is not JPEG/PNG.
 */
ImageType platform_detect_image_type(const char *path);

/*
 * Build an output path for a cleaned file.
 *
 * If output_dir is NULL, dst receives a copy of src_path (overwrite in place).
 * Otherwise, dst receives "<output_dir>/<basename(src_path)>".
 *
 * dst must be at least dst_size bytes. Returns 0 on success, -1 on error.
 */
int platform_build_output_path(char *dst, size_t dst_size, const char *src_path,
                               const char *output_dir);

/*
 * Create a directory (and parents) if it does not already exist.
 * Returns 0 on success, -1 on error.
 */
int platform_mkdir_p(const char *path);

/*
 * Rename src to dst atomically where possible.
 * Falls back to copy+delete on platforms that don't support cross-device
 * rename. Returns 0 on success, -1 on error.
 */
int platform_replace_file(const char *dst, const char *src);

#endif /* GHOSTIM_PLATFORM_H */
