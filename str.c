#include "str.h"
#include "arena.h"
#include <string.h>
#include <stdint.h>

typedef struct interned_str {
    struct interned_str *next;
    uint16_t len;
    char str[];
} interned_str;

static interned_str *string_table = NULL;
static arena *string_arena = NULL;

const char *intern(const char *str) {
    if (!str) { return NULL; }
    if (!string_arena) {
        string_arena = arena_new(4096);
    }
    uint16_t len = strlen(str);
    for (interned_str *s = string_table; s != NULL; s = s->next) {
        if (len != s->len) {
            continue;
        }
        if (strcmp(str, s->str) == 0) {
            return s->str;
        }
    }
    size_t sz = sizeof(interned_str) + len + 1;
    interned_str *s = (interned_str*)arena_alloc(string_arena, sz);
    strncpy(s->str, str, len+1);
    s->len = len;
    s->next = string_table;
    string_table = s;
    return s->str;
}

