#ifndef GENESIS_EDITOR
#define GENESIS_EDITOR

#include "resource_bundle.hpp"
#include "key_event.hpp"
#include "find_file_widget.hpp"
#include "gui.hpp"

class AudioEditWidget;
class GuiWindow;
struct GenesisContext;

class GenesisEditor {
public:
    GenesisEditor();
    ~GenesisEditor();

    void edit_file(const char *filename);
    void create_new_file();

    void exec();

private:
    GenesisContext *_genesis_context;
    ResourceBundle _resource_bundle;
    Gui _gui;
    GuiWindow *_gui_window;

    FindFileWidget *_find_file_widget;
    AudioEditWidget *_audio_edit_widget;

    bool on_key_event(GuiWindow *window, const KeyEvent *event);
    bool on_text_event(GuiWindow *window, const TextInputEvent *event);
    void on_close_event(GuiWindow *window);

    void destroy_find_file_widget();
    void destroy_audio_edit_widget();

    void on_choose_file(const ByteBuffer &file_path);
    void on_choose_save_file(const ByteBuffer &file_path);
    void show_open_file();
    void show_save_file();
    void ensure_find_file_widget();

    static bool static_on_key_event(GuiWindow *window, const KeyEvent *event) {
        return reinterpret_cast<GenesisEditor*>(window->_userdata)->on_key_event(window, event);
    }

    static bool static_on_text_event(GuiWindow *window, const TextInputEvent *event) {
        return reinterpret_cast<GenesisEditor*>(window->_userdata)->on_text_event(window, event);
    }

    static void static_on_close_event(GuiWindow *window) {
        return reinterpret_cast<GenesisEditor*>(window->_userdata)->on_close_event(window);
    }

    static void static_on_choose_file(FindFileWidget *find_file_widget, const ByteBuffer &file_path) {
        return reinterpret_cast<GenesisEditor*>(find_file_widget->_userdata)->on_choose_file(file_path);
    }

    static void on_choose_save_file(FindFileWidget *find_file_widget, const ByteBuffer &file_path) {
        return reinterpret_cast<GenesisEditor*>(find_file_widget->_userdata)->on_choose_save_file(file_path);
    }

    GenesisEditor(const GenesisEditor &copy) = delete;
    GenesisEditor &operator=(const GenesisEditor &copy) = delete;
};

#endif
