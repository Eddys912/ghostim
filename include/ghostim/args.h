#ifndef GHOSTIM_ARGS_H
#define GHOSTIM_ARGS_H

/* Supported CLI commands */
typedef enum { CMD_UNKNOWN = 0, CMD_INFO, CMD_CLEAN } Command;

/*
 * Strip mode for --strip (applies only to EXIF content):
 *   STRIP_ALL  — remove the entire EXIF segment (default).
 *   STRIP_GPS  — remove only the GPS SubIFD pointer, keep other EXIF tags.
 *
 * Note: 'clean' always removes ALL non-image segments regardless of StripMode:
 * EXIF, JFIF headers, embedded thumbnails, comments, vendor APP segments.
 * Only compressed pixel data is preserved (DQT, DHT, SOF*, SOS, EOI).
 */
typedef enum { STRIP_ALL = 0, STRIP_GPS } StripMode;

/* Parsed CLI arguments */
typedef struct {
  Command command;
  StripMode strip_mode;
  const char *output_dir;
  int dry_run;
  int verbose;
  char **files;
  int file_count;
} Args;

int args_parse(Args *args, int argc, char *argv[]);
void args_free(Args *args);

#endif /* GHOSTIM_ARGS_H */
