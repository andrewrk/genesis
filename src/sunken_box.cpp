#include "sunken_box.hpp"
#include "color.hpp"
#include "gui_window.hpp"

SunkenBox::SunkenBox() {
    set_scheme(SunkenBoxSchemeSunken);
}

void SunkenBox::set_scheme(SunkenBoxScheme color_scheme) {
    switch (color_scheme) {
        case SunkenBoxSchemeSunkenBorders:
            top_color = color_dark_bg();
            bottom_color = color_dark_bg_highlight();
            borders_on = true;
            border_top_color = color_light_border();
            border_right_color = color_light_border();
            border_bottom_color = color_light_border();
            border_left_color = color_light_border();
            break;
        case SunkenBoxSchemeSunken:
            top_color = color_dark_bg();
            bottom_color = color_dark_bg_highlight();
            borders_on = false;
            break;
        case SunkenBoxSchemeRaised:
            top_color = color_dark_bg_highlight();
            bottom_color = color_dark_bg();
            borders_on = false;
            break;
        case SunkenBoxSchemeRaisedBorders:
            top_color = color_dark_bg_highlight();
            bottom_color = color_dark_bg();
            borders_on = true;
            border_top_color = color_light_border();
            border_right_color = color_light_border();
            border_bottom_color = color_light_border();
            border_left_color = color_light_border();
            break;
        case SunkenBoxSchemeInactive:
            top_color = color_dark_bg_inactive();
            bottom_color = color_dark_bg_inactive();
            borders_on = false;
            break;
    }
}

void SunkenBox::update(Widget *widget, int left, int top, int width, int height) {
    gradient_model = widget->transform2d(left, top, width, height);
    if (borders_on) {
        border_top_model = widget->transform2d(left, top, width, 1);
        border_bottom_model = widget->transform2d(left, top + height - 1, width, 1);
        border_left_model = widget->transform2d(left, top, 1, height);
        border_right_model = widget->transform2d(left + width, top, 1, height);
    }
}

void SunkenBox::draw(GuiWindow *gui_window, const glm::mat4 &projection) {
    gui_window->fill_rect_gradient(top_color, bottom_color, projection * gradient_model);
    if (borders_on) {
        gui_window->fill_rect(border_top_color, projection * border_top_model);
        gui_window->fill_rect(border_right_color, projection * border_right_model);
        gui_window->fill_rect(border_bottom_color, projection * border_bottom_model);
        gui_window->fill_rect(border_left_color, projection * border_left_model);
    }
}
