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
    const String &text() const {
        return _text;
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

    int cursor_at_pos(int x, int y) const;
    void pos_at_cursor(int index, int &x, int &y) const;
    void get_slice_dimensions(int start, int end, int &start_x, int &end_x) const;

    int above_size() const {
        return _above_size;
    }

    int below_size() const {
        return _below_size;
    }

private:
    struct Letter {
        uint32_t codepoint;

        int left; // half-way between prev letter and this one. 0 for first letter
        int bitmap_left; // left + bitmap_left is the first pixel of the letter
        int bitmap_width; // left + bitmap_left + bitmap_width is the last pixel of the letter
        int full_width; // left + full_width is half-way between this letter and next

        int above_size;
        int below_size;
        int bitmap_top;
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

    int _above_size;
    int _below_size;

};

#endif

