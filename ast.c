#include "ast.h"
#include "fatal.h"
#include "str.h"
#include "tok.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// We share this buffer across each iteration of recursion
// to build the string across multiple calls.
static char sexp_str[4096];
static int sexp_idx = 0;

static int sexp_left() {
    int r = sizeof(sexp_str) - sexp_idx;
    return r < 0 ? 0 : r;
}

#define SPRINT(...)                                                            \
    do {                                                                       \
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(), __VA_ARGS__);   \
    } while (0)

static void ast_sexp_recur(ast_node *root) {
    if (!root) {
        SPRINT("()");
        return;
    }
    switch (root->ast_type) {
    case AST_TYPE:
        SPRINT("(type ");
        ast_sexp_recur(root->type.components);
        SPRINT(")");
        break;

    case AST_NAMED_TYPE: SPRINT("(typename %s)", root->named_type.name); break;

    case AST_TYPE_COMPONENT:
        SPRINT("(");
        ast_sexp_recur(root->type_component.count);
        SPRINT(" %s", root->type_component.type);
        if (root->type_component.field) {
            SPRINT(" .%s", root->type_component.field);
        }
        SPRINT(")");
        break;

    case AST_TYPE_DEF:
        SPRINT("(typedef %s ", root->type_def.name);
        ast_sexp_recur(root->type_def.type);
        SPRINT(")");
        break;

    case AST_LITERAL: SPRINT("%lu", root->literal.value); break;

    case AST_GLOBAL_DEF:
        SPRINT("(global %s ", root->global_def.name);
        if (root->global_def.is_const) { SPRINT("const "); }
        ast_sexp_recur(root->global_def.type);
        if (root->global_def.init) {
            SPRINT(" ");
            ast_sexp_recur(root->global_def.init);
        }
        SPRINT(")");
        break;

    case AST_LOCAL_DEF:
        SPRINT("(local %s ", root->local_def.name);
        if (root->local_def.is_const) { SPRINT("const "); }
        ast_sexp_recur(root->local_def.type);
        if (root->local_def.init) {
            SPRINT(" ");
            ast_sexp_recur(root->local_def.init);
        }
        SPRINT(")");
        break;

    case AST_FORWARD_DECL:
        SPRINT("(decl %s ", root->forward_decl.name);
        ast_sexp_recur(root->forward_decl.ret_type);
        SPRINT(" ");
        ast_sexp_recur(root->forward_decl.args);
        SPRINT(")");
        break;

    case AST_FUNCTION_DEF:
        SPRINT("(func %s ", root->function_def.name);
        if (root->function_def.is_inline) { SPRINT("inline "); }
        ast_sexp_recur(root->function_def.ret_type);
        SPRINT(" ");
        ast_sexp_recur(root->function_def.args);
        SPRINT(" ");
        ast_sexp_recur(root->function_def.body);
        SPRINT(")");
        break;

    case AST_BLOCK:
        SPRINT("(block ");
        ast_sexp_recur(root->block.statements);
        SPRINT(")");
        break;

    case AST_RET:
        SPRINT("(return");
        if (root->ret.expr != NULL) {
            SPRINT(" ");
            ast_sexp_recur(root->ret.expr);
        }
        SPRINT(")");
        break;

    case AST_VARIABLE: SPRINT("(var %s)", root->variable.name); break;

    default: SPRINT("?"); break;
    }

    if (root->cdr) {
        SPRINT(" ");
        ast_sexp_recur(root->cdr);
    }
}

#undef SPRINT

const char *ast_sexp(ast_node *root) {
    memset(sexp_str, 0, sizeof(sexp_str));
    sexp_idx = 0;
    ast_sexp_recur(root);
    return sexp_str;
}

ast_node *new_node(arena *a, int ast_type) {
    ast_node *n = (ast_node *)arena_alloc(a, sizeof(ast_node));
    memset(n, 0, sizeof(ast_node));
    n->ast_type = ast_type;
    return n;
}

ast_node *ast_type_node(arena *a, ast_node *components) {
    ast_node *node = new_node(a, AST_TYPE);
    node->type.components = components;
    return node;
}

ast_node *ast_named_type_node(arena *a, const char *name) {
    ast_node *node = new_node(a, AST_NAMED_TYPE);
    node->named_type.name = intern(name);
    return node;
}

ast_node *ast_type_component_node(arena *a, ast_node *count, const char *type,
                                  const char *field) {
    ast_node *node = new_node(a, AST_TYPE_COMPONENT);
    node->type_component.count = count;
    node->type_component.type = intern(type);
    node->type_component.field = intern(field);
    return node;
}

ast_node *ast_type_def_node(arena *a, const char *name, ast_node *type) {
    ast_node *node = new_node(a, AST_TYPE_DEF);
    node->type_def.name = intern(name);
    node->type_def.type = type;
    return node;
}

ast_node *ast_literal_node(arena *a, uint64_t value) {
    ast_node *node = new_node(a, AST_LITERAL);
    node->literal.value = value;
    return node;
}

ast_node *ast_global_def_node(arena *a, const char *name, ast_node *type,
                              ast_node *init, bool is_const) {
    ast_node *node = new_node(a, AST_GLOBAL_DEF);
    node->global_def.name = intern(name);
    node->global_def.type = type;
    node->global_def.init = init;
    node->global_def.is_const = is_const;
    return node;
}

ast_node *ast_local_def_node(arena *a, const char *name, ast_node *type,
                             ast_node *init, bool is_const) {
    ast_node *node = new_node(a, AST_LOCAL_DEF);
    node->local_def.name = intern(name);
    node->local_def.type = type;
    node->local_def.init = init;
    node->local_def.is_const = is_const;
    return node;
}

ast_node *ast_forward_decl_node(arena *a, const char *name, ast_node *ret_type,
                                ast_node *args) {
    ast_node *node = new_node(a, AST_FORWARD_DECL);
    node->forward_decl.name = intern(name);
    node->forward_decl.ret_type = ret_type;
    node->forward_decl.args = args;
    return node;
}

ast_node *ast_function_def_node(arena *a, const char *name, ast_node *ret_type,
                                ast_node *args, ast_node *body,
                                bool is_inline) {
    ast_node *node = new_node(a, AST_FUNCTION_DEF);
    node->function_def.name = intern(name);
    node->function_def.ret_type = ret_type;
    node->function_def.args = args;
    node->function_def.body = body;
    node->function_def.is_inline = is_inline;
    return node;
}

ast_node *ast_block_node(arena *a, ast_node *statements) {
    ast_node *node = new_node(a, AST_BLOCK);
    node->block.statements = statements;
    return node;
}

ast_node *ast_ret_node(arena *a, ast_node *expr) {
    ast_node *node = new_node(a, AST_RET);
    node->ret.expr = expr;
    return node;
}

ast_node *ast_variable_node(arena *a, const char *name) {
    ast_node *node = new_node(a, AST_VARIABLE);
    node->variable.name = intern(name);
    return node;
}
