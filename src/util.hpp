#ifndef UTIL_HPP
#define UTIL_HPP

#include "stdlib.h"
#include <GL/glew.h>

void panic(const char * str) __attribute__ ((noreturn));

template<typename T>
static inline T *not_null(T *thing) {
    if (thing == NULL)
        panic("unexpected NULL");
    return thing;
}

static inline void assert_no_gl_error() {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        char *string = (char *)gluErrorString(err);
        panic(string);
    }
}

#endif
