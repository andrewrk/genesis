#ifndef MIXER_WIDGET_HPP
#define MIXER_WIDGET_HPP

#include "widget.hpp"

class GuiWindow;
class Project;

class MixerWidget : public Widget {
public:
    MixerWidget(GuiWindow *window, Project *project) : Widget(window) {}
    ~MixerWidget() override {}
    void draw(const glm::mat4 &projection) override;
};

#endif
