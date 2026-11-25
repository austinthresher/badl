#ifndef ARENA_H_
#define ARENA_H_

#include <stdlib.h>

typedef struct arena arena;

arena *arena_new(size_t size);
void arena_free(arena *a);
void arena_reset(arena *a);
void *arena_alloc(arena *a, size_t n);

#endif
