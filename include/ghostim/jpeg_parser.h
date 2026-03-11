#ifndef GHOSTIM_JPEG_PARSER_H
#define GHOSTIM_JPEG_PARSER_H

#include "ghostim/args.h"

/* Print metadata report for the JPEG at path. */
int jpeg_print_info(const char *path, int verbose);

/*
 * Write a cleaned copy of src to dst.
 *
 * Removes ALL non-image segments: EXIF, JFIF/APP0, embedded thumbnails,
 * comments (COM), vendor APP segments (APP3-APPF).
 * APP2 (ICC color profile) is preserved — it affects color rendering.
 * Compressed pixel data (DQT, DHT, SOF*, SOS) is never touched.
 *
 * strip_mode controls EXIF handling:
 *   STRIP_ALL — drop the entire EXIF segment (default).
 *   STRIP_GPS — keep EXIF but zero out the GPS SubIFD pointer.
 */
int jpeg_clean(const char *src, const char *dst,
               StripMode strip_mode, int dry_run, int verbose);

#endif /* GHOSTIM_JPEG_PARSER_H */
