#ifndef FIND_FILE_WIDGET_HPP
#define FIND_FILE_WIDGET_HPP

#include "widget.hpp"
#include "text_widget.hpp"
#include "byte_buffer.hpp"

class Gui;
class FindFileWidget {
public:
    Widget _widget;

    FindFileWidget(Gui *gui);
    FindFileWidget(const FindFileWidget &copy) = delete;
    FindFileWidget &operator=(const FindFileWidget &copy) = delete;

    ~FindFileWidget() {}
    void draw(const glm::mat4 &projection);
    int left() const { return _left; }
    int top() const { return _top; }
    int width() const;
    int height() const;
    void on_mouse_move(const MouseEvent *event);
    void on_mouse_out(const MouseEvent *event);
    void on_mouse_over(const MouseEvent *event);
    void on_gain_focus();
    void on_lose_focus();
    void on_text_input(const TextInputEvent *event);
    void on_key_event(const KeyEvent *event);

    enum Mode {
        ModeOpen,
        ModeSave,
    };

    void set_mode(Mode mode) {
        _mode = mode;
    }

    void set_pos(int new_left, int new_top) {
        _left = new_left;
        _top = new_top;
        update_model();
    }
private:
    Mode _mode;

    int _left;
    int _top;
    int _width;
    int _height;
    int _padding_left;
    int _padding_right;
    int _padding_top;
    int _padding_bottom;
    int _margin; // space between widgets

    TextWidget _current_path_widget;
    TextWidget _filter_widget;

    Gui *_gui;

    ByteBuffer _current_path;

    void update_model();

    // widget methods
    static void destructor(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->~FindFileWidget();
    }
    static void draw(Widget *widget, const glm::mat4 &projection) {
        return (reinterpret_cast<FindFileWidget*>(widget))->draw(projection);
    }
    static int left(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->left();
    }
    static int top(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->top();
    }
    static int width(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->width();
    }
    static int height(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->height();
    }
    static void on_mouse_move(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_mouse_move(event);
    }
    static void on_mouse_out(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_mouse_out(event);
    }
    static void on_mouse_over(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_mouse_over(event);
    }
    static void on_gain_focus(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_gain_focus();
    }
    static void on_lose_focus(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_lose_focus();
    }
    static void on_text_input(Widget *widget, const TextInputEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_text_input(event);
    }
    static void on_key_event(Widget *widget, const KeyEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_key_event(event);
    }
};

#endif
