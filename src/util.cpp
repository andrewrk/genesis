#include "util.hpp"

#include <stdlib.h>
#include <stdio.h>

void panic(const char * str) {
    fprintf(stderr, "%s\n", str);
    abort();
}
