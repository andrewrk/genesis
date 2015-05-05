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
class GenesisEditor;
class DockAreaWidget;
class TextWidget;
struct SettingsFilePerspective;
struct SettingsFileOpenWindow;
struct SettingsFileDock;
class DockablePaneWidget;

struct EditorWindow {
    GenesisEditor *genesis_editor;
    GuiWindow *window;
    MenuWidgetItem *undo_menu;
    MenuWidgetItem *redo_menu;
    MenuWidgetItem *always_show_tabs_menu;
    bool always_show_tabs;
    DockAreaWidget* dock_area;
    TextWidget *fps_widget;
    List<DockablePaneWidget*> all_panes;
};

class GenesisEditor {
public:
    GenesisEditor();
    ~GenesisEditor();

    void exec();
    void create_window(SettingsFileOpenWindow *sf_open_window);


    GenesisContext *_genesis_context;
    ResourceBundle *resource_bundle;
    Gui *gui;

    List<EditorWindow *> windows;
    Project *project;
    User *user;
    SettingsFile *settings_file;

    bool on_key_event(GuiWindow *window, const KeyEvent *event);
    bool on_text_event(GuiWindow *window, const TextInputEvent *event);

    void destroy_find_file_widget();
    void destroy_audio_edit_widget();

    void refresh_menu_state();
    int window_index(EditorWindow *window);
    void close_window(EditorWindow *window);
    void close_others(EditorWindow *window);

    void do_undo();
    void do_redo();

    void load_perspective(EditorWindow *window, SettingsFilePerspective *perspective);
    void load_dock(EditorWindow *editor_window, DockAreaWidget *dock_area, SettingsFileDock *sf_dock);
    DockablePaneWidget *get_pane_widget(EditorWindow *editor_window, const String &title);
    void create_editor_window();
    SettingsFileOpenWindow *create_sf_open_window();
    void save_window_config();
    void save_perspective_config(EditorWindow *editor_window);
    void save_dock(DockAreaWidget *dock_area, SettingsFileDock *sf_dock);

    GenesisEditor(const GenesisEditor &copy) = delete;
    GenesisEditor &operator=(const GenesisEditor &copy) = delete;
};

#endif
