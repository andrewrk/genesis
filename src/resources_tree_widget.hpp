#ifndef RESOURCES_TREE_WIDGET
#define RESOURCES_TREE_WIDGET

#include "widget.hpp"
#include "color.hpp"

class GuiWindow;

class ResourcesTreeWidget : public Widget {
public:

    ResourcesTreeWidget(GuiWindow *gui_window) :
        Widget(gui_window),
        bg_color(parse_color("#333333"))
    {
    }
    ~ResourcesTreeWidget() override { }

    void draw(const glm::mat4 &projection) override {
        glm::mat4 bg_mvp = projection * bg_model;
        gui_window->fill_rect(bg_color, bg_mvp);
    }

    void on_resize() override {
        bg_model = glm::scale(
                    glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(left, top, 0.0f)),
                    glm::vec3(width, height, 1.0f));
    }

    glm::vec4 bg_color;
    glm::mat4 bg_model;
};

#endif
