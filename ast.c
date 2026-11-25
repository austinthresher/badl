#include "ast.h"
#include "str.h"
#include "fatal.h"
#include <ctype.h>
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

static void ast_sexp_recur(ast_node *root) {
    if (!root) {
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(), "()");
        return;
    }
    switch (root->ast_type) {
    case AST_TYPE:
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(), "(type ");
        ast_sexp_recur(root->type.components);
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(), ")");
        break;

    case AST_TYPE_COMPONENT:
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(), "(");
        ast_sexp_recur(root->type_component.count);
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(),
            " %s", root->type_component.type);
        if (root->type_component.field) {
            sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(),
                " .%s", root->type_component.field);
        }
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(), ")");
        break;

    case AST_TYPE_DEF:
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(),
            "(typedef %s ", root->type_def.name);
        ast_sexp_recur(root->type_def.type);
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(), ")");
        break;

    case AST_CONST_EXPR:
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(),
            "%lu", root->const_expr.value);
        break;

    default:
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(), "?");
        break;
    }

    if (root->cdr) {
        sexp_idx += snprintf(&sexp_str[sexp_idx], sexp_left(), " ");
        ast_sexp_recur(root->cdr);
    }
}

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

static ast_node *const_expr_node(uint64_t value) {
    ast_node *node = new_node(AST_CONST_EXPR);
    node->const_expr.value = value;
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
    static char buf[128];
    switch (tok) {
    case TOK_LSQUARE: return "[";
    case TOK_RSQUARE: return "]";
    case TOK_LPAREN: return "(";
    case TOK_RPAREN: return ")";
    case TOK_LBRACE: return "{";
    case TOK_RBRACE: return "}";
    case TOK_COMMA: return ",";
    case TOK_SEMI: return ";";
    case TOK_COLON: return ":";
    case TOK_PLUS: return "+";
    case TOK_MINUS: return "-";
    case TOK_MULT: return "*";
    case TOK_DIV: return "/";
    case TOK_ASSIGN: return "=";
    case TOK_IASSIGN: return "@=";
    case TOK_LT: return "<";
    case TOK_GT: return ">";
    case TOK_LTE: return "<=";
    case TOK_GTE: return ">=";
    case TOK_EQUAL: return "==";
    case TOK_AND: return "&";
    case TOK_OR: return "|";
    case TOK_XOR: return "^";
    case TOK_INVERT: return "~";
    case TOK_NOT: return "!";
    case TOK_ANDL: return "&&";
    case TOK_ORL: return "||";
    case TOK_RSHIFT: return ">>";
    case TOK_LSHIFT: return "<<";
    case TOK_DOT: return ".";
    case TOK_AT: return "@";
    case TOK_TYPE: return "type";
    case TOK_INLINE: return "inline";
    case TOK_CONST: return "const";
    case TOK_END: return "end";
    case TOK_GOTO: return "goto";
    case TOK_IF: return "if";
    case TOK_WHILE: return "while";
    case TOK_DISPATCH: return "dispatch";
    case TOK_RETURN: return "return";
    case TOK_EOF: return "EOF";
    case TOK_IDENT:
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%s", tok_buf);
        return intern(buf);
    case TOK_LITERAL:
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%lu", tok_value);
        return intern(buf);
    default:
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "unknown %d", tok);
        return intern(buf);
    }
};


int hex_value(char c) {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return 0;
}

static void parse_hex() {
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

static void parse_binary() {
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

static void parse_decimal() {
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

static void parse_identifier() {
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
            while (c != '\n') { c = nextc(); }
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
        parse_hex();
        return TOK_LITERAL;
    }
    if (c == '%') {
        parse_binary();
        return TOK_LITERAL;
    }
    if (isdigit(c)) {
        prevc();
        parse_decimal();
        return TOK_LITERAL;
    }
    if (!isalpha(c) && c != '_') {
        // todo: handle this more gracefully
        FATAL("unexpected character: %c (%d)\n", (char)c, c);
    }
    prevc();
    parse_identifier();
    
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

static ast_node *parse_const_expr() {
    // Later this will actually resolve expressions, not just literals
    if (peek_token() == TOK_LITERAL) {
        next_token();
        return const_expr_node(tok_value);
    }
    return NULL;
}

static ast_node *parse_type_component() {
    ast_node *count = parse_const_expr();
    if (count == NULL) {
        return NULL;
    }
    int tok = next_token();
    if (tok != TOK_IDENT) {
        FATAL("expected a type, got %s\n", token_string(tok));
    }
    const char *type_name = intern(tok_buf);
    const char *label = NULL;
    if (peek_token() == TOK_DOT) {
        next_token();
        tok = next_token();
        if (tok != TOK_IDENT) {
            FATAL("expected a field name, got %s\n", token_string(tok));
        }
        label = intern(tok_buf);
    }
    return type_component_node(count, type_name, label);
}

static ast_node *parse_type_spec() {
    if (peek_token() != TOK_LSQUARE) {
        return NULL;
    }
    next_token();
    ast_node *component = parse_type_component();
    if (component == NULL) {
        FATAL("empty or missing type component\n");
    }
    ast_node *result = type_node(component);
    while ((component = parse_type_component())) {
        result->type.components = append(result->type.components, component);
    }
    int tok = next_token();
    if (tok != TOK_RSQUARE) {
        FATAL("expected ']', got %s\n", token_string(tok));
    }
    return result;
}

static ast_node *parse_type() {
    int tok = next_token();
    if (tok == TOK_IDENT) {
        // we'd actually need to check the symtab here
        const char *type_name = intern(tok_buf);
        // If we're getting a raw name for a type there can't be a field name.
        // This means that 'type foo bar;` is the same as `type foo [1 bar];`.
        // Once we have a symtab we should probably look up the definition and
        // copy it instead, because indexing that would not work as expected.
        ast_node *component = type_component_node(const_expr_node(1),
                                                  type_name, NULL);
        return type_node(component);
    }
    undo_token(tok);
    return parse_type_spec();
}

static ast_node *parse_type_def() {
    if (peek_token() != TOK_TYPE) {
        return NULL;
    }
    next_token();
    int tok = next_token();
    if (tok != TOK_IDENT) {
        FATAL("expected identifier, got token %s\n", token_string(tok));
    }
    const char *name = intern(tok_buf);
    ast_node *type = parse_type();
    if (type == NULL) {
        FATAL("definition for type %s is invalid\n", name);
    }
    return type_def_node(name, type);
}

static ast_node *parse_definition() {
    // this needs to handle inline and const later
    ast_node *result = parse_type_def();
    if (result) {
        int tok = next_token();
        if (tok != TOK_SEMI) {
            FATAL("expected ';', got %s\n", token_string(tok));
        }
    }
    return result;
}

static ast_node *parse_top_level() {
    ast_node *result = NULL;
    while (1) {
        if (peek_token() == TOK_EOF) { return result; }
        ast_node *next = NULL;

        next = parse_definition();

        if (next == NULL) {
            FATAL("unexpected token %d %s\n", peek_token(), tok_buf);
        } else {
            result = append(result, next);
        }
    }
}

ast_node *parse_string(arena *a, const char *text) {
    ar = a;
    in_buf = text;
    in_len = strlen(text);
    in_idx = 0;
    reset_tokenizer();
    return parse_top_level();
}

