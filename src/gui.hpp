#ifndef GUI_HPP
#define GUI_HPP

#include "shader_program.hpp"
#include "freetype.hpp"
#include "list.hpp"
#include "glm.hpp"
#include "hash_map.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <SDL2/SDL.h>

typedef void Widget;

struct MouseEvent {
    int x;
    int y;
};

struct FontCacheKey {
    int font_size;
    uint32_t codepoint;
};

struct FontCacheValue {
    FT_Glyph glyph;
    FT_BitmapGlyph bitmap_glyph;
    FT_UInt glyph_index;
};

static inline bool operator==(FontCacheKey a, FontCacheKey b) {
    return a.font_size == b.font_size && a.codepoint == b.codepoint;
}

static inline bool operator!=(FontCacheKey a, FontCacheKey b) {
    return !(a == b);
}

uint32_t hash_font_key(const FontCacheKey &k);

class LabelWidget;
class Gui {
public:
    Gui(SDL_Window *window);
    ~Gui();

    void exec();

    LabelWidget *create_label_widget();

    void remove_widget(LabelWidget *label_widget);

    FontCacheValue font_cache_entry(const FontCacheKey &key);


    FT_Face _default_font_face;

    ShaderProgram _text_shader_program;
    GLint _text_attrib_tex_coord;
    GLint _text_attrib_position;
    GLint _text_uniform_mvp;
    GLint _text_uniform_tex;
    GLint _text_uniform_color;

    ShaderProgram _primitive_shader_program;
    GLint _primitive_attrib_position;
    GLint _primitive_uniform_mvp;
    GLint _primitive_uniform_color;

    void fill_rect(const glm::vec4 &color, int x, int y, int w, int h);
    void fill_rect(const glm::vec4 &color, const glm::mat4 &mvp);


    SDL_Cursor* _cursor_ibeam;
    SDL_Cursor* _cursor_default;
private:
    SDL_Window *_window;


    FT_Library _ft_library;

    int _width;
    int _height;

    glm::mat4 _projection;

    List<Widget *> _widget_list;

    HashMap<FontCacheKey, FontCacheValue, hash_font_key> _font_cache;

    GLuint _primitive_vertex_array;
    GLuint _primitive_vertex_buffer;

    Widget *_mouse_over_widget;

    void resize();
    void on_mouse_motion(const MouseEvent &event);
};

#endif
