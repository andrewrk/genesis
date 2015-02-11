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
        _text_color(0.0f, 0.0f, 0.0f, 1.0f),
        _sel_text_color(1.0f, 1.0f, 1.0f, 1.0f),
        _background_color(0.788f, 0.812f, 0.886f, 1.0f),
        _selection_color(0.1216f, 0.149f, 0.2078, 1.0f),
        _cursor_color(0.1216f, 0.149f, 0.2078, 1.0f),
        _has_background(true),
        _gui(gui),
        _cursor_start(-1),
        _cursor_end(-1),
        _select_down(false),
        _have_focus(false)
{
    update_model();
}

void LabelWidget::draw(const glm::mat4 &projection) {
    glm::mat4 bg_mvp = projection * _bg_model;
    _gui->fill_rect(_background_color, bg_mvp);

    glm::mat4 label_mvp = projection * _label_model;
    _label.draw(label_mvp, _text_color);

    if (_have_focus && _cursor_start != -1 && _cursor_end != -1) {
        if (_cursor_start == _cursor_end) {
            // draw cursor
            glm::mat4 cursor_mvp = projection * _cursor_model;
            _gui->fill_rect(_selection_color, cursor_mvp);
        } else {
            // draw selection rectangle
            glm::mat4 sel_mvp = projection * _sel_model;
            _gui->fill_rect(_selection_color, sel_mvp);

            glm::mat4 sel_text_mvp = projection * _sel_text_model;
            _label.draw_slice(sel_text_mvp, _sel_text_color);
        }
    }
}

void LabelWidget::update_model() {
    float label_left = _left + _padding_left;
    float label_top = _top + _padding_top;
    _label_model = glm::translate(glm::mat4(1.0f), glm::vec3(label_left, label_top, 0.0f));


    _bg_model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(_left, _top, 0.0f)),
                        glm::vec3(width(), height(), 0.0f));
}

void LabelWidget::on_mouse_over(const MouseEvent &event) {
    SDL_SetCursor(_gui->_cursor_ibeam);
}

void LabelWidget::on_mouse_out(const MouseEvent &event) {
    SDL_SetCursor(_gui->_cursor_default);
}

void LabelWidget::on_mouse_move(const MouseEvent &event) {
    switch (event.action) {
        case MouseActionDown:
            if (event.button == MouseButtonLeft) {
                int index = cursor_at_pos(event.x, event.y);
                _cursor_start = index;
                _cursor_end = index;
                _select_down = true;
                update_selection_model();
                break;
            }
        case MouseActionUp:
            if (event.button == MouseButtonLeft && _select_down) {
                _select_down = false;
            }
            break;
        case MouseActionMove:
            if (event.buttons.left && _select_down) {
                _cursor_end = cursor_at_pos(event.x, event.y);
                update_selection_model();
            }
            break;
    }
}

int LabelWidget::cursor_at_pos(int x, int y) const {
    int inner_x = x - _padding_left;
    int inner_y = y - _padding_top;
    return _label.cursor_at_pos(inner_x, inner_y);
}

void LabelWidget::pos_at_cursor(int index, int &x, int &y) const {
    _label.pos_at_cursor(index, x, y);
    x += _padding_left;
    y += _padding_top;
}

void LabelWidget::get_cursor_slice(int &start, int &end) const {
    if (_cursor_start <= _cursor_end) {
        start = _cursor_start;
        end = _cursor_end;
    } else {
        start = _cursor_end;
        end = _cursor_start;
    }
}

void LabelWidget::update_selection_model() {
    if (_cursor_start == _cursor_end) {
        int x, y;
        pos_at_cursor(_cursor_start, x, y);
        _cursor_model = glm::scale(
                            glm::translate(
                                glm::mat4(1.0f),
                                glm::vec3(_left + x, _top + _padding_top, 0.0f)),
                            glm::vec3(2.0f, _label.height(), 1.0f));;
    } else {
        int start, end;
        get_cursor_slice(start, end);
        _label.set_slice(start, end);
        int start_x, end_x;
        _label.get_slice_dimensions(start, end, start_x, end_x);
        _sel_model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(_left + _padding_left + start_x, _top + _padding_top, 0.0f)),
                        glm::vec3(end_x - start_x, _label.height(), 1.0f));
        float label_left = _left + _padding_left;
        float label_top = _top + _padding_top;
        _sel_text_model = glm::translate(glm::mat4(1.0f),
                glm::vec3(label_left + start_x, label_top, 0.0f));
    }
}

void LabelWidget::set_selection(int start, int end) {
    _cursor_start = clamp(0, start, _label.text().length() - 1);
    _cursor_end = clamp(0, end, _label.text().length());
    update_selection_model();
    _select_down = false;
}

void LabelWidget::on_gain_focus() {
    _have_focus = true;
    _gui->start_text_editing(left(), top(), width(), height());
}

void LabelWidget::on_lose_focus() {
    _have_focus = false;
    _gui->stop_text_editing();
}

void LabelWidget::on_text_input(const TextInputEvent &event) {
    if (event.action == TextInputActionCandidate) {
        // 1. Need a ttf font that supports more characters
        // 2. Support displaying this event
        panic("TODO text editing event support");
    }

    int start, end;
    get_cursor_slice(start, end);
    _cursor_start = start + 1;
    _cursor_end = _cursor_start;

    _label.replace_text(start, end, event.text);

    _label.update();
    update_model();
    update_selection_model();
}

void LabelWidget::on_key_event(const KeyEvent &event) {
    if (event.action == KeyActionUp)
        return;

    int start, end;
    get_cursor_slice(start, end);
    switch (event.virt_key) {
        case VirtKeyLeft:
            if (event.shift()) {
                set_selection(_cursor_start, _cursor_end - 1);
            } else {
                if (start == end) {
                    set_selection(start - 1, start - 1);
                } else {
                    set_selection(start, start);
                }
            }
            break;
        case VirtKeyRight:
            if (event.shift()) {
                set_selection(_cursor_start, _cursor_end + 1);
            } else {
                if (start == end) {
                    set_selection(start + 1, end + 1);
                } else {
                    set_selection(end, end);
                }
            }
            break;
        default:
            // do nothing
            break;
    }
}
