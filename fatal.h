#include <stdlib.h>
#include <stdio.h>

#define FATAL(fmt, args...) do { \
        fprintf(stderr, "[%s:%d:%s] fatal: ", __FILE__, __LINE__, __func__); \
        fprintf(stderr, fmt, ## args); \
        exit(EXIT_FAILURE); \
    } while (0)

