#ifndef GENESIS_BUTTON_WIDGET_HPP
#define GENESIS_BUTTON_WIDGET_HPP

#include "string.hpp"
#include "label.hpp"
#include "widget.hpp"
#include "sunken_box.hpp"
#include "event_dispatcher.hpp"

struct SpritesheetImage;

class ButtonWidget : public Widget {
public:
    ButtonWidget(GuiWindow *gui_window);
    ~ButtonWidget() override;

    void draw(const glm::mat4 &projection) override;

    void on_mouse_move(const MouseEvent *event) override;
    void on_mouse_out(const MouseEvent *event) override;
    void on_mouse_over(const MouseEvent *event) override;
    void on_resize() override { update_model(); }

    int min_width() const override;
    int max_width() const override;
    int min_height() const override;
    int max_height() const override;

    void set_text(const String &text);

    EventDispatcher events;

    Label label;
    glm::mat4 label_model;
    SunkenBox bg;

    int padding_left;
    int padding_right;
    int padding_top;
    int padding_bottom;
    glm::vec4 text_color;

    bool hovering;
    bool mouse_down;

    void update_model();
};

#endif
