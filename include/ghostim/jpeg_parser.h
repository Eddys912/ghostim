#ifndef GHOSTIM_JPEG_PARSER_H
#define GHOSTIM_JPEG_PARSER_H

#include "ghostim/args.h"

/* Print metadata report for the JPEG at path. */
int jpeg_print_info(const char *path, int verbose);

/*
 * Clean and optionally optimize a JPEG file.
 *
 * Lossless (OPT_LOSSLESS):
 *   Removes all non-image segments (EXIF, APP0, comments, vendor APPs).
 *   Pixel data is copied verbatim — no quality loss whatsoever.
 *
 * Lossy (OPT_LOSSY):
 *   Decodes the image fully via libjpeg-turbo, strips all metadata,
 *   then re-encodes at `quality` (1-100). Maximum size reduction.
 *   Uses 4:2:0 chroma subsampling for quality < 90, 4:4:4 above.
 */
int jpeg_clean(const char *src, const char *dst, StripMode strip_mode,
               OptMode opt_mode, int quality, int dry_run, int verbose);

#endif /* GHOSTIM_JPEG_PARSER_H */
