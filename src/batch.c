#include "ghostim/batch.h"
#include "ghostim/jpeg_parser.h"
#include "ghostim/platform.h"
#include "ghostim/png_parser.h"
#include <stdio.h>
#include <string.h>

#define MAX_PATH_LEN 4096

int batch_run(const BatchConfig *cfg) {
  /* Create output directory if specified */
  if (cfg->output_dir) {
    if (platform_mkdir_p(cfg->output_dir) != 0) {
      fprintf(stderr, "Error: cannot create output directory '%s'.\n",
              cfg->output_dir);
      return 1;
    }
  }

  int errors = 0;

  for (int i = 0; i < cfg->file_count; i++) {
    const char *src = cfg->files[i];

    /* Detect image type */
    ImageType type = platform_detect_image_type(src);
    if (type == IMAGE_UNKNOWN) {
      fprintf(stderr, "Skipping '%s': not a supported image (JPEG/PNG).\n",
              src);
      errors++;
      continue;
    }

    /* Build output path */
    char dst[MAX_PATH_LEN];
    if (platform_build_output_path(dst, sizeof(dst), src, cfg->output_dir) !=
        0) {
      fprintf(stderr, "Error: output path too long for '%s'.\n", src);
      errors++;
      continue;
    }

    if (cfg->verbose)
      printf("Processing: %s\n", src);

    int result = 0;
    if (type == IMAGE_JPEG) {
      result =
          jpeg_clean(src, dst, cfg->strip_mode, cfg->dry_run, cfg->verbose);
    } else {
      result = png_clean(src, dst, cfg->strip_mode, cfg->dry_run, cfg->verbose);
    }

    if (result != 0) {
      fprintf(stderr, "Failed to process '%s'.\n", src);
      errors++;
    } else if (!cfg->verbose && !cfg->dry_run) {
      printf("Cleaned: %s\n", src);
    }
  }

  /* Summary */
  int ok = cfg->file_count - errors;
  printf("\nDone: %d file(s) processed", ok);
  if (errors > 0)
    printf(", %d error(s)", errors);
  printf(".\n");

  return (errors > 0) ? 1 : 0;
}
