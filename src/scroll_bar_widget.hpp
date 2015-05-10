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

    void set_value(int value);

    int min_width() const override;
    int max_width() const override;
    int min_height() const override;
    int max_height() const override;

    void on_resize() override { update_model(); }

    ScrollBarLayout layout;
    int min_value;
    int max_value;
    int value;
    EventDispatcher events;
    SunkenBox sunken_box;

    void update_model();
};

#endif
