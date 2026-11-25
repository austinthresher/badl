#include <stdlib.h>
#include <stdio.h>

// Used when we want to report our caller's location.
// Assumes same file for now.
#define FATAL_AT(_line, _func, ...) do { \
        fprintf(stderr, "[%s:%d:%s] fatal: ", __FILE__, _line, _func); \
        fprintf(stderr, __VA_ARGS__); \
        exit(EXIT_FAILURE); \
    } while (0)

#define FATAL(...) FATAL_AT(__LINE__, __func__, __VA_ARGS__)

