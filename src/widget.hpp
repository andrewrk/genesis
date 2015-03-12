#ifndef WIDGET_HPP
#define WIDGET_HPP

#include "glm.hpp"
#include "mouse_event.hpp"
#include "key_event.hpp"

class GuiWindow;
class Widget {
public:
    Widget() : _gui_index(-1), _is_visible(true) {}
    virtual ~Widget() {}
    virtual void draw(GuiWindow *window, const glm::mat4 &projection) = 0;
    virtual int left() const = 0;
    virtual int top() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void on_mouse_move(const MouseEvent *) {}
    virtual void on_mouse_out(const MouseEvent *) {}
    virtual void on_mouse_over(const MouseEvent *) {}
    virtual void on_gain_focus() {}
    virtual void on_lose_focus() {}
    virtual void on_text_input(const TextInputEvent *event) {}
    virtual void on_key_event(const KeyEvent *) {}
    virtual void on_mouse_wheel(const MouseWheelEvent *) {}

    virtual void flush_events() {}

    int _gui_index;
    bool _is_visible;
};

#endif
