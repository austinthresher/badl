#ifndef AST_H_
#define AST_H_

#include "arena.h"
#include <stdbool.h>
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
    AST_NAMED_TYPE,
    AST_TYPE_COMPONENT,
    AST_TYPE_DEF,
    AST_LITERAL,
    AST_GLOBAL_DEF,
    AST_LOCAL_DEF, // also used for function args
    AST_FORWARD_DECL,
    AST_FUNCTION_DEF,
    AST_BLOCK,
};

typedef struct ast_node {
    int ast_type;
    union {
        struct { // AST_TYPE
            struct ast_node *components;
        } type;

        struct { // AST_NAMED_TYPE
            const char *name;
        } named_type;

        struct { // AST_TYPE_COMPONENT
            struct ast_node *count;
            const char *type;
            const char *field;
        } type_component;

        struct { // AST_TYPE_DEF
            const char *name;
            struct ast_node *type;
        } type_def;

        struct { // AST_LITERAL
            uint64_t value;
        } literal;

        struct { // AST_GLOBAL_DEF
            const char *name;
            struct ast_node *type;
            struct ast_node *init;
            bool is_const;
        } global_def;

        struct { // AST_LOCAL_DEF
            const char *name;
            struct ast_node *type;
            struct ast_node *init;
            bool is_const;
        } local_def;

        struct { // AST_FORWARD_DECL
            const char *name;
            struct ast_node *ret_type;
            struct ast_node *args;
        } forward_decl;

        struct { // AST_FUNCTION_DEF
            const char *name;
            struct ast_node *ret_type;
            struct ast_node *args;
            struct ast_node *body;
            bool is_inline;
        } function_def;

        struct { // AST_BLOCK
            struct ast_node *statements;
        } block;

    };

    // A node never sets its own cdr, only a containing node.
    // This allows any ast_node to be the head of a list without needing a
    // separate list type.
    struct ast_node *cdr;
} ast_node;

// Returns NULL on failure
ast_node *parse_string(arena *a, const char *text);

// Get the error that stopped the most recent parse attempt, if any
const char *parse_error();

// Render an AST as an S-expression
const char *ast_sexp(ast_node *root);

#endif
