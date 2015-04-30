#ifndef GENESIS_EDITOR
#define GENESIS_EDITOR

#include "key_event.hpp"
#include "gui.hpp"

class GuiWindow;
struct GenesisContext;
struct Project;
struct SettingsFile;
struct User;
class MenuWidgetItem;

class GenesisEditor {
public:
    GenesisEditor();
    ~GenesisEditor();

    void exec();
    void create_window();


    GenesisContext *_genesis_context;
    ResourceBundle *resource_bundle;
    Gui *_gui;

    struct EditorWindow {
        GuiWindow *window;
        MenuWidgetItem *undo_menu;
        MenuWidgetItem *redo_menu;
    };
    List<EditorWindow *> windows;
    Project *project;
    User *user;
    SettingsFile *settings_file;

    bool on_key_event(GuiWindow *window, const KeyEvent *event);
    bool on_text_event(GuiWindow *window, const TextInputEvent *event);

    void destroy_find_file_widget();
    void destroy_audio_edit_widget();

    void refresh_menu_state();
    int window_index(GuiWindow *window);
    void on_close_event(GuiWindow *window);

    void do_undo();
    void do_redo();

    GenesisEditor(const GenesisEditor &copy) = delete;
    GenesisEditor &operator=(const GenesisEditor &copy) = delete;
};

#endif
