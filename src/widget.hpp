#ifndef WIDGET_HPP
#define WIDGET_HPP

#include "glm.hpp"
#include "mouse_event.hpp"
#include "key_event.hpp"

class Widget {
public:
    void (*destructor)(Widget *);
    void (*draw)(Widget *, const glm::mat4 &projection);
    int (*left)(Widget *);
    int (*top)(Widget *);
    int (*width)(Widget *);
    int (*height)(Widget *);
    void (*on_mouse_move)(Widget *, const MouseEvent *);
    void (*on_mouse_out)(Widget *, const MouseEvent *);
    void (*on_mouse_over)(Widget *, const MouseEvent *);
    void (*on_gain_focus)(Widget *);
    void (*on_lose_focus)(Widget *);
    void (*on_text_input)(Widget *, const TextInputEvent *event);
    void (*on_key_event)(Widget *, const KeyEvent *);

    int _gui_index;
    bool _is_visible;
};

#endif
