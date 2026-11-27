#ifndef SYMTAB_H_
#define SYMTAB_H_

#include "arena.h"
#include "ast.h"
#include <stdbool.h>

typedef struct symtab_entry {
    const char *name;
    ast_node *def;
    int line;
    struct symtab_entry *next;
} symtab_entry;

typedef struct symtab_frame {
    struct symtab_entry *env;
    struct symtab_frame *next;
} symtab_frame;

void symtab_init(arena *a);
void symtab_enter();
void symtab_exit();
void symtab_add(const char *name, ast_node *def, int line);
symtab_entry *symtab_find(const char *name);
symtab_entry *symtab_find_current(const char *name);

bool symtab_is_type(const char *name);
bool symtab_is_variable(const char *name);
bool symtab_is_function(const char *name);

#endif
