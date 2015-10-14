#include "project_props_widget.hpp"
#include "project.hpp"
#include "text_widget.hpp"
#include "select_widget.hpp"

static const int sample_rate_list[] = {
    8000,
    11025,
    16000,
    22050,
    32000,
    44100,
    48000,
    88200,
    96000,
    176400,
    192000,
    352800,
    384000,
};

static void on_sample_rate_changed(Event, void *userdata) {
    ProjectPropsWidget *project_props_widget = (ProjectPropsWidget *)userdata;
    project_props_widget->select_project_sample_rate();
}

static void on_selected_sample_rate_change(SelectWidget *select_widget) {
    int sample_rate = sample_rate_list[select_widget->selected_index];
    ProjectPropsWidget *project_props_widget = (ProjectPropsWidget *)select_widget->userdata;
    project_set_sample_rate(project_props_widget->project, sample_rate);
}

ProjectPropsWidget::~ProjectPropsWidget() {
    project->events.detach_handler(EventProjectSampleRateChanged, on_sample_rate_changed);
}

ProjectPropsWidget::ProjectPropsWidget(GuiWindow *gui_window, Project *project) :
    Widget(gui_window),
    project(project),
    layout(gui_window)
{
    sample_rate_select = create<SelectWidget>(gui_window);
    for (int i = 0; i < array_length(sample_rate_list); i += 1) {
        int sample_rate = sample_rate_list[i];
        sample_rate_select->append_choice(ByteBuffer::format("%d", sample_rate));
    }
    sample_rate_select->userdata = this;
    sample_rate_select->on_selected_index_change = on_selected_sample_rate_change;
    layout.add_widget(create_form_label("Sample Rate:"), 0, 0, HAlignRight, VAlignCenter);
    layout.add_widget(sample_rate_select, 0, 1, HAlignLeft, VAlignCenter);

    SelectWidget *loop_mode_select = create<SelectWidget>(gui_window);
    layout.add_widget(create_form_label("Loop Mode:"), 1, 0, HAlignRight, VAlignCenter);
    layout.add_widget(loop_mode_select, 1, 1, HAlignLeft, VAlignCenter);

    TextWidget *title_text = create<TextWidget>(gui_window);
    title_text->set_text(project->tag_title);
    layout.add_widget(create_form_label("Title:"), 2, 0, HAlignRight, VAlignCenter);
    layout.add_widget(title_text, 2, 1, HAlignLeft, VAlignCenter);

    TextWidget *artist_text = create<TextWidget>(gui_window);
    artist_text->set_text(project->tag_artist);
    layout.add_widget(create_form_label("Artist:"), 3, 0, HAlignRight, VAlignCenter);
    layout.add_widget(artist_text, 3, 1, HAlignLeft, VAlignCenter);

    TextWidget *album_text = create<TextWidget>(gui_window);
    album_text->set_text(project->tag_album);
    layout.add_widget(create_form_label("Album:"), 4, 0, HAlignRight, VAlignCenter);
    layout.add_widget(album_text, 4, 1, HAlignLeft, VAlignCenter);

    TextWidget *album_artist_text = create<TextWidget>(gui_window);
    album_artist_text->set_text(project->tag_album_artist);
    layout.add_widget(create_form_label("Album Artist:"), 5, 0, HAlignRight, VAlignCenter);
    layout.add_widget(album_artist_text, 5, 1, HAlignLeft, VAlignCenter);

    TextWidget *year_text = create<TextWidget>(gui_window);
    year_text->set_text(ByteBuffer::format("%d", project->tag_year));
    layout.add_widget(create_form_label("Year:"), 6, 0, HAlignRight, VAlignCenter);
    layout.add_widget(year_text, 6, 1, HAlignLeft, VAlignCenter);

    select_project_sample_rate();

    project->events.attach_handler(EventProjectSampleRateChanged, on_sample_rate_changed, this);
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
    for (int i = 0; i < array_length(sample_rate_list); i += 1) {
        int sample_rate = sample_rate_list[i];
        if (project->sample_rate == sample_rate) {
            sample_rate_select->select_index(i);
            return;
        }
    }
    panic("project sample rate not in list: %d", project->sample_rate);
}
