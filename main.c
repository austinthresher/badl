#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "arena.h"
#include "ast.h"

int main(int argc, const char *argv[]) {
    arena *ar = arena_new(0x8000);
    if (argc < 2) {
        fprintf(stderr, "usage: %s <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Read whole text file into string
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "failed to open file: %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *file_buf = arena_alloc(ar, len+1);
    if (fread(file_buf, 1, len, f) != len) {
        fprintf(stderr, "file read error\n");
        return EXIT_FAILURE;
    }
    fclose(f);
    file_buf[len] = '\0';

    ast_node *n = ast_from_string(ar, file_buf);
    if (n == NULL) {
        fprintf(stderr, "%s\n", ast_error());
        return EXIT_FAILURE;
    }
    printf("%s\n", ast_sexp(n));
    return EXIT_SUCCESS;
}
