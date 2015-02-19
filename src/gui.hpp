#ifndef GUI_HPP
#define GUI_HPP

#include "shader_program.hpp"
#include "freetype.hpp"
#include "list.hpp"
#include "glm.hpp"
#include "hash_map.hpp"
#include "string.hpp"
#include "font_size.hpp"
#include "widget.hpp"
#include "resource_bundle.hpp"
#include "spritesheet.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <SDL2/SDL.h>

uint32_t hash_int(const int &x);

class TextWidget;
class FindFileWidget;
class Gui {
public:
    Gui(SDL_Window *window, ResourceBundle *resource_bundle);
    ~Gui();
    Gui(const Gui &copy) = delete;
    Gui &operator=(const Gui &copy) = delete;

    void exec();

    TextWidget *create_text_widget();
    FindFileWidget *create_find_file_widget();

    void destroy_widget(Widget *widget);
    void set_focus_widget(Widget *widget);

    FontSize *get_font_size(int font_size);


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

    typedef Spritesheet::ImageInfo Image;
    void draw_image(const Image *img, int x, int y, int w, int h);
    void draw_image(const Image *img, const glm::mat4 &mvp);

    void start_text_editing(int x, int y, int w, int h);
    void stop_text_editing();

    void set_clipboard_string(const String &str);
    String get_clipboard_string() const;
    bool clipboard_has_string() const;

    bool try_mouse_move_event_on_widget(Widget *widget, const MouseEvent *event);

    // return true if you ate the event
    void set_on_key_event(bool (*fn)(Gui *, const KeyEvent *event)) {
        _on_key_event = fn;
    }

    SDL_Cursor* _cursor_ibeam;
    SDL_Cursor* _cursor_default;

    SDL_Window *_window;


    FT_Library _ft_library;

    int _width;
    int _height;

    glm::mat4 _projection;

    List<Widget *> _widget_list;

    // key is font size
    HashMap<int, FontSize *, hash_int> _font_size_cache;

    GLuint _primitive_vertex_array;
    GLuint _primitive_vertex_buffer;

    Widget *_mouse_over_widget;
    Widget *_focus_widget;

    ResourceBundle *_resource_bundle;
    ByteBuffer _default_font_buffer;

    Spritesheet _spritesheet;

    Image *_img_entry_dir;
    Image *_img_entry_file;
    Image *_img_null;

    void *_userdata;
    bool (*_on_key_event)(Gui *, const KeyEvent *event);

private:

    void resize();
    void on_mouse_move(const MouseEvent *event);
    void on_text_input(const TextInputEvent *event);
    void on_key_event(const KeyEvent *event);
    void init_widget(Widget *widget);
};

#endif
