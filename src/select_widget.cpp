#include "select_widget.hpp"
#include "gui.hpp"
#include "color.hpp"

static void default_on_selected_index_change(SelectWidget *) {
    // do nothing
}

SelectWidget::SelectWidget(GuiWindow *gui_window, Gui *gui) :
    _gui_window(gui_window),
    _gui(gui),
    _left(0),
    _top(0),
    _autosize(true),
    _padding_left(4),
    _padding_top(4),
    _padding_right(4),
    _padding_bottom(4),
    _bg_color(parse_color("#CECECE")),
    _text_color(parse_color("#263753")),
    _selected_index(0),
    _on_selected_index_change(default_on_selected_index_change)
{
}

SelectWidget::~SelectWidget() {
    destroy_all_labels();
}

void SelectWidget::destroy_all_labels() {
    for (int i = 0; i < _label_list.length(); i += 1) {
        destroy(_label_list.at(i), 1);
    }
    _label_list.clear();
}

void SelectWidget::draw(GuiWindow *window, const glm::mat4 &projection) {
    glm::mat4 bg_mvp = projection * _bg_model;
    _gui->fill_rect(window, _bg_color, bg_mvp);

    if (_label_list.length() > 0) {
        int index = clamp(0, _selected_index, (int)(_label_list.length() - 1));
        glm::mat4 label_mvp = projection * _text_model;
        _label_list.at(index)->draw(window, label_mvp, _text_color);
    }
}

int SelectWidget::width() const {
    int biggest_width = 0;
    for (int i = 0; i < _label_list.length(); i += 1) {
        biggest_width = max(biggest_width, _label_list.at(i)->width());
    }
    return biggest_width + _padding_left + _padding_right;
}

int SelectWidget::height() const {
    int biggest_height = 0;
    for (int i = 0; i < _label_list.length(); i += 1) {
        biggest_height = max(biggest_height, _label_list.at(i)->height());
    }
    return biggest_height + _padding_top + _padding_bottom;
}

void SelectWidget::clear() {
    destroy_all_labels();
}

void SelectWidget::append_choice(String choice) {
    Label *label = create<Label>(_gui);
    label->set_text(choice);
    label->update();
    _label_list.append(label);
    update_model();
}

void SelectWidget::update_model() {
    _bg_model = glm::scale(
                    glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(_left, _top, 0.0f)),
                    glm::vec3(width(), height(), 1.0f));

    _text_model = glm::scale(
                    glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(_left + _padding_left, _top + _padding_top, 0.0f)),
                    glm::vec3(1.0f, 1.0f, 1.0f));
}

void SelectWidget::select_index(int index) {
    _selected_index = index;
}

void SelectWidget::on_mouse_move(const MouseEvent *event) {
    switch (event->action) {
    case MouseActionDown:
        _selected_index = euclidean_mod(_selected_index + 1, (int)_label_list.length());
        _on_selected_index_change(this);
        break;
    case MouseActionUp:
        break;
    case MouseActionMove:
        break;
    }
}

void SelectWidget::on_mouse_over(const MouseEvent *event) {
    _gui_window->set_cursor_default();
}
