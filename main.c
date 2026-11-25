#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "str.h"

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
