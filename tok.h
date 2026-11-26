#ifndef TOK_H_
#define TOK_H_

#include <stdbool.h>
#include <stdint.h>

enum tokens {
    TOK_LSQUARE,
    TOK_RSQUARE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_COMMA,
    TOK_SEMI,
    TOK_COLON,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULT,
    TOK_DIV,
    TOK_ASSIGN,
    TOK_IASSIGN,
    TOK_LT,
    TOK_GT,
    TOK_LTE,
    TOK_GTE,
    TOK_EQUAL,
    TOK_AND,
    TOK_OR,
    TOK_XOR,
    TOK_INVERT,
    TOK_NOT,
    TOK_ANDL,
    TOK_ORL,
    TOK_RSHIFT,
    TOK_LSHIFT,
    TOK_DOT,
    TOK_AT,
    TOK_LITERAL,
    TOK_IDENT,
    TOK_TYPE,
    TOK_INLINE,
    TOK_CONST,
    TOK_END,
    TOK_GOTO,
    TOK_IF,
    TOK_WHILE,
    TOK_DISPATCH,
    TOK_RETURN,
    TOK_EOF
    // todo: Decide if modulo operator uses "mod". If so, maybe use "and"
    // and "or" for the logical versions to distinguish from & and |
};

void tok_reset();
void tok_from_string(const char *text);
const char *tok_name(int tok);
int tok_next();
int tok_peek();
void tok_undo(int tok);
bool tok_maybe(int tok);
uint64_t tok_value();
const char *tok_identifier();
void tok_expect(int tok);
void tok_expect_ident(const char *desc);
int tok_line();

typedef void (*tok_error_cb)(const char *);
void tok_on_error(tok_error_cb fn);

#endif
