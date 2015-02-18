#ifndef FIND_FILE_WIDGET_HPP
#define FIND_FILE_WIDGET_HPP

#include "widget.hpp"
#include "text_widget.hpp"
#include "byte_buffer.hpp"
#include "path.hpp"

class Gui;
class FindFileWidget {
public:
    Widget _widget;

    FindFileWidget(Gui *gui);
    FindFileWidget(const FindFileWidget &copy) = delete;
    FindFileWidget &operator=(const FindFileWidget &copy) = delete;

    ~FindFileWidget();
    void draw(const glm::mat4 &projection);
    int left() const { return _left; }
    int top() const { return _top; }
    int width() const;
    int height() const;
    void on_mouse_move(const MouseEvent *event);
    void on_mouse_out(const MouseEvent *event);
    void on_mouse_over(const MouseEvent *event);
    void on_gain_focus();
    void on_lose_focus();
    void on_text_input(const TextInputEvent *event);
    void on_key_event(const KeyEvent *event);

    enum Mode {
        ModeOpen,
        ModeSave,
    };

    void set_mode(Mode mode) {
        _mode = mode;
    }

    void set_pos(int new_left, int new_top) {
        _left = new_left;
        _top = new_top;
        update_model();
    }
private:
    struct TextWidgetUserData {
        FindFileWidget *find_file_widget;
        DirEntry *dir_entry;
    };

    Mode _mode;

    int _left;
    int _top;
    int _width;
    int _height;
    int _padding_left;
    int _padding_right;
    int _padding_top;
    int _padding_bottom;
    int _margin; // space between widgets

    TextWidget _current_path_widget;
    TextWidget _filter_widget;

    Gui *_gui;

    ByteBuffer _current_path;
    List<DirEntry*> _entries;

    struct DisplayEntry {
        DirEntry *entry;
        TextWidget *widget;
    };
    List<DisplayEntry> _displayed_entries;

    bool _show_hidden_files;

    void update_model();
    bool on_filter_key(const KeyEvent *event);
    void go_up_one();
    void on_filter_text_change();
    bool on_entry_mouse(TextWidget *entry_widget, TextWidgetUserData *userdata,
            const MouseEvent *event);


    static bool on_filter_key(TextWidget *text_widget, const KeyEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(text_widget->_userdata))->on_filter_key(event);
    }

    static void on_filter_text_change(TextWidget *text_widget) {
        return (reinterpret_cast<FindFileWidget*>(text_widget->_userdata))->on_filter_text_change();
    }

    static bool on_entry_mouse(TextWidget *text_widget, const MouseEvent *event) {
        TextWidgetUserData *userdata = reinterpret_cast<TextWidgetUserData*>(text_widget->_userdata);
        return userdata->find_file_widget->on_entry_mouse(text_widget, userdata, event);
    }

    void update_current_path_display();
    void update_entries_display();
    void destroy_all_displayed_entries();
    void destroy_all_dir_entries();
    void change_current_path(const ByteBuffer &dir);
    bool should_show_entry(DirEntry *dir_entry, const String &text,
            const List<String> &search_words);

    void choose_dir_entry(DirEntry *dir_entry);

    static int compare_display_name(DisplayEntry a, DisplayEntry b) {
        if (a.entry->is_dir && !b.entry->is_dir) {
            return -1;
        } else if (b.entry->is_dir && !a.entry->is_dir) {
            return 1;
        } else {
            return String::compare_insensitive(a.widget->text(), b.widget->text());
        }
    }

    // widget methods
    static void destructor(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->~FindFileWidget();
    }
    static void draw(Widget *widget, const glm::mat4 &projection) {
        return (reinterpret_cast<FindFileWidget*>(widget))->draw(projection);
    }
    static int left(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->left();
    }
    static int top(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->top();
    }
    static int width(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->width();
    }
    static int height(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->height();
    }
    static void on_mouse_move(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_mouse_move(event);
    }
    static void on_mouse_out(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_mouse_out(event);
    }
    static void on_mouse_over(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_mouse_over(event);
    }
    static void on_gain_focus(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_gain_focus();
    }
    static void on_lose_focus(Widget *widget) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_lose_focus();
    }
    static void on_text_input(Widget *widget, const TextInputEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_text_input(event);
    }
    static void on_key_event(Widget *widget, const KeyEvent *event) {
        return (reinterpret_cast<FindFileWidget*>(widget))->on_key_event(event);
    }
};

#endif
