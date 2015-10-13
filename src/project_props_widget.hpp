#ifndef PROJECT_PROPS_WIDGET_HPP
#define PROJECT_PROPS_WIDGET_HPP

#include "widget.hpp"
#include "grid_layout_widget.hpp"

struct Project;
class TextWidget;

class ProjectPropsWidget : public Widget {
public:
    ProjectPropsWidget(GuiWindow *gui_window, Project *project);
    ~ProjectPropsWidget() override;
    void draw(const glm::mat4 &projection) override;
    void on_resize() override;

    void on_mouse_move(const MouseEvent *) override;


    Project *project;
    GridLayoutWidget layout;

    TextWidget *create_form_label(const char *text);

};

#endif
