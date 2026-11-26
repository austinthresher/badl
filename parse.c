#include "parse.h"
#include "ast.h"
#include "tok.h"
#include "str.h"
#include "fatal.h"
#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static char error_msg[512];
const char *parse_error() { return error_msg; }

static jmp_buf err_jmp_buf;

static void fail(const char *fmt, const char *arg) {
    char linebuf[32];
    int off = snprintf(linebuf, sizeof(linebuf), "[error] line %d: ", tok_line()+1);
    strncpy(error_msg, linebuf, sizeof(error_msg));
    if (arg == NULL) {
        strncpy(&error_msg[off], fmt, sizeof(error_msg)-1-off);
    } else {
        snprintf(&error_msg[off], sizeof(error_msg)-off, fmt, arg);
    }
    longjmp(err_jmp_buf, 1);
}

static void tokenizer_fail(const char *msg) { fail(msg, NULL); }

// This gets set by parse_from_string. It's more likely this would live in a
// parser state struct but this is quick and easy for now.
static arena *ar = NULL;

// Place a node at the end of the list `dst` is a member of.
// Returns the head of the list, which is `node` when `dst` is NULL.
static ast_node *append(ast_node *dst, ast_node *node) {
    if (dst == NULL) { return node; }
    ast_node *head = dst;
    while (dst->cdr != NULL) { dst = dst->cdr; }
    dst->cdr = node;
    return head;
}

static ast_node *parse_literal() {
    if (tok_maybe(TOK_LITERAL)) {
        return ast_literal_node(ar, tok_value());
    }
    return NULL;
}

static ast_node *parse_variable() {
    if (tok_maybe(TOK_IDENT)) {
        return ast_variable_node(ar, tok_identifier());
    }
    return NULL;
}

static ast_node *parse_expr(bool is_const) {
    // todo: actually fill this in

    ast_node *result = parse_literal();
    if (result) { return result; }
    // fixme: if we're const we need to check if the variable is const too,
    // disallowing variables for const until the symtab is in
    if (!is_const) {
        result = parse_variable();
        if (result) { return result; }
    }

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
    return ast_type_component_node(ar, count, type_name, label);
}

static ast_node *parse_type_spec() {
    if (!tok_maybe(TOK_LSQUARE)) {
        return NULL;
    }
    ast_node *component = parse_type_component();
    if (component == NULL) {
        fail("empty or missing type component", NULL);
    }
    ast_node *result = ast_type_node(ar, component);
    while ((component = parse_type_component())) {
        result->type.components = append(result->type.components, component);
    }
    tok_expect(TOK_RSQUARE);
    return result;
}

static ast_node *parse_type() {
    if (tok_maybe(TOK_IDENT)) {
        // fixme: assignment statements are unparseable until we check the
        // symtab here to see if the identifier is actually a type
        return ast_named_type_node(ar, intern(tok_identifier()));
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
        fail("definition for type '%s' is invalid", name);
    }
    tok_expect(TOK_SEMI);
    return ast_type_def_node(ar, name, type);
}

static ast_node *parse_arg() {
    ast_node *type = parse_type();
    if (type == NULL) { return NULL; }
    tok_expect_ident("an argument name");
    const char *name = intern(tok_identifier());
    return ast_local_def_node(ar, name, type, NULL, false);
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

static ast_node *parse_return_statement() {
    if (!tok_maybe(TOK_RETURN)) {
        return NULL;
    }
    ast_node *expr = NULL;
    if (!tok_maybe(TOK_SEMI)) {
        expr = parse_expr(false);
        if (!expr) {
            fail("expected expression or ';' after 'return'", NULL);
        }
        tok_expect(TOK_SEMI);
    }
    return ast_ret_node(ar, expr);
}

static ast_node *parse_var_or_func_def(bool is_global);

static ast_node *parse_statement() {
    ast_node *stmt = parse_var_or_func_def(false);
    if (stmt) { return stmt; }
    stmt = parse_return_statement();
    if (stmt) { return stmt; }
    // todo:
    // while
    // if
    // goto
    // break
    // continue
    // assignment
    // standalone expression (function call, etc)
    return NULL;
}

// Just collect statements until we can't; doesn't check for `end` or anything
static ast_node *parse_block() {
    ast_node *b = ast_block_node(ar, NULL);
    while (1) {
        ast_node *stmt = parse_statement();
        if (!stmt) { return b; }
        b->block.statements = append(b->block.statements, stmt);
    }
}

// This is pretty long but I'm not sure how else we can hold off on committing
// to a function or variable until we see past the name
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
            fail("expected function definition after 'inline'", NULL);
        }
        if (is_const) {
            fail("expected variable definition after 'const'", NULL);
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
            fail("invalid init expression for '%s'", name);
        }
    } else if (tok_maybe(TOK_LPAREN)) { // Parens?
        if (!is_global && !is_inline) {
            fail("nested function '%s' must be inline", name);
        }
        init_or_args = parse_args();
        tok_expect(TOK_RPAREN);
        if (is_const) {
            fail("function '%s' can't be const", name);
        }
        if (tok_maybe(TOK_SEMI)) {
            if (!is_global) {
                fail("can't forward declare nested function '%s'", name);
            }
            if (is_inline) {
                fail("can't forward declare inline function '%s'", name);
            }
            return ast_forward_decl_node(ar, name, type, init_or_args);
        }
        ast_node *body = parse_block();
        tok_expect(TOK_END);
        return ast_function_def_node(ar, name, type, init_or_args, body, is_inline);
    }

    // Not a function
    tok_expect(TOK_SEMI);
    if (is_global) {
        return ast_global_def_node(ar, name, type, init_or_args, is_const);
    }
    return ast_local_def_node(ar, name, type, init_or_args, is_const);
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
            fail("unexpected token: %s", tok_name(tok_peek()));
        }
        result = append(result, next);
    }
}

void ast_init_parser(arena *a) {
    memset(error_msg, 0, sizeof(error_msg));
    error_msg[0] = '?';
    ar = a;
    tok_on_error(tokenizer_fail);
}

ast_node *parse_from_string(arena *a, const char *text) {
    tok_from_string(text);
    ast_init_parser(a);
    if (setjmp(err_jmp_buf)) {
        return NULL;
    }
    return parse_top_level();
}

