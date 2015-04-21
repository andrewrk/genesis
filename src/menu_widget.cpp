#include "menu_widget.hpp"
#include "color.hpp"
#include "gui_window.hpp"

MenuWidget::MenuWidget(GuiWindow *gui_window) :
    Widget(gui_window),
    bg_color(parse_color("#CECECE")),
    text_color(parse_color("#353535")),
    spacing_left(12),
    spacing_right(12),
    spacing_top(4),
    spacing_bottom(4)
{
}

void MenuWidget::draw(const glm::mat4 &projection) {
    // background
    glm::mat4 bg_mvp = projection * bg_model;
    gui_window->fill_rect(bg_color, bg_mvp);

    for (int i = 0; i < children.length(); i += 1) {
        TopLevelMenu *child = &children.at(i);
        glm::mat4 label_mvp = projection * child->label_model;
        child->label->draw(gui_window, label_mvp, text_color);
    }
}
