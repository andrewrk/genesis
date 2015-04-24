#include "widget.hpp"
#include "gui_window.hpp"
#include "gui.hpp"

Widget::Widget(GuiWindow *gui_window) :
    gui_window(gui_window),
    parent_widget(nullptr),
    gui(gui_window->_gui),
    left(0),
    top(0),
    width(100),
    height(100),
    set_index(-1),
    is_visible(true)
{
}

Widget::~Widget() {
    gui_window->remove_widget(this);
    if (parent_widget)
        parent_widget->remove_widget(this);
}

void Widget::remove_widget(Widget *widget) {
    panic("unimplemented");
}

void Widget::on_size_hints_changed() {
    if (parent_widget) {
        parent_widget->on_resize();
    } else {
        bool changed = false;
        int min_w = min_width();
        int min_h = min_height();
        int max_w = max_width();
        int max_h = max_height();
        if (width < min_w) {
            width = min_w;
            changed = true;
        } else if (width > max_w) {
            width = max_w;
            changed = true;
        }
        if (height < min_h) {
            height = min_h;
            changed = true;
        } else if (height > max_h) {
            height = max_h;
            changed = true;
        }
        if (changed)
            on_resize();
    }
}

glm::mat4 Widget::transform2d(int rel_left, int rel_top, float scale_width, float scale_height) {
    return glm::scale(
                glm::translate(
                    glm::mat4(1.0f),
                    glm::vec3(left + rel_left, top + rel_top, 0.0f)),
                glm::vec3(scale_width, scale_height, 1.0f));
}

glm::mat4 Widget::transform2d(int rel_left, int rel_top) {
    return glm::translate(glm::mat4(1.0f),
                glm::vec3(left + rel_left, top + rel_top, 0.0f));
}

ContextMenuWidget *Widget::pop_context_menu(MenuWidgetItem *menu_widget_item,
        int rel_left, int rel_top, int box_width, int box_height)
{
    return gui_window->pop_context_menu(menu_widget_item, left + rel_left, top + rel_top, box_width, box_height);
}
