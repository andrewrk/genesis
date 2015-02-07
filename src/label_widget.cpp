#include "label_widget.hpp"
#include "gui.hpp"

LabelWidget::LabelWidget(Gui *gui, int gui_index) :
        _gui_index(gui_index),
        _label(gui),
        _is_visible(true),
        _padding_left(4),
        _padding_right(4),
        _padding_top(4),
        _padding_bottom(4),
        _background_color(0.788f, 0.812f, 0.886f, 1.0f),
        _has_background(true),
        _gui(gui)
{
    update_model();
    _label.set_color(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

void LabelWidget::draw(const glm::mat4 &projection) {
    glm::mat4 bg_mvp = projection * _bg_model;
    _gui->fill_rect(_background_color, bg_mvp);

    glm::mat4 label_mvp = projection * _label_model;
    _label.draw(label_mvp);
}

void LabelWidget::update_model() {
    _label_model = glm::translate(glm::mat4(1.0f), glm::vec3((float)_left, (float)_top, 0.0f));

    float bg_width = _label.width() + _padding_left + _padding_right;
    float bg_height = _label.height() + _padding_top + _padding_bottom;
    _bg_model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(_left - _padding_left, _top - _padding_top, 0.0f)),
                        glm::vec3(bg_width, bg_height, 0.0f));
}

void LabelWidget::on_mouse_over(const MouseEvent &event) {
    SDL_SetCursor(_gui->_cursor_ibeam);
}

void LabelWidget::on_mouse_out(const MouseEvent &event) {
    SDL_SetCursor(_gui->_cursor_default);
}
