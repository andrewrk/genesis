#ifndef TRACK_EDITOR_WIDGET_HPP
#define TRACK_EDITOR_WIDGET_HPP

#include "widget.hpp"

class Project;

class TrackEditorWidget : public Widget {
public:
    TrackEditorWidget(GuiWindow *gui_window, Project *project) :
        Widget(gui_window),
        project(project)
    {
        update_model();
    }
    ~TrackEditorWidget() override {}

    void draw(const glm::mat4 &projection) override;
    void on_resize() override { update_model(); }


    Project *project;

    void update_model();
};

#endif
