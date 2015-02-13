#include "find_file_widget.hpp"
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
    _gui(gui)
{
    change_current_path(genesis_home_dir);

    _current_path_widget.set_background(false);
    _current_path_widget.set_text_interaction(false);
    _filter_widget.set_placeholder_text("file filter");
    _filter_widget._userdata = this;
    _filter_widget.set_on_key_event(on_filter_key);
    update_model();
}

FindFileWidget::~FindFileWidget() {
    destroy_all_displayed_entries();
    for (int i = 0; i < _entries.length(); i += 1) {
        DirEntry *entry = _entries.at(i);
        destroy(entry);
    }
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

    int y = _filter_widget.top() + _filter_widget.height() + _margin;
    for (int i = 0; i < _displayed_entries.length(); i += 1) {
        TextWidget *text_widget = _displayed_entries.at(i);
        text_widget->set_pos(
                _current_path_widget.left(),
                y);
        text_widget->set_width(_current_path_widget.width());
        y += text_widget->height() + _margin;
    }
}

void FindFileWidget::draw(const glm::mat4 &projection) {
    _gui->fill_rect(glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), _left, _top, _width, _height);
    _current_path_widget.draw(projection);
    _filter_widget.draw(projection);

    for (int i = 0; i < _displayed_entries.length(); i += 1) {
        TextWidget *text_widget = _displayed_entries.at(i);
        text_widget->draw(projection);
    }
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
    change_current_path(path_dirname(_current_path));
}

void FindFileWidget::update_current_path_display() {
    bool ok;
    _current_path_widget.set_text(String(_current_path, &ok));
    if (!ok)
        fprintf(stderr, "Invalid UTF-8 in path\n");
}

void FindFileWidget::change_current_path(const ByteBuffer &dir) {
    _current_path = dir;
    update_current_path_display();
    int err = path_readdir(_current_path.raw(), _entries);
    if (err) {
        // TODO display error in UI
        fprintf(stderr, "Unable to read directory\n");
    } else {
        update_entries_display();
    }
}

void FindFileWidget::update_entries_display() {
    destroy_all_displayed_entries();
    bool ok;
    for (int i = 0; i < _entries.length(); i += 1) {
        DirEntry *entry = _entries.at(i);
        TextWidget *text_widget = create<TextWidget>(_gui);
        text_widget->set_background(false);
        text_widget->set_text_interaction(false);
        text_widget->set_text(String(entry->name, &ok));
        _displayed_entries.append(text_widget);
    }
    update_model();
}

void FindFileWidget::destroy_all_displayed_entries() {
    for (int i = 0; i < _displayed_entries.length(); i += 1) {
        TextWidget *text_widget = _displayed_entries.at(i);
        destroy(text_widget);
    }
    _displayed_entries.clear();
}
