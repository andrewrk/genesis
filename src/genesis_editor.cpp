#include "genesis_editor.hpp"
#include "list.hpp"
#include "gui_window.hpp"
#include "genesis.h"
#include "grid_layout_widget.hpp"
#include "menu_widget.hpp"
#include "resources_tree_widget.hpp"
#include "track_editor_widget.hpp"
#include "project.hpp"
#include "settings_file.hpp"
#include "resource_bundle.hpp"
#include "path.hpp"
#include "dockable_pane_widget.hpp"

static void exit_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->gui->_running = false;
}

static void new_window_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->create_window();
}

static void close_window_handler(void *userdata) {
    EditorWindow *editor_window = (EditorWindow *)userdata;
    editor_window->genesis_editor->close_window(editor_window);
}

static void close_others_handler(void *userdata) {
    EditorWindow *editor_window = (EditorWindow *)userdata;
    editor_window->genesis_editor->close_others(editor_window);
}

static void report_bug_handler(void *userdata) {
    os_open_in_browser("https://github.com/andrewrk/genesis/issues");
}

static void undo_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->do_undo();
}

static void redo_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->do_redo();
}

static void on_undo_changed(Project *, ProjectEvent, void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->refresh_menu_state();
}

static void always_show_tabs_handler(void *userdata) {
    EditorWindow *editor_window = (EditorWindow *)userdata;
    GenesisEditor *genesis_editor = editor_window->genesis_editor;
    editor_window->always_show_tabs = !editor_window->always_show_tabs;
    genesis_editor->refresh_menu_state();
    editor_window->dock_area->set_auto_hide_tabs(!editor_window->always_show_tabs);
}

GenesisEditor::GenesisEditor() :
    project(nullptr)
{
    ByteBuffer config_dir = os_get_app_config_dir();
    int err = path_mkdirp(config_dir);
    if (err)
        panic("unable to make genesis path: %s", genesis_error_string(err));
    ByteBuffer config_path = os_get_app_config_path();
    settings_file = settings_file_open(config_path);

    resource_bundle = create<ResourceBundle>("resources.bundle");

    err = genesis_create_context(&_genesis_context);
    if (err)
        panic("unable to create genesis context: %s", genesis_error_string(err));

    gui = create<Gui>(_genesis_context, resource_bundle);

    bool settings_dirty = false;
    if (settings_file->user_name.length() == 0) {
        settings_file->user_name = os_get_user_name();
        settings_dirty = true;
    }
    if (settings_file->user_id == uint256::zero()) {
        settings_file->user_id = uint256::random();
        settings_dirty = true;
    }
    user = user_create(settings_file->user_id, settings_file->user_name);

    bool create_new = true;
    if (settings_file->open_project_id != uint256::zero()) {
        ByteBuffer proj_dir = path_join(os_get_projects_dir(), settings_file->open_project_id.to_string());
        ByteBuffer proj_path = path_join(proj_dir, "project.gdaw");
        int err = project_open(proj_path.raw(), user, &project);
        if (err) {
            fprintf(stderr, "Unable to load project: %s\n", genesis_error_string(err));
        } else {
            create_new = false;
        }
    }

    if (create_new) {
        uint256 id = uint256::random();
        ByteBuffer proj_dir = path_join(os_get_projects_dir(), id.to_string());
        ByteBuffer proj_path = path_join(proj_dir, "project.gdaw");
        ok_or_panic(path_mkdirp(proj_dir));
        ok_or_panic(project_create(proj_path.raw(), id, user, &project));

        settings_file->open_project_id = id;
        settings_dirty = true;
    }

    if (settings_dirty)
        settings_file_commit(settings_file);

    project_attach_event_handler(project, ProjectEventUndoChanged, on_undo_changed, this);

    create_window();
}

int GenesisEditor::window_index(EditorWindow *window) {
    for (int i = 0; i < windows.length(); i += 1) {
        EditorWindow *editor_window = windows.at(i);
        if (window == editor_window)
            return i;
    }
    panic("window not found");
}

void GenesisEditor::close_window(EditorWindow *editor_window) {
    int index = window_index(editor_window);
    windows.swap_remove(index);
    gui->destroy_window(editor_window->window);
}

void GenesisEditor::close_others(EditorWindow *window) {
    while (windows.length() > 1) {
        EditorWindow *target_window = windows.at(windows.length() - 1);
        if (target_window == window)
            target_window = windows.at(windows.length() - 2);
        close_window(target_window);
    }
}

static void static_on_close_event(GuiWindow *window) {
    EditorWindow *editor_window = (EditorWindow *)window->_userdata;
    editor_window->genesis_editor->close_window(editor_window);
}

void GenesisEditor::create_window() {
    EditorWindow *new_editor_window = create<EditorWindow>();

    GuiWindow *new_window = gui->create_window(true);
    new_window->_userdata = new_editor_window;
    new_window->set_on_close_event(static_on_close_event);

    new_editor_window->genesis_editor = this;
    new_editor_window->window = new_window;
    new_editor_window->always_show_tabs = true;


    ok_or_panic(windows.append(new_editor_window));

    MenuWidget *menu_widget = create<MenuWidget>(new_window);
    MenuWidgetItem *project_menu = menu_widget->add_menu("&Project");
    MenuWidgetItem *edit_menu = menu_widget->add_menu("&Edit");
    MenuWidgetItem *window_menu = menu_widget->add_menu("&Window");
    MenuWidgetItem *help_menu = menu_widget->add_menu("&Help");

    MenuWidgetItem *exit_menu = project_menu->add_menu("E&xit", ctrl_shortcut(VirtKeyQ));

    MenuWidgetItem *undo_menu = edit_menu->add_menu("&Undo", ctrl_shortcut(VirtKeyZ));
    MenuWidgetItem *redo_menu = edit_menu->add_menu("&Redo", ctrl_shift_shortcut(VirtKeyZ));

    MenuWidgetItem *new_window_menu = window_menu->add_menu("&New Window", no_shortcut());
    MenuWidgetItem *close_window_menu = window_menu->add_menu("&Close", alt_shortcut(VirtKeyF4));
    MenuWidgetItem *close_others_menu = window_menu->add_menu("Close &Others", no_shortcut());
    MenuWidgetItem *always_show_tabs_menu = window_menu->add_menu("Always Show &Tabs", no_shortcut());

    MenuWidgetItem *report_bug_menu = help_menu->add_menu("&Report a Bug", shortcut(VirtKeyF1));

    exit_menu->set_activate_handler(exit_handler, this);

    undo_menu->set_activate_handler(undo_handler, this);
    redo_menu->set_activate_handler(redo_handler, this);

    new_window_menu->set_activate_handler(new_window_handler, this);
    close_window_menu->set_activate_handler(close_window_handler, new_editor_window);
    close_others_menu->set_activate_handler(close_others_handler, new_editor_window);
    always_show_tabs_menu->set_activate_handler(always_show_tabs_handler, new_editor_window);

    report_bug_menu->set_activate_handler(report_bug_handler, this);

    new_editor_window->undo_menu = undo_menu;
    new_editor_window->redo_menu = redo_menu;
    new_editor_window->always_show_tabs_menu = always_show_tabs_menu;

    ResourcesTreeWidget *resources_tree = create<ResourcesTreeWidget>(new_window);
    DockablePaneWidget *resources_tree_dock = create<DockablePaneWidget>(resources_tree, "Resources");

    TrackEditorWidget *track_editor = create<TrackEditorWidget>(new_window, project);
    DockablePaneWidget *track_editor_dock = create<DockablePaneWidget>(track_editor, "Track Editor");

    DockAreaWidget *dock_area = create<DockAreaWidget>(new_window);
    new_editor_window->dock_area = dock_area;
    dock_area->add_left_pane(resources_tree_dock);
    dock_area->add_right_pane(track_editor_dock);

    GridLayoutWidget *main_grid_layout = create<GridLayoutWidget>(new_window);
    main_grid_layout->padding = 0;
    main_grid_layout->spacing = 0;
    main_grid_layout->add_widget(menu_widget, 0, 0, HAlignLeft, VAlignTop);
    main_grid_layout->add_widget(dock_area, 1, 0, HAlignLeft, VAlignTop);
    new_window->set_main_widget(main_grid_layout);

    refresh_menu_state();
}

GenesisEditor::~GenesisEditor() {
    project_close(project);
    user_destroy(user);
    genesis_destroy_context(_genesis_context);
}

void GenesisEditor::exec() {
    gui->exec();
}

void GenesisEditor::refresh_menu_state() {
    bool undo_enabled = (project->undo_stack_index > 0);
    bool redo_enabled = (project->undo_stack_index < project->undo_stack.length());
    String undo_caption;
    String redo_caption;
    if (undo_enabled) {
        Command *cmd = project->undo_stack.at(project->undo_stack_index - 1);
        undo_caption = "&Undo ";
        undo_caption.append(cmd->description());
    } else {
        undo_caption = "&Undo";
    }
    if (redo_enabled) {
        Command *cmd = project->undo_stack.at(project->undo_stack_index);
        redo_caption = "&Redo ";
        redo_caption.append(cmd->description());
    } else {
        redo_caption = "&Redo";
    }

    for (int i = 0; i < windows.length(); i += 1) {
        EditorWindow *editor_window = windows.at(i);
        editor_window->undo_menu->set_enabled(undo_enabled);
        editor_window->undo_menu->set_caption(undo_caption);

        editor_window->redo_menu->set_enabled(redo_enabled);
        editor_window->redo_menu->set_caption(redo_caption);

        editor_window->always_show_tabs_menu->set_icon(
                editor_window->always_show_tabs ? gui->img_check : nullptr);

        editor_window->window->refresh_context_menu();
    }
}

void GenesisEditor::do_undo() {
    project_undo(project);
}

void GenesisEditor::do_redo() {
    project_redo(project);
}
