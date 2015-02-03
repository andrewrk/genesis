#include <stdio.h>
#include <stdlib.h>

void panic(const char * str) {
    fprintf(stderr, "%s\n", str);
    abort();
}
