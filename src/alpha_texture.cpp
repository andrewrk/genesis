#include "alpha_texture.hpp"
#include "debug.hpp"
#include "gui.hpp"

AlphaTexture::AlphaTexture(Gui *gui) :
    _gui(gui),
    _width(0),
    _height(0)
{
    glGenTextures(1, &_texture_id);
    glBindTexture(GL_TEXTURE_2D, _texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenVertexArrays(1, &_vertex_array);
    glBindVertexArray(_vertex_array);

    glGenBuffers(1, &_vertex_buffer);
    glGenBuffers(1, &_tex_coord_buffer);

    GLfloat vertexes[4][3] = {
        {0,    0,    0},
        {0,    1.0f, 0},
        {1.0f, 0,    0},
        {1.0f, 1.0f, 0},
    };
    glBindBuffer(GL_ARRAY_BUFFER, _vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(GLfloat), vertexes, GL_STATIC_DRAW);
    glEnableVertexAttribArray(_gui->_shader_program_manager->_text_attrib_position);
    glVertexAttribPointer(_gui->_shader_program_manager->_text_attrib_position, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    GLfloat coords[4][2] = {
        {0, 0},
        {0, 1},
        {1, 0},
        {1, 1},
    };
    glBindBuffer(GL_ARRAY_BUFFER, _tex_coord_buffer);
    glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(GLfloat), coords, GL_STATIC_DRAW);
    glEnableVertexAttribArray(_gui->_shader_program_manager->_text_attrib_tex_coord);
    glVertexAttribPointer(_gui->_shader_program_manager->_text_attrib_tex_coord, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    assert_no_gl_error();
}

AlphaTexture::~AlphaTexture() {
    glDeleteBuffers(1, &_tex_coord_buffer);
    glDeleteBuffers(1, &_vertex_buffer);
    glDeleteVertexArrays(1, &_vertex_array);
    glDeleteTextures(1, &_texture_id);
}

void AlphaTexture::send_pixels(const ByteBuffer &pixels, int width, int height) {
    _width = width;
    _height = height;

    if (pixels.length() != (size_t)(width * height))
        panic("expected pixel length: %d received pixel length: %zu", width * height, pixels.length());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture_id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height,
            0, GL_RED, GL_UNSIGNED_BYTE, pixels.raw());

    assert_no_gl_error();
}

void AlphaTexture::draw(const glm::vec4 &color, const glm::mat4 &mvp) {
    _gui->_shader_program_manager->_text_shader_program.bind();

    _gui->_shader_program_manager->_text_shader_program.set_uniform(
            _gui->_shader_program_manager->_text_uniform_color, color);

    _gui->_shader_program_manager->_text_shader_program.set_uniform(
            _gui->_shader_program_manager->_text_uniform_tex, 0);

    _gui->_shader_program_manager->_text_shader_program.set_uniform(
            _gui->_shader_program_manager->_text_uniform_mvp, mvp);

    glBindVertexArray(_vertex_array);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture_id);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

