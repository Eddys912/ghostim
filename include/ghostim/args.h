#ifndef GHOSTIM_ARGS_H
#define GHOSTIM_ARGS_H

/* Supported CLI commands */
typedef enum { CMD_UNKNOWN = 0, CMD_INFO, CMD_CLEAN } Command;

/*
 * Strip mode (within EXIF):
 *   STRIP_ALL — remove entire EXIF segment (default).
 *   STRIP_GPS — remove only GPS SubIFD pointer, keep other EXIF tags.
 */
typedef enum { STRIP_ALL = 0, STRIP_GPS } StripMode;

/*
 * Optimization mode:
 *   OPT_LOSSLESS — remove metadata only, pixel data untouched (default).
 *   OPT_LOSSY    — re-encode at controlled quality for maximum size reduction.
 */
typedef enum { OPT_LOSSLESS = 0, OPT_LOSSY } OptMode;

/* Parsed CLI arguments */
typedef struct {
  Command command;
  StripMode strip_mode;
  OptMode opt_mode;
  int quality; /* 1-100, used when opt_mode = OPT_LOSSY (default 85) */
  const char *output_dir;
  int dry_run;
  int verbose;
  char **files;
  int file_count;
} Args;

int args_parse(Args *args, int argc, char *argv[]);
void args_free(Args *args);

#endif /* GHOSTIM_ARGS_H */
