#ifndef SELECT_WIDGET_HPP
#define SELECT_WIDGET_HPP

#include "glm.hpp"
#include "widget.hpp"
#include "string.hpp"
#include "label.hpp"

class Gui;
class GuiWindow;

class SelectWidget : public Widget {
public:
    SelectWidget(GuiWindow *gui_window, Gui *gui);
    ~SelectWidget();

    void draw(GuiWindow *window, const glm::mat4 &projection);
    int left() const override {
        return _left;
    }

    int top() const override {
        return _top;
    }

    int width() const override;
    int height() const override;

    void set_pos(int left, int top) {
        _left = left;
        _top = top;
        update_model();
    }

    void clear();
    void append_choice(String choice);
    void select_index(int index);

    void on_mouse_move(const MouseEvent *event);
    void on_mouse_over(const MouseEvent *event);

    void set_on_selected_index_change(void (*fn)(SelectWidget *)) {
        _on_selected_index_change = fn;
    }

    int selected_index() const { return _selected_index; }

    void *_userdata;

private:
    GuiWindow *_gui_window;
    Gui *_gui;
    int _left;
    int _top;
    int _padding_left;
    int _padding_top;
    int _padding_right;
    int _padding_bottom;
    glm::vec4 _bg_color;
    glm::vec4 _text_color;
    glm::mat4 _bg_model;
    glm::mat4 _text_model;

    List<Label*> _label_list;
    int _selected_index;

    void (*_on_selected_index_change)(SelectWidget *);

    void update_model();
    void destroy_all_labels();

    SelectWidget(const SelectWidget &copy) = delete;
    SelectWidget &operator=(const SelectWidget &copy) = delete;
};

#endif
