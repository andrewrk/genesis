#ifndef GUI_WINDOW_HPP
#define GUI_WINDOW_HPP

#include "list.hpp"
#include "glm.hpp"
#include "key_event.hpp"
#include "mouse_event.hpp"

#include <SDL2/SDL.h>

class Gui;
class Widget;
class TextWidget;
class FindFileWidget;
class AudioEditWidget;
class SpritesheetImage;

class GuiWindow {
public:
    GuiWindow(Gui *gui, bool with_borders);
    ~GuiWindow();

    void draw();

    TextWidget *create_text_widget();
    FindFileWidget *create_find_file_widget();
    AudioEditWidget *create_audio_edit_widget();

    void destroy_widget(Widget *widget);
    void set_focus_widget(Widget *widget);

    // return true if you ate the event
    void set_on_key_event(bool (*fn)(GuiWindow *, const KeyEvent *event)) {
        _on_key_event = fn;
    }

    void set_on_close_event(void (*fn)(GuiWindow *)) {
        _on_close_event = fn;
    }

    bool try_mouse_move_event_on_widget(Widget *widget, const MouseEvent *event);

    void fill_rect(const glm::vec4 &color, int x, int y, int w, int h);
    void draw_image(const SpritesheetImage *img, int x, int y, int w, int h);

    void resize();
    void on_close();
    void on_mouse_move(const MouseEvent *event);
    void on_text_input(const TextInputEvent *event);
    void on_key_event(const KeyEvent *event);
    void on_mouse_wheel(const MouseWheelEvent *event);


    void *_userdata;
    // index into Gui's list of windows
    int _gui_index;

    Gui *_gui;
    SDL_Window *_window;
    SDL_GLContext _context;
    int _last_mouse_x, _last_mouse_y;

    int _width;
    int _height;

private:

    glm::mat4 _projection;
    List<Widget *> _widget_list;
    Widget *_mouse_over_widget;
    Widget *_focus_widget;

    bool (*_on_key_event)(GuiWindow *, const KeyEvent *event);
    void (*_on_close_event)(GuiWindow *);


    void init_widget(Widget *widget);

};

#endif
