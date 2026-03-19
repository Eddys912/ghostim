#include "ghostim/batch.h"
#include "ghostim/jpeg_parser.h"
#include "ghostim/platform.h"
#include "ghostim/png_parser.h"
#include "ghostim/webp_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int file_exists(const char *p) {
  struct stat st;
  return stat(p, &st) == 0;
}

/* ── Process a single file ───────────────────────────────────────────────── */
static int process_file(const char *src, const Args *args, int total) {
  char dst_buf[4096];
  const char *dst;

  if (args->output_dir) {
    if (platform_build_dst(src, args->output_dir, dst_buf, sizeof(dst_buf)) !=
        0) {
      fprintf(stderr, "Error: cannot build output path for '%s'.\n", src);
      return -1;
    }
    dst = dst_buf;
  } else {
    dst = src; /* in-place */
  }

  if (!args->dry_run && total > 1)
    printf("Processing: %s\n", src);

  ImageType t = platform_detect(src);
  switch (t) {
  case IMG_JPEG:
    if (args->command == CMD_INFO)
      return jpeg_print_info(src, args->verbose);
    return jpeg_clean(src, dst, args->strip_mode, args->opt_mode, args->quality,
                      args->dry_run, args->verbose);
  case IMG_PNG:
    if (args->command == CMD_INFO)
      return png_print_info(src, args->verbose);
    return png_clean(src, dst, args->opt_mode, args->dry_run, args->verbose);
  case IMG_WEBP:
    if (args->command == CMD_INFO)
      return webp_print_info(src, args->verbose);
    return webp_clean(src, dst, args->opt_mode, args->quality, args->dry_run,
                      args->verbose);
  default:
    fprintf(stderr, "Error: '%s' is not a supported image (JPEG/PNG/WebP).\n",
            src);
    return -1;
  }
}

/* ════════════════════════════════════════════════════════════════════════════
 * PUBLIC: batch_run
 * ════════════════════════════════════════════════════════════════════════════
 */
int batch_run(const Args *args) {
  if (args->file_count == 0) {
    fprintf(stderr, "Error: no input files specified.\n");
    return 1;
  }

  /* Build final file list — expand directories recursively */
  char **all_files = NULL;
  int all_count = 0;
  int all_cap = 0;
  int dir_expanded = 0;

  for (int i = 0; i < args->file_count; i++) {
    const char *input = args->files[i];

    if (platform_is_dir(input)) {
      /* Expand directory → collect all images recursively */
      char **found = NULL;
      int n = platform_collect_images(input, &found);
      if (n <= 0) {
        fprintf(stderr, "Warning: no images found in '%s'.\n", input);
        if (found)
          free(found);
        continue;
      }
      dir_expanded = 1;
      printf("Found %d image(s) in '%s'\n", n, input);

      /* Grow all_files */
      if (all_count + n > all_cap) {
        all_cap = (all_count + n) * 2;
        char **tmp =
            (char **)realloc(all_files, (size_t)all_cap * sizeof(char *));
        if (!tmp) {
          fprintf(stderr, "Error: out of memory.\n");
          free(found);
          return 1;
        }
        all_files = tmp;
      }
      for (int j = 0; j < n; j++)
        all_files[all_count++] = found[j];
      free(found); /* free array, not the strings */
    } else {
      /* Single file */
      if (all_count >= all_cap) {
        all_cap = all_cap ? all_cap * 2 : 16;
        char **tmp =
            (char **)realloc(all_files, (size_t)all_cap * sizeof(char *));
        if (!tmp) {
          fprintf(stderr, "Error: out of memory.\n");
          return 1;
        }
        all_files = tmp;
      }
      all_files[all_count++] = (char *)input; /* not owned */
    }
  }

  if (all_count == 0) {
    fprintf(stderr, "Error: no valid images to process.\n");
    free(all_files);
    return 1;
  }

  /* Create output dir if needed */
  if (args->output_dir && !args->dry_run) {
    if (!file_exists(args->output_dir))
      platform_mkdir(args->output_dir);
  }

  /* Single file: print filename inline (no "Processing:" prefix) */
  if (all_count == 1 && !dir_expanded)
    printf("Processing: %s\n", all_files[0]);

  /* Process all files */
  int errors = 0;
  for (int i = 0; i < all_count; i++) {
    if (process_file(all_files[i], args, all_count) != 0)
      errors++;
  }

  /* Summary */
  if (all_count > 1 || dir_expanded) {
    int ok = all_count - errors;
    if (!args->dry_run)
      printf("\nDone: %d/%d file(s) processed successfully.\n", ok, all_count);
  }

  /* Free strings collected from directories */
  if (dir_expanded) {
    for (int i = 0; i < all_count; i++) {
      /* Only free strings we allocated — directory results */
      /* Single file inputs point directly to argv, don't free */
    }
  }
  free(all_files);

  return errors > 0 ? 1 : 0;
}
