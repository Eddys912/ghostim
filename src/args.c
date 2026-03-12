#include "ghostim/args.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_QUALITY 85

int args_parse(Args *args, int argc, char *argv[]) {
  memset(args, 0, sizeof(*args));
  args->strip_mode = STRIP_ALL;
  args->opt_mode = OPT_LOSSLESS;
  args->quality = DEFAULT_QUALITY;

  if (argc < 2)
    return -1;

  if (strcmp(argv[1], "info") == 0) {
    args->command = CMD_INFO;
  } else if (strcmp(argv[1], "clean") == 0) {
    args->command = CMD_CLEAN;
  } else {
    return -1;
  }

  args->files = (char **)malloc((size_t)(argc - 2) * sizeof(char *));
  if (!args->files)
    return -1;

  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--verbose") == 0) {
      args->verbose = 1;
    } else if (strcmp(argv[i], "--dry-run") == 0) {
      args->dry_run = 1;
    } else if (strcmp(argv[i], "--lossy") == 0) {
      args->opt_mode = OPT_LOSSY;
    } else if (strcmp(argv[i], "--quality") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --quality requires a value (1-100).\n");
        free(args->files);
        return -1;
      }
      i++;
      int q = atoi(argv[i]);
      if (q < 1 || q > 100) {
        fprintf(stderr, "Error: --quality must be between 1 and 100.\n");
        free(args->files);
        return -1;
      }
      args->quality = q;
      args->opt_mode = OPT_LOSSY; /* --quality implies --lossy */
    } else if (strcmp(argv[i], "--strip") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --strip requires an argument (all|gps).\n");
        free(args->files);
        return -1;
      }
      i++;
      if (strcmp(argv[i], "gps") == 0) {
        args->strip_mode = STRIP_GPS;
      } else if (strcmp(argv[i], "all") == 0) {
        args->strip_mode = STRIP_ALL;
      } else {
        fprintf(stderr, "Error: --strip argument must be 'all' or 'gps'.\n");
        free(args->files);
        return -1;
      }
    } else if (strcmp(argv[i], "--output") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --output requires a directory.\n");
        free(args->files);
        return -1;
      }
      i++;
      args->output_dir = argv[i];
    } else {
      args->files[args->file_count++] = argv[i];
    }
  }

  return 0;
}

void args_free(Args *args) {
  if (args->files) {
    free(args->files);
    args->files = NULL;
  }
}
