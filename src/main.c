#include "ghostim/args.h"
#include "ghostim/batch.h"
#include <stdio.h>
#include <string.h>

#define VERSION "2.0.0"

static void print_help(void) {
  printf(
      "ghostim v" VERSION " — image metadata remover and optimizer\n\n"
      "Usage:\n"
      "  ghostim info  <file(s)>   Show metadata report\n"
      "  ghostim clean <file(s)>   Remove metadata (and optionally "
      "optimize)\n\n"
      "Options:\n"
      "  --lossy            Re-encode for maximum size reduction\n"
      "  --quality <1-100>  Set encode quality (implies --lossy, default 85)\n"
      "  --strip gps        Remove only GPS, keep other EXIF\n"
      "  --output <dir>     Write output files to directory\n"
      "  --dry-run          Preview changes without writing\n"
      "  --verbose          Show every segment/chunk removed\n\n"
      "Examples:\n"
      "  ghostim info photo.jpg\n"
      "  ghostim clean photo.jpg\n"
      "  ghostim clean photo.jpg --lossy --quality 90\n"
      "  ghostim clean *.jpg --output ./clean/\n"
      "  ghostim clean photo.jpg --dry-run --verbose\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_help();
    return 0;
  }
  if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
    printf("ghostim v" VERSION "\n");
    return 0;
  }
  if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
    print_help();
    return 0;
  }

  Args args;
  if (args_parse(&args, argc, argv) != 0) {
    fprintf(stderr, "Error: invalid arguments. Run 'ghostim --help'.\n");
    return 1;
  }

  int r = batch_run(&args);
  args_free(&args);
  return r;
}
