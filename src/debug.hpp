#ifndef DEBUG_HPP
#define DEBUG_HPP

#include "util.hpp"
#include "byte_buffer.hpp"
#include "glfw.hpp"

#include <stdio.h>
#include <stdlib.h>

static inline void assert_no_gl_error() {
#ifdef NDEBUG
#else
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        panic("GL error: %d\n", (int) err);
    }
#endif
}

static inline void ok_or_panic(int err) {
    if (err)
        panic("%s", genesis_error_string(err));
}

static inline int want_debug_context(void) {
#ifdef NDEBUG
    return 0;
#else
    return 1;
#endif
}

void dump_rgba_img(const ByteBuffer &img, int w, int h, const char *filename);

#endif
