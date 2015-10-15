#include "project_props_widget.hpp"
#include "project.hpp"
#include "text_widget.hpp"
#include "select_widget.hpp"
#include "spacer_widget.hpp"
#include "audio_file.hpp"

static void on_sample_rate_changed(Event, void *userdata) {
    ProjectPropsWidget *project_props_widget = (ProjectPropsWidget *)userdata;
    project_props_widget->select_project_sample_rate();
}

static void on_channel_layout_changed(Event, void *userdata) {
    ProjectPropsWidget *project_props_widget = (ProjectPropsWidget *)userdata;
    project_props_widget->select_project_channel_layout();
}

static void on_selected_sample_rate_change(Event, void *userdata) {
    ProjectPropsWidget *project_props_widget = (ProjectPropsWidget *)userdata;
    int sample_rate = audio_file_sample_rate_index(project_props_widget->sample_rate_select->selected_index);
    project_set_sample_rate(project_props_widget->project, sample_rate);
}

static void on_selected_channel_layout_change(Event, void *userdata) {
    ProjectPropsWidget *project_props_widget = (ProjectPropsWidget *)userdata;
    const SoundIoChannelLayout *layout = soundio_channel_layout_get_builtin(
            project_props_widget->channel_layout_select->selected_index);
    project_set_channel_layout(project_props_widget->project, layout);
}

ProjectPropsWidget::~ProjectPropsWidget() {
    channel_layout_select->events.detach_handler(EventSelectedIndexChanged, on_selected_channel_layout_change);
    sample_rate_select->events.detach_handler(EventSelectedIndexChanged, on_selected_sample_rate_change);

    project->events.detach_handler(EventProjectChannelLayoutChanged, on_channel_layout_changed);
    project->events.detach_handler(EventProjectSampleRateChanged, on_sample_rate_changed);
}

ProjectPropsWidget::ProjectPropsWidget(GuiWindow *gui_window, Project *project) :
    Widget(gui_window),
    project(project),
    layout(gui_window)
{
    channel_layout_select = create<SelectWidget>(gui_window);
    int channel_layout_count = soundio_channel_layout_builtin_count();
    for (int i = 0; i < channel_layout_count; i += 1) {
        const SoundIoChannelLayout *layout = soundio_channel_layout_get_builtin(i);
        channel_layout_select->append_choice(layout->name);
    }
    channel_layout_select->events.attach_handler(EventSelectedIndexChanged,
            on_selected_channel_layout_change, this);
    layout.add_widget(create_form_label("Channel Layout:"), 0, 0, HAlignRight, VAlignCenter);
    layout.add_widget(channel_layout_select, 0, 1, HAlignLeft, VAlignCenter);

    sample_rate_select = create<SelectWidget>(gui_window);
    int sample_rate_count = audio_file_sample_rate_count();
    for (int i = 0; i < sample_rate_count; i += 1) {
        int sample_rate = audio_file_sample_rate_index(i);
        sample_rate_select->append_choice(ByteBuffer::format("%d", sample_rate));
    }
    sample_rate_select->events.attach_handler(EventSelectedIndexChanged, on_selected_sample_rate_change, this);
    layout.add_widget(create_form_label("Sample Rate:"), 1, 0, HAlignRight, VAlignCenter);
    layout.add_widget(sample_rate_select, 1, 1, HAlignLeft, VAlignCenter);

    SelectWidget *loop_mode_select = create<SelectWidget>(gui_window);
    loop_mode_select->append_choice("Off (Leave Ending)");
    loop_mode_select->append_choice("On (Wrap Remainder)");
    loop_mode_select->select_index(0);
    layout.add_widget(create_form_label("Loop Mode:"), 2, 0, HAlignRight, VAlignCenter);
    layout.add_widget(loop_mode_select, 2, 1, HAlignLeft, VAlignCenter);

    TextWidget *title_text = create<TextWidget>(gui_window);
    title_text->set_text(project->tag_title);
    layout.add_widget(create_form_label("Title:"), 3, 0, HAlignRight, VAlignCenter);
    layout.add_widget(title_text, 3, 1, HAlignLeft, VAlignCenter);

    TextWidget *artist_text = create<TextWidget>(gui_window);
    artist_text->set_text(project->tag_artist);
    layout.add_widget(create_form_label("Artist:"), 4, 0, HAlignRight, VAlignCenter);
    layout.add_widget(artist_text, 4, 1, HAlignLeft, VAlignCenter);

    TextWidget *album_text = create<TextWidget>(gui_window);
    album_text->set_text(project->tag_album);
    layout.add_widget(create_form_label("Album:"), 5, 0, HAlignRight, VAlignCenter);
    layout.add_widget(album_text, 5, 1, HAlignLeft, VAlignCenter);

    TextWidget *album_artist_text = create<TextWidget>(gui_window);
    album_artist_text->set_text(project->tag_album_artist);
    layout.add_widget(create_form_label("Album Artist:"), 6, 0, HAlignRight, VAlignCenter);
    layout.add_widget(album_artist_text, 6, 1, HAlignLeft, VAlignCenter);

    TextWidget *year_text = create<TextWidget>(gui_window);
    year_text->set_text(ByteBuffer::format("%d", project->tag_year));
    layout.add_widget(create_form_label("Year:"), 7, 0, HAlignRight, VAlignCenter);
    layout.add_widget(year_text, 7, 1, HAlignLeft, VAlignCenter);

    SpacerWidget *spacer_widget = create<SpacerWidget>(gui_window, false, true);
    layout.add_widget(spacer_widget, 8, 0, HAlignCenter, VAlignCenter);

    select_project_sample_rate();
    select_project_channel_layout();

    project->events.attach_handler(EventProjectSampleRateChanged, on_sample_rate_changed, this);
    project->events.attach_handler(EventProjectChannelLayoutChanged, on_channel_layout_changed, this);
}

TextWidget *ProjectPropsWidget::create_form_label(const char *text) {
    TextWidget *text_widget = create<TextWidget>(gui_window);

    text_widget->set_background_on(false);
    text_widget->set_text(text);
    text_widget->set_auto_size(true);
    return text_widget;
}


void ProjectPropsWidget::draw(const glm::mat4 &projection) {
    layout.draw(projection);
}

void ProjectPropsWidget::on_resize() {
    layout.left = left;
    layout.top = top;
    layout.width = width;
    layout.height = height;
    layout.on_resize();
}

void ProjectPropsWidget::on_mouse_move(const MouseEvent *ev) {
    forward_mouse_event(&layout, ev);
}

void ProjectPropsWidget::select_project_sample_rate() {
    int sample_rate_count = audio_file_sample_rate_count();
    for (int i = 0; i < sample_rate_count; i += 1) {
        int sample_rate = audio_file_sample_rate_index(i);
        if (project->sample_rate == sample_rate) {
            sample_rate_select->select_index(i);
            return;
        }
    }
    panic("project sample rate not in list: %d", project->sample_rate);
}

void ProjectPropsWidget::select_project_channel_layout() {
    int channel_layout_count = soundio_channel_layout_builtin_count();
    for (int i = 0; i < channel_layout_count; i += 1) {
        const SoundIoChannelLayout *layout = soundio_channel_layout_get_builtin(i);
        if (soundio_channel_layout_equal(layout, &project->channel_layout)) {
            channel_layout_select->select_index(i);
            return;
        }
    }
    panic("project channel layout not in list");
}
