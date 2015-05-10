#include "scroll_bar_widget.hpp"
#include "gui_window.hpp"

static const int NON_EXPANDING_AXIS_SIZE = 20;
static const int MIN_SIZE = 64;
static const int HANDLE_SIZE = 16;

ScrollBarWidget::ScrollBarWidget(GuiWindow *gui_window, ScrollBarLayout layout) :
    Widget(gui_window)
{
    this->layout = layout;
    sunken_box.set_scheme(SunkenBoxSchemeSunkenBorders);
    min_value = 0;
    max_value = 0;
    value = 0;
}

ScrollBarWidget::~ScrollBarWidget() { }

void ScrollBarWidget::update_model() {
    sunken_box.update(this, 0, 0, width, height);
    // TODO model for handle
}

void ScrollBarWidget::draw(const glm::mat4 &projection) {
    sunken_box.draw(gui_window, projection);
    // TODO draw handle
}

void ScrollBarWidget::on_mouse_move(const MouseEvent *) {
    // TODO handle scrolling
}

int ScrollBarWidget::min_width() const {
    if (layout == ScrollBarLayoutVert) {
        return NON_EXPANDING_AXIS_SIZE;
    } else {
        return MIN_SIZE;
    }
}

int ScrollBarWidget::max_width() const {
    if (layout == ScrollBarLayoutVert) {
        return NON_EXPANDING_AXIS_SIZE;
    } else {
        return -1;
    }
}

int ScrollBarWidget::min_height() const {
    if (layout == ScrollBarLayoutVert) {
        return MIN_SIZE;
    } else {
        return NON_EXPANDING_AXIS_SIZE;
    }
}

int ScrollBarWidget::max_height() const {
    if (layout == ScrollBarLayoutVert) {
        return -1;
    } else {
        return NON_EXPANDING_AXIS_SIZE;
    }
}

void ScrollBarWidget::set_value(int new_value) {
    value = clamp(min_value, new_value, max_value);
    update_model();
}
