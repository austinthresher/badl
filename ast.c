#include "ast.h"
#include "str.h"
#include "fatal.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const char *parse_error() { return "?"; } // todo

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

    case AST_FUNCTION_DECL:
        SPRINT("(decl %s ", root->function_decl.name);
        ast_sexp_recur(root->function_decl.ret_type);
        SPRINT(" ");
        ast_sexp_recur(root->function_decl.args);
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

// This gets set by parse_string, it's more likely this would live in a
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

static ast_node *function_decl_node(const char *name,
                                    ast_node *ret_type,
                                    ast_node *args) {
    ast_node *node = new_node(AST_FUNCTION_DECL);
    node->function_decl.name = intern(name);
    node->function_decl.ret_type = ret_type;
    node->function_decl.args = args;
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

static const char *in_buf = NULL;
static int in_idx = 0;
static int in_len = 0;

static int in_left() {
    int r = in_len - in_idx;
    return r < 0 ? 0 : r;
}
static int nextc() {
    if (!in_buf || !in_left()) { return EOF; }
    return in_buf[in_idx++];
}
static void prevc() {
    if (in_idx > 0) { in_idx--; }
}

enum tokens {
    TOK_LSQUARE, TOK_RSQUARE, TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_COMMA, TOK_SEMI, TOK_COLON, TOK_PLUS, TOK_MINUS, TOK_MULT, TOK_DIV,
    TOK_ASSIGN, TOK_IASSIGN, TOK_LT, TOK_GT, TOK_LTE, TOK_GTE, TOK_EQUAL,
    TOK_AND, TOK_OR, TOK_XOR, TOK_INVERT, TOK_NOT, TOK_ANDL, TOK_ORL,
    TOK_RSHIFT, TOK_LSHIFT, TOK_DOT, TOK_AT, TOK_LITERAL, TOK_IDENT,
    TOK_TYPE, TOK_INLINE, TOK_CONST, TOK_END, TOK_GOTO, TOK_IF, TOK_WHILE,
    TOK_DISPATCH, TOK_RETURN, TOK_EOF
    // todo: Decide if modulo operator uses "mod". If so, maybe use "and"
    // and "or" for the logical versions to distinguish from & and |
};

static char tok_buf[128]; // Only set for identifiers, expand for strings later
uint64_t tok_value = 0; // Only set for literals
static int stashed_token = -1;
uint64_t stashed_value = 0;
static char stashed_buf[sizeof(tok_buf)];

static void reset_tokenizer() {
    memset(tok_buf, 0, sizeof(tok_buf));
    memset(stashed_buf, 0, sizeof(stashed_buf));
    stashed_token = -1;
    tok_value = stashed_value = 0;
}

const char *token_string(int tok) {
    static char buf[256];
    switch (tok) {
    case TOK_LSQUARE: return "'['";
    case TOK_RSQUARE: return "']'";
    case TOK_LPAREN: return "'('";
    case TOK_RPAREN: return "')'";
    case TOK_LBRACE: return "'{'";
    case TOK_RBRACE: return "'}'";
    case TOK_COMMA: return "','";
    case TOK_SEMI: return "';'";
    case TOK_COLON: return "':'";
    case TOK_PLUS: return "'+'";
    case TOK_MINUS: return "'-'";
    case TOK_MULT: return "'*'";
    case TOK_DIV: return "'/'";
    case TOK_ASSIGN: return "'='";
    case TOK_IASSIGN: return "'@='";
    case TOK_LT: return "'<'";
    case TOK_GT: return "'>'";
    case TOK_LTE: return "'<='";
    case TOK_GTE: return "'>='";
    case TOK_EQUAL: return "'=='";
    case TOK_AND: return "'&'";
    case TOK_OR: return "'|'";
    case TOK_XOR: return "'^'";
    case TOK_INVERT: return "'~'";
    case TOK_NOT: return "'!'";
    case TOK_ANDL: return "'&&'";
    case TOK_ORL: return "'||'";
    case TOK_RSHIFT: return "'>>'";
    case TOK_LSHIFT: return "'<<'";
    case TOK_DOT: return "'.'";
    case TOK_AT: return "'@'";
    case TOK_TYPE: return "'type'";
    case TOK_INLINE: return "'inline'";
    case TOK_CONST: return "'const'";
    case TOK_END: return "'end'";
    case TOK_GOTO: return "'goto'";
    case TOK_IF: return "'if'";
    case TOK_WHILE: return "'while'";
    case TOK_DISPATCH: return "'dispatch'";
    case TOK_RETURN: return "'return'";
    case TOK_EOF: return "'EOF'";
    case TOK_IDENT:
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "identifier '%s'", tok_buf);
        return intern(buf);
    case TOK_LITERAL:
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "literal '%lu'", tok_value);
        return intern(buf);
    default:
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "unknown token type %d", tok);
        return intern(buf);
    }
};

static int hex_value(char c) {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return 0;
}

static void tokenize_hex() {
    tok_value = 0;
    while (1) {
        int c = nextc();
        if (!isxdigit(c)) { // todo: error on no digits
            prevc();
            return;
        }
        tok_value *= 16;
        tok_value += hex_value(c);
    }
}

static void tokenize_binary() {
    tok_value = 0;
    while (1) {
        int c = nextc();
        if (c != '0' && c != '1') { // todo: error on no digits
            prevc();
            return;
        }
        tok_value *= 2;
        tok_value += c - '0';
    }
}

static void tokenize_decimal() {
    tok_value = 0;
    while (1) {
        int c = nextc();
        if (!isdigit(c)) {
            prevc();
            return;
        }
        tok_value *= 10;
        tok_value += c - '0';
    }
}

static void tokenize_identifier() {
    memset(tok_buf, 0, sizeof(tok_buf));
    int idx = 0;
    while (idx < sizeof(tok_buf)-1) {
        int c = nextc();
        if (!isalnum(c) && c != '_') {
            prevc();
            return;
        }
        tok_buf[idx++] = c;
    }
    FATAL("identifier too long: %s\n", tok_buf);
}

static int advance_token() {
    int c;
    do {
        c = nextc();
        if (c == '#') { // skip comments
            while (c != '\n' && c != EOF) { c = nextc(); }
        }
    } while (isspace(c));

    // unambiguous single-character tokens
    switch (c) {
    case '[': return TOK_LSQUARE;
    case ']': return TOK_RSQUARE;
    case '(': return TOK_LPAREN;
    case ')': return TOK_RPAREN;
    case '{': return TOK_LBRACE;
    case '}': return TOK_RBRACE;
    case ',': return TOK_COMMA;
    case ';': return TOK_SEMI;
    case ':': return TOK_COLON;
    case '+': return TOK_PLUS;
    case '-': return TOK_MINUS; // todo: negative literals
    case '*': return TOK_MULT;
    case '/': return TOK_DIV;
    case '^': return TOK_XOR;
    case '~': return TOK_INVERT;
    case '!': return TOK_NOT;
    case '.': return TOK_DOT;
    case EOF: return TOK_EOF;
    default: break;
    }

    // Disambiguate between one and two character variants
    switch (c) {
    case '=':
        if (nextc() == '=') { return TOK_EQUAL; }
        prevc();
        return TOK_ASSIGN;
    case '<':
        if (nextc() == '<') { return TOK_LSHIFT; }
        prevc();
        if (nextc() == '=') { return TOK_LTE; }
        prevc();
        return TOK_LT;
    case '>':
        if (nextc() == '>') { return TOK_RSHIFT; }
        prevc();
        if (nextc() == '=') { return TOK_GTE; }
        prevc();
        return TOK_GT;
    case '&':
        if (nextc() == '&') { return TOK_ANDL; }
        prevc();
        return TOK_AND;
    case '|':
        if (nextc() == '|') { return TOK_ORL; }
        prevc();
        return TOK_OR;
    case '@':
        if (nextc() == '=') { return TOK_IASSIGN; }
        prevc();
        return TOK_AT;
    default: break;
    }
    if (c == '$') {
        tokenize_hex();
        return TOK_LITERAL;
    }
    if (c == '%') {
        tokenize_binary();
        return TOK_LITERAL;
    }
    if (isdigit(c)) {
        prevc();
        tokenize_decimal();
        return TOK_LITERAL;
    }
    if (!isalpha(c) && c != '_') {
        // todo: handle this more gracefully
        FATAL("unexpected character: %c (%d)\n", (char)c, c);
    }
    prevc();
    tokenize_identifier();

    // Now check for keywords before we decide this is actually an identifier
    if (strcmp(tok_buf, "end") == 0) { return TOK_END; }
    if (strcmp(tok_buf, "type") == 0) { return TOK_TYPE; }
    if (strcmp(tok_buf, "if") == 0) { return TOK_IF; }
    if (strcmp(tok_buf, "while") == 0) { return TOK_WHILE; }
    if (strcmp(tok_buf, "const") == 0) { return TOK_CONST; }
    if (strcmp(tok_buf, "return") == 0) { return TOK_RETURN; }
    if (strcmp(tok_buf, "inline") == 0) { return TOK_INLINE; }
    if (strcmp(tok_buf, "goto") == 0) { return TOK_GOTO; }
    if (strcmp(tok_buf, "dispatch") == 0) { return TOK_DISPATCH; }

    // We'll have to look at the text with more context
    return TOK_IDENT;
}

static int next_token() {
    if (stashed_token != -1) {
        int tok = stashed_token;
        tok_value = stashed_value;
        memcpy(tok_buf, stashed_buf, sizeof(tok_buf));
        stashed_token = -1;
        return tok;
    }
    return advance_token();
}

static void undo_token(int tok) {
    if (stashed_token != -1) {
        FATAL("undo would overwrite stashed token %s (%d) with %s (%d)\n",
              token_string(stashed_token), stashed_token,
              token_string(tok), tok);
    }
    stashed_token = tok;
    memcpy(stashed_buf, tok_buf, sizeof(stashed_buf));
    stashed_value = tok_value;
}

// I might need to temporarily grab stashed_token and co if this errors
static int peek_token() {
    int tok = next_token();
    undo_token(tok);
    return tok;
}

static bool maybe_token(int tok) {
    if (peek_token() == tok) {
        next_token();
        return true;
    }
    return false;
}

static void expect_token_impl(int line, const char *fn, int tok) {
    int next = next_token();
    if (next != tok) {
        FATAL_AT(line, fn, "expected %s, got: %s\n",
                 token_string(tok), token_string(next));
    }
}
#define expect_token(_tok) expect_token_impl(__LINE__, __func__, (_tok))

static void expect_ident_impl(int line, const char *fn, const char *desc) {
    int next = next_token();
    if (next != TOK_IDENT) {
        FATAL_AT(line, fn, "expected %s, got: %s\n",
                 desc, token_string(next));
    }
}
#define expect_ident(_desc) expect_ident_impl(__LINE__, __func__, (_desc))


static ast_node *parse_literal() {
    if (maybe_token(TOK_LITERAL)) {
        return literal_node(tok_value);
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
    expect_ident("a type");
    const char *type_name = intern(tok_buf);
    const char *label = NULL;
    if (maybe_token(TOK_DOT)) {
        expect_ident("a field name");
        label = intern(tok_buf);
    }
    return type_component_node(count, type_name, label);
}

static ast_node *parse_type_spec() {
    if (!maybe_token(TOK_LSQUARE)) {
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
    expect_token(TOK_RSQUARE);
    return result;
}

static ast_node *parse_type() {
    if (maybe_token(TOK_IDENT)) {
        // we'll actually need to check the symtab at some point
        return named_type_node(intern(tok_buf));
    }
    return parse_type_spec();
}

static ast_node *parse_type_definition() {
    if (!maybe_token(TOK_TYPE)) {
        return NULL;
    }
    expect_ident("a name");
    const char *name = intern(tok_buf);
    ast_node *type = parse_type();
    if (type == NULL) {
        FATAL("definition for type %s is invalid\n", name);
    }
    expect_token(TOK_SEMI);
    return type_def_node(name, type);
}

static ast_node *parse_arg() {
    ast_node *type = parse_type();
    if (type == NULL) { return NULL; }
    expect_ident("an argument name");
    const char *name = intern(tok_buf);
    return local_def_node(name, type, NULL, false);
}

static ast_node *parse_args() {
    ast_node *head = NULL;
    while (1) {
        ast_node *arg = parse_arg();
        if (!arg) { return head; }
        head = append(head, arg);
        if (!maybe_token(TOK_COMMA)) {
            return head;
        }
    }
}

static ast_node *parse_var_or_func_def(bool is_global) {
    bool is_const = false;
    bool is_inline = false;
    if (maybe_token(TOK_CONST)) {
        is_const = true;
    } else if (maybe_token(TOK_INLINE)) {
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

    expect_ident("a name");
    const char *name = intern(tok_buf);

    // Now, is this a variable, a foward declaration, or a function?

    // Is there an initial value assignment?
    ast_node *init_or_args = NULL;
    if (maybe_token(TOK_ASSIGN)) { // must be a variable, then
        // local const can refer to runtime values, global can't
        init_or_args = parse_expr(is_global && is_const);
        if (init_or_args == NULL) {
            FATAL("expected valid init expression for '%s'", name);
        }
    } else if (maybe_token(TOK_LPAREN)) { // Parens?
        init_or_args = parse_args();
        expect_token(TOK_RPAREN);
        if (maybe_token(TOK_SEMI)) {
            if (is_inline) {
                FATAL("can't forward declare inline function '%s'\n", name);
            }
            return function_decl_node(name, type, init_or_args);
        }
        FATAL("todo: function body");
    }

variable:
    expect_token(TOK_SEMI);
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
        if (peek_token() == TOK_EOF) { return result; }
        ast_node *next = NULL;

        next = parse_definition(true);

        if (next == NULL) {
            FATAL("unexpected token: %s\n", token_string(peek_token()));
        } else {
            result = append(result, next);
        }
    }
}

// fixme: this name is now confusing with the internal naming I've been using
ast_node *parse_string(arena *a, const char *text) {
    ar = a;
    in_buf = text;
    in_len = strlen(text);
    in_idx = 0;
    reset_tokenizer();
    return parse_top_level();
}

