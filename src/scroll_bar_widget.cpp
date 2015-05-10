#include "scroll_bar_widget.hpp"
#include "gui_window.hpp"

static const int NON_EXPANDING_AXIS_SIZE = 20;
static const int MIN_SIZE = 64;
static const int BAR_SIZE = 16;

ScrollBarWidget::ScrollBarWidget(GuiWindow *gui_window, ScrollBarLayout layout) :
    Widget(gui_window)
{
    this->layout = layout;
}

ScrollBarWidget::~ScrollBarWidget() { }

void ScrollBarWidget::update_model() {
    sunken_box.update(this, 0, 0, width, height);
}

void ScrollBarWidget::draw(const glm::mat4 &projection) {
    sunken_box.draw(gui_window, projection);
}

void ScrollBarWidget::on_mouse_move(const MouseEvent *) {
    // TODO
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
