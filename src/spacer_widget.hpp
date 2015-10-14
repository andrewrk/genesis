#ifndef GENESIS_SPACER_WIDGET_HPP
#define GENESIS_SPACER_WIDGET_HPP

class SpacerWidget : public Widget {
public:
    SpacerWidget(GuiWindow *gui_window, bool horiz, bool vert) : Widget(gui_window) {
        this->horiz = horiz;
        this->vert = vert;
    }
    ~SpacerWidget() override {}

    void draw(const glm::mat4 &projection) override {}

    int max_width() const override {
        return horiz ? -1 : 0;
    }

    int max_height() const override {
        return vert ? -1 : 0;
    }

    bool horiz;
    bool vert;
};

#endif
