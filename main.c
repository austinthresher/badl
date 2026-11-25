#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "arena.h"
#include "ast.h"

int main(int argc, const char *argv[]) {
    arena *ar = arena_new(0x8000);
    ast_node *n = parse_string(ar,
        "type byte [8 bit 4 bit];"
    );
    printf("%s\n", ast_sexp(n));
    return EXIT_SUCCESS;
}
