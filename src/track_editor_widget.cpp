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
    track_height(60),
    track_name_color(color_fg_text()),
    track_head_bg_color(color_light_bg()),
    track_main_bg_color(color_dark_bg()),
    timeline_bg_color(color_dark_bg_alt())
{
    update_model();
}

void TrackEditorWidget::draw(const glm::mat4 &projection) {
    gui_window->fill_rect(timeline_bg_color, projection * timeline_model);

    for (int i = 0; i < tracks.length(); i += 1) {
        GuiTrack *gui_track = &tracks.at(i);
        gui_window->fill_rect(track_head_bg_color, projection * gui_track->head_model);
        gui_window->fill_rect(track_main_bg_color, projection * gui_track->body_model);
    }
}

void TrackEditorWidget::update_model() {
    ok_or_panic(tracks.resize(project->track_list.length()));

    int timeline_top = padding_top;
    timeline_model = transform2d(padding_left, padding_top,
            width - padding_left - padding_right, timeline_height);

    int next_top = timeline_top + timeline_height;
    for (int i = 0; i < tracks.length(); i += 1) {
        Track *track = project->track_list.at(i);
        GuiTrack *gui_track = &tracks.at(i);

        gui_track->track = track;

        int head_left = padding_left;
        int head_top = next_top;
        next_top += track_height;
        gui_track->head_model = transform2d(padding_left, head_top, track_head_width, track_height);

        int body_left = head_left + track_head_width;
        int body_width = width - padding_right - body_left;
        gui_track->body_model = transform2d(body_left, head_top, body_width, track_height);
    }

}
