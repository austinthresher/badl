#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "ast.h"

arena *ar = NULL;
int passed = 0;
int total = 0;
int verbosity = 0;

void run_tests();

int main(int argc, const char *argv[]) {
    if (argc > 1) {
        const char *arg = argv[1];
        while (*arg) {
            if (*arg++ == 'v') { verbosity++; }
        }
    }
    ar = arena_new(0x8000);
    run_tests();
    printf("%d / %d passed\n", passed, total);
    return passed == total ? EXIT_SUCCESS : EXIT_FAILURE;
}

void test(const char *bdl, const char *sexp) {
    total++;
    arena_reset(ar);
    ast_node *node = parse_string(ar, bdl);
    const char *result = ast_sexp(node);
    if (strcmp(result, sexp) == 0) {
        passed++;
        if (verbosity > 0) {
            printf("%3d pass: %s -> %s\n", total, bdl, result);
        }
    } else {
        fprintf(stderr, "--- test %d failed ---\n%s\nresult: %s\nexpect: %s\n",
            total, bdl, result, sexp);
    }
}

void run_tests() {
    test("# a comment", "()");

    test("type byte [8 bit];",
        "(typedef byte (type (8 bit)))");

    test("type another_bit bit;",
        "(typedef another_bit (typename bit))");

    test("type byte [%1000 bit];",
        "(typedef byte (type (8 bit)))");

    test("type half [$10 bit];",
        "(typedef half (type (16 bit)))");

    test("type bar [1 bit 2 bit 3 bit 4 bit];",
        "(typedef bar (type (1 bit) (2 bit) (3 bit) (4 bit)))");

    test("type named [4 bit .lo 4 bit .hi];",
        "(typedef named (type (4 bit .lo) (4 bit .hi)))");

    test("type nibb [4 bit]; type byte [2 nibb];",
        "(typedef nibb (type (4 bit))) "
        "(typedef byte (type (2 nibb)))");

    test("bit enabled;",
        "(global enabled (typename bit))");

    test("const bit flag;",
        "(global flag const (typename bit))");

    test("const bit flag = 1;",
        "(global flag const (typename bit) 1)");

    test("bit flag = 0;",
        "(global flag (typename bit) 0)");

    test("[1 bit] flag = 0;",
        "(global flag (type (1 bit)) 0)");

    test("void foo();",
        "(decl foo (typename void) ())");

    test("bit foo();",
        "(decl foo (typename bit) ())");

    test("[8 bit .bar] foo();",
        "(decl foo (type (8 bit .bar)) ())");

    test("void foo(bit a, bit b);",
        "(decl foo (typename void)"
        " (local a (typename bit)) (local b (typename bit)))");

}

