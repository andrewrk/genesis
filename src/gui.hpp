#ifndef GUI_HPP
#define GUI_HPP

#include "shader_program.hpp"
#include "freetype.hpp"
#include "list.hpp"
#include "glm.hpp"
#include "hash_map.hpp"
#include "string.hpp"
#include "font_size.hpp"
#include "virt_key.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <SDL2/SDL.h>

typedef void Widget;

enum KeyAction {
    KeyActionDown,
    KeyActionUp,
};

struct KeyEvent {
    KeyAction action;
    VirtKey virt_key;
    uint16_t modifiers;
};

enum TextInputAction {
    TextInputActionCandidate,
    TextInputActionCommit,
};

struct TextInputEvent {
    TextInputAction action;
    String text;
};

enum MouseButton {
    MouseButtonNone,
    MouseButtonLeft,
    MouseButtonMiddle,
    MouseButtonRight,
};

enum MouseAction {
    MouseActionMove,
    MouseActionDown,
    MouseActionUp,
};

struct MouseButtons {
    unsigned left   : 1;
    unsigned middle : 1;
    unsigned right  : 1;
};

struct MouseEvent {
    int x;
    int y;
    MouseButton button;
    MouseAction action;
    MouseButtons buttons;
};

uint32_t hash_int(const int &x);

class LabelWidget;
class Gui {
public:
    Gui(SDL_Window *window);
    ~Gui();

    void exec();

    LabelWidget *create_label_widget();

    void remove_widget(Widget *widget);

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

    void start_text_editing(int x, int y, int w, int h);
    void stop_text_editing();


    SDL_Cursor* _cursor_ibeam;
    SDL_Cursor* _cursor_default;
private:
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

    void resize();
    void on_mouse_move(const MouseEvent &event);
    void on_text_input(const TextInputEvent &event);
    void on_key_event(const KeyEvent &event);
};

#endif
