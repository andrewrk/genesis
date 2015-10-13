#include "select_widget.hpp"
#include "gui_window.hpp"
#include "gui.hpp"
#include "color.hpp"

SelectWidget::~SelectWidget() {

}

SelectWidget::SelectWidget(GuiWindow *gui_window) :
    Widget(gui_window),
    label(gui_window->gui)
{
    arrow_icon_img = gui->img_caret_down;
    padding_left = 2;
    padding_right = 2;
    padding_top = 2;
    padding_bottom = 2;
    hovering = false;
    text_color = color_fg_text();
    selected_index = -1;

    update_model();
}

void SelectWidget::draw(const glm::mat4 &projection) {
    bg.draw(gui_window, projection);
    if (selected_index >= 0)
        label.draw(projection * label_model, text_color);
}

void SelectWidget::update_model() {
    bg.set_scheme(hovering ? SunkenBoxSchemeSunkenBorders : SunkenBoxSchemeRaisedBorders);
    bg.update(this, 0, 0, width, height);

    label_model = transform2d(padding_left, padding_top);
}

void SelectWidget::clear() {
    items.clear();
}

void SelectWidget::append_choice(const String &choice) {
    ok_or_panic(items.append(choice));
}

void SelectWidget::select_index(int index) {
    selected_index = index;
    if (selected_index >= 0) {
        label.set_text(items.at(selected_index));
        label.update();
        update_model();
    }
}

void SelectWidget::on_mouse_move(const MouseEvent *event) {
    // TODO
}

void SelectWidget::on_mouse_out(const MouseEvent *event) {
    hovering = false;
}

void SelectWidget::on_mouse_over(const MouseEvent *event) {
    hovering = true;
}

int SelectWidget::min_height() const {
    return padding_top + label.height() + padding_bottom;
}

int SelectWidget::max_height() const {
    return min_height();
}

int SelectWidget::min_width() const {
    return padding_left + label.width() + padding_right;
}
