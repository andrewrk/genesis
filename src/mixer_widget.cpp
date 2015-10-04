#include "mixer_widget.hpp"
#include "label.hpp"
#include "color.hpp"

static void on_mixer_lines_changed(Event, void *userdata) {
    MixerWidget *mixer_widget = (MixerWidget *)userdata;
    mixer_widget->refresh_lines();
    mixer_widget->update_model();
}

MixerWidget::MixerWidget(GuiWindow *window, Project *project) :
    Widget(window),
    line_name_color(color_fg_text())
{
    this->project = project;

    refresh_lines();
    update_model();
    project->events.attach_handler(EventProjectMixerLinesChanged, on_mixer_lines_changed, this);
}

MixerWidget::~MixerWidget() {
    project->events.detach_handler(EventProjectMixerLinesChanged, on_mixer_lines_changed);
}

void MixerWidget::draw(const glm::mat4 &projection) {
    for (int i = 0; i < gui_lines.length(); i += 1) {
        GuiMixerLine *line = gui_lines.at(i);
        line->bg.draw(gui_window, projection);

        line->name_label->draw(projection * line->name_label_model, line_name_color);
    }
}

void MixerWidget::update_model() {
    static const int line_width = 60;
    static const int line_spacing = 4;
    static const int padding_top = 4;
    static const int padding_bottom = 4;
    static const int name_padding = 2;

    int next_x = line_spacing;
    for (int i = 0; i < gui_lines.length(); i += 1) {
        GuiMixerLine *line = gui_lines.at(i);
        int right_x = next_x + line_width;
        line->bg.set_scheme(SunkenBoxSchemeSunken);
        line->bg.update(this, next_x, padding_top, right_x, height - padding_top - padding_bottom);

        line->name_label_model = transform2d(next_x + name_padding, padding_top + name_padding);

        next_x = right_x + line_spacing;
    }
}

void MixerWidget::refresh_lines() {
    int line_i;
    for (line_i = 0; line_i < project->mixer_line_list.length(); line_i += 1) {
        MixerLine *mixer_line = project->mixer_line_list.at(line_i);
        GuiMixerLine *gui_line;
        if (line_i < gui_lines.length()) {
            gui_line = gui_lines.at(line_i);
        } else {
            gui_line = create_gui_mixer_line();
            gui_line->name_label->set_text(mixer_line->name);
            gui_line->name_label->update();
            ok_or_panic(gui_lines.append(gui_line));
        }

        gui_line->mixer_line = mixer_line;
    }
    while (line_i < gui_lines.length()) {
        GuiMixerLine *gui_line = gui_lines.pop();
        destroy_gui_mixer_line(gui_line);
    }
}

GuiMixerLine * MixerWidget::create_gui_mixer_line() {
    GuiMixerLine *gui_line = ok_mem(create_zero<GuiMixerLine>());
    gui_line->name_label = create<Label>(gui);
    return gui_line;
}


void MixerWidget::destroy_gui_mixer_line(GuiMixerLine *gui_mixer_line) {
    if (!gui_mixer_line)
        return;
    destroy(gui_mixer_line->name_label, 1);
    destroy(gui_mixer_line, 1);
}
