#ifndef GENESIS_SEQUENCER_WIDGET_HPP
#define GENESIS_SEQUENCER_WIDGET_HPP

#include "widget.hpp"

class GuiWindow;
struct Project;

class SequencerWidget : public Widget {
public:
    SequencerWidget(GuiWindow *window, Project *project);
    ~SequencerWidget() override {}
    void draw(const glm::mat4 &projection) override;

    Project *project;
};

#endif

