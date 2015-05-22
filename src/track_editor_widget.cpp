#include "track_editor_widget.hpp"
#include "project.hpp"
#include "color.hpp"
#include "gui_window.hpp"
#include "label.hpp"
#include "menu_widget.hpp"
#include "scroll_bar_widget.hpp"
#include "dragged_sample_file.hpp"
#include "gui.hpp"
#include "audio_graph.hpp"

static const int SEGMENT_PADDING = 2;
static const int EXTRA_SCROLL_WIDTH = 200;
static const int SEGMENT_TITLE_PADDING = 2;

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

static void on_play_head_changed(Event, void *userdata) {
    TrackEditorWidget *track_editor_widget = (TrackEditorWidget *)userdata;
    track_editor_widget->update_play_head_model();
}

static void scroll_callback(Event, void *userdata) {
    TrackEditorWidget *track_editor = (TrackEditorWidget *)userdata;
    track_editor->update_model();
}

TrackEditorWidget::TrackEditorWidget(GuiWindow *gui_window, Project *project) :
    Widget(gui_window),
    project(project),
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
    play_head_color = parse_color("#F47A28AA");
    play_head_icon = gui->img_play_head;
    timeline_bottom_border_color = color_light_border();

    display_track_count = 0;
    vert_scroll_bar = create<ScrollBarWidget>(gui_window, ScrollBarLayoutVert);
    vert_scroll_bar->parent_widget = this;
    horiz_scroll_bar = create<ScrollBarWidget>(gui_window, ScrollBarLayoutHoriz);
    horiz_scroll_bar->parent_widget = this;
    pixels_per_whole_note = 100.0;

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
    project->events.attach_handler(EventProjectAudioClipSegmentsChanged, on_tracks_changed, this);
    project->events.attach_handler(EventProjectPlayHeadChanged, on_play_head_changed, this);
    vert_scroll_bar->events.attach_handler(EventScrollValueChange, scroll_callback, this);
    horiz_scroll_bar->events.attach_handler(EventScrollValueChange, scroll_callback, this);
}

TrackEditorWidget::~TrackEditorWidget() {
    project->events.detach_handler(EventProjectTracksChanged, on_tracks_changed);

    destroy(vert_scroll_bar, 1);
    destroy(horiz_scroll_bar, 1);

    for (int i = 0; i < gui_tracks.length(); i += 1) {
        destroy_gui_track(gui_tracks.at(i));
    }
    for (int i = 0; i < display_tracks.length(); i += 1) {
        destroy_display_track(display_tracks.at(i));
    }
}

void TrackEditorWidget::draw(const glm::mat4 &projection) {
    glm::vec4 white = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::mat4 track_area_stencil_mvp = projection * track_area_stencil;
    glm::mat4 widget_stencil_mvp = projection * widget_stencil;

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear(GL_STENCIL_BUFFER_BIT);

    gui_window->fill_rect(white, track_area_stencil_mvp);

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilMask(0x00);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    for (int track_i = 0; track_i < display_track_count; track_i += 1) {
        DisplayTrack *display_track = display_tracks.at(track_i);

        display_track->body_bg.draw(gui_window, projection);

        for (int segment_i = 0; segment_i < display_track->display_audio_clip_segment_count; segment_i += 1) {
            DisplayAudioClipSegment *segment = display_track->display_audio_clip_segments.at(segment_i);
            segment->body.draw(gui_window, projection);
            segment->title_bar.draw(gui_window, projection);
        }
    }

    glStencilMask(0xFF);
    glStencilOp(GL_KEEP, GL_INCR, GL_INCR);

    for (int track_i = 0; track_i < display_track_count; track_i += 1) {
        DisplayTrack *display_track = display_tracks.at(track_i);

        for (int segment_i = 0; segment_i < display_track->display_audio_clip_segment_count; segment_i += 1) {
            DisplayAudioClipSegment *segment = display_track->display_audio_clip_segments.at(segment_i);

            glClear(GL_STENCIL_BUFFER_BIT);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

            gui_window->fill_rect(white, track_area_stencil_mvp);

            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glStencilFunc(GL_LEQUAL, 1, 0xFF);

            segment->title_bar.draw(gui_window, projection);

            glStencilFunc(GL_LEQUAL, 2, 0xFF);

            segment->label->draw(projection * segment->label_model, track_name_color);
        }
    }

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear(GL_STENCIL_BUFFER_BIT);

    gui_window->fill_rect(white, widget_stencil_mvp);

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilMask(0x00);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    gui_window->fill_rect(play_head_color, projection * play_head_model);

    glDisable(GL_STENCIL_TEST);

    for (int track_i = 0; track_i < display_track_count; track_i += 1) {
        DisplayTrack *display_track = display_tracks.at(track_i);

        display_track->head_bg.draw(gui_window, projection);
        display_track->track_name_label->draw(
                projection * display_track->track_name_label_model, track_name_color);

        gui_window->fill_rect(light_border_color, projection * display_track->border_top_model);
        gui_window->fill_rect(dark_border_color, projection * display_track->border_bottom_model);
    }

    timeline_bg.draw(gui_window, projection);
    gui_window->fill_rect(timeline_bottom_border_color, projection * timeline_bottom_border_model);
    gui->draw_image_color(gui_window, play_head_icon, projection * play_head_icon_model, play_head_color);

    horiz_scroll_bar->draw(projection);
    vert_scroll_bar->draw(projection);
}

TrackEditorWidget::DisplayTrack * TrackEditorWidget::create_display_track(GuiTrack *gui_track) {
    DisplayTrack *result = create<DisplayTrack>();
    result->track_name_label = create<Label>(gui);
    result->gui_track = gui_track;
    gui_track->display_track = result;
    ok_or_panic(display_tracks.append(result));
    return result;
}

TrackEditorWidget::DisplayAudioClipSegment * TrackEditorWidget::create_display_audio_clip_segment(
        DisplayTrack *display_track, GuiAudioClipSegment *gui_audio_clip_segment)
{
    DisplayAudioClipSegment *result = create<DisplayAudioClipSegment>();
    result->label = create<Label>(gui);
    result->gui_segment = gui_audio_clip_segment;
    gui_audio_clip_segment->display_segment = result;
    ok_or_panic(display_track->display_audio_clip_segments.append(result));
    return result;
}

void TrackEditorWidget::destroy_display_track(DisplayTrack *display_track) {
    if (display_track) {
        if (display_track->gui_track)
            display_track->gui_track->display_track = nullptr;
        destroy(display_track->track_name_label, 1);
        for (int i = 0; i < display_track->display_audio_clip_segments.length(); i += 1) {
            DisplayAudioClipSegment *display_segment = display_track->display_audio_clip_segments.at(i);
            destroy_display_audio_clip_segment(display_segment);
        }
        destroy(display_track, 1);
    }
}

void TrackEditorWidget::destroy_display_audio_clip_segment(
        DisplayAudioClipSegment *display_audio_clip_segment)
{
    if (display_audio_clip_segment) {
        destroy(display_audio_clip_segment->label, 1);
        destroy(display_audio_clip_segment, 1);
    }
}

// these do not take scroll into account
int TrackEditorWidget::whole_note_to_pixel(double whole_note_pos) {
    return body_left + pixels_per_whole_note * whole_note_pos;
}

double TrackEditorWidget::pixel_to_whole_note(int pixel_x) {
    return (pixel_x - body_left) / pixels_per_whole_note;
}

void TrackEditorWidget::update_play_head_model() {
    static const int ICON_WIDTH = 12;
    static const int ICON_HEIGHT = 12;
    float icon_scale_width = ICON_WIDTH / (float)play_head_icon->width;
    float icon_scale_height = ICON_HEIGHT / (float)play_head_icon->height;
    int play_head_x = whole_note_to_pixel(project->play_head_pos) - horiz_scroll_bar->value;
    int icon_left = play_head_x - ICON_WIDTH / 2;
    int icon_top = timeline_bottom - ICON_HEIGHT;
    play_head_icon_model = transform2d(icon_left, icon_top, icon_scale_width, icon_scale_height);
    play_head_model = transform2d(play_head_x, timeline_bottom, 1, track_area_bottom - timeline_bottom);
}

void TrackEditorWidget::update_model() {
    timeline_top = horiz_scroll_bar->min_height();
    timeline_bottom = timeline_top + timeline_height;
    timeline_bg.update(this, 0, timeline_top, width, timeline_height);
    timeline_bottom_border_model = transform2d(0, timeline_bottom - 1, width, 1);

    int track_area_top = timeline_bottom;
    track_area_bottom = track_area_top + (height - track_area_top);

    int first_top = timeline_top + timeline_height;
    int next_top = first_top;
    head_left = 0;
    body_left = head_left + track_head_width;
    int track_width = width - vert_scroll_bar->min_width();
    int body_width = track_width - body_left;

    // compute track positions
    int max_right = 0;
    for (int track_i = 0; track_i < gui_tracks.length(); track_i += 1) {
        GuiTrack *gui_track = gui_tracks.at(track_i);
        gui_track->left = 0;
        gui_track->right = body_left + body_width;
        gui_track->top = next_top;
        next_top += track_height;
        gui_track->bottom = next_top;
        next_top += 1;

        for (int segment_i = 0; segment_i < gui_track->gui_audio_clip_segments.length(); segment_i += 1) {
            GuiAudioClipSegment *gui_audio_clip_segment = gui_track->gui_audio_clip_segments.at(segment_i);
            AudioClipSegment *segment = gui_audio_clip_segment->segment;

            int frame_rate = project_audio_clip_sample_rate(project, segment->audio_clip);
            int frame_count = project_audio_clip_frame_count(project, segment->audio_clip);
            double whole_note_len = genesis_frames_to_whole_notes(
                    project->genesis_context, frame_count, frame_rate);
            double whole_note_end = segment->pos + whole_note_len;

            gui_audio_clip_segment->left = whole_note_to_pixel(segment->pos);
            gui_audio_clip_segment->right = whole_note_to_pixel(whole_note_end);
            gui_audio_clip_segment->top = gui_track->top + SEGMENT_PADDING;
            gui_audio_clip_segment->bottom = gui_track->bottom - SEGMENT_PADDING;

            max_right = max(max_right, gui_audio_clip_segment->right);
        }
    }
    int full_width = max_right + EXTRA_SCROLL_WIDTH;
    int full_height = next_top - first_top;
    int available_height = track_area_bottom - track_area_top;

    track_area_stencil = transform2d(0, track_area_top, track_width, available_height);
    widget_stencil = transform2d(0, 0, width, height);

    vert_scroll_bar->left = left + width - vert_scroll_bar->min_width();
    vert_scroll_bar->top = top + timeline_bottom;
    vert_scroll_bar->width = vert_scroll_bar->min_width();
    vert_scroll_bar->height = height - timeline_bottom;
    vert_scroll_bar->min_value = 0;
    vert_scroll_bar->max_value = max(0, full_height - available_height);
    vert_scroll_bar->set_handle_ratio(available_height, full_height);
    vert_scroll_bar->set_value(vert_scroll_bar->value);
    vert_scroll_bar->on_resize();

    horiz_scroll_bar->left = left;
    horiz_scroll_bar->top = top;
    horiz_scroll_bar->width = width;
    horiz_scroll_bar->height = horiz_scroll_bar->min_height();
    horiz_scroll_bar->min_value = 0;
    horiz_scroll_bar->max_value = max(0, full_width - body_width);
    horiz_scroll_bar->set_handle_ratio(body_width, full_width);
    horiz_scroll_bar->set_value(horiz_scroll_bar->value);
    horiz_scroll_bar->on_resize();


    // now consider scroll position and create display tracks for tracks that
    // are visible
    display_track_count = 0;
    for (int track_i = 0; track_i < gui_tracks.length(); track_i += 1) {
        GuiTrack *gui_track = gui_tracks.at(track_i);
        bool visible = (gui_track->bottom - vert_scroll_bar->value >= track_area_top &&
                gui_track->top - vert_scroll_bar->value < track_area_bottom);
        if (!visible)
            continue;

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
        display_track->head_bg.set_scheme(SunkenBoxSchemeRaised);
        display_track->head_bg.update(this, 0, head_top, track_head_width, track_height);
        display_track->body_bg.update(this, body_left, head_top, body_width, track_height);

        display_track->track_name_label->set_text(gui_track->track->name);
        display_track->track_name_label->update();

        int label_left = head_left + track_name_label_padding_left;
        int label_top = head_top + track_name_label_padding_top;
        display_track->track_name_label_model = transform2d(label_left, label_top);

        display_track->border_top_model = transform2d(
                gui_track->left, display_track->top, track_width, 1.0f);
        display_track->border_bottom_model = transform2d(
                gui_track->left, display_track->bottom, track_width, 1.0f);

        display_track->display_audio_clip_segment_count = 0;
        for (int segment_i = 0; segment_i < gui_track->gui_audio_clip_segments.length(); segment_i += 1) {
            GuiAudioClipSegment *gui_audio_clip_segment = gui_track->gui_audio_clip_segments.at(segment_i);
            bool visible = (gui_audio_clip_segment->right - horiz_scroll_bar->value >= body_left &&
                    gui_audio_clip_segment->left - horiz_scroll_bar->value < gui_track->right);

            if (!visible)
                continue;

            DisplayAudioClipSegment *display_audio_clip_segment;
            if (display_track->display_audio_clip_segment_count >=
                    display_track->display_audio_clip_segments.length())
            {
                display_audio_clip_segment = create_display_audio_clip_segment(
                        display_track, gui_audio_clip_segment);
            } else {
                display_audio_clip_segment =
                    display_track->display_audio_clip_segments.at(display_track->display_audio_clip_segment_count);
            }
            display_track->display_audio_clip_segment_count += 1;

            display_audio_clip_segment->gui_segment = gui_audio_clip_segment;
            gui_audio_clip_segment->display_segment = display_audio_clip_segment;

            // calculate positions
            display_audio_clip_segment->top = gui_audio_clip_segment->top - vert_scroll_bar->value;
            display_audio_clip_segment->bottom = gui_audio_clip_segment->bottom - vert_scroll_bar->value;
            display_audio_clip_segment->left = gui_audio_clip_segment->left - horiz_scroll_bar->value;
            display_audio_clip_segment->right = gui_audio_clip_segment->right - horiz_scroll_bar->value;

            int segment_width = display_audio_clip_segment->right - display_audio_clip_segment->left;
            int segment_height = display_audio_clip_segment->bottom - display_audio_clip_segment->top;

            display_audio_clip_segment->body.set_scheme(SunkenBoxSchemeRaisedBorders);
            display_audio_clip_segment->body.update(this,
                    display_audio_clip_segment->left, display_audio_clip_segment->top,
                    segment_width, segment_height);

            AudioClip *audio_clip = display_audio_clip_segment->gui_segment->segment->audio_clip;
            display_audio_clip_segment->label->set_text(audio_clip->name);
            display_audio_clip_segment->label->update();

            int label_left;
            if (display_audio_clip_segment->label->width() >= segment_width) {
                label_left = display_audio_clip_segment->left;
            } else {
                label_left = display_audio_clip_segment->left + segment_width / 2 -
                    display_audio_clip_segment->label->width() / 2;
            }
            int label_top = display_audio_clip_segment->top + SEGMENT_TITLE_PADDING;
            display_audio_clip_segment->label_model = transform2d(label_left, label_top);

            int title_bar_height = display_audio_clip_segment->label->height() + SEGMENT_TITLE_PADDING * 2;
            display_audio_clip_segment->title_bar.set_scheme(SunkenBoxSchemeRaisedBorders);
            display_audio_clip_segment->title_bar.update(this,
                    display_audio_clip_segment->left, display_audio_clip_segment->top,
                    segment_width, title_bar_height);
        }
    }

    update_play_head_model();
}

TrackEditorWidget::GuiTrack * TrackEditorWidget::create_gui_track() {
    return ok_mem(create_zero<GuiTrack>());
}

TrackEditorWidget::GuiAudioClipSegment * TrackEditorWidget::create_gui_audio_clip_segment() {
    return ok_mem(create_zero<GuiAudioClipSegment>());
}

void TrackEditorWidget::destroy_gui_audio_clip_segment(GuiAudioClipSegment *gui_audio_clip_segment) {
    if (gui_audio_clip_segment->display_segment)
        gui_audio_clip_segment->display_segment->gui_segment = nullptr;
    destroy(gui_audio_clip_segment, 1);
}

void TrackEditorWidget::destroy_gui_track(GuiTrack *gui_track) {
    if (gui_track) {
        if (menu_track == gui_track)
            clear_track_context_menu();
        for (int i = 0; i < gui_track->gui_audio_clip_segments.length(); i += 1) {
            destroy_gui_audio_clip_segment(gui_track->gui_audio_clip_segments.at(i));
        }
        destroy(gui_track, 1);
    }
}

void TrackEditorWidget::scrub(const MouseEvent *event) {
    double whole_note = pixel_to_whole_note(event->x + horiz_scroll_bar->value);
    project_set_play_head(project, whole_note);
}

void TrackEditorWidget::on_mouse_move(const MouseEvent *event) {
    if (scrub_mouse_down) {
        if (event->button == MouseButtonLeft && event->action == MouseActionUp) {
            scrub_mouse_down = false;
        } else if (event->action == MouseActionMove) {
            scrub(event);
        }
        return;
    }
    if (forward_mouse_event(vert_scroll_bar, event))
        return;
    if (forward_mouse_event(horiz_scroll_bar, event))
        return;
    if (event->button == MouseButtonRight && event->action == MouseActionDown) {
        GuiTrack *gui_track = get_track_head_at(event->x, event->y);
        if (gui_track)
            right_click_track_head(gui_track, event->x, event->y);
        return;
    }
    if (event->button == MouseButtonLeft && event->action == MouseActionDown) {
        if (event->y >= timeline_top && event->y <= timeline_bottom) {
            scrub_mouse_down = true;
            scrub(event);
        }
        return;
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
        case DragTypeAudioClip:
            on_drag_audio_clip((AudioClip *)event->drag_data->ptr, event);
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

void TrackEditorWidget::on_drag_audio_clip(AudioClip *audio_clip, const DragEvent *event) {
    if (event->action == DragActionDrop) {
        GuiTrack *gui_track = get_track_body_at(event->mouse_event.x, event->mouse_event.y);
        if (!gui_track)
            return;

        int x_with_scroll = event->mouse_event.x + horiz_scroll_bar->value;
        long end = project_audio_clip_frame_count(project, audio_clip);
        double pos = pixel_to_whole_note(x_with_scroll);
        project_add_audio_clip_segment(project, audio_clip, gui_track->track, 0, end, pos);
    }
}

void TrackEditorWidget::refresh_tracks() {
    int track_i;
    for (track_i = 0; track_i < project->track_list.length(); track_i += 1) {
        Track *track = project->track_list.at(track_i);
        GuiTrack *gui_track;
        if (track_i < gui_tracks.length()) {
            gui_track = gui_tracks.at(track_i);
        } else {
            gui_track = create_gui_track();
            ok_or_panic(gui_tracks.append(gui_track));
        }

        gui_track->track = track;

        int segment_i;
        for (segment_i = 0; segment_i < track->audio_clip_segments.length(); segment_i += 1) {
            AudioClipSegment *segment = track->audio_clip_segments.at(segment_i);
            GuiAudioClipSegment *gui_segment;
            if (segment_i < gui_track->gui_audio_clip_segments.length()) {
                gui_segment = gui_track->gui_audio_clip_segments.at(segment_i);
            } else {
                gui_segment = create_gui_audio_clip_segment();
                ok_or_panic(gui_track->gui_audio_clip_segments.append(gui_segment));
            }

            gui_segment->segment = segment;
        }
        while (segment_i < gui_track->gui_audio_clip_segments.length()) {
            GuiAudioClipSegment *gui_segment = gui_track->gui_audio_clip_segments.pop();
            destroy_gui_audio_clip_segment(gui_segment);
        }
    }
    while (track_i < gui_tracks.length()) {
        GuiTrack *gui_track = gui_tracks.pop();
        destroy_gui_track(gui_track);
    }
}

void TrackEditorWidget::on_mouse_wheel(const MouseWheelEvent *event) {
    {
        float range = vert_scroll_bar->max_value - vert_scroll_bar->min_value;
        vert_scroll_bar->set_value(vert_scroll_bar->value - event->wheel_y * range * 0.18f * vert_scroll_bar->handle_ratio);
    }
    {
        float range = horiz_scroll_bar->max_value - horiz_scroll_bar->min_value;
        horiz_scroll_bar->set_value(horiz_scroll_bar->value - event->wheel_x * range * 0.18f * horiz_scroll_bar->handle_ratio);
    }
    update_model();
}
