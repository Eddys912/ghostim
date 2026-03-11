#ifndef GHOSTIM_PNG_PARSER_H
#define GHOSTIM_PNG_PARSER_H

#include "ghostim/args.h"

/* Print metadata report for the PNG at path. */
int png_print_info(const char *path, int verbose);

/*
 * Write a cleaned copy of src to dst.
 *
 * Removes ALL non-image chunks: eXIf, tEXt, iTXt, zTXt, tIME.
 * Preserves: IHDR, PLTE, IDAT, IEND, cHRM, gAMA, sRGB, iCCP, bKGD, pHYs.
 * Compressed pixel data (IDAT) is never touched.
 */
int png_clean(const char *src, const char *dst, StripMode strip_mode,
              int dry_run, int verbose);

#endif /* GHOSTIM_PNG_PARSER_H */
