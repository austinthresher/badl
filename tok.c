#include "tok.h"
#include "fatal.h"
#include "str.h"
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

static tok_error_cb on_error = NULL;
static char error_msg[512];

void tok_on_error(tok_error_cb cb) { on_error = cb; }

static void fail(const char *fmt, const char *arg0, const char *arg1) {
    if (arg0 == NULL && arg1 == NULL) {
        strncpy(error_msg, fmt, sizeof(error_msg) - 1);
    } else if (arg1 == NULL) {
        snprintf(error_msg, sizeof(error_msg), fmt, arg0);
    } else {
        snprintf(error_msg, sizeof(error_msg), fmt, arg0, arg1);
    }
    if (on_error == NULL) {
        FATAL("tokenizer error: %s\n", error_msg);
    } else {
        // This is expected to not return
        on_error(error_msg);
    }
}

static char tok_buf[128]; // Only set for identifiers, expand for strings later
static uint64_t token_value = 0; // Only set for literals
static int stashed_token = -1;
static uint64_t stashed_value = 0;
static char stashed_buf[sizeof(tok_buf)];
static const char *in_buf = NULL;
static int in_idx = 0;
static int in_len = 0;
static int cur_line = 0;

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

int tok_line() { return cur_line; }

void tok_reset() {
    memset(tok_buf, 0, sizeof(tok_buf));
    memset(stashed_buf, 0, sizeof(stashed_buf));
    stashed_token = -1;
    token_value = stashed_value = 0;
    in_buf = "";
    in_len = 0;
    in_idx = 0;
    cur_line = 0;
}

void tok_from_string(const char *text) {
    tok_reset();
    in_buf = text;
    in_len = strlen(text);
    in_idx = 0;
}

const char *tok_name(int tok) {
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
        snprintf(buf, sizeof(buf), "literal '%lu'", token_value);
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
    token_value = 0;
    while (1) {
        int c = nextc();
        if (!isxdigit(c)) { // todo: error on no digits
            if (c != EOF) { prevc(); }
            return;
        }
        token_value *= 16;
        token_value += hex_value(c);
    }
}

static void tokenize_binary() {
    token_value = 0;
    while (1) {
        int c = nextc();
        if (c != '0' && c != '1') { // todo: error on no digits
            if (c != EOF) { prevc(); }
            return;
        }
        token_value *= 2;
        token_value += c - '0';
    }
}

static void tokenize_decimal() {
    token_value = 0;
    while (1) {
        int c = nextc();
        if (!isdigit(c)) {
            if (c != EOF) { prevc(); }
            return;
        }
        token_value *= 10;
        token_value += c - '0';
    }
}

static void tokenize_identifier() {
    memset(tok_buf, 0, sizeof(tok_buf));
    int idx = 0;
    while (idx < sizeof(tok_buf) - 1) {
        int c = nextc();
        if (!isalnum(c) && c != '_') {
            if (c != EOF) { prevc(); }
            return;
        }
        tok_buf[idx++] = c;
    }
    fail("identifier too long: %s", tok_buf, NULL);
}

static int advance_token() {
    int c;
    do {
        c = nextc();
        if (c == '#') { // skip comments
            while (c != '\n' && c != EOF) { c = nextc(); }
        }
        if (c == '\n') { cur_line++; }
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
        char buf[2];
        buf[0] = (char)c;
        buf[1] = '\0';
        fail("unexpected character: %s", buf, NULL);
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

int tok_next() {
    if (stashed_token != -1) {
        int tok = stashed_token;
        token_value = stashed_value;
        memcpy(tok_buf, stashed_buf, sizeof(tok_buf));
        stashed_token = -1;
        return tok;
    }
    return advance_token();
}

int tok_peek() {
    int tok = tok_next();
    tok_undo(tok);
    return tok;
}

void tok_undo(int tok) {
    if (stashed_token != -1) {
        // This should never happen so I'm leaving it fatal
        FATAL("undo would overwrite stashed token %s (%d) with %s (%d)\n",
              tok_name(stashed_token), stashed_token, tok_name(tok), tok);
    }
    stashed_token = tok;
    memcpy(stashed_buf, tok_buf, sizeof(stashed_buf));
    stashed_value = token_value;
}

bool tok_maybe(int tok) {
    if (tok_peek() == tok) {
        tok_next();
        return true;
    }
    return false;
}

uint64_t tok_value() { return token_value; }

const char *tok_identifier() { return tok_buf; }

void tok_expect(int tok) {
    int next = tok_next();
    if (next != tok) {
        fail("expected %s, got %s", tok_name(tok), tok_name(next));
    }
}

void tok_expect_ident(const char *desc) {
    int next = tok_next();
    if (next != TOK_IDENT) {
        fail("expected %s, got %s", desc, tok_name(next));
    }
}
