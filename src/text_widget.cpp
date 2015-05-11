#include "text_widget.hpp"
#include "gui.hpp"
#include "gui_window.hpp"
#include "color.hpp"

static bool default_on_key_event(TextWidget *, const KeyEvent *event) {
    return false;
}

static void default_on_text_change_event(TextWidget *) {
    // do nothing
}

static bool default_on_mouse_event(TextWidget *, const MouseEvent *event) {
    return false;
}

TextWidget::TextWidget(GuiWindow *gui_window) :
    Widget(gui_window),
    _userdata(NULL),
    _label(gui),
    _padding_left(4),
    _padding_right(4),
    _padding_top(4),
    _padding_bottom(4),
    _text_color(0.0f, 0.0f, 0.0f, 1.0f),
    _sel_text_color(1.0f, 1.0f, 1.0f, 1.0f),
    _background_color(0.828f, 0.862f, 0.916f, 1.0f),
    _hover_color(0.7f, 0.7f, 0.8f, 1.0f),
    _selection_color(color_selection()),
    _cursor_color(0.1216f, 0.149f, 0.2078, 1.0f),
    _auto_size(false),
    fixed_min_width(0),
    fixed_max_width(200),
    _icon_size_w(16),
    _icon_size_h(16),
    _icon_margin(4),
    _cursor_start(0),
    _cursor_end(0),
    _select_down(false),
    _have_focus(false),
    _scroll_x(0),
    _placeholder_label(gui),
    _placeholder_color(0.4f, 0.4f, 0.4f, 1.0f),
    _mouse_down_dbl(false),
    _background_on(true),
    _hover_on(false),
    _text_interaction_on(true),
    _icon_img(NULL),
    _hovering(false),
    _on_key_event(default_on_key_event),
    _on_text_change_event(default_on_text_change_event),
    _on_mouse_event(default_on_mouse_event)
{
    update_model();
}

void TextWidget::draw(const glm::mat4 &projection) {
    bool should_hover = (_hover_on && _hovering);
    if (_background_on || should_hover) {
        glm::mat4 bg_mvp = projection * _bg_model;
        glm::vec4 color = should_hover ? _hover_color : _background_color;
        gui_window->fill_rect(color, bg_mvp);
    }

    if (_icon_img) {
        glm::mat4 icon_mvp = projection * _icon_model;
        gui->draw_image(gui_window, _icon_img, icon_mvp);
    }

    glm::mat4 label_mvp = projection * _label_model;
    if (_text_interaction_on) {
        if (_placeholder_label.text().length() > 0 && _label.text().length() == 0)
            _placeholder_label.draw(label_mvp, _placeholder_color);
    }

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear(GL_STENCIL_BUFFER_BIT);

    gui_window->fill_rect(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            left + label_start_x(), top + label_start_y(),
            label_area_width(), _label.height());

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    _label.draw(label_mvp, _text_color);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    glClear(GL_STENCIL_BUFFER_BIT);

    if (_text_interaction_on && _have_focus && _cursor_start != -1 && _cursor_end != -1) {
        if (_cursor_start == _cursor_end) {
            // draw cursor
            glm::mat4 cursor_mvp = projection * _cursor_model;
            gui_window->fill_rect(_selection_color, cursor_mvp);
        } else {

            // draw selection rectangle
            glm::mat4 sel_mvp = projection * _sel_model;
            gui_window->fill_rect(_selection_color, sel_mvp);

            glStencilFunc(GL_EQUAL, 1, 0xFF);
            glStencilMask(0x00);

            _label.draw(label_mvp, _sel_text_color);
        }
    }

    glDisable(GL_STENCIL_TEST);

}

void TextWidget::update_model() {
    if (_icon_img) {
        float scale_x = ((float)_icon_size_w) / ((float)_icon_img->width);
        float scale_y = ((float)_icon_size_h) / ((float)_icon_img->height);
        _icon_model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(left + _padding_left, top + _padding_top, 0.0f)),
                        glm::vec3(scale_x, scale_y, 0.0f));
    }

    float label_left = left + label_start_x() - _scroll_x;
    float label_top = top + label_start_y();
    _label_model = glm::translate(glm::mat4(1.0f),
            glm::vec3(label_left, label_top, 0.0f));

    _bg_model = glm::scale(
                    glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(left, top, 0.0f)),
                    glm::vec3(bg_width(), bg_height(), 0.0f));
}

void TextWidget::on_mouse_over(const MouseEvent *event) {
    if (_text_interaction_on)
        gui_window->set_cursor_beam();
    _hovering = true;
}

void TextWidget::on_mouse_out(const MouseEvent *event) {
    _hovering = false;
}

void TextWidget::on_mouse_move(const MouseEvent *event) {
    if (_on_mouse_event(this, event))
        return;

    if (!_text_interaction_on)
        return;

    switch (event->action) {
        case MouseActionDown:
            if (event->button == MouseButtonLeft) {
                if (event->is_double_click) {
                    int start = advance_word_from_index(_cursor_end + 1, -1);
                    int end = advance_word_from_index(_cursor_end - 1, 1);
                    set_selection(start, end);
                    _dbl_select_start = start;
                    _dbl_select_end = end;
                    _mouse_down_dbl = true;
                    _select_down = true;
                } else {
                    int index = cursor_at_pos(event->x + _scroll_x, event->y);
                    _cursor_start = index;
                    _cursor_end = index;
                    _select_down = true;
                    scroll_cursor_into_view();
                }
            }
            break;
        case MouseActionUp:
            if (event->button == MouseButtonLeft && _select_down) {
                _select_down = false;
                _mouse_down_dbl = false;
            }
            break;
        case MouseActionMove:
            if (event->buttons.left && _select_down) {
                int new_end = cursor_at_pos(event->x + _scroll_x, event->y);
                if (_mouse_down_dbl) {
                    if (new_end >= _dbl_select_start) {
                        int new_end2 = advance_word_from_index(new_end - 1,  1);
                        set_selection(_dbl_select_start, new_end2);
                    } else {
                        int new_start = advance_word_from_index(new_end + 1, -1);
                        set_selection(_dbl_select_end, new_start);
                    }
                } else {
                    _cursor_end = new_end;
                }
                scroll_cursor_into_view();
            }
            break;
    }
}

int TextWidget::label_start_x() const {
    int x = _padding_left;
    if (_icon_img)
        x += _icon_size_w + _icon_margin;
    return x;
}

int TextWidget::label_start_y() const {
    return (bg_height() - _label.height()) / 2;
}

int TextWidget::label_area_width() const {
    if (_auto_size) {
        return _label.width();
    } else {
        return width - label_start_x() - _padding_right;
    }
}

int TextWidget::cursor_at_pos(int x, int y) const {
    int inner_x = x - label_start_x();
    int inner_y = y - label_start_y();
    return _label.cursor_at_pos(inner_x, inner_y);
}

void TextWidget::pos_at_cursor(int index, int &x, int &y) const {
    _label.pos_at_cursor(index, x, y);
    x += label_start_x() - _scroll_x;
    y += label_start_y();
}

void TextWidget::get_cursor_slice(int &start, int &end) const {
    if (_cursor_start <= _cursor_end) {
        start = _cursor_start;
        end = _cursor_end;
    } else {
        start = _cursor_end;
        end = _cursor_start;
    }
}

void TextWidget::update_selection_model() {
    int sel_height = bg_height() - _padding_top - _padding_bottom;
    if (_cursor_start == _cursor_end) {
        int x, y;
        pos_at_cursor(_cursor_start, x, y);
        _cursor_model = glm::scale(
                            glm::translate(
                                glm::mat4(1.0f),
                                glm::vec3(left + x, top + _padding_top, 0.0f)),
                            glm::vec3(2.0f, sel_height, 1.0f));;
    } else {
        int start, end;
        get_cursor_slice(start, end);
        int start_x, end_x;
        _label.get_slice_dimensions(start, end, start_x, end_x);
        start_x -= _scroll_x;
        end_x -= _scroll_x;
        start_x = max(0, start_x);
        end_x = min(label_area_width(), end_x);
        int sel_width = end_x - start_x;
        _sel_model = glm::scale(
                        glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(left + label_start_x() + start_x, top + _padding_top, 0.0f)),
                        glm::vec3(sel_width, sel_height, 1.0f));
    }
}

void TextWidget::set_selection(int start, int end) {
    if (_text_interaction_on) {
        _cursor_start = clamp(0, start, (int)_label.text().length());
        _cursor_end = clamp(0, end, (int)_label.text().length());
        scroll_cursor_into_view();
    }
}

void TextWidget::on_gain_focus() {
    _have_focus = true;
}

void TextWidget::on_lose_focus() {
    _have_focus = false;
}

void TextWidget::on_text_input(const TextInputEvent *event) {
    if (!_text_interaction_on)
        return;

    int start, end;
    get_cursor_slice(start, end);
    String one_char;
    one_char.append(event->codepoint);
    replace_text(start, end, one_char, 1);
}

void TextWidget::replace_text(int start, int end, const String &text, int cursor_modifier) {
    start = clamp(0, start, (int)_label.text().length());
    end = clamp(0, end, (int)_label.text().length());

    _label.replace_text(start, end, text);

    _cursor_start = clamp(0, start + cursor_modifier, (int)_label.text().length());
    _cursor_end = clamp(0, start + cursor_modifier, (int)_label.text().length());

    _label.update();
    scroll_cursor_into_view();
    on_size_hints_changed();

    _on_text_change_event(this);
}

bool TextWidget::on_key_event(const KeyEvent *event) {
    if (!_text_interaction_on)
        return false;

    if (_on_key_event(this, event)) {
        // user ate this event
        return true;
    }

    if (event->action == KeyActionUp)
        return false;

    int start, end;
    get_cursor_slice(start, end);
    switch (event->virt_key) {
        case VirtKeyLeft:
            if (key_mod_ctrl(event->modifiers) && key_mod_shift(event->modifiers)) {
                int new_end = backward_word();
                set_selection(_cursor_start, new_end);
            } else if (key_mod_ctrl(event->modifiers)) {
                int new_start = backward_word();
                set_selection(new_start, new_start);
            } else if (key_mod_shift(event->modifiers)) {
                set_selection(_cursor_start, _cursor_end - 1);
            } else {
                if (start == end) {
                    set_selection(start - 1, start - 1);
                } else {
                    set_selection(start, start);
                }
            }
            return true;
        case VirtKeyRight:
            if (key_mod_ctrl(event->modifiers) && key_mod_shift(event->modifiers)) {
                int new_end = forward_word();
                set_selection(_cursor_start, new_end);
            } else if (key_mod_ctrl(event->modifiers)) {
                int new_start = forward_word();
                set_selection(new_start, new_start);
            } else if (key_mod_shift(event->modifiers)) {
                set_selection(_cursor_start, _cursor_end + 1);
            } else {
                if (start == end) {
                    set_selection(start + 1, end + 1);
                } else {
                    set_selection(end, end);
                }
            }
            return true;
        case VirtKeyBackspace:
            if (start == end) {
                if (key_mod_ctrl(event->modifiers)) {
                    int new_start = backward_word();
                    replace_text(new_start, start, "", 0);
                } else {
                    replace_text(start - 1, end, "", 0);
                }
            } else {
                replace_text(start, end, "", 0);
            }
            return true;
        case VirtKeyDelete:
            if (start == end) {
                if (key_mod_ctrl(event->modifiers)) {
                    int new_start = forward_word();
                    replace_text(start, new_start, "", 0);
                } else {
                    replace_text(start, end + 1, "", 0);
                }
            } else {
                replace_text(start, end, "", 0);
            }
            return true;
        case VirtKeyHome:
            if (key_mod_shift(event->modifiers)) {
                set_selection(_cursor_start, 0);
            } else {
                set_selection(0, 0);
            }
            return true;
        case VirtKeyEnd:
            if (key_mod_shift(event->modifiers)) {
                set_selection(_cursor_start, _label.text().length());
            } else {
                set_selection(_label.text().length(), _label.text().length());
            }
            return true;
        case VirtKeyA:
            if (key_mod_only_ctrl(event->modifiers)) {
                select_all();
                return true;
            }
        case VirtKeyX:
            if (key_mod_only_ctrl(event->modifiers)) {
                cut();
                return true;
            }
        case VirtKeyC:
            if (key_mod_only_ctrl(event->modifiers)) {
                copy();
                return true;
            }
        case VirtKeyV:
            if (key_mod_only_ctrl(event->modifiers)) {
                paste();
                return true;
            }
        default:
            // do nothing
            return false;
    }
}

int TextWidget::backward_word() {
    return advance_word(-1);
}

int TextWidget::forward_word() {
    return advance_word(1);
}

int TextWidget::advance_word(int dir) {
    return advance_word_from_index(_cursor_end, dir);
}

int TextWidget::advance_word_from_index(int start_index, int dir) {
    int init_advance = (dir > 0) ? 0 : 1;
    int new_cursor = clamp(0, start_index - init_advance, (int)_label.text().length() - 1);
    bool found_non_whitespace = false;
    while (new_cursor >= 0 && new_cursor < _label.text().length()) {
        uint32_t c = _label.text().at(new_cursor);
        if (String::is_whitespace(c)) {
            if (found_non_whitespace)
                return new_cursor + init_advance;
        } else {
            found_non_whitespace = true;
        }
        new_cursor += dir;
    }
    return new_cursor;
}

void TextWidget::select_all() {
    set_selection(0, _label.text().length());
}

void TextWidget::cut() {
    int start, end;
    get_cursor_slice(start, end);
    gui_window->set_clipboard_string(_label.text().substring(start, end));
    replace_text(start, end, "", 0);
}

void TextWidget::copy() {
    int start, end;
    get_cursor_slice(start, end);
    gui_window->set_clipboard_string(_label.text().substring(start, end));
}

void TextWidget::paste() {
    if (!gui_window->clipboard_has_string())
        return;
    int start, end;
    get_cursor_slice(start, end);
    String str = gui_window->get_clipboard_string();
    replace_text(start, end, str, str.length());
}

int TextWidget::min_width() const {
    return _auto_size ? (_label.width() + _padding_left + _padding_right) : fixed_min_width;
}

int TextWidget::max_width() const {
    return _auto_size ? (_label.width() + _padding_left + _padding_right) : fixed_max_width;
}

int TextWidget::bg_height() const {
    int height_from_label = _label.height() + _padding_top + _padding_bottom;

    if (_icon_img) {
        int height_from_icon = _padding_top + _padding_bottom + _icon_size_h;
        return max(height_from_label, height_from_icon);
    } else {
        return height_from_label;
    }
}

int TextWidget::min_height() const {
    return bg_height();
}

int TextWidget::max_height() const {
    return min_height();
}

int TextWidget::bg_width() const {
    return label_area_width() + label_start_x() + _padding_right;
}

void TextWidget::set_min_width(int new_width) {
    fixed_min_width = new_width;
    _auto_size = false;
    on_size_hints_changed();
}

void TextWidget::set_max_width(int new_width) {
    fixed_max_width = new_width;
    _auto_size = false;
    on_size_hints_changed();
}

void TextWidget::set_auto_size(bool value) {
    _auto_size = value;
    scroll_cursor_into_view();
}

void TextWidget::scroll_index_into_view(int char_index) {
    if (_auto_size) {
        _scroll_x = 0;
    } else {
        int x, y;
        pos_at_cursor(char_index, x, y);

        int cursor_too_far_right = x - (width - _padding_right);
        if (cursor_too_far_right > 0)
            _scroll_x += cursor_too_far_right;

        int cursor_too_far_left = -x + label_start_x();
        if (cursor_too_far_left > 0)
            _scroll_x -= cursor_too_far_left;

        int max_scroll_x = max(0, _label.width() - (width - label_start_x() - _padding_right));
        _scroll_x = clamp(0, _scroll_x, max_scroll_x);
    }

    update_model();
    update_selection_model();
}

void TextWidget::scroll_cursor_into_view() {
    return scroll_index_into_view(_cursor_end);
}

void TextWidget::set_placeholder_text(const String &text) {
    _placeholder_label.set_text(text);
    _placeholder_label.update();
    update_model();
}

void TextWidget::set_text(const String &text) {
    if (String::compare(text, _label.text()) != 0) {
        _label.set_text(text);
        _label.update();
        set_selection(_cursor_start, _cursor_end);
        if (_auto_size)
            on_size_hints_changed();
    }
}

void TextWidget::set_icon(const SpritesheetImage *icon) {
    _icon_img = icon;
    update_model();
    update_selection_model();
}
