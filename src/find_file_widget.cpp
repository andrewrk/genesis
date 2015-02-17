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
    _gui(gui),
    _show_hidden_files(false)
{
    change_current_path(genesis_home_dir);

    _current_path_widget.set_background(false);
    _current_path_widget.set_text_interaction(false);
    _filter_widget.set_placeholder_text("file filter");
    _filter_widget._userdata = this;
    _filter_widget.set_on_key_event(on_filter_key);
    _filter_widget.set_on_text_change_event(on_filter_text_change);
    update_model();
}

FindFileWidget::~FindFileWidget() {
    destroy_all_displayed_entries();
    destroy_all_dir_entries();
}

void FindFileWidget::destroy_all_dir_entries() {
    for (int i = 0; i < _entries.length(); i += 1) {
        destroy(_entries.at(i), 1);
    }
    _entries.clear();
}

void FindFileWidget::destroy_all_displayed_entries() {
    for (int i = 0; i < _displayed_entries.length(); i += 1) {
        DisplayEntry display_entry = _displayed_entries.at(i);
        destroy(display_entry.widget, 1);
    }
    _displayed_entries.clear();
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
        DisplayEntry *display_entry = &_displayed_entries.at(i);
        TextWidget *text_widget = display_entry->widget;
        text_widget->set_pos(
                _current_path_widget.left(),
                y);
        text_widget->set_width(_current_path_widget.width());
        text_widget->_widget._is_visible = (text_widget->top() <= _top + _padding_top + _height);
        y += text_widget->height() + _margin;
    }
}

void FindFileWidget::draw(const glm::mat4 &projection) {
    _gui->fill_rect(glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), _left, _top, _width, _height);
    _current_path_widget.draw(projection);
    _filter_widget.draw(projection);

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear(GL_STENCIL_BUFFER_BIT);

    _gui->fill_rect(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            _left + _padding_left, _top + _padding_top,
            _width - _padding_left - _padding_right, _height - _padding_bottom - _padding_top);

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilMask(0x00);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    for (int i = 0; i < _displayed_entries.length(); i += 1) {
        DisplayEntry *display_entry = &_displayed_entries.at(i);
        TextWidget *text_widget = display_entry->widget;
        if (text_widget->_widget._is_visible)
            text_widget->draw(projection);
    }

    glDisable(GL_STENCIL_TEST);
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

    if (event->virt_key == VirtKeyH && event->ctrl()) {
        _show_hidden_files = !_show_hidden_files;
        update_entries_display();
        return true;
    }

    if (event->virt_key == VirtKeyReturn) {
        if (_displayed_entries.length() > 0)
            choose_entry(_displayed_entries.at(0));
        return true;
    }

    return false;
}

void FindFileWidget::choose_entry(DisplayEntry display_entry) {
    if (display_entry.entry->is_dir) {
        change_current_path(path_join(_current_path, display_entry.entry->name));
    } else {
        fprintf(stderr, "TODO: you have chosen: %s\n", display_entry.entry->name.raw());
    }
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
    _filter_widget.set_text("");
    update_current_path_display();
    destroy_all_dir_entries();
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

    List<String> search_words;
    _filter_widget.text().split_on_whitespace(search_words);

    bool ok;
    for (int i = 0; i < _entries.length(); i += 1) {
        DirEntry *entry = _entries.at(i);
        String text(entry->name, &ok);
        if (should_show_entry(entry, text, search_words)) {
            TextWidget *text_widget = create<TextWidget>(_gui);
            text_widget->set_background(false);
            text_widget->set_text_interaction(false);
            text_widget->set_text(text);
            _displayed_entries.append({
                    entry,
                    text_widget,
            });
        }
    }
    _displayed_entries.sort<compare_display_name>();
    update_model();
}

void FindFileWidget::on_filter_text_change() {
    update_entries_display();
}

bool FindFileWidget::should_show_entry(DirEntry *dir_entry, const String &text,
        const List<String> &search_words)
{
    if (!_show_hidden_files && dir_entry->is_hidden)
        return false;

    for (int i = 0; i < search_words.length(); i += 1) {
        if (text.index_of_insensitive(search_words.at(i)) == -1)
            return false;
    }

    return true;
}
