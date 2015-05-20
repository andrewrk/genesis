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
    track_editor_widget->refresh_tracks();
    track_editor_widget->update_model();
}

static void vert_scroll_callback(Event, void *userdata) {
    TrackEditorWidget *track_editor = (TrackEditorWidget *)userdata;
    track_editor->update_model();
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
    display_track_count = 0;
    vert_scroll_bar = create<ScrollBarWidget>(gui_window, ScrollBarLayoutVert);
    horiz_scroll_bar = create<ScrollBarWidget>(gui_window, ScrollBarLayoutHoriz);

    refresh_tracks();
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
    vert_scroll_bar->events.attach_handler(EventScrollValueChange, vert_scroll_callback, this);
}

TrackEditorWidget::~TrackEditorWidget() {
    project->events.detach_handler(EventProjectTracksChanged, on_tracks_changed);

    destroy(vert_scroll_bar, 1);
    destroy(horiz_scroll_bar, 1);

    for (int i = 0; i < gui_tracks.length(); i += 1) {
        destroy_gui_track(gui_tracks.at(i));
    }
}

void TrackEditorWidget::draw(const glm::mat4 &projection) {
    timeline_bg.draw(gui_window, projection);
    horiz_scroll_bar->draw(projection);
    vert_scroll_bar->draw(projection);

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear(GL_STENCIL_BUFFER_BIT);

    gui_window->fill_rect(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), projection * stencil_model);

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilMask(0x00);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    for (int i = 0; i < display_track_count; i += 1) {
        DisplayTrack *display_track = display_tracks.at(i);
        display_track->head_bg.draw(gui_window, projection);
        display_track->body_bg.draw(gui_window, projection);
        display_track->track_name_label->draw(
                projection * display_track->track_name_label_model, track_name_color);

        gui_window->fill_rect(light_border_color, projection * display_track->border_top_model);
        gui_window->fill_rect(dark_border_color, projection * display_track->border_bottom_model);
    }

    glDisable(GL_STENCIL_TEST);

}

TrackEditorWidget::DisplayTrack * TrackEditorWidget::create_display_track(GuiTrack *gui_track) {
    DisplayTrack *result = create<DisplayTrack>();
    result->track_name_label = create<Label>(gui);
    result->gui_track = gui_track;
    gui_track->display_track = result;
    ok_or_panic(display_tracks.append(result));
    return result;
}

void TrackEditorWidget::destroy_display_track(DisplayTrack *display_track) {
    if (display_track) {
        if (display_track->gui_track)
            display_track->gui_track->display_track = nullptr;
        destroy(display_track->track_name_label, 1);
        destroy(display_track, 1);
    }
}


void TrackEditorWidget::update_model() {
    horiz_scroll_bar->left = left;
    horiz_scroll_bar->top = top;
    horiz_scroll_bar->width = width;
    horiz_scroll_bar->height = horiz_scroll_bar->min_height();
    horiz_scroll_bar->on_resize();

    int timeline_top = horiz_scroll_bar->min_height();
    int timeline_bottom = timeline_top + timeline_height;
    timeline_bg.update(this, padding_left, timeline_top,
            width - padding_left - padding_right, timeline_height);

    int track_area_top = timeline_bottom;
    int track_area_bottom = track_area_top + (height - track_area_top);

    int first_top = timeline_top + timeline_height;
    int next_top = first_top;
    head_left = padding_left;
    body_left = head_left + track_head_width;
    int body_width = width - vert_scroll_bar->min_width() - padding_right - body_left;

    // compute track positions
    for (int i = 0; i < gui_tracks.length(); i += 1) {
        GuiTrack *gui_track = gui_tracks.at(i);
        gui_track->left = padding_left;
        gui_track->right = body_width;
        gui_track->top = next_top;
        next_top += track_height;
        gui_track->bottom = next_top;
        next_top += 1;
    }
    int full_height = next_top - first_top;
    int available_width = width - vert_scroll_bar->min_width();
    int available_height = track_area_bottom - track_area_top;

    stencil_model = transform2d(0, track_area_top, available_width, available_height);

    vert_scroll_bar->left = left + width - vert_scroll_bar->min_width();
    vert_scroll_bar->top = top + timeline_bottom;
    vert_scroll_bar->width = vert_scroll_bar->min_width();
    vert_scroll_bar->height = height - timeline_bottom;
    vert_scroll_bar->min_value = 0;
    vert_scroll_bar->max_value = max(0, full_height - available_height);
    vert_scroll_bar->set_handle_ratio(available_height, full_height);
    vert_scroll_bar->set_value(vert_scroll_bar->value);
    vert_scroll_bar->on_resize();

    // now consider scroll position and create display tracks for tracks that
    // are visible
    display_track_count = 0;
    for (int i = 0; i < gui_tracks.length(); i += 1) {
        GuiTrack *gui_track = gui_tracks.at(i);
        if (gui_track->bottom - vert_scroll_bar->value >= track_area_top &&
            gui_track->top - vert_scroll_bar->value < track_area_bottom)
        {
            DisplayTrack *display_track;
            if (display_track_count >= display_tracks.length())
                display_track = create_display_track(gui_track);
            else
                display_track = display_tracks.at(display_track_count);
            display_track_count += 1;

            display_track->gui_track = gui_track;
            gui_track->display_track = display_track;

            display_track->top = gui_track->top - vert_scroll_bar->value;
            display_track->bottom = gui_track->bottom - vert_scroll_bar->value;

            int head_top = display_track->top;
            display_track->head_bg.update(this, padding_left, head_top, track_head_width, track_height);
            display_track->head_bg.set_scheme(SunkenBoxSchemeRaised);
            display_track->body_bg.update(this, body_left, head_top, body_width, track_height);

            display_track->track_name_label->set_text(gui_track->track->name);
            display_track->track_name_label->update();

            int label_left = head_left + track_name_label_padding_left;
            int label_top = head_top + track_name_label_padding_top;
            display_track->track_name_label_model = transform2d(label_left, label_top);

            display_track->border_top_model = transform2d(
                    gui_track->left, display_track->top, available_width, 1.0f);
            display_track->border_bottom_model = transform2d(
                    gui_track->left, display_track->bottom, available_width, 1.0f);
        }
    }
}

TrackEditorWidget::GuiTrack * TrackEditorWidget::create_gui_track() {
    return create<GuiTrack>();
}

void TrackEditorWidget::destroy_gui_track(GuiTrack *gui_track) {
    if (gui_track) {
        if (menu_track == gui_track)
            clear_track_context_menu();
        destroy(gui_track, 1);
    }
}

void TrackEditorWidget::on_mouse_move(const MouseEvent *event) {
    if (forward_mouse_event(vert_scroll_bar, event))
        return;
    if (forward_mouse_event(horiz_scroll_bar, event))
        return;
    if (event->button == MouseButtonRight && event->action == MouseActionDown) {
        GuiTrack *gui_track = get_track_head_at(event->x, event->y);
        if (gui_track)
            right_click_track_head(gui_track, event->x, event->y);
    }
}

TrackEditorWidget::GuiTrack * TrackEditorWidget::get_track_head_at(int x, int y) {
    for (int i = 0; i < display_tracks.length(); i += 1) {
        DisplayTrack *display_track = display_tracks.at(i);

        if (y >= display_track->top && y < display_track->bottom && x >= head_left && x < body_left)
            return display_track->gui_track;
    }
    return nullptr;
}

TrackEditorWidget::GuiTrack *TrackEditorWidget::get_track_body_at(int x, int y) {
    for (int i = 0; i < display_tracks.length(); i += 1) {
        DisplayTrack *display_track = display_tracks.at(i);

        if (y >= display_track->top && y < display_track->bottom && x >= body_left)
            return display_track->gui_track;
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

        project_add_audio_clip(project, audio_asset);
    }
}

void TrackEditorWidget::on_drag_audio_asset(AudioAsset *audio_asset, const DragEvent *event) {
    if (event->action == DragActionDrop) {
        GuiTrack *gui_track = get_track_body_at(event->mouse_event.x, event->mouse_event.y);
        if (!gui_track)
            return;

        project_add_audio_clip(project, audio_asset);
    }
}

void TrackEditorWidget::refresh_tracks() {
    int i;
    for (i = 0; i < project->track_list.length(); i += 1) {
        Track *track = project->track_list.at(i);
        GuiTrack *gui_track;
        if (i < gui_tracks.length()) {
            gui_track = gui_tracks.at(i);
        } else {
            gui_track = create_gui_track();
            ok_or_panic(gui_tracks.append(gui_track));
        }

        gui_track->track = track;
    }
    while (i < gui_tracks.length()) {
        GuiTrack *gui_track = gui_tracks.pop();
        destroy_gui_track(gui_track);
    }
}

void TrackEditorWidget::on_mouse_wheel(const MouseWheelEvent *event) {
    {
        float range = vert_scroll_bar->max_value - vert_scroll_bar->min_value;
        vert_scroll_bar->set_value(vert_scroll_bar->value - event->wheel_y * range * 0.18f * vert_scroll_bar->handle_ratio);
    }
    update_model();
}

