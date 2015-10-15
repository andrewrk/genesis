#include "button_widget.hpp"
#include "gui_window.hpp"
#include "color.hpp"


ButtonWidget::~ButtonWidget() {

}

ButtonWidget::ButtonWidget(GuiWindow *gui_window) :
    Widget(gui_window),
    label(gui_window->gui)
{
    padding_left = 8;
    padding_right = 8;
    padding_top = 8;
    padding_bottom = 8;

    hovering = false;
    mouse_down = false;
    text_color = color_fg_text();

    update_model();
}

void ButtonWidget::draw(const glm::mat4 &projection) {
    bg.draw(gui_window, projection);
    label.draw(projection * label_model, text_color);
}

void ButtonWidget::on_mouse_move(const MouseEvent *event) {
    switch (event->action) {
        case MouseActionDown:
            if (event->button == MouseButtonLeft) {
                mouse_down = true;
                update_model();
            }
            break;
        case MouseActionUp:
            if (event->button == MouseButtonLeft) {
                mouse_down = false;
                update_model();
                events.trigger(EventActivate);
            }
            break;
        case MouseActionMove:
            break;
    }
}

void ButtonWidget::on_mouse_out(const MouseEvent *event) {
    hovering = false;
}

void ButtonWidget::on_mouse_over(const MouseEvent *event) {
    hovering = true;
}

void ButtonWidget::set_text(const String &text) {
    label.set_text(text);
    label.update();
    update_model();
}

void ButtonWidget::update_model() {
    bg.set_scheme(mouse_down ? SunkenBoxSchemeSunkenBorders : SunkenBoxSchemeRaisedBorders);
    bg.update(this, 0, 0, width, height);

    label_model = transform2d(width / 2 - label.width() / 2, height / 2 - label.height() / 2);
}

int ButtonWidget::min_width() const {
    return padding_left + label.width() + padding_right;
}

int ButtonWidget::max_width() const {
    return min_width();
}

int ButtonWidget::min_height() const {
    return padding_top + label.height() + padding_bottom;
}

int ButtonWidget::max_height() const {
    return min_height();
}
