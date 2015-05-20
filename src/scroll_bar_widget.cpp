#include "scroll_bar_widget.hpp"
#include "gui_window.hpp"

static const int NON_EXPANDING_AXIS_SIZE = 20;
static const int MIN_SIZE = 64;
static const int HANDLE_SIZE = 16;
static const int MIN_HANDLE_LONG_SIZE = 8;
static const int TRACK_PADDING_BEGIN = 2;
static const int TRACK_PADDING_END = 2;

ScrollBarWidget::ScrollBarWidget(GuiWindow *gui_window, ScrollBarLayout layout) :
    Widget(gui_window)
{
    this->layout = layout;
    bg.set_scheme(SunkenBoxSchemeSunkenBorders);
    min_value = 0;
    max_value = 0;
    value = 0;
    handle_ratio = 0.0f;
    dragging_handle = false;
}

ScrollBarWidget::~ScrollBarWidget() { }

int ScrollBarWidget::long_dimension() const {
    return (layout == ScrollBarLayoutVert) ? height : width;
}

void ScrollBarWidget::update_model() {
    bg.update(this, 0, 0, width, height);

    int range = max_value - min_value;
    float amt = (range > 0) ? ((value - min_value) / (float)range) : 0.0f;
    handle_track_long_size = long_dimension() - TRACK_PADDING_BEGIN - TRACK_PADDING_END;
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
        handle_left = width / 2 - HANDLE_SIZE / 2;
        handle_top = TRACK_PADDING_BEGIN + amt * full_scroll;
        handle_right = handle_left + HANDLE_SIZE;
        handle_bottom = handle_top + handle_long_size;
    } else {
        handle_left = TRACK_PADDING_BEGIN + amt * full_scroll;
        handle_top = height / 2 - HANDLE_SIZE / 2,
        handle_right = handle_left + handle_long_size;
        handle_bottom = handle_top + HANDLE_SIZE;
    }
    handle.set_scheme(dragging_handle ? SunkenBoxSchemeSunkenBorders : SunkenBoxSchemeRaisedBorders);
    handle.update(this, handle_left, handle_top, handle_right - handle_left, handle_bottom - handle_top);
}

void ScrollBarWidget::draw(const glm::mat4 &projection) {
    bg.draw(gui_window, projection);
    handle.draw(gui_window, projection);
}

float ScrollBarWidget::get_pos_amt(int x, int y) {
    if (layout == ScrollBarLayoutVert) {
        return y / (float)height;
    } else {
        return x / (float)width;
    }
}

void ScrollBarWidget::on_mouse_wheel(const MouseWheelEvent *event) {
    if (parent_widget)
        forward_mouse_wheel_event(parent_widget, event);
}

void ScrollBarWidget::on_mouse_move(const MouseEvent *event) {
    switch (event->action) {
        case MouseActionDown:
            {
                if (event->button != MouseButtonLeft)
                    return;
                drag_start_x = event->x;
                drag_start_y = event->y;
                dragging_handle = true;
                bool clicked_handle = (event->x >= handle_left && event->x < handle_right &&
                                       event->y >= handle_top && event->y < handle_bottom);
                if (!clicked_handle) {
                    float amt = get_pos_amt(event->x, event->y);
                    set_value(min_value + amt * (max_value - min_value));
                    events.trigger(EventScrollValueChange);
                }
                drag_start_value = value;
                update_model();
                break;
            }
        case MouseActionUp:
            if (!dragging_handle || event->button != MouseButtonLeft)
                return;
            dragging_handle = false;
            update_model();
            break;
        case MouseActionMove:
            if (!dragging_handle)
                return;

            int range = max_value - min_value;
            int delta;
            if (layout == ScrollBarLayoutVert) {
                delta = (event->y - drag_start_y);
            } else {
                delta = (event->x - drag_start_x);
            }
            float amt = delta / (float)(handle_track_long_size - handle_long_size);
            set_value(drag_start_value + range * amt);
            events.trigger(EventScrollValueChange);

            update_model();
            break;
    }
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

