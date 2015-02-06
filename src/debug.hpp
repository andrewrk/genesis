#ifndef DEBUG_HPP
#define DEBUG_HPP

#include "util.hpp"
#include "byte_buffer.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <epoxy/gl.h>
#include <epoxy/glx.h>

static inline void assert_no_gl_error() {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        panic("GL error: %d\n", (int) err);
    }
}

void dump_rgba_img(const ByteBuffer &img, int w, int h, const char *filename);

#endif
