#include "shader_program.hpp"
#include "byte_buffer.hpp"

static void init_shader(const char *source, const char *name, GLenum type, GLuint &shader_id) {
    shader_id = glCreateShader(type);
    glShaderSource(shader_id, 1, &source, NULL);
    glCompileShader(shader_id);

    GLint ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &ok);
    if (ok)
        return;

    GLint error_size;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &error_size);

    ByteBuffer error_string;
    error_string.resize(error_size);
    glGetShaderInfoLog(shader_id, error_size, &error_size, error_string.raw());
    panic("Error compiling %s shader:\n%s", name, error_string.raw());
}

ShaderProgram::ShaderProgram(const char *vertex_shader_source,
                             const char *fragment_shader_source,
                             const char *geometry_shader_source)
{
    init_shader(vertex_shader_source, "vertex", GL_VERTEX_SHADER, vertex_id);
    init_shader(fragment_shader_source, "fragment", GL_FRAGMENT_SHADER, fragment_id);
    if (geometry_shader_source) {
        init_shader(geometry_shader_source, "geometry", GL_GEOMETRY_SHADER, geometry_id);
        have_geometry = true;
    } else {
        have_geometry = false;
    }

    program_id = glCreateProgram();
    glAttachShader(program_id, vertex_id);
    glAttachShader(program_id, fragment_id);
    if (geometry_shader_source)
        glAttachShader(program_id, geometry_id);
    glLinkProgram(program_id);

    GLint ok;
    glGetProgramiv(program_id, GL_LINK_STATUS, &ok);

    if (ok)
        return;

    GLint error_size;
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &error_size);

    ByteBuffer error_string;
    error_string.resize(error_size);
    glGetProgramInfoLog(program_id, error_size, &error_size, error_string.raw());
    panic("Error linking shader program: %s", error_string.raw());
}

ShaderProgram::~ShaderProgram() {
    if (have_geometry)
        glDetachShader(program_id, geometry_id);
    glDetachShader(program_id, fragment_id);
    glDetachShader(program_id, vertex_id);

    if (have_geometry)
        glDeleteShader(geometry_id);
    glDeleteShader(fragment_id);
    glDeleteShader(vertex_id);

    glDeleteProgram(program_id);
}

void ShaderProgram::set_uniform(GLint uniform_id, int value) const
{
    glUniform1i(uniform_id, value);
}

void ShaderProgram::set_uniform(GLint uniform_id, float value) const
{
    glUniform1f(uniform_id, value);
}

void ShaderProgram::set_uniform(GLint uniform_id, const glm::vec3 &value) const
{
    glUniform3fv(uniform_id, 1, &value[0]);
}

void ShaderProgram::set_uniform(GLint uniform_id, const glm::vec4 &value) const
{
    glUniform4fv(uniform_id, 1, &value[0]);
}

void ShaderProgram::set_uniform(GLint uniform_id, const glm::mat4 &value) const
{
    glUniformMatrix4fv(uniform_id, 1, GL_FALSE, &value[0][0]);
}

void ShaderProgram::set_uniform(GLint uniform_id, const glm::mat3 &value) const
{
    glUniformMatrix3fv(uniform_id, 1, GL_FALSE, &value[0][0]);
}

