#include "ast.h"
#include "tok.h"
#include "str.h"
#include "fatal.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const char *ast_error() { return "?"; } // todo

// We share this buffer across each iteration of recursion
// to build the string across multiple calls.
static char sexp_str[4096];
static int sexp_idx = 0;
static int sexp_left() {
    int r = sizeof(sexp_str) - sexp_idx;
    return r < 0 ? 0 : r;
}

#define SPRINT(...) do { \
    sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(), \
        __VA_ARGS__); } while (0)

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

    case AST_NAMED_TYPE:
        SPRINT("(typename %s)", root->named_type.name);
        break;

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

    case AST_LITERAL:
        SPRINT("%lu", root->literal.value);
        break;

    case AST_GLOBAL_DEF:
        SPRINT("(global %s ", root->global_def.name);
        if (root->global_def.is_const) {
            SPRINT("const ");
        }
        ast_sexp_recur(root->global_def.type);
        if (root->global_def.init) {
            SPRINT(" ");
            ast_sexp_recur(root->global_def.init);
        }
        SPRINT(")");
        break;

    case AST_LOCAL_DEF:
        SPRINT("(local %s ", root->local_def.name);
        if (root->local_def.is_const) {
            SPRINT("const ");
        }
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
        if (root->function_def.is_inline) {
            SPRINT("inline ");
        }
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

    default:
        SPRINT("?");
        break;
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

// This gets set by ast_from_string. It's more likely this would live in a
// parser state struct but this is quick and easy for now.
static arena *ar = NULL;

static ast_node *new_node(int ast_type) {
    ast_node *n = (ast_node*)arena_alloc(ar, sizeof(ast_node));
    memset(n, 0, sizeof(ast_node));
    n->ast_type = ast_type;
    return n;
}

// Place a node at the end of the list `dst` is a member of.
// Returns the head of the list, which is `node` when `dst` is NULL.
static ast_node *append(ast_node *dst, ast_node *node) {
    if (dst == NULL) { return node; }
    ast_node *head = dst;
    while (dst->cdr != NULL) { dst = dst->cdr; }
    dst->cdr = node;
    return head;
}

static ast_node *type_node(ast_node *components) {
    ast_node *node = new_node(AST_TYPE);
    node->type.components = components;
    return node;
}

static ast_node *named_type_node(const char *name) {
    ast_node *node = new_node(AST_NAMED_TYPE);
    node->named_type.name = intern(name);
    return node;
}

static ast_node *type_component_node(ast_node *count,
                                     const char *type,
                                     const char *field) {
    ast_node *node = new_node(AST_TYPE_COMPONENT);
    node->type_component.count = count;
    node->type_component.type = intern(type);
    node->type_component.field = intern(field);
    return node;
}

static ast_node *type_def_node(const char *name, ast_node *type) {
    ast_node *node = new_node(AST_TYPE_DEF);
    node->type_def.name = intern(name);
    node->type_def.type = type;
    return node;
}

static ast_node *literal_node(uint64_t value) {
    ast_node *node = new_node(AST_LITERAL);
    node->literal.value = value;
    return node;
}

static ast_node *global_def_node(const char *name,
                                 ast_node *type,
                                 ast_node *init,
                                 bool is_const) {
    ast_node *node = new_node(AST_GLOBAL_DEF);
    node->global_def.name = intern(name);
    node->global_def.type = type;
    node->global_def.init = init;
    node->global_def.is_const = is_const;
    return node;
}

static ast_node *local_def_node(const char *name,
                                ast_node *type,
                                ast_node *init,
                                bool is_const) {
    ast_node *node = new_node(AST_LOCAL_DEF);
    node->local_def.name = intern(name);
    node->local_def.type = type;
    node->local_def.init = init;
    node->local_def.is_const = is_const;
    return node;
}

static ast_node *forward_decl_node(const char *name,
                                    ast_node *ret_type,
                                    ast_node *args) {
    ast_node *node = new_node(AST_FORWARD_DECL);
    node->forward_decl.name = intern(name);
    node->forward_decl.ret_type = ret_type;
    node->forward_decl.args = args;
    return node;
}

static ast_node *function_def_node(const char *name,
                                   ast_node *ret_type,
                                   ast_node *args,
                                   ast_node *body,
                                   bool is_inline) {
    ast_node *node = new_node(AST_FUNCTION_DEF);
    node->function_def.name = intern(name);
    node->function_def.ret_type = ret_type;
    node->function_def.args = args;
    node->function_def.body = body;
    node->function_def.is_inline = is_inline;
    return node;
}

static ast_node *block_node(ast_node *statements) {
    ast_node *node = new_node(AST_BLOCK);
    node->block.statements = statements;
    return node;
}

static ast_node *parse_literal() {
    if (tok_maybe(TOK_LITERAL)) {
        return literal_node(tok_value());
    }
    return NULL;
}

static ast_node *parse_expr(bool is_const) {
    // todo: actually fill this in

    ast_node *result = parse_literal();
    if (result) { return result; }

    return NULL;
}

static ast_node *parse_type_component() {
    ast_node *count = parse_expr(true);
    if (count == NULL) {
        return NULL;
    }
    tok_expect_ident("a type");
    const char *type_name = intern(tok_identifier());
    const char *label = NULL;
    if (tok_maybe(TOK_DOT)) {
        tok_expect_ident("a field name");
        label = intern(tok_identifier());
    }
    return type_component_node(count, type_name, label);
}

static ast_node *parse_type_spec() {
    if (!tok_maybe(TOK_LSQUARE)) {
        return NULL;
    }
    ast_node *component = parse_type_component();
    if (component == NULL) {
        FATAL("empty or missing type component\n");
    }
    ast_node *result = type_node(component);
    while ((component = parse_type_component())) {
        result->type.components = append(result->type.components, component);
    }
    tok_expect(TOK_RSQUARE);
    return result;
}

static ast_node *parse_type() {
    if (tok_maybe(TOK_IDENT)) {
        // we'll actually need to check the symtab at some point
        return named_type_node(intern(tok_identifier()));
    }
    return parse_type_spec();
}

static ast_node *parse_type_definition() {
    if (!tok_maybe(TOK_TYPE)) {
        return NULL;
    }
    tok_expect_ident("a name");
    const char *name = intern(tok_identifier());
    ast_node *type = parse_type();
    if (type == NULL) {
        FATAL("definition for type %s is invalid\n", name);
    }
    tok_expect(TOK_SEMI);
    return type_def_node(name, type);
}

static ast_node *parse_arg() {
    ast_node *type = parse_type();
    if (type == NULL) { return NULL; }
    tok_expect_ident("an argument name");
    const char *name = intern(tok_identifier());
    return local_def_node(name, type, NULL, false);
}

static ast_node *parse_args() {
    ast_node *head = NULL;
    while (1) {
        ast_node *arg = parse_arg();
        if (!arg) { return head; }
        head = append(head, arg);
        if (!tok_maybe(TOK_COMMA)) {
            return head;
        }
    }
}

static ast_node *parse_var_or_func_def(bool is_global);

static ast_node *parse_statement() {
    ast_node *stmt = parse_var_or_func_def(false);
    if (stmt) { return stmt; }
    // todo:
    // while
    // if
    // return
    // goto
    // break
    // continue
    // assignment
    // standalone expression (function call, etc)
    return NULL;
}

// Just collect statements until we can't; doesn't check for `end` or anything
static ast_node *parse_block() {
    ast_node *b = block_node(NULL);
    while (1) {
        ast_node *stmt = parse_statement();
        if (!stmt) { return b; }
        b->block.statements = append(b->block.statements, stmt);
    }
}

static ast_node *parse_var_or_func_def(bool is_global) {
    bool is_const = false;
    bool is_inline = false;
    if (tok_maybe(TOK_CONST)) {
        is_const = true;
    } else if (tok_maybe(TOK_INLINE)) {
        is_inline = true;
    }
    ast_node *type = parse_type();
    if (type == NULL) {
        if (is_inline) {
            FATAL("expected function definition after 'inline'\n");
        }
        if (is_const) {
            FATAL("expected variable definition after 'const'\n");
        }
        return NULL;
    }

    tok_expect_ident("a name");
    const char *name = intern(tok_identifier());

    // Now, is this a variable, a foward declaration, or a function?

    // Is there an initial value assignment?
    ast_node *init_or_args = NULL;
    if (tok_maybe(TOK_ASSIGN)) { // must be a variable, then
        // local const can refer to runtime values, global can't
        init_or_args = parse_expr(is_global && is_const);
        if (init_or_args == NULL) {
            FATAL("expected valid init expression for '%s'", name);
        }
    } else if (tok_maybe(TOK_LPAREN)) { // Parens?
        if (!is_global && !is_inline) {
            FATAL("nested function '%s' must be inline\n", name);
        }
        init_or_args = parse_args();
        tok_expect(TOK_RPAREN);
        if (tok_maybe(TOK_SEMI)) {
            if (!is_global) {
                FATAL("can't forward declare nested function '%s'\n", name);
            }
            if (is_inline) {
                FATAL("can't forward declare inline function '%s'\n", name);
            }
            return forward_decl_node(name, type, init_or_args);
        }
        ast_node *body = parse_block();
        tok_expect(TOK_END);
        return function_def_node(name, type, init_or_args, body, is_inline);
    }

    // Not a function
    tok_expect(TOK_SEMI);
    if (is_global) {
        return global_def_node(name, type, init_or_args, is_const);
    }
    return local_def_node(name, type, init_or_args, is_const);
}

static ast_node *parse_definition(bool is_global) {
    // this needs to handle inline and const later
    ast_node *result = NULL;
    if (is_global) {
        result = parse_type_definition();
        if (result) { return result; }
    }
    return parse_var_or_func_def(is_global);
}

static ast_node *parse_top_level() {
    ast_node *result = NULL;
    while (1) {
        if (tok_maybe(TOK_EOF)) { return result; }
        // later some sort of include probably goes here too
        ast_node *next = parse_definition(true);
        if (next == NULL) {
            FATAL("unexpected token: %s\n", tok_name(tok_peek()));
        }
        result = append(result, next);
    }
}

ast_node *ast_from_string(arena *a, const char *text) {
    ar = a;
    tok_from_string(text);
    return parse_top_level();
}

