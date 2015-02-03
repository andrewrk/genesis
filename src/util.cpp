#include "util.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

void panic(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}
