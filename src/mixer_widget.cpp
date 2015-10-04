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
    project->events.attach_handler(EventProjectEffectsChanged, on_mixer_lines_changed, this);
}

MixerWidget::~MixerWidget() {
    project->events.detach_handler(EventProjectEffectsChanged, on_mixer_lines_changed);
    project->events.detach_handler(EventProjectMixerLinesChanged, on_mixer_lines_changed);
}

void MixerWidget::draw(const glm::mat4 &projection) {
    for (int i = 0; i < gui_lines.length(); i += 1) {
        GuiMixerLine *line = gui_lines.at(i);
        line->bg.draw(gui_window, projection);

        line->name_label->draw(projection * line->name_label_model, line_name_color);
    }

    fx_area_bg.draw(gui_window, projection);
    for (int i = 0; i < gui_effects.length(); i += 1) {
        GuiEffect *effect = gui_effects.at(i);

        effect->bg.draw(gui_window, projection);
        effect->name_label->draw(projection * effect->name_label_model, line_name_color);
    }
}

void MixerWidget::update_model() {
    static const int line_width = 60;
    static const int line_spacing = 4;
    static const int padding_left = 4;
    static const int padding_right = 4;
    static const int padding_top = 4;
    static const int padding_bottom = 4;
    static const int name_padding = 2;
    static const int effect_spacing = 4;
    static const int effect_height = 32;

    int next_x = line_spacing;
    for (int i = 0; i < gui_lines.length(); i += 1) {
        GuiMixerLine *line = gui_lines.at(i);
        int right_x = next_x + line_width;
        line->bg.set_scheme(SunkenBoxSchemeSunken);
        line->bg.update(this, next_x, padding_top, right_x - next_x, height - padding_top - padding_bottom);

        line->name_label_model = transform2d(next_x + name_padding, padding_top + name_padding);

        next_x = right_x + line_spacing;
    }

    int fx_area_left = width / 2 + padding_left;
    int fx_area_top = padding_top;
    int fx_area_width = width / 2 - padding_left - padding_right;
    int fx_area_height = height - padding_top - padding_bottom;

    fx_area_bg.set_scheme(SunkenBoxSchemeSunken);
    fx_area_bg.update(this, fx_area_left, fx_area_top, fx_area_width, fx_area_height);

    int next_y = fx_area_top + padding_top;
    for (int i = 0; i < gui_effects.length(); i += 1) {
        GuiEffect *effect = gui_effects.at(i);

        int bottom_y = next_y + effect_height;
        int effect_left = fx_area_left + padding_left;
        int effect_width = fx_area_width - padding_left - padding_right;

        effect->bg.set_scheme(SunkenBoxSchemeRaisedBorders);
        effect->bg.update(this, effect_left, next_y, effect_width, effect_height);

        int label_left = effect_left + effect_width / 2 - effect->name_label->width() / 2;
        int label_top = next_y + effect_height / 2 - effect->name_label->height() / 2;
        effect->name_label_model = transform2d(label_left, label_top);

        next_y = bottom_y + effect_spacing;
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
            ok_or_panic(gui_lines.append(gui_line));
        }

        gui_line->mixer_line = mixer_line;
        gui_line->name_label->set_text(mixer_line->name);
        gui_line->name_label->update();
    }
    while (line_i < gui_lines.length()) {
        GuiMixerLine *gui_line = gui_lines.pop();
        destroy_gui_mixer_line(gui_line);
    }

    refresh_fx_list();
}

GuiEffect *MixerWidget::create_gui_effect() {
    GuiEffect *gui_effect = ok_mem(create_zero<GuiEffect>());
    gui_effect->name_label = create<Label>(gui);
    return gui_effect;
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

void MixerWidget::destroy_gui_effect(GuiEffect *gui_effect) {
    if (!gui_effect)
        return;
    destroy(gui_effect->name_label, 1);
    destroy(gui_effect, 1);
}

void MixerWidget::refresh_fx_list() {
    int selected_mixer_line_index = 0;

    GuiMixerLine *selected_gui_line = gui_lines.at(selected_mixer_line_index);
    MixerLine *mixer_line = selected_gui_line->mixer_line;

    int effect_i;
    for (effect_i = 0; effect_i < mixer_line->effects.length(); effect_i += 1) {
        Effect *effect = mixer_line->effects.at(effect_i);
        GuiEffect *gui_effect;
        if (effect_i < gui_effects.length()) {
            gui_effect = gui_effects.at(effect_i);
        } else {
            gui_effect = create_gui_effect();
            ok_or_panic(gui_effects.append(gui_effect));
        }

        gui_effect->effect = effect;
        project_get_effect_string(project, effect, gui_effect->name_label->text());
        gui_effect->name_label->update();
    }
    while (effect_i < gui_effects.length()) {
        GuiEffect *gui_effect = gui_effects.pop();
        destroy_gui_effect(gui_effect);
    }
}
