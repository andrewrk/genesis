#ifndef SHADER_PROGRAM_HPP
#define SHADER_PROGRAM_HPP

#include <GL/glew.h>

class ShaderProgram {
public:
    ShaderProgram(const char *vertex_shader_source,
            const char *fragment_shader_source,
            const char *geometry_shader_source);
    ~ShaderProgram();

    void bind() {
        glUseProgram(program_id);
    }

private:

    GLuint program_id;
    GLuint vertex_id;
    GLuint fragment_id;

    GLuint geometry_id;
    bool have_geometry;
};

#endif
