#include "piano_roll_widget.hpp"

PianoRollWidget::PianoRollWidget(GuiWindow *window, Project *project) :
    Widget(window)
{
    this->project = project;
}

void PianoRollWidget::draw(const glm::mat4 &projection) {
    // TODO
}
