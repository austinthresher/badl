#include "str.h"
#include "arena.h"
#include <string.h>
#include <stdint.h>

typedef struct InternStr {
    struct InternStr *next;
    uint16_t len;
    char str[];
} InternStr;

static InternStr *string_table = NULL;
static Arena *string_arena = NULL;

const char *intern(const char *str) {
    if (!string_arena) {
        string_arena = arena_new(4096);
    }
    uint16_t len = strlen(str);
    for (InternStr *s = string_table; s != NULL; s = s->next) {
        if (len != s->len) {
            continue;
        }
        if (strcmp(str, s->str) == 0) {
            return s->str;
        }
    }
    size_t sz = sizeof(InternStr) + len + 1;
    InternStr *s = (InternStr*)arena_alloc(string_arena, sz);
    strncpy(s->str, str, len+1);
    s->len = len;
    s->next = string_table;
    string_table = s;
    return s->str;
}

