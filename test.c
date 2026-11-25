#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "ast.h"

arena *ar = NULL;
int passed = 0;
int total = 0;

void run_tests();

int main(int argc, const char *argv[]) {
    ar = arena_new(0x8000);
    run_tests();
    printf("%d / %d passed\n", passed, total);
    return passed == total ? EXIT_SUCCESS : EXIT_FAILURE;
}

void test(const char *bdl, const char *sexp) {
    total++;
//    arena_reset(ar);
    ast_node *node = parse_string(ar, bdl);
    const char *result = ast_sexp(node);
    if (strcmp(result, sexp) == 0) {
        passed++;
    } else {
        fprintf(stderr, "--- test %d failed ---\n%s\nresult: %s\nexpect: %s\n",
            total, bdl, result, sexp);
    }
}

void run_tests() {
    test("", "()");

    test("type byte [8 bit];",
        "(typedef byte (type (8 bit)))");

    test("type bar [1 bit 2 bit 3 bit 4 bit];",
        "(typedef bar (type (1 bit) (2 bit) (3 bit) (4 bit)))");

    test("type named [4 bit .lo 4 bit .hi];",
        "(typedef named (type (4 bit .lo) (4 bit .hi)))");

    test("type nibb [4 bit]; type byte [2 nibb];",
        "(typedef nibb (type (4 bit))) "
        "(typedef byte (type (2 nibb)))");
}

