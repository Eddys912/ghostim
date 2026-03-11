/*
 * ghostim — Image metadata remover for JPEG and PNG files.
 *
 * Pure C99, zero external dependencies.
 * Parses raw binary format specs directly.
 */

#include "ghostim/args.h"
#include "ghostim/batch.h"
#include "ghostim/jpeg_parser.h"
#include "ghostim/png_parser.h"
#include "ghostim/platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GHOSTIM_VERSION "1.0.0"

static void print_version(void) {
    printf("ghostim v%s\n", GHOSTIM_VERSION);
}

static void print_help(const char *prog) {
    printf("Usage: %s <command> [options] <file(s)>\n\n", prog);
    printf("Commands:\n");
    printf("  info   <file>          Show metadata found in a file\n");
    printf("  clean  <file(s)>       Remove all metadata from file(s)\n\n");
    printf("  'clean' removes everything that is not image data:\n");
    printf("  EXIF, GPS, camera info, timestamps, comments, thumbnails,\n");
    printf("  vendor segments. Pixel data is never modified.\n");
    printf("  ICC color profiles (APP2) are always preserved.\n\n");
    printf("Options:\n");
    printf("  --strip gps            Remove only GPS location data\n");
    printf("  --output <dir>         Write cleaned files to this directory\n");
    printf("  --dry-run              Show what would be removed, don't write\n");
    printf("  --verbose              Show segment-level details\n");
    printf("  --version              Show version\n");
    printf("  --help                 Show this help\n\n");
    printf("Examples:\n");
    printf("  %s info photo.jpg\n", prog);
    printf("  %s clean photo.jpg\n", prog);
    printf("  %s clean photo.jpg --strip gps\n", prog);
    printf("  %s clean *.jpg --output ./clean/\n", prog);
    printf("  %s clean photo.jpg --dry-run --verbose\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--version") == 0) { print_version(); return 0; }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help(argv[0]);
        return 0;
    }

    Args args;
    if (args_parse(&args, argc, argv) != 0) {
        fprintf(stderr, "Error: invalid arguments. Use --help for usage.\n");
        return 1;
    }

    if (args.command == CMD_INFO) {
        if (args.file_count == 0) {
            fprintf(stderr, "Error: 'info' requires a file argument.\n");
            args_free(&args);
            return 1;
        }

        ImageType type = platform_detect_image_type(args.files[0]);
        int result = 0;

        if (type == IMAGE_JPEG) {
            result = jpeg_print_info(args.files[0], args.verbose);
        } else if (type == IMAGE_PNG) {
            result = png_print_info(args.files[0], args.verbose);
        } else {
            fprintf(stderr, "Error: '%s' is not a supported image (JPEG/PNG).\n",
                    args.files[0]);
            result = 1;
        }

        args_free(&args);
        return result;

    } else if (args.command == CMD_CLEAN) {
        if (args.file_count == 0) {
            fprintf(stderr, "Error: 'clean' requires at least one file.\n");
            args_free(&args);
            return 1;
        }

        BatchConfig cfg;
        cfg.files      = args.files;
        cfg.file_count = args.file_count;
        cfg.strip_mode = args.strip_mode;
        cfg.output_dir = args.output_dir;
        cfg.dry_run    = args.dry_run;
        cfg.verbose    = args.verbose;

        int result = batch_run(&cfg);
        args_free(&args);
        return result;

    } else {
        fprintf(stderr, "Error: unknown command '%s'.\n", argv[1]);
        print_help(argv[0]);
        args_free(&args);
        return 1;
    }
}
