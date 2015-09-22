#ifndef DEBUG_GL_HPP
#define DEBUG_GL_HPP

#include "glfw.hpp"
#include "util.hpp"

static inline void assert_no_gl_error() {
#ifdef NDEBUG
#else
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        panic("GL error: %d\n", (int) err);
    }
#endif
}

#endif
