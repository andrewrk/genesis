#ifndef TEXT_WIDGET_HPP
#define TEXT_WIDGET_HPP

#include "string.hpp"
#include "label.hpp"
#include "glm.hpp"
#include "gui.hpp"
#include "widget.hpp"

class TextWidget {
public:
    Widget _widget;

    TextWidget(Gui *gui);
    TextWidget(const TextWidget &copy) = delete;
    TextWidget &operator=(const TextWidget &copy) = delete;

    ~TextWidget() {}
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

    // return true if you ate the event
    void set_on_key_event(bool (*fn)(TextWidget *, const KeyEvent *event)) {
        _on_key_event = fn;
    }

    // return true if you ate the event
    void set_on_text_change_event(void (*fn)(TextWidget *)) {
        _on_text_change_event = fn;
    }

    void set_text(const String &text) {
        _label.set_text(text);
        _label.update();
    }
    const String &text() const {
        return _label.text();
    }

    void set_placeholder_text(const String &text);

    void set_font_size(int size) {
        _label.set_font_size(size);
        _label.update();
    }

    void set_pos(int new_left, int new_top) {
        _left = new_left;
        _top = new_top;
        update_model();
    }



    void set_width(int new_width);

    void set_auto_size(bool value);


    void set_selection(int start, int end);

    bool have_focus() {
        return _have_focus;
    }

    void select_all();
    void cut();
    void copy();
    void paste();

    void set_background(bool value) {
        _background_on = value;
    }

    void set_text_interaction(bool value) {
        _text_interaction_on = false;
    }

    void *_userdata;

private:
    Label _label;
    int _left;
    int _top;
    glm::mat4 _label_model;
    glm::mat4 _bg_model;

    int _padding_left;
    int _padding_right;
    int _padding_top;
    int _padding_bottom;
    glm::vec4 _text_color;
    glm::vec4 _sel_text_color;
    glm::vec4 _background_color;
    glm::vec4 _selection_color;
    glm::vec4 _cursor_color;
    bool _auto_size;

    Gui *_gui;

    int _cursor_start;
    int _cursor_end;
    bool _select_down;

    glm::mat4 _sel_model;
    glm::mat4 _cursor_model;
    glm::mat4 _sel_text_model;

    bool _have_focus;

    // used when _auto_size is false
    int _width;

    int _scroll_x;

    Label _placeholder_label;
    glm::vec4 _placeholder_color;
    bool _mouse_down_dbl;
    int _dbl_select_start;
    int _dbl_select_end;

    bool _background_on;
    bool _text_interaction_on;

    void update_model();

    int cursor_at_pos(int x, int y) const;
    void pos_at_cursor(int index, int &x, int &y) const;
    void get_cursor_slice(int &start, int &end) const;
    void update_selection_model();
    void replace_text(int start, int end, const String &text, int cursor_modifier);
    int backward_word();
    int forward_word();
    int advance_word(int dir);
    int advance_word_from_index(int index, int dir);
    void scroll_cursor_into_view();
    void scroll_index_into_view(int char_index);

    bool (*_on_key_event)(TextWidget *, const KeyEvent *event);
    void (*_on_text_change_event)(TextWidget *);

    // widget methods
    static void destructor(Widget *widget) {
        return (reinterpret_cast<TextWidget*>(widget))->~TextWidget();
    }
    static void draw(Widget *widget, const glm::mat4 &projection) {
        return (reinterpret_cast<TextWidget*>(widget))->draw(projection);
    }
    static int left(Widget *widget) {
        return (reinterpret_cast<TextWidget*>(widget))->left();
    }
    static int top(Widget *widget) {
        return (reinterpret_cast<TextWidget*>(widget))->top();
    }
    static int width(Widget *widget) {
        return (reinterpret_cast<TextWidget*>(widget))->width();
    }
    static int height(Widget *widget) {
        return (reinterpret_cast<TextWidget*>(widget))->height();
    }
    static void on_mouse_move(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<TextWidget*>(widget))->on_mouse_move(event);
    }
    static void on_mouse_out(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<TextWidget*>(widget))->on_mouse_out(event);
    }
    static void on_mouse_over(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<TextWidget*>(widget))->on_mouse_over(event);
    }
    static void on_gain_focus(Widget *widget) {
        return (reinterpret_cast<TextWidget*>(widget))->on_gain_focus();
    }
    static void on_lose_focus(Widget *widget) {
        return (reinterpret_cast<TextWidget*>(widget))->on_lose_focus();
    }
    static void on_text_input(Widget *widget, const TextInputEvent *event) {
        return (reinterpret_cast<TextWidget*>(widget))->on_text_input(event);
    }
    static void on_key_event(Widget *widget, const KeyEvent *event) {
        return (reinterpret_cast<TextWidget*>(widget))->on_key_event(event);
    }
};

#endif
