#include "find_file_widget.hpp"
#include "os.hpp"
#include "gui_window.hpp"

static void default_on_choose_file(FindFileWidget *find_file_widget,
        const ByteBuffer &file_path)
{
    fprintf(stderr, "you have chosen: %s\n", file_path.raw());
}

FindFileWidget::FindFileWidget(GuiWindow *gui_window) :
    Widget(gui_window),
    _padding_left(4),
    _padding_right(4),
    _padding_top(4),
    _padding_bottom(4),
    _margin(4),
    _show_hidden_files(false),
    _on_choose_file(default_on_choose_file)
{
    _current_path_widget = create<TextWidget>(gui_window);
    _filter_widget = create<TextWidget>(gui_window);

    _current_path_widget->set_background_on(false);
    _current_path_widget->set_text_interaction(false);

    _filter_widget->set_placeholder_text("file filter");
    _filter_widget->_userdata = this;
    _filter_widget->set_on_key_event(on_filter_key);
    _filter_widget->set_on_text_change_event(on_filter_text_change);

    change_current_path(os_get_home_dir());
    update_model();
}

FindFileWidget::~FindFileWidget() {
    destroy(_current_path_widget, 1);
    destroy(_filter_widget, 1);
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
        destroy((TextWidgetUserData *)display_entry.widget->_userdata, 1);
        destroy(display_entry.widget, 1);
    }
    _displayed_entries.clear();
}

void FindFileWidget::update_model() {
    _current_path_widget->left = left + _padding_left;
    _current_path_widget->top = top + _padding_top;
    _current_path_widget->width = width - _padding_right - _padding_left;
    _current_path_widget->on_resize();

    _filter_widget->left = _current_path_widget->left;
    _filter_widget->top = _current_path_widget->top + _current_path_widget->height + _margin;
    _filter_widget->width = _current_path_widget->width;
    _filter_widget->on_resize();

    int y = _filter_widget->top + _filter_widget->height + _margin;
    for (int i = 0; i < _displayed_entries.length(); i += 1) {
        DisplayEntry *display_entry = &_displayed_entries.at(i);
        TextWidget *text_widget = display_entry->widget;
        text_widget->left = _current_path_widget->left;
        text_widget->top = y;
        text_widget->width = _current_path_widget->width;
        text_widget->is_visible = (text_widget->top <= top + _padding_top + height);
        text_widget->on_resize();
        y += text_widget->height;
    }
}

void FindFileWidget::draw(const glm::mat4 &projection) {
    gui_window->fill_rect(glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), left, top, width, height);
    _current_path_widget->draw(projection);
    _filter_widget->draw(projection);

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear(GL_STENCIL_BUFFER_BIT);

    gui_window->fill_rect(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            left + _padding_left, top + _padding_top,
            width - _padding_left - _padding_right, height - _padding_bottom - _padding_top);

    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilMask(0x00);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    for (int i = 0; i < _displayed_entries.length(); i += 1) {
        DisplayEntry *display_entry = &_displayed_entries.at(i);
        TextWidget *text_widget = display_entry->widget;
        if (text_widget->is_visible)
            text_widget->draw(projection);
    }

    glDisable(GL_STENCIL_TEST);
}

void FindFileWidget::on_mouse_move(const MouseEvent *event) {
    MouseEvent mouse_event = *event;
    mouse_event.x += left;
    mouse_event.y += top;
    if (gui_window->try_mouse_move_event_on_widget(_current_path_widget, &mouse_event))
        return;
    if (gui_window->try_mouse_move_event_on_widget(_filter_widget, &mouse_event))
        return;

    for (int i = 0; i < _displayed_entries.length(); i += 1) {
        DisplayEntry *display_entry = &_displayed_entries.at(i);
        TextWidget *text_widget = display_entry->widget;
        if (gui_window->try_mouse_move_event_on_widget(text_widget, &mouse_event))
            return;
    }
}

void FindFileWidget::on_gain_focus() {
    gui_window->set_focus_widget(_filter_widget);
}

bool FindFileWidget::on_filter_key(const KeyEvent *event) {
    if (event->action != KeyActionDown)
        return false;

    if (event->virt_key == VirtKeyBackspace && _filter_widget->text().length() == 0) {
        go_up_one();
        return true;
    }

    if (event->virt_key == VirtKeyH && key_mod_ctrl(event->modifiers)) {
        _show_hidden_files = !_show_hidden_files;
        update_entries_display();
        return true;
    }

    if (event->virt_key == VirtKeyEnter) {
        if (_displayed_entries.length() > 0)
            choose_dir_entry(_displayed_entries.at(0).entry);
        return true;
    }

    return false;
}

void FindFileWidget::choose_dir_entry(OsDirEntry *dir_entry) {
    ByteBuffer selected_path = os_path_join(_current_path, dir_entry->name);
    if (dir_entry->is_dir) {
        change_current_path(selected_path);
    } else {
        _on_choose_file(this, selected_path);
    }
}

void FindFileWidget::go_up_one() {
    change_current_path(os_path_dirname(_current_path));
}

void FindFileWidget::update_current_path_display() {
    bool ok;
    _current_path_widget->set_text(String(_current_path, &ok));
    if (!ok)
        fprintf(stderr, "Invalid UTF-8 in path\n");
}

void FindFileWidget::change_current_path(const ByteBuffer &dir) {
    _current_path = dir;
    _filter_widget->set_text("");
    update_current_path_display();
    destroy_all_dir_entries();
    int err = os_readdir(_current_path.raw(), _entries);
    if (err) {
        char *err_str = strerror(err);
        fprintf(stderr, "Unable to read directory: %s\n", err_str);
    } else {
        update_entries_display();
    }
}

void FindFileWidget::update_entries_display() {
    destroy_all_displayed_entries();

    List<String> search_words;
    _filter_widget->text().split_on_whitespace(search_words);

    bool ok;
    for (int i = 0; i < _entries.length(); i += 1) {
        OsDirEntry *entry = _entries.at(i);
        String text(entry->name, &ok);
        if (should_show_entry(entry, text, search_words)) {
            TextWidget *text_widget = create<TextWidget>(gui_window);
            text_widget->set_background_on(false);
            text_widget->set_text_interaction(false);
            text_widget->set_text(text);
            text_widget->set_hover_color(glm::vec4(0.663f, 0.663f, 0.663f, 1.0f));
            text_widget->set_hover_on(true);
            text_widget->set_on_mouse_event(on_entry_mouse);

            TextWidgetUserData *userdata = create<TextWidgetUserData>();
            userdata->find_file_widget = this;
            userdata->dir_entry = entry;
            text_widget->_userdata = userdata;

            if (entry->is_dir) {
                text_widget->set_icon(gui->img_entry_dir);
            } else if (entry->is_file) {
                text_widget->set_icon(gui->img_entry_file);
            } else {
                text_widget->set_icon(gui->img_null);
            }

            ok_or_panic(_displayed_entries.append({
                    entry,
                    text_widget,
            }));
        }
    }
    _displayed_entries.sort<compare_display_name>();
    update_model();
}

void FindFileWidget::on_filter_text_change() {
    update_entries_display();
}

bool FindFileWidget::should_show_entry(OsDirEntry *dir_entry, const String &text,
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

bool FindFileWidget::on_entry_mouse(TextWidget *entry_widget,
        TextWidgetUserData *userdata, const MouseEvent *event)
{
    if (event->action != MouseActionDown)
        return false;

    choose_dir_entry(userdata->dir_entry);

    return true;
}
