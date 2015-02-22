#ifndef SHADER_PROGRAM_MANAGER
#define SHADER_PROGRAM_MANAGER

#include "shader_program.hpp"

class ShaderProgramManager {
public:
    ShaderProgramManager();
    ~ShaderProgramManager();

    ShaderProgramManager(const ShaderProgramManager &copy) = delete;
    ShaderProgramManager &operator=(const ShaderProgramManager &copy) = delete;

    ShaderProgram _texture_shader_program;
    GLint _texture_attrib_tex_coord;
    GLint _texture_attrib_position;
    GLint _texture_uniform_mvp;
    GLint _texture_uniform_tex;

    ShaderProgram _text_shader_program;
    GLint _text_attrib_tex_coord;
    GLint _text_attrib_position;
    GLint _text_uniform_mvp;
    GLint _text_uniform_tex;
    GLint _text_uniform_color;

    ShaderProgram _primitive_shader_program;
    GLint _primitive_attrib_position;
    GLint _primitive_uniform_mvp;
    GLint _primitive_uniform_color;
};

#endif
