#ifndef SCROLL_BAR_WIDGET
#define SCROLL_BAR_WIDGET

#include "widget.hpp"
#include "event_dispatcher.hpp"
#include "sunken_box.hpp"

enum ScrollBarLayout {
    ScrollBarLayoutVert,
    ScrollBarLayoutHoriz,
};

class GuiWindow;

class ScrollBarWidget : public Widget {
public:
    ScrollBarWidget(GuiWindow *gui_window, ScrollBarLayout layout);
    ~ScrollBarWidget() override;

    void draw(const glm::mat4 &projection) override;
    void on_mouse_move(const MouseEvent *) override;
    void on_mouse_wheel(const MouseWheelEvent *) override;

    void set_value(int value);
    void set_handle_ratio(int container_size, int content_size);

    int min_width() const override;
    int max_width() const override;
    int min_height() const override;
    int max_height() const override;

    void on_resize() override { update_model(); }

    ScrollBarLayout layout;
    float handle_ratio;
    int min_value;
    int max_value;
    int value;
    EventDispatcher events;
    SunkenBox bg;
    SunkenBox handle;
    int handle_left;
    int handle_right;
    int handle_top;
    int handle_bottom;
    int handle_track_long_size;
    int handle_long_size;
    int drag_start_x;
    int drag_start_y;
    bool dragging_handle;
    int drag_start_value;

    void update_model();
    int long_dimension() const;
    float get_pos_amt(int x, int y);
};

#endif
