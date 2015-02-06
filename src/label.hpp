#ifndef LABEL_HPP
#define LABEL_HPP

#include "freetype.hpp"
#include "string.hpp"
#include "glm.hpp"

class Gui;

class Label {
public:
    Label(Gui *gui);
    ~Label();

    // need to call update() to make it take effect
    void set_text(const String &text) {
        _text = text;
    }
    // need to call update() to make it take effect
    void set_font_size(int size) {
        _font_size = size;
    }
    void set_color(glm::vec4 color) {
        _color = color;
    }

    void update();

    int width() const {
        return _width;
    }

    int height() const {
        return _height;
    }

    void draw(const glm::mat4 &mvp);

private:
    Gui *_gui;
    int _width;
    int _height;
    GLuint _texture_id;
    GLuint _vertex_array;
    GLuint _vertex_buffer;
    GLuint _tex_coord_buffer;

    String _text;
    glm::vec4 _color;
    int _font_size;

    ByteBuffer _img_buffer;
};

#endif

