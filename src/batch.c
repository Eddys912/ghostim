#include "ghostim/batch.h"
#include "ghostim/jpeg_parser.h"
#include "ghostim/platform.h"
#include "ghostim/png_parser.h"
#include "ghostim/webp_parser.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

static int file_exists(const char *p) {
  struct stat st;
  return stat(p, &st) == 0;
}

int batch_run(const Args *args) {
  if (args->file_count == 0) {
    fprintf(stderr, "Error: no input files specified.\n");
    return 1;
  }
  if (args->output_dir && !args->dry_run) {
    if (!file_exists(args->output_dir))
      platform_mkdir(args->output_dir);
  }

  int errors = 0;
  for (int i = 0; i < args->file_count; i++) {
    const char *src = args->files[i];
    char dst_buf[4096];
    const char *dst;

    if (args->output_dir) {
      if (platform_build_dst(src, args->output_dir, dst_buf, sizeof(dst_buf)) !=
          0) {
        fprintf(stderr, "Error: cannot build output path for '%s'.\n", src);
        errors++;
        continue;
      }
      dst = dst_buf;
    } else {
      dst = src; /* in-place */
    }

    if (!args->dry_run)
      printf("Processing: %s\n", src);

    ImageType t = platform_detect(src);
    int r = 0;
    switch (t) {
    case IMG_JPEG:
      if (args->command == CMD_INFO)
        r = jpeg_print_info(src, args->verbose);
      else
        r = jpeg_clean(src, dst, args->strip_mode, args->opt_mode,
                       args->quality, args->dry_run, args->verbose);
      break;
    case IMG_PNG:
      if (args->command == CMD_INFO)
        r = png_print_info(src, args->verbose);
      else
        r = png_clean(src, dst, args->opt_mode, args->dry_run, args->verbose);
      break;
    case IMG_WEBP:
      if (args->command == CMD_INFO)
        r = webp_print_info(src, args->verbose);
      else
        r = webp_clean(src, dst, args->opt_mode, args->quality, args->dry_run,
                       args->verbose);
      break;
    default:
      fprintf(stderr, "Error: '%s' is not a supported image (JPEG/PNG/WebP).\n",
              src);
      errors++;
      continue;
    }
    if (r != 0)
      errors++;
  }

  if (!args->dry_run && args->file_count > 1)
    printf("\nDone: %d file(s) processed.\n", args->file_count - errors);
  return errors > 0 ? 1 : 0;
}
