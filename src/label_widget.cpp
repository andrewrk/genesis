#include "label_widget.hpp"
#include "gui.hpp"

static const uint32_t whitespace[] = {9, 10, 11, 12, 13, 32, 133, 160, 5760,
    8192, 8193, 8194, 8195, 8196, 8197, 8198, 8199, 8200, 8201, 8202, 8232,
    8233, 8239, 8287, 12288};
static bool is_whitespace(uint32_t c) {
    for (size_t i = 0; i < array_length(whitespace); i+= 1) {
        if (c == whitespace[i])
            return true;
    }
    return false;
}

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
        _auto_size(false),
        _gui(gui),
        _cursor_start(-1),
        _cursor_end(-1),
        _select_down(false),
        _have_focus(false),
        _width(100),
        _scroll_x(0),
        _placeholder_label(gui),
        _placeholder_color(0.5f, 0.5f, 0.5f, 1.0f)
{
    update_model();
}

void LabelWidget::draw(const glm::mat4 &projection) {
    glm::mat4 bg_mvp = projection * _bg_model;
    _gui->fill_rect(_background_color, bg_mvp);

    glm::mat4 label_mvp = projection * _label_model;
    if (_placeholder_label.text().length() > 0 && _label.text().length() == 0)
        _placeholder_label.draw(label_mvp, _placeholder_color);

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
            _label.draw_sel_slice(sel_text_mvp, _sel_text_color);
        }
    }
}

void LabelWidget::update_model() {
    int slice_start_x, slice_end_x;
    if (_auto_size) {
        slice_start_x = -1;
        slice_end_x = -1;
    } else {
        slice_start_x = _scroll_x;
        slice_end_x = _scroll_x + _width - _padding_left - _padding_right;
    }
    _label.set_slice(slice_start_x, slice_end_x);

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
                int index = cursor_at_pos(event.x + _scroll_x, event.y);
                _cursor_start = index;
                _cursor_end = index;
                _select_down = true;
                scroll_cursor_into_view();
                break;
            }
        case MouseActionUp:
            if (event.button == MouseButtonLeft && _select_down) {
                _select_down = false;
            }
            break;
        case MouseActionMove:
            if (event.buttons.left && _select_down) {
                _cursor_end = cursor_at_pos(event.x + _scroll_x, event.y);
                scroll_cursor_into_view();
            }
            break;
        case MouseActionDbl:
            {
                int start = advance_word_from_index(_cursor_end + 1, -1);
                int end = advance_word_from_index(_cursor_end - 1, 1);
                set_selection(start, end);
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
    x += _padding_left - _scroll_x;
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
        _label.set_sel_slice(start, end);
        int start_x, end_x;
        _label.get_slice_dimensions(start, end, start_x, end_x);
        start_x -= _scroll_x;
        end_x -= _scroll_x;
        int max_end_x = width() - _padding_left - _padding_right;
        int min_start_x = 0;
        start_x = max(min_start_x, start_x);
        end_x = min(max_end_x, end_x);
        int sel_width = end_x - start_x;
        _sel_model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(_left + _padding_left + start_x, _top + _padding_top, 0.0f)),
                        glm::vec3(sel_width, _label.height(), 1.0f));
        float label_left = _left + _padding_left;
        float label_top = _top + _padding_top;
        _sel_text_model = glm::translate(glm::mat4(1.0f),
                glm::vec3(label_left + start_x, label_top, 0.0f));
    }
}

void LabelWidget::set_selection(int start, int end) {
    _cursor_start = clamp(0, start, _label.text().length());
    _cursor_end = clamp(0, end, _label.text().length());
    scroll_cursor_into_view();
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
    replace_text(start, end, event.text, 1);
}

void LabelWidget::replace_text(int start, int end, const String &text, int cursor_modifier) {
    start = clamp(0, start, _label.text().length());
    end = clamp(0, end, _label.text().length());

    _label.replace_text(start, end, text);

    _cursor_start = clamp(0, start + cursor_modifier, _label.text().length());
    _cursor_end = clamp(0, start + cursor_modifier, _label.text().length());

    _label.update();
    scroll_cursor_into_view();
}

void LabelWidget::on_key_event(const KeyEvent &event) {
    if (event.action == KeyActionUp)
        return;

    int start, end;
    get_cursor_slice(start, end);
    switch (event.virt_key) {
        case VirtKeyLeft:
            if (event.ctrl() && event.shift()) {
                int new_end = backward_word();
                set_selection(_cursor_start, new_end);
            } else if (event.ctrl()) {
                int new_start = backward_word();
                set_selection(new_start, new_start);
            } else if (event.shift()) {
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
            if (event.ctrl() && event.shift()) {
                int new_end = forward_word();
                set_selection(_cursor_start, new_end);
            } else if (event.ctrl()) {
                int new_start = forward_word();
                set_selection(new_start, new_start);
            } else if (event.shift()) {
                set_selection(_cursor_start, _cursor_end + 1);
            } else {
                if (start == end) {
                    set_selection(start + 1, end + 1);
                } else {
                    set_selection(end, end);
                }
            }
            break;
        case VirtKeyBackspace:
            if (start == end) {
                if (event.ctrl()) {
                    int new_start = backward_word();
                    replace_text(new_start, start, "", 0);
                } else {
                    replace_text(start - 1, end, "", 0);
                }
            } else {
                replace_text(start, end, "", 0);
            }
            break;
        case VirtKeyDelete:
            if (start == end) {
                if (event.ctrl()) {
                    int new_start = forward_word();
                    replace_text(start, new_start, "", 0);
                } else {
                    replace_text(start, end + 1, "", 0);
                }
            } else {
                replace_text(start, end, "", 0);
            }
            break;
        case VirtKeyHome:
            if (event.shift()) {
                set_selection(_cursor_start, 0);
            } else {
                set_selection(0, 0);
            }
            break;
        case VirtKeyEnd:
            if (event.shift()) {
                set_selection(_cursor_start, _label.text().length());
            } else {
                set_selection(_label.text().length(), _label.text().length());
            }
            break;
        case VirtKeyA:
            if (event.ctrl())
                select_all();
            break;
        case VirtKeyX:
            if (event.ctrl())
                cut();
            break;
        case VirtKeyC:
            if (event.ctrl())
                copy();
            break;
        case VirtKeyV:
            if (event.ctrl())
                paste();
            break;
        default:
            // do nothing
            break;
    }
}

int LabelWidget::backward_word() {
    return advance_word(-1);
}

int LabelWidget::forward_word() {
    return advance_word(1);
}

int LabelWidget::advance_word(int dir) {
    return advance_word_from_index(_cursor_end, dir);
}

int LabelWidget::advance_word_from_index(int start_index, int dir) {
    int init_advance = (dir > 0) ? 0 : 1;
    int new_cursor = clamp(0, start_index - init_advance, _label.text().length());
    bool found_non_whitespace = false;
    while ((new_cursor + dir) >= 0 && (new_cursor + dir) <= _label.text().length()) {
        uint32_t c = _label.text().at(new_cursor);
        if (is_whitespace(c)) {
            if (found_non_whitespace)
                return new_cursor + init_advance;
        } else {
            found_non_whitespace = true;
        }
        new_cursor += dir;
    }
    return new_cursor;
}

void LabelWidget::select_all() {
    set_selection(0, _label.text().length());
}

void LabelWidget::cut() {
    int start, end;
    get_cursor_slice(start, end);
    _gui->set_clipboard_string(_label.text().substring(start, end));
    replace_text(start, end, "", 0);
}

void LabelWidget::copy() {
    int start, end;
    get_cursor_slice(start, end);
    _gui->set_clipboard_string(_label.text().substring(start, end));
}

void LabelWidget::paste() {
    if (!_gui->clipboard_has_string())
        return;
    int start, end;
    get_cursor_slice(start, end);
    String str = _gui->get_clipboard_string();
    replace_text(start, end, str, str.length());
}

int LabelWidget::width() const {
    if (_auto_size) {
        return _label.width() + _padding_left + _padding_right;
    } else {
        return _width;
    }
}

int LabelWidget::height() const {
    return _label.height() + _padding_top + _padding_bottom;
}

void LabelWidget::set_width(int new_width) {
    _width = new_width;
    _auto_size = false;
    scroll_cursor_into_view();
}

void LabelWidget::set_auto_size(bool value) {
    _auto_size = value;
    scroll_cursor_into_view();
}

void LabelWidget::scroll_cursor_into_view() {
    if (_auto_size) {
        _scroll_x = 0;
    } else {
        int x, y;
        pos_at_cursor(_cursor_end, x, y);

        int cursor_too_far_right = x - (_width - _padding_right);
        if (cursor_too_far_right > 0)
            _scroll_x += cursor_too_far_right;

        int cursor_too_far_left = -x + _padding_left;
        if (cursor_too_far_left > 0)
            _scroll_x -= cursor_too_far_left;

        int max_scroll_x = max(0, _label.width() - (_width - _padding_left - _padding_right));
        _scroll_x = clamp(0, _scroll_x, max_scroll_x);
    }

    update_model();
    update_selection_model();
}

void LabelWidget::set_placeholder_text(const String &text) {
    _placeholder_label.set_text(text);
    update_model();
}
