#ifndef TRACK_EDITOR_WIDGET_HPP
#define TRACK_EDITOR_WIDGET_HPP

#include "widget.hpp"

struct Project;
struct Track;

class TrackEditorWidget : public Widget {
public:
    TrackEditorWidget(GuiWindow *gui_window, Project *project);
    ~TrackEditorWidget() override {}

    void draw(const glm::mat4 &projection) override;
    void on_resize() override { update_model(); }


    Project *project;

    int padding_top;
    int padding_bottom;
    int padding_left;
    int padding_right;
    int timeline_height;
    int track_head_width;
    int track_height;

    glm::vec4 track_name_color;
    glm::vec4 track_head_bg_color;
    glm::vec4 track_main_bg_color;
    glm::vec4 timeline_bg_color;

    glm::mat4 timeline_model;

    struct GuiTrack {
        Track *track;
        glm::mat4 head_model;
        glm::mat4 body_model;
    };
    List<GuiTrack> tracks;

    void update_model();
};

#endif
