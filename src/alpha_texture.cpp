#include "alpha_texture.hpp"
#include "debug.hpp"
#include "gui.hpp"

AlphaTexture::AlphaTexture(Gui *gui) :
    _gui(gui),
    _width(0),
    _height(0),
    _vertex_array(gui, static_init_vertex_array, this)
{
    glGenTextures(1, &_texture_id);
    glBindTexture(GL_TEXTURE_2D, _texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

AlphaTexture::~AlphaTexture() {
    glDeleteTextures(1, &_texture_id);
}

void AlphaTexture::init_vertex_array() {
    glBindBuffer(GL_ARRAY_BUFFER, _gui->_static_geometry._rect_2d_vertex_buffer);
    glEnableVertexAttribArray(_gui->_shader_program_manager._text_attrib_position);
    glVertexAttribPointer(_gui->_shader_program_manager._text_attrib_position, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    glBindBuffer(GL_ARRAY_BUFFER, _gui->_static_geometry._rect_2d_tex_coord_buffer);
    glEnableVertexAttribArray(_gui->_shader_program_manager._text_attrib_tex_coord);
    glVertexAttribPointer(_gui->_shader_program_manager._text_attrib_tex_coord, 2, GL_FLOAT, GL_FALSE, 0, NULL);
}

void AlphaTexture::send_pixels(const ByteBuffer &pixels, int width, int height) {
    _width = width;
    _height = height;

    if (pixels.length() != width * height)
        panic("expected pixel length: %d received pixel length: %d", width * height, pixels.length());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture_id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height,
            0, GL_RED, GL_UNSIGNED_BYTE, pixels.raw());

    assert_no_gl_error();
}

void AlphaTexture::draw(GuiWindow *window, const glm::vec4 &color, const glm::mat4 &mvp) {
    _gui->_shader_program_manager._text_shader_program.bind();

    _gui->_shader_program_manager._text_shader_program.set_uniform(
            _gui->_shader_program_manager._text_uniform_color, color);

    _gui->_shader_program_manager._text_shader_program.set_uniform(
            _gui->_shader_program_manager._text_uniform_tex, 0);

    _gui->_shader_program_manager._text_shader_program.set_uniform(
            _gui->_shader_program_manager._text_uniform_mvp, mvp);

    _vertex_array.bind(window);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _texture_id);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

