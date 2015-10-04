#ifndef MIXER_WIDGET_HPP
#define MIXER_WIDGET_HPP

#include "widget.hpp"
#include "project.hpp"
#include "sunken_box.hpp"

class GuiWindow;
class Label;
struct Project;

struct GuiMixerLine {
    MixerLine *mixer_line;
    SunkenBox bg;
    Label *name_label;
    glm::mat4 name_label_model;
};

class MixerWidget : public Widget {
public:
    MixerWidget(GuiWindow *window, Project *project);
    ~MixerWidget() override;
    void draw(const glm::mat4 &projection) override;
    void on_resize() override { update_model(); }

    Project *project;

    void update_model();
    void refresh_lines();

private:
    glm::vec4 line_name_color;

    List<GuiMixerLine *> gui_lines;

    GuiMixerLine * create_gui_mixer_line();
    void destroy_gui_mixer_line(GuiMixerLine *gui_mixer_line);
};

#endif
