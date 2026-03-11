#ifndef GHOSTIM_BATCH_H
#define GHOSTIM_BATCH_H

#include "ghostim/args.h"

typedef struct {
    char      **files;
    int         file_count;
    StripMode   strip_mode;
    const char *output_dir;
    int         dry_run;
    int         verbose;
} BatchConfig;

int batch_run(const BatchConfig *cfg);

#endif /* GHOSTIM_BATCH_H */
