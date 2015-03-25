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

char * create_formatted_str(const char *format, ...) {
    va_list ap, ap2;
    va_start(ap, format);
    va_copy(ap2, ap);

    int ret = vsnprintf(NULL, 0, format, ap);
    if (ret < 0)
        return nullptr;

    int required_length = ret + 1;
    char *result = allocate_zero<char>(required_length);
    
    ret = vsnprintf(result, required_length, format, ap2);
    if (ret < 0) {
        destroy(result, required_length);
        return nullptr;
    }

    va_end(ap2);
    va_end(ap);

    result[required_length - 1] = 0;

    return result;
}
