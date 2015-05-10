#ifndef SUNKEN_BOX
#define SUNKEN_BOX

#include "widget.hpp"

enum SunkenBoxScheme {
    SunkenBoxSchemeSunken,
    SunkenBoxSchemeRaised,
    SunkenBoxSchemeInactive,
    SunkenBoxSchemeSunkenBorders,
};

class SunkenBox {
public:
    SunkenBox();
    ~SunkenBox() {}

    void set_scheme(SunkenBoxScheme color_scheme);
    void update(Widget *widget, int left, int top, int width, int height);
    void draw(GuiWindow *gui_window, const glm::mat4 &projection);

    glm::vec4 top_color;
    glm::vec4 bottom_color;
    glm::mat4 gradient_model;
    glm::mat4 border_top_model;
    glm::mat4 border_right_model;
    glm::mat4 border_bottom_model;
    glm::mat4 border_left_model;
    bool borders_on;
    glm::vec4 border_top_color;
    glm::vec4 border_right_color;
    glm::vec4 border_bottom_color;
    glm::vec4 border_left_color;
};


#endif
