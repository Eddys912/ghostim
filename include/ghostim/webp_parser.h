#ifndef GHOSTIM_WEBP_PARSER_H
#define GHOSTIM_WEBP_PARSER_H

#include "ghostim/args.h"

int webp_print_info(const char *path, int verbose);

/*
 * Lossless: strips EXIF, XMP, ICC chunks from the RIFF container.
 *   Pixel bitstream (VP8/VP8L/VP8X) untouched.
 *
 * Lossy: decode via libwebp → re-encode at target quality.
 *   Uses WebP lossy encoder for quality < 100,
 *   lossless encoder for quality == 100.
 */
int webp_clean(const char *src, const char *dst,
               OptMode opt_mode, int quality,
               int dry_run, int verbose);

#endif /* GHOSTIM_WEBP_PARSER_H */
