#include "find_file_widget.hpp"
#include "path.hpp"
#include "genesis.hpp"

FindFileWidget::FindFileWidget(Gui *gui) :
    _widget(Widget {
        destructor,
        draw,
        left,
        top,
        width,
        height,
        on_mouse_move,
        on_mouse_out,
        on_mouse_over,
        on_gain_focus,
        on_lose_focus,
        on_text_input,
        on_key_event,
    }),
    _left(0),
    _top(0),
    _width(300),
    _height(500),
    _padding_left(4),
    _padding_right(4),
    _padding_top(4),
    _padding_bottom(4),
    _margin(4),
    _current_path_widget(gui),
    _filter_widget(gui),
    _gui(gui),
    _current_path(genesis_home_dir)
{
    update_current_path_display();
    _current_path_widget.set_background(false);
    _current_path_widget.set_text_interaction(false);
    _filter_widget.set_placeholder_text("file filter");
    _filter_widget._userdata = this;
    _filter_widget.set_on_key_event(on_filter_key);
    update_model();
}

void FindFileWidget::update_model() {
    _current_path_widget.set_pos(
            _left + _padding_left,
            _top + _padding_top);
    _current_path_widget.set_width(_width - _padding_right - _padding_left);
    _filter_widget.set_pos(
            _current_path_widget.left(),
            _current_path_widget.top() + _current_path_widget.height() + _margin);
    _filter_widget.set_width(_current_path_widget.width());
}

void FindFileWidget::draw(const glm::mat4 &projection) {
    _gui->fill_rect(glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), _left, _top, _width, _height);
    _current_path_widget.draw(projection);
    _filter_widget.draw(projection);
}

int FindFileWidget::width() const {
    return _width;
}

int FindFileWidget::height() const {
    return _height;
}

void FindFileWidget::on_mouse_move(const MouseEvent *event) {
    MouseEvent mouse_event = *event;
    mouse_event.x += _left;
    mouse_event.y += _top;
    if (_gui->try_mouse_move_event_on_widget(&_current_path_widget._widget, &mouse_event))
        return;
    if (_gui->try_mouse_move_event_on_widget(&_filter_widget._widget, &mouse_event))
        return;
}

void FindFileWidget::on_mouse_out(const MouseEvent *event) {

}

void FindFileWidget::on_mouse_over(const MouseEvent *event) {

}

void FindFileWidget::on_gain_focus() {
    _gui->set_focus_widget(&_filter_widget._widget);
}

void FindFileWidget::on_lose_focus() {

}

void FindFileWidget::on_text_input(const TextInputEvent *event) {

}

void FindFileWidget::on_key_event(const KeyEvent *event) {

}

bool FindFileWidget::on_filter_key(const KeyEvent *event) {
    if (event->action != KeyActionDown)
        return false;

    if (event->virt_key == VirtKeyBackspace && _filter_widget.text().length() == 0) {
        go_up_one();
        return true;
    }

    return false;
}

void FindFileWidget::go_up_one() {
    _current_path = path_dirname(_current_path);
    update_current_path_display();
}

void FindFileWidget::update_current_path_display() {
    bool ok;
    _current_path_widget.set_text(String(_current_path, &ok));
    if (!ok)
        fprintf(stderr, "Invalid UTF-8 in path\n");
}
