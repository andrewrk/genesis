#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include "byte_buffer.hpp"
#include "glm.hpp"
#include "glfw.hpp"

class Gui;
class GuiWindow;
class Texture {
public:
    Texture(Gui *gui);
    ~Texture();

    void send_pixels(const ByteBuffer &pixels, int width, int height);

    void draw(GuiWindow *window, const glm::mat4 &mvp);

    Gui *_gui;
    int _width;
    int _height;

private:
    GLuint _texture_id;

    Texture(const Texture &copy) = delete;
    Texture &operator=(const Texture &copy) = delete;
};

#endif
