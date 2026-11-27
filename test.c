#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "parse.h"

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
    ast_node *node = parse_from_string(ar, bdl);
    const char *result = ast_sexp(node);
    if ((sexp != NULL && strcmp(result, sexp) == 0) ||
        (sexp == NULL && node == NULL)) {
        passed++;
        if (node == NULL && sexp == NULL) { result = parse_error(); }
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

    test("type byte [8 8 bit];", NULL);
    test("type byte [8 bit]", NULL);
    test("type byte [8];", NULL);
    test("type bit [1 bit];", NULL);
    test("type byte [8 bit]; type byte [4 bit];", NULL);

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

    test("type named [4 bit .lo .hi];", NULL);

    test("type nibb [4 bit]; type byte [2 nibb];",
        "(typedef nibb (type (4 bit))) "
        "(typedef byte (type (2 nibb)))");

    test("bit enabled", NULL);
    test("bit enabled =", NULL);
    test("bit enabled = 0", NULL);
    test("const bit;", NULL);

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

    test("inline void foo();", NULL);
    test("const void foo();", NULL);
    test("void const foo();", NULL);
    test("const void foo();", NULL);

    test("void foo();",
        "(decl foo (typename void) ())");

    test("bit foo();",
        "(decl foo (typename bit) ())");

    test("[8 bit .bar] foo();",
        "(decl foo (type (8 bit .bar)) ())");

    test("void foo(bit a, bit b);",
        "(decl foo (typename void)"
        " (local a (typename bit))"
        " (local b (typename bit)))");

    test("void foo() void bar() end end", NULL);
    test("void const foo() end", NULL);
    test("void inline foo() end", NULL);
    test("void end() end", NULL);
    test("bit foo; void foo() end", NULL);
    test("type foo [1 bit]; void foo() end", NULL);
    test("void foo() end type foo [1 bit]; ", NULL);

    test("void foo() end",
        "(func foo (typename void) () (block ()))");

    test("void foo() bit bar; end",
        "(func foo (typename void) ()"
        " (block (local bar (typename bit))))");

    test("void foo() bit bar = 0; end",
        "(func foo (typename void) ()"
        " (block (local bar (typename bit) 0)))");

    test("void foo() bit a; bit b; end",
        "(func foo (typename void) () (block"
        " (local a (typename bit))"
        " (local b (typename bit))))");

    test("void foo() inline void bar() end end",
        "(func foo (typename void) () (block"
        " (func bar inline (typename void) () (block ()))))");

    test("inline void foo() inline void bar() end end",
        "(func foo inline (typename void) () (block"
        " (func bar inline (typename void) () (block ()))))");

    test("void foo() inline void foo() end end",
        "(func foo (typename void) () (block"
        " (func foo inline (typename void) () (block ()))))");

    test("void foo() bit foo; end",
        "(func foo (typename void) () (block"
        " (local foo (typename bit))))");

    test("bit bar; void foo() [2 bit] bar; end",
        "(global bar (typename bit))"
        " (func foo (typename void) ()"
        " (block (local bar (type (2 bit)))))");

    test("bit foo = bar;", NULL);
    test("bit bar = 1; bit foo = bar;", NULL);
    test("const bit bar = 1; bit foo = bar;", 
        "(global bar const (typename bit) 1) "
        "(global foo (typename bit) (var bar))");
    test("const bit bar = 1; const bit foo = bar;",
        "(global bar const (typename bit) 1) "
        "(global foo const (typename bit) (var bar))");

    test("void foo(bit a) bit a; end", NULL);
}

