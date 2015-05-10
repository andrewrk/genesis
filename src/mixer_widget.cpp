#include "mixer_widget.hpp"

MixerWidget::MixerWidget(GuiWindow *window, Project *project) :
    Widget(window)
{
    this->project = project;
}

void MixerWidget::draw(const glm::mat4 &projection) {
    // TODO
}
