#ifndef LABEL_WIDGET_HPP
#define LABEL_WIDGET_HPP

#include "string.hpp"
#include "label.hpp"
#include "glm.hpp"
#include "gui.hpp"

class LabelWidget {
public:
    LabelWidget(Gui *gui, int gui_index);
    ~LabelWidget() {}
    LabelWidget(LabelWidget &copy) = delete;

    void set_text(const String &text) {
        _label.set_text(text);
        _label.update();
    }

    void set_font_size(int size) {
        _label.set_font_size(size);
        _label.update();
    }

    void set_color(glm::vec4 color) {
        _label.set_color(color);
    }

    void set_pos(int new_left, int new_top) {
        _left = new_left;
        _top = new_top;
        update_model();
    }

    int left() const {
        return _left;
    }

    int top() const {
        return _top;
    }

    int width() const {
        return _label.width() + _padding_left + _padding_right;
    }

    int height() const {
        return _label.height() + _padding_top + _padding_bottom;
    }

    void draw(const glm::mat4 &projection);

    bool is_visible() const {
        return _is_visible;
    }

    void on_mouse_over(const MouseEvent &event);
    void on_mouse_out(const MouseEvent &event);
    void on_mouse_move(const MouseEvent &event);

    int _gui_index;

private:
    Label _label;
    int _left;
    int _top;
    glm::mat4 _label_model;
    glm::mat4 _bg_model;

    bool _is_visible;
    int _padding_left;
    int _padding_right;
    int _padding_top;
    int _padding_bottom;
    glm::vec4 _background_color;
    glm::vec4 _selection_color;
    glm::vec4 _cursor_color;
    bool _has_background;

    Gui *_gui;

    int _cursor_start;
    int _cursor_end;
    bool _select_down;

    glm::mat4 _sel_model;
    glm::mat4 _cursor_model;

    void update_model();

    int cursor_at_pos(int x, int y) const;
    void pos_at_cursor(int index, int &x, int &y) const;
    void get_cursor_slice(int &start, int &end) const;
    void update_selection_model();
};

#endif
