#ifndef TRACK_EDITOR_WIDGET_HPP
#define TRACK_EDITOR_WIDGET_HPP

#include "widget.hpp"

class TrackEditorWidget : public Widget {
public:
    TrackEditorWidget(GuiWindow *gui_window) :
        Widget(gui_window)
    {
    }
    ~TrackEditorWidget() override {}

    void draw(const glm::mat4 &projection) override {
        // TODO
    }

};

#endif
