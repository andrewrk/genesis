#include "sunken_box.hpp"
#include "color.hpp"
#include "gui_window.hpp"

SunkenBox::SunkenBox() {
    set_scheme(SunkenBoxSchemeSunken);
}

void SunkenBox::set_scheme(SunkenBoxScheme color_scheme) {
    switch (color_scheme) {
        case SunkenBoxSchemeSunken:
            top_color = color_dark_bg();
            bottom_color = color_dark_bg_highlight();
            break;
        case SunkenBoxSchemeRaised:
            top_color = color_dark_bg_highlight();
            bottom_color = color_dark_bg();
            break;
        case SunkenBoxSchemeInactive:
            top_color = color_dark_bg_inactive();
            bottom_color = color_dark_bg_inactive();
            break;
    }
}

void SunkenBox::update(Widget *widget, int left, int top, int width, int height) {
    gradient_model = widget->transform2d(left, top, width, height);
}

void SunkenBox::draw(GuiWindow *gui_window, const glm::mat4 &projection) {
    gui_window->fill_rect_gradient(top_color, bottom_color, projection * gradient_model);
}
