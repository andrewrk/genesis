#include "sequencer_widget.hpp"

SequencerWidget::SequencerWidget(GuiWindow *window, Project *project) :
    Widget(window)
{
    this->project = project;
}

void SequencerWidget::draw(const glm::mat4 &projection) {
}
