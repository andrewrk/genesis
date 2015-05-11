#include "scroll_bar_widget.hpp"
#include "gui_window.hpp"

static const int NON_EXPANDING_AXIS_SIZE = 20;
static const int MIN_SIZE = 64;
static const int HANDLE_SIZE = 16;
static const int MIN_HANDLE_LONG_SIZE = 8;

ScrollBarWidget::ScrollBarWidget(GuiWindow *gui_window, ScrollBarLayout layout) :
    Widget(gui_window)
{
    this->layout = layout;
    bg.set_scheme(SunkenBoxSchemeSunkenBorders);
    handle.set_scheme(SunkenBoxSchemeRaisedBorders);
    min_value = 0;
    max_value = 0;
    value = 0;
    handle_ratio = 0.0f;
}

ScrollBarWidget::~ScrollBarWidget() { }

int ScrollBarWidget::long_dimension() const {
    return (layout == ScrollBarLayoutVert) ? height : width;
}

void ScrollBarWidget::update_model() {
    bg.update(this, 0, 0, width, height);

    int range = max_value - min_value;
    float amt = (range > 0) ? ((value - min_value) / (float)range) : 0.0f;
    int track_padding_begin = 2;
    int track_padding_end = 2;
    int handle_track_long_size = long_dimension() - track_padding_begin - track_padding_end;
    int handle_long_size;
    if (range > 0) {
        if (handle_ratio > 0.0f) {
            handle_long_size = max(MIN_HANDLE_LONG_SIZE, handle_track_long_size * handle_ratio);
        } else {
            handle_long_size = HANDLE_SIZE;
        }
    } else {
        handle_long_size = handle_track_long_size;
    }
    int full_scroll = handle_track_long_size - handle_long_size;

    if (layout == ScrollBarLayoutVert) {
        handle.update(this,
                width / 2 - HANDLE_SIZE / 2,
                track_padding_begin + amt * full_scroll,
                HANDLE_SIZE, handle_long_size);
    } else {
        handle.update(this,
                track_padding_begin + amt * full_scroll,
                height / 2 - HANDLE_SIZE / 2,
                handle_long_size, HANDLE_SIZE);
    }
}

void ScrollBarWidget::draw(const glm::mat4 &projection) {
    bg.draw(gui_window, projection);
    handle.draw(gui_window, projection);
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
}

void ScrollBarWidget::set_handle_ratio(int container_size, int content_size) {
    assert(content_size != 0);
    handle_ratio = container_size / (float)content_size;
}
