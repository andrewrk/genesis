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
struct EditorWindow;

struct EditorPane {
    DockablePaneWidget *pane;
    EditorWindow *editor_window;
    MenuWidgetItem *show_menu_item;
};

struct EditorWindow {
    GenesisEditor *genesis_editor;
    GuiWindow *window;
    MenuWidgetItem *undo_menu;
    MenuWidgetItem *redo_menu;
    MenuWidgetItem *always_show_tabs_menu;
    MenuWidgetItem *show_view_menu;
    bool always_show_tabs;
    DockAreaWidget* dock_area;
    TextWidget *fps_widget;
    List<EditorPane *> all_panes;
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
    void save_dock(DockAreaWidget *dock_area, SettingsFileDock *sf_dock);
    void add_dock(EditorWindow *editor_window, Widget *widget, const char *title);

    void show_view(EditorPane *editor_pane);
    DockablePaneWidget *find_pane(EditorPane *editor_pane, DockAreaWidget *dock_area);

    GenesisEditor(const GenesisEditor &copy) = delete;
    GenesisEditor &operator=(const GenesisEditor &copy) = delete;
};

#endif
