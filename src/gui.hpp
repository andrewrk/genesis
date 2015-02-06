#ifndef GUI_HPP
#define GUI_HPP

#include "shader_program.hpp"
#include "freetype.hpp"
#include "list.hpp"
#include "label_widget.hpp"
#include "glm.hpp"
#include "hash_map.hpp"

#include <GL/glew.h>
#include <SDL2/SDL.h>

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

class Gui {
public:
    Gui(SDL_Window *window);
    ~Gui();

    void exec();

    LabelWidget *create_label_widget();

    void remove_label_widget(LabelWidget *label_widget) {
        _widget_list.swap_remove(label_widget->_gui_index);
        destroy(label_widget);
    }

    FontCacheValue font_cache_entry(const FontCacheKey &key);




    GLint _attrib_tex_coord;
    GLint _attrib_position;
    FT_Face _default_font_face;

    ShaderProgram _text_shader_program;

    GLint _uniform_mvp;
    GLint _uniform_tex;
    GLint _uniform_color;
private:
    SDL_Window *_window;


    FT_Library _ft_library;

    int _width;
    int _height;

    glm::mat4 _projection;

    List<void *> _widget_list;

    HashMap<FontCacheKey, FontCacheValue, hash_font_key> _font_cache;

    void resize();
};

#endif
