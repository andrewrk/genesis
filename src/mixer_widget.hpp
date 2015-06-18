#ifndef MIXER_WIDGET_HPP
#define MIXER_WIDGET_HPP

#include "widget.hpp"
#include "project.hpp"

class GuiWindow;
struct Project;

struct GuiMixerLine {
    MixerLine *mixer_line;
};

class MixerWidget : public Widget {
public:
    MixerWidget(GuiWindow *window, Project *project);
    ~MixerWidget() override {}
    void draw(const glm::mat4 &projection) override;

    Project *project;
};

#endif
