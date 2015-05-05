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
#include "text_widget.hpp"

static void exit_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->gui->_running = false;
}

static void new_window_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->create_editor_window();
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

static void on_fps_change(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    ByteBuffer fps_text = ByteBuffer::format("%.0f fps", genesis_editor->gui->fps);
    for (int i = 0; i < genesis_editor->windows.length(); i += 1) {
        EditorWindow *editor_window = genesis_editor->windows.at(i);
        editor_window->fps_widget->set_text(fps_text);
    }
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

    gui->set_fps_callback(on_fps_change, this);

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

    if (settings_file->open_windows.length() == 0) {
        create_sf_open_window();
        settings_dirty = true;
    }
    if (settings_file->perspectives.length() == 0) {
        ok_or_panic(settings_file->perspectives.add_one());
        SettingsFilePerspective *perspective = &settings_file->perspectives.last();
        perspective->name = "Default";
        perspective->always_show_tabs = true;
        perspective->dock.split_ratio = 0.30f;
        perspective->dock.dock_type = SettingsFileDockTypeHoriz;
        perspective->dock.child_a = create_zero<SettingsFileDock>();
        perspective->dock.child_b = create_zero<SettingsFileDock>();

        perspective->dock.child_a->dock_type = SettingsFileDockTypeTabs;
        ok_or_panic(perspective->dock.child_a->tabs.append("Resources"));

        perspective->dock.child_b->dock_type = SettingsFileDockTypeTabs;
        ok_or_panic(perspective->dock.child_b->tabs.append("Track Editor"));
        settings_dirty = true;
    }
    for (int i = 0; i < settings_file->open_windows.length(); i += 1) {
        SettingsFileOpenWindow *sf_open_window = &settings_file->open_windows.at(i);
        create_window(sf_open_window);
    }

    if (settings_dirty)
        settings_file_commit(settings_file);

    project_attach_event_handler(project, ProjectEventUndoChanged, on_undo_changed, this);
}

void GenesisEditor::create_editor_window() {
    create_window(create_sf_open_window());
    settings_file_commit(settings_file);
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
    bool last_one = (windows.length() == 1);
    windows.swap_remove(index);
    gui->destroy_window(editor_window->window);
    if (!last_one)
        save_window_config();
}

void GenesisEditor::close_others(EditorWindow *window) {
    while (windows.length() > 1) {
        EditorWindow *target_window = windows.at(windows.length() - 1);
        if (target_window == window)
            target_window = windows.at(windows.length() - 2);
        close_window(target_window);
    }
    save_window_config();
}

static void static_on_close_event(GuiWindow *window) {
    EditorWindow *editor_window = (EditorWindow *)window->_userdata;
    editor_window->genesis_editor->close_window(editor_window);
}

SettingsFileOpenWindow *GenesisEditor::create_sf_open_window() {
    ok_or_panic(settings_file->open_windows.add_one());
    SettingsFileOpenWindow *sf_open_window = &settings_file->open_windows.last();
    sf_open_window->perspective_index = 0;
    sf_open_window->left = 0;
    sf_open_window->top = 0;
    sf_open_window->width = 1366;
    sf_open_window->height = 768;
    sf_open_window->maximized = false;
    return sf_open_window;
}

void GenesisEditor::create_window(SettingsFileOpenWindow *sf_open_window) {
    EditorWindow *new_editor_window = create<EditorWindow>();

    GuiWindow *new_window = gui->create_window(
            sf_open_window->left, sf_open_window->top,
            sf_open_window->width, sf_open_window->height);
    new_window->_userdata = new_editor_window;
    new_window->set_on_close_event(static_on_close_event);

    if (sf_open_window->maximized)
        new_window->maximize();

    new_editor_window->genesis_editor = this;
    new_editor_window->window = new_window;

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

    TextWidget *fps_widget = create<TextWidget>(new_window);
    fps_widget->set_text_interaction(false);
    fps_widget->set_background_color(menu_widget->bg_color);
    fps_widget->set_min_width(50);
    fps_widget->set_max_width(50);
    new_editor_window->fps_widget = fps_widget;

    GridLayoutWidget *top_bar_grid_layout = create<GridLayoutWidget>(new_window);
    top_bar_grid_layout->padding = 0;
    top_bar_grid_layout->spacing = 0;
    top_bar_grid_layout->add_widget(menu_widget, 0, 0, HAlignLeft, VAlignTop);
    top_bar_grid_layout->add_widget(fps_widget, 0, 1, HAlignRight, VAlignTop);

    ResourcesTreeWidget *resources_tree = create<ResourcesTreeWidget>(new_window);
    DockablePaneWidget *resources_tree_dock = create<DockablePaneWidget>(resources_tree, "Resources");
    ok_or_panic(new_editor_window->all_panes.append(resources_tree_dock));

    TrackEditorWidget *track_editor = create<TrackEditorWidget>(new_window, project);
    DockablePaneWidget *track_editor_dock = create<DockablePaneWidget>(track_editor, "Track Editor");
    ok_or_panic(new_editor_window->all_panes.append(track_editor_dock));

    DockAreaWidget *dock_area = create<DockAreaWidget>(new_window);
    new_editor_window->dock_area = dock_area;

    GridLayoutWidget *main_grid_layout = create<GridLayoutWidget>(new_window);
    main_grid_layout->padding = 0;
    main_grid_layout->spacing = 0;
    main_grid_layout->add_widget(top_bar_grid_layout, 0, 0, HAlignLeft, VAlignTop);
    main_grid_layout->add_widget(dock_area, 1, 0, HAlignLeft, VAlignTop);
    new_window->set_main_widget(main_grid_layout);

    SettingsFilePerspective *perspective = &settings_file->perspectives.at(sf_open_window->perspective_index);
    load_perspective(new_editor_window, perspective);
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

void GenesisEditor::load_perspective(EditorWindow *editor_window, SettingsFilePerspective *perspective) {
    editor_window->always_show_tabs = perspective->always_show_tabs;

    editor_window->dock_area->reset_state();

    load_dock(editor_window, editor_window->dock_area, &perspective->dock);


    editor_window->dock_area->set_auto_hide_tabs(!editor_window->always_show_tabs);
    refresh_menu_state();
    editor_window->window->main_widget->on_resize();
}

void GenesisEditor::load_dock(EditorWindow *editor_window, DockAreaWidget *dock_area, SettingsFileDock *sf_dock) {
    dock_area->layout = (DockAreaLayout)sf_dock->dock_type;
    switch (sf_dock->dock_type) {
        case SettingsFileDockTypeTabs:
            for (int i = 0; i < sf_dock->tabs.length(); i += 1) {
                String name = sf_dock->tabs.at(i);
                dock_area->add_tab_pane(get_pane_widget(editor_window, name));
            }
            break;
        case SettingsFileDockTypeHoriz:
        case SettingsFileDockTypeVert:
            assert(sf_dock->child_a);
            assert(sf_dock->child_b);
            dock_area->child_a = create_zero<DockAreaWidget>(editor_window->window);
            dock_area->child_b = create_zero<DockAreaWidget>(editor_window->window);
            dock_area->split_ratio = sf_dock->split_ratio;
            load_dock(editor_window, dock_area->child_a, sf_dock->child_a);
            load_dock(editor_window, dock_area->child_b, sf_dock->child_b);
            dock_area->child_a->parent_widget = dock_area;
            dock_area->child_b->parent_widget = dock_area;
            break;
    }
}

DockablePaneWidget *GenesisEditor::get_pane_widget(EditorWindow *editor_window, const String &title) {
    for (int i = 0; i < editor_window->all_panes.length(); i += 1) {
        DockablePaneWidget *pane = editor_window->all_panes.at(i);
        if (String::compare(pane->title, title) == 0)
            return pane;
    }
    panic("pane not found: %s", title.encode().raw());
}

void GenesisEditor::save_window_config() {
    ok_or_panic(settings_file->open_windows.resize(windows.length()));
    for (int i = 0; i < windows.length(); i += 1) {
        EditorWindow *editor_window = windows.at(i);
        GuiWindow *window = editor_window->window;
        SettingsFileOpenWindow *sf_open_window = &settings_file->open_windows.at(i);
        sf_open_window->perspective_index = 0;
        sf_open_window->left = window->client_left;
        sf_open_window->top = window->client_top;
        sf_open_window->width = window->_client_width;
        sf_open_window->height = window->_client_height;
        sf_open_window->maximized = window->is_maximized;
    }
    settings_file_commit(settings_file);
}
