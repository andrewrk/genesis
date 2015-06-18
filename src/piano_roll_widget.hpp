#ifndef PIANO_ROLL_WIDGET_HPP
#define PIANO_ROLL_WIDGET_HPP

#include "widget.hpp"

class GuiWindow;
struct Project;

class PianoRollWidget : public Widget {
public:
    PianoRollWidget(GuiWindow *window, Project *project);
    ~PianoRollWidget() override {}
    void draw(const glm::mat4 &projection) override;

    Project *project;
};

#endif

