#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "arena.h"

Arena *string_arena = NULL;

typedef struct InternStr {
    struct InternStr *next;
    uint16_t len;
    char str[];
} InternStr;

InternStr *string_table = NULL;

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

int main(int argc, const char *argv[]) {
    char buf[16];
    buf[0] = 'h';
    buf[1] = 'i';
    buf[2] = '\0';
    
    const char *a = intern("hi");
    const char *b = intern(buf);
    printf("%p, %p\n", (void*)a, (void*)b);

    return EXIT_SUCCESS;
}
