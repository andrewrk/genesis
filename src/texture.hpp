#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include "byte_buffer.hpp"
#include "glm.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>

class Gui;
class Texture {
public:
    Texture(Gui *gui);
    ~Texture();

    Texture(const Texture &copy) = delete;
    Texture &operator=(const Texture &copy) = delete;

    void send_pixels(const ByteBuffer &pixels, int width, int height);

    void draw(const glm::mat4 &mvp);

private:
    Gui *_gui;
    GLuint _texture_id;
    GLuint _vertex_array;
    GLuint _vertex_buffer;
    GLuint _tex_coord_buffer;

    int _width;
    int _height;
};

#endif
