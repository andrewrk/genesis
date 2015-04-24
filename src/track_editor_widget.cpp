#include "track_editor_widget.hpp"
#include "project.hpp"
#include "color.hpp"
#include "gui_window.hpp"

TrackEditorWidget::TrackEditorWidget(GuiWindow *gui_window, Project *project) :
    Widget(gui_window),
    project(project),
    padding_top(0),
    padding_bottom(0),
    padding_left(0),
    padding_right(0),
    timeline_height(24),
    track_head_width(90),
    track_name_color(color_fg_text()),
    track_head_bg_color(color_light_bg()),
    track_main_bg_color(color_dark_bg()),
    timeline_bg_color(color_dark_bg_alt())
{
    update_model();
}

void TrackEditorWidget::draw(const glm::mat4 &projection) {
    gui_window->fill_rect(timeline_bg_color, projection * timeline_model);
}

void TrackEditorWidget::update_model() {
    ok_or_panic(tracks.resize(project->track_list.length()));

    for (int i = 0; i < tracks.length(); i += 1) {
        Track *track = project->track_list.at(i);
        GuiTrack *gui_track = &tracks.at(i);

        gui_track->track = track;
    }


    timeline_model = glm::scale(
                    glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(left, top, 0.0f)),
                    glm::vec3(width, timeline_height, 1.0f));
}
