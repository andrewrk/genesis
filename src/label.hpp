#ifndef LABEL_HPP
#define LABEL_HPP

#include "freetype.hpp"
#include "string.hpp"
#include "glm.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>

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
    struct Letter {
        uint32_t codepoint;
        int bitmap_left;
        int bitmap_top;
        int left;
        int width;
        int above_size;
        int below_size; // height is above_size + below_size
    };

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

    // cached from _text on update()
    List<Letter> _letters;
    int _padding_left;
    int _padding_right;
    int _padding_top;
    int _padding_bottom;
    glm::vec4 _background_color;
    bool _has_background;

    int _above_size;
    int _below_size;

};

#endif

