#ifndef GHOSTIM_PLATFORM_H
#define GHOSTIM_PLATFORM_H

#include <stddef.h>

typedef enum { IMG_UNKNOWN = 0, IMG_JPEG, IMG_PNG, IMG_WEBP } ImageType;

ImageType platform_detect(const char *path);
int platform_mkdir(const char *dir);
int platform_build_dst(const char *src, const char *dir, char *dst, size_t sz);

#endif /* GHOSTIM_PLATFORM_H */
