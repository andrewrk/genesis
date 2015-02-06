#ifndef LABEL_WIDGET_HPP
#define LABEL_WIDGET_HPP

#include "string.hpp"
#include "label.hpp"
#include "glm.hpp"

class Gui;
class LabelWidget {
public:
    LabelWidget() : _label(gui), _gui_index(gui_index) {}
    ~LabelWidget() {}
    LabelWidget(LabelWidget &copy) = delete;

    void set_text(const String &text) {
        _label.set_text(text);
        _label.update();
    }

    void set_font_size(int size) {
        _label.set_font_size(size);
        _label.update();
    }

    void set_color(glm::vec4 color) {
        _label.set_color(color);
    }

    void set_pos(int x, int y) {
        _x = x;
        _y = y;
        update_model();
    }

    int x() const {
        return _x;
    }

    int y() const {
        return _y;
    }

    int width() const {
        return _label.width();
    }

    int height() const {
        return _label.height();
    }

    void draw(const glm::mat4 &projection) {
        glm::mat4 mvp = projection * _model;
        _label.draw(mvp);
    }

    bool is_visible() const {
        return _is_visible;
    }

    int _gui_index;
private:
    Label _label;
    int _x;
    int _y;
    glm::mat4 _model;
    bool _is_visible;
    void update_model() {
        _model = glm::translate(glm::mat4(1), glm::vec3((float)_x, (float)_y, 0.0f));
    }
};

#endif
