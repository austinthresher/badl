#include "arena.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "fatal.h"

Arena *arena_new(size_t size) {
    Arena *a = (Arena *)malloc(sizeof(Arena));
    a->base = malloc(size);
    a->size = size;
    a->used = 0;
    a->next = NULL;
    return a;
}

void arena_free(Arena *a) {
    if (a == NULL) { return; }
    if (a->next) { arena_free(a->next); }
    free(a->base);
    a->base = NULL;
    free(a);
}

void arena_reset(Arena *a) {
    if (a == NULL) { FATAL("arena is null"); }
    memset(a->base, 0, a->size);
    a->used = 0;
    if (a->next) {
        arena_reset(a->next);
    }
}

void *arena_alloc(Arena *a, size_t n) {
    if (a == NULL) { FATAL("arena is null"); }
    size_t avail = a->size - a->used;
    if (n <= avail) {
        uintptr_t ptr = (uintptr_t)a->base + a->used;
        a->used += n;
        return (void*)ptr;
    }
    if (!a->next) {
        size_t next_size = a->size;
        // Make sure the next link in the arena chain is big enough
        while (next_size < n) { next_size *= 2; }
        a->next = arena_new(next_size);
    }
    return arena_alloc(a->next, n);
}
