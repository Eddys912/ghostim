#ifndef GHOSTIM_PNG_PARSER_H
#define GHOSTIM_PNG_PARSER_H

#include "ghostim/args.h"

int png_print_info(const char *path, int verbose);

/*
 * Lossless: walks raw PNG chunks, drops metadata chunks only.
 * Lossy:    decode via libpng → re-encode with maximum zlib compression.
 *           PNG is lossless by nature — "lossy" here means heavier
 *           compression (zlib level 9 + filter optimization), not pixel loss.
 */
int png_clean(const char *src, const char *dst, OptMode opt_mode, int dry_run,
              int verbose);

#endif /* GHOSTIM_PNG_PARSER_H */
