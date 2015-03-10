#ifndef FIND_FILE_WIDGET_HPP
#define FIND_FILE_WIDGET_HPP

#include "widget.hpp"
#include "text_widget.hpp"
#include "byte_buffer.hpp"
#include "path.hpp"

class Gui;
class GuiWindow;

class FindFileWidget : public Widget {
public:
    FindFileWidget(GuiWindow *window, Gui *gui);
    ~FindFileWidget();

    void draw(GuiWindow *window, const glm::mat4 &projection) override;
    int left() const override { return _left; }
    int top() const override { return _top; }
    int width() const override;
    int height() const override;
    void on_mouse_move(const MouseEvent *event) override;
    void on_gain_focus() override;

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

    void set_on_choose_file(void (*fn)(FindFileWidget *, const ByteBuffer &file_path)) {
        _on_choose_file = fn;
    }

    void *_userdata;
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

    TextWidget *_current_path_widget;
    TextWidget *_filter_widget;

    GuiWindow *_gui_window;
    Gui *_gui;

    ByteBuffer _current_path;
    List<DirEntry*> _entries;

    struct DisplayEntry {
        DirEntry *entry;
        TextWidget *widget;
    };
    List<DisplayEntry> _displayed_entries;

    bool _show_hidden_files;

    void (*_on_choose_file)(FindFileWidget *, const ByteBuffer &file_path);

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

    FindFileWidget(const FindFileWidget &copy) = delete;
    FindFileWidget &operator=(const FindFileWidget &copy) = delete;
};

#endif
