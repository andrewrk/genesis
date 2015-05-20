#include "track_editor_widget.hpp"
#include "project.hpp"
#include "color.hpp"
#include "gui_window.hpp"
#include "label.hpp"
#include "menu_widget.hpp"
#include "scroll_bar_widget.hpp"
#include "dragged_sample_file.hpp"

static void insert_track_before_handler(void *userdata) {
    TrackEditorWidget *track_editor_widget = (TrackEditorWidget *)userdata;
    project_insert_track(track_editor_widget->project, nullptr, track_editor_widget->menu_track->track);
}

static void insert_track_after_handler(void *userdata) {
    TrackEditorWidget *track_editor_widget = (TrackEditorWidget *)userdata;
    project_insert_track(track_editor_widget->project, track_editor_widget->menu_track->track, nullptr);
}

static void delete_track_handler(void *userdata) {
    TrackEditorWidget *track_editor_widget = (TrackEditorWidget *)userdata;
    project_delete_track(track_editor_widget->project, track_editor_widget->menu_track->track);
}

static void on_tracks_changed(Event, void *userdata) {
    TrackEditorWidget *track_editor_widget = (TrackEditorWidget *)userdata;
    track_editor_widget->update_model();
}

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
    track_name_label_padding_left(4),
    track_name_label_padding_top(4),
    track_name_color(color_fg_text()),
    track_main_bg_color(color_dark_bg()),
    timeline_bg_color(color_dark_bg_alt()),
    dark_border_color(color_dark_border()),
    light_border_color(color_light_border()),
    menu_track(nullptr)
{
    vert_scroll_bar = create<ScrollBarWidget>(gui_window, ScrollBarLayoutVert);
    horiz_scroll_bar = create<ScrollBarWidget>(gui_window, ScrollBarLayoutHoriz);

    update_model();

    track_context_menu = create<MenuWidgetItem>(gui_window);
    track_context_menu->add_menu("&Rename Track", shortcut(VirtKeyF2));
    MenuWidgetItem *insert_track_before_menu = track_context_menu->add_menu("Insert Track &Before", no_shortcut());
    MenuWidgetItem *insert_track_after_menu = track_context_menu->add_menu("Insert Track &After", no_shortcut());
    MenuWidgetItem *delete_track_menu = track_context_menu->add_menu("&Delete Track", no_shortcut());

    insert_track_before_menu->set_activate_handler(insert_track_before_handler, this);
    insert_track_after_menu->set_activate_handler(insert_track_after_handler, this);
    delete_track_menu->set_activate_handler(delete_track_handler, this);

    project->events.attach_handler(EventProjectTracksChanged, on_tracks_changed, this);
}

TrackEditorWidget::~TrackEditorWidget() {
    project->events.detach_handler(EventProjectTracksChanged, on_tracks_changed);

    destroy(vert_scroll_bar, 1);
    destroy(horiz_scroll_bar, 1);

    for (int i = 0; i < tracks.length(); i += 1) {
        destroy(tracks.at(i), 1);
        destroy_gui_track(tracks.at(i));
    }
}

void TrackEditorWidget::draw(const glm::mat4 &projection) {
    timeline_bg.draw(gui_window, projection);

    for (int i = 0; i < tracks.length(); i += 1) {
        GuiTrack *gui_track = tracks.at(i);
        gui_track->head_bg.draw(gui_window, projection);
        gui_track->body_bg.draw(gui_window, projection);
        gui_track->track_name_label->draw(
                projection * gui_track->track_name_label_model, track_name_color);

        gui_window->fill_rect(light_border_color, projection * gui_track->border_top_model);
        gui_window->fill_rect(dark_border_color, projection * gui_track->border_bottom_model);
    }

    horiz_scroll_bar->draw(projection);
}

void TrackEditorWidget::update_model() {
    horiz_scroll_bar->left = left;
    horiz_scroll_bar->top = top;
    horiz_scroll_bar->width = width;
    horiz_scroll_bar->height = horiz_scroll_bar->min_height();
    horiz_scroll_bar->on_resize();

    int timeline_top = horiz_scroll_bar->height;
    timeline_bg.update(this, padding_left, timeline_top,
            width - padding_left - padding_right, timeline_height);

    int next_top = timeline_top + timeline_height;
    head_left = padding_left;
    body_left = head_left + track_head_width;
    int body_width = width - padding_right - body_left;

    int i;
    for (i = 0; i < project->track_list.length(); i += 1) {
        Track *track = project->track_list.at(i);
        GuiTrack *gui_track;
        if (i < tracks.length()) {
            gui_track = tracks.at(i);
        } else {
            gui_track = create_gui_track();
            ok_or_panic(tracks.append(gui_track));
        }

        gui_track->track = track;

        gui_track->left = padding_left;
        gui_track->top = next_top;
        next_top += track_height;
        gui_track->bottom = next_top;
        next_top += 1;

        int head_top = gui_track->top;
        gui_track->head_bg.update(this, padding_left, head_top, track_head_width, track_height);
        gui_track->head_bg.set_scheme(SunkenBoxSchemeRaised);
        gui_track->body_bg.update(this, body_left, head_top, body_width, track_height);

        gui_track->track_name_label->set_text(track->name);
        gui_track->track_name_label->update();
        int label_left = head_left + track_name_label_padding_left;
        int label_top = head_top + track_name_label_padding_top;
        gui_track->track_name_label_model = transform2d(label_left, label_top);

        int entire_width = width - padding_left - padding_right;
        gui_track->border_top_model = transform2d(gui_track->left, gui_track->top, entire_width, 1.0f);
        gui_track->border_bottom_model = transform2d(gui_track->left, gui_track->bottom, entire_width, 1.0f);
    }
    while (i < tracks.length()) {
        GuiTrack *gui_track = tracks.pop();
        destroy_gui_track(gui_track);
    }

}

TrackEditorWidget::GuiTrack * TrackEditorWidget::create_gui_track() {
    GuiTrack *gui_track = create<GuiTrack>();
    gui_track->track_name_label = create<Label>(gui_window->gui);
    return gui_track;
}

void TrackEditorWidget::destroy_gui_track(GuiTrack *gui_track) {
    if (gui_track) {
        if (menu_track == gui_track)
            clear_track_context_menu();
        destroy(gui_track->track_name_label, 1);
        destroy(gui_track, 1);
    }
}

void TrackEditorWidget::on_mouse_move(const MouseEvent *event) {
    if (event->button == MouseButtonRight && event->action == MouseActionDown) {
        GuiTrack *gui_track = get_track_head_at(event->x, event->y);
        if (gui_track)
            right_click_track_head(gui_track, event->x, event->y);
    }
}

TrackEditorWidget::GuiTrack * TrackEditorWidget::get_track_head_at(int x, int y) {
    for (int i = 0; i < tracks.length(); i += 1) {
        GuiTrack *gui_track = tracks.at(i);

        if (y >= gui_track->top && y < gui_track->bottom && x >= head_left && x < body_left)
            return gui_track;
    }
    return nullptr;
}

TrackEditorWidget::GuiTrack *TrackEditorWidget::get_track_body_at(int x, int y) {
    for (int i = 0; i < tracks.length(); i += 1) {
        GuiTrack *gui_track = tracks.at(i);

        if (y >= gui_track->top && y < gui_track->bottom && x >= body_left)
            return gui_track;
    }
    return nullptr;
}

static void on_context_menu_destroy(ContextMenuWidget *context_menu) {
    TrackEditorWidget *track_editor_widget = (TrackEditorWidget*)context_menu->userdata;
    track_editor_widget->menu_track = nullptr;
}

void TrackEditorWidget::right_click_track_head(GuiTrack *gui_track, int x, int y) {
    menu_track = gui_track;
    ContextMenuWidget *context_menu = pop_context_menu(track_context_menu, x, y, 1, 1);
    context_menu->userdata = this;
    context_menu->on_destroy = on_context_menu_destroy;
}

void TrackEditorWidget::clear_track_context_menu() {
    if (menu_track)
        gui_window->destroy_context_menu();
}

void TrackEditorWidget::on_drag(const DragEvent *event) {
    switch (event->drag_data->drag_type) {
        default: break;
        case DragTypeSampleFile:
            on_drag_sample_file((DraggedSampleFile *)event->drag_data->ptr, event);
            break;
        case DragTypeAudioAsset:
            on_drag_audio_asset((AudioAsset *)event->drag_data->ptr, event);
            break;
    }
}

void TrackEditorWidget::on_drag_sample_file(DraggedSampleFile *dragged_sample_file, const DragEvent *event) {
    if (event->action == DragActionDrop) {
        GuiTrack *gui_track = get_track_body_at(event->mouse_event.x, event->mouse_event.y);
        if (!gui_track)
            return;

        int err;
        AudioAsset *audio_asset;
        if ((err = project_add_audio_asset(project, dragged_sample_file->full_path, &audio_asset))) {
            if (err != GenesisErrorAlreadyExists)
                ok_or_panic(err);
        }

        AudioClip *audio_clip;
        ok_or_panic(project_add_audio_clip(project, audio_asset, &audio_clip));
    }
}

void TrackEditorWidget::on_drag_audio_asset(AudioAsset *audio_asset, const DragEvent *event) {
    if (event->action == DragActionDrop) {
        GuiTrack *gui_track = get_track_body_at(event->mouse_event.x, event->mouse_event.y);
        if (!gui_track)
            return;

        AudioClip *audio_clip;
        ok_or_panic(project_add_audio_clip(project, audio_asset, &audio_clip));
    }
}
