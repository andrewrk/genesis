#ifndef UTIL_HPP
#define UTIL_HPP

#include "stdlib.h"

void panic(const char * str) __attribute__ ((noreturn));

template<typename T>
static inline T *not_null(T *thing) {
    if (thing == NULL)
        panic("unexpected NULL");
    return thing;
}

#endif
