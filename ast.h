#ifndef AST_H_
#define AST_H_

#include "arena.h"
#include <stdint.h>

/*
Drafting out what AST nodes we'll need and what they'll have:

(literal n)
(binary-expr x y)
(unary-expr x)
(cast expr type)

Variable definitions

(global name type [constant-expr])
(local name type [expr])
(const-global name type [constant-expr])
(const-local name type [expr])

(args (arg-name arg-type)*)
(function-decl name return-type args)
(function name return-type args body)
(inline-function name return-type args body)

(component n
    bit|pad|align|name
    | (type components))
(components component+)
(type components)
(typedef name type)

Flow control

(while condition body)
(label name)
(goto label-name)
(dispatch label-prefix expr)

"elif" is converted to a nested "if" node, and "else" goes in the
false-branch of the last

(if condition true-branch [false-branch])

(block statement*)
(statement
    assignment-expr
    | while
    | label
    | goto
    | dispatch
    | if
    | local
    | const-local
    | inline-function)

*/

enum node_type {
    AST_TYPE,
    AST_TYPE_COMPONENT,
    AST_TYPE_DEF,
    AST_CONST_EXPR,
};

typedef struct ast_node {
    int ast_type;
    // A node never sets its own cdr, only a containing node
    struct ast_node *cdr;
    union {
        struct { struct ast_node *components; } type;
        struct {
            struct ast_node *count;
            const char *type;
            const char *field;
        } type_component;
        struct {
            const char *name;
            struct ast_node *type;
        } type_def;
        struct { uint64_t value; } const_expr;
    };
} ast_node;

// Returns NULL on failure
ast_node *parse_string(arena *a, const char *text);

// Get the error that stopped the most recent parse attempt, if any
const char *parse_error();

// Render an AST as an S-expression
const char *ast_sexp(ast_node *root);

#endif
