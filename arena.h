#ifndef ARENA_H_
#define ARENA_H_

#include <stdlib.h>

typedef struct Arena {
    void *base;
    size_t used;
    size_t size;
    struct Arena *next;
} Arena;

Arena *arena_new(size_t size);
void arena_free(Arena *a);
void arena_reset(Arena *a);
void *arena_alloc(Arena *a, size_t n);

#endif
