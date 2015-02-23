#ifndef ALPHA_TEXTURE_HPP
#define ALPHA_TEXTURE_HPP

#include "byte_buffer.hpp"
#include "glm.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>

class Gui;
class AlphaTexture {
public:
    AlphaTexture(Gui *gui);
    ~AlphaTexture();

    AlphaTexture(const AlphaTexture &copy) = delete;
    AlphaTexture &operator=(const AlphaTexture &copy) = delete;

    void send_pixels(const ByteBuffer &pixels, int width, int height);

    void draw(const glm::vec4 &color, const glm::mat4 &mvp);

    Gui *_gui;
    int _width;
    int _height;

private:
    GLuint _texture_id;
    GLuint _vertex_array;
    GLuint _vertex_buffer;
    GLuint _tex_coord_buffer;
};

#endif

