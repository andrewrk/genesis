#ifndef ALPHA_TEXTURE_HPP
#define ALPHA_TEXTURE_HPP

#include "byte_buffer.hpp"
#include "glm.hpp"
#include "vertex_array.hpp"
#include "glfw.hpp"

class Gui;
class GuiWindow;
class AlphaTexture {
public:
    AlphaTexture(Gui *gui);
    ~AlphaTexture();

    void send_pixels(const ByteBuffer &pixels, int width, int height);

    void draw(GuiWindow *window, const glm::vec4 &color, const glm::mat4 &mvp);

    Gui *_gui;
    int _width;
    int _height;

private:
    GLuint _texture_id;
    VertexArray _vertex_array;

    void init_vertex_array();

    static void static_init_vertex_array(void *userdata) {
        return static_cast<AlphaTexture*>(userdata)->init_vertex_array();
    }

    AlphaTexture(const AlphaTexture &copy) = delete;
    AlphaTexture &operator=(const AlphaTexture &copy) = delete;
};

#endif

