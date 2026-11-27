#include "symtab.h"
#include "ast.h"
#include "fatal.h"
#include "str.h"
#include <string.h>

static arena *ar = NULL;

static symtab_frame *stack = NULL;

void symtab_init(arena *a) {
    ar = a;
    // Enter top-level scope. We never exit this.
    stack = NULL;
    symtab_enter();
}

void symtab_enter() {
    symtab_frame *stf = (symtab_frame *)arena_alloc(ar, sizeof(symtab_frame));
    stf->env = NULL;
    stf->next = stack;
    stack = stf;
}

// This leaks, but that's what the arena is for
void symtab_exit() {
    // If these happen it's a compiler issue, not a user error
    if (stack == NULL) { FATAL("symtab stack underflow\n"); }
    if (stack->next == NULL) { FATAL("attempt to exit global scope\n"); }
    stack = stack->next;
}

void symtab_add(const char *name, ast_node *def, int line) {
    if (stack == NULL) { FATAL("symtab stack is null\n"); }
    symtab_entry *se = (symtab_entry *)arena_alloc(ar, sizeof(symtab_entry));
    se->name = intern(name);
    se->def = def;
    se->line = line;
    se->next = stack->env;
    stack->env = se;
}

// name has always been interned by this point, so we can compare pointers
static symtab_entry *symtab_find_in(symtab_frame *frame, const char *name) {
    for (symtab_entry *e = frame->env; e != NULL; e = e->next) {
        if (e->name == name) { return e; }
    }
    return NULL;
}

symtab_entry *symtab_find(const char *name) {
    name = intern(name);
    for (symtab_frame *f = stack; f != NULL; f = f->next) {
        symtab_entry *e = symtab_find_in(f, name);
        if (e != NULL) { return e; }
    }
    return NULL;
}

symtab_entry *symtab_find_current(const char *name) {
    return symtab_find_in(stack, name);
}

bool symtab_is_variable(const char *name) {
    symtab_entry *e = symtab_find(name);
    if (e == NULL) { return false; }
    if (e->def == NULL) { return false; }
    int ast_type = e->def->ast_type;
    if (ast_type == AST_GLOBAL_DEF || ast_type == AST_LOCAL_DEF) {
        return true;
    }
    return false;
}

bool symtab_is_function(const char *name) {
    symtab_entry *e = symtab_find(name);
    if (e == NULL) { return false; }
    if (e->def == NULL) { return false; }
    int ast_type = e->def->ast_type;
    if (ast_type == AST_FORWARD_DECL || ast_type == AST_FUNCTION_DEF) {
        return true;
    }
    return false;
}

bool symtab_is_type(const char *name) {
    symtab_entry *e = symtab_find(name);
    if (e == NULL) { return false; }
    if (e->def == NULL) { return false; }
    return e->def->ast_type == AST_TYPE || e->def->ast_type == AST_NAMED_TYPE;
}
