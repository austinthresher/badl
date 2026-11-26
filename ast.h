#ifndef AST_H_
#define AST_H_

#include "arena.h"
#include <stdbool.h>
#include <stdint.h>

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
    AST_BREAK,
    AST_CONTINUE,
    AST_RET,
    AST_VARIABLE,
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

        // AST_BREAK and AST_CONTINUE don't need any fields

        struct { // AST_RET
            struct ast_node *expr;
        } ret;

        struct { // AST_VARIABLE
            const char *name;
        } variable;
    };

    // A node never sets its own cdr, only a containing node.
    // This allows any ast_node to be the head of a list without needing a
    // separate list type.
    struct ast_node *cdr;
} ast_node;

ast_node *ast_type_node(arena *a, ast_node *components);
ast_node *ast_named_type_node(arena *a, const char *name);
ast_node *ast_type_component_node(arena *a, ast_node *count, const char *type,
                                  const char *field);
ast_node *ast_type_def_node(arena *a, const char *name, ast_node *type);
ast_node *ast_literal_node(arena *a, uint64_t value);
ast_node *ast_global_def_node(arena *a, const char *name, ast_node *type,
                              ast_node *init, bool is_const);
ast_node *ast_local_def_node(arena *a, const char *name, ast_node *type,
                             ast_node *init, bool is_const);
ast_node *ast_forward_decl_node(arena *a, const char *name, ast_node *rtype,
                                ast_node *args);
ast_node *ast_function_def_node(arena *a, const char *name, ast_node *rtype,
                                ast_node *args, ast_node *body, bool is_inline);
ast_node *ast_block_node(arena *a, ast_node *statements);
ast_node *ast_ret_node(arena *a, ast_node *expr);
ast_node *ast_variable_node(arena *a, const char *name);

// Render an AST as an S-expression
const char *ast_sexp(ast_node *root);

#endif
