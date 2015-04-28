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

static void exit_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->_gui->_running = false;
}

static void new_window_handler(void *userdata) {
    GenesisEditor *genesis_editor = (GenesisEditor *)userdata;
    genesis_editor->create_window();
}

static void report_bug_handler(void *userdata) {
    os_open_in_browser("https://github.com/andrewrk/genesis/issues");
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

    _gui = create<Gui>(_genesis_context, resource_bundle);

    bool settings_dirty = false;
    if (settings_file->user_name.length() == 0) {
        settings_file->user_name = os_get_user_name();
        settings_dirty = true;
    }
    User *user = user_create(settings_file->user_name);

    if (settings_file->open_project_id == uint256::zero()) {
        uint256 id = uint256::random();
        ByteBuffer proj_path = path_join(os_get_projects_dir(), id.to_string());
        ok_or_panic(project_create(proj_path.raw(), id, user, &project));

        settings_file->open_project_id = id;
        settings_dirty = true;
    } else {
        ByteBuffer proj_path = path_join(os_get_projects_dir(), settings_file->open_project_id.to_string());
        ok_or_panic(project_open(proj_path.raw(), user, &project));
    }

    if (settings_dirty)
        settings_file_commit(settings_file);

    create_window();
}

static int window_index(GenesisEditor *genesis_editor, GuiWindow *window) {
    for (int i = 0; i < genesis_editor->windows.length(); i += 1) {
        if (window == genesis_editor->windows.at(i))
            return i;
    }
    panic("window not found");
}

static void on_close_event(GuiWindow *window) {
    GenesisEditor *genesis_editor = (GenesisEditor *)window->_userdata;
    int index = window_index(genesis_editor, window);
    genesis_editor->windows.swap_remove(index);
    genesis_editor->_gui->destroy_window(window);
}

void GenesisEditor::create_window() {
    GuiWindow *new_window = _gui->create_window(true);
    new_window->_userdata = this;
    new_window->set_on_close_event(on_close_event);

    if (windows.append(new_window))
        panic("out of memory");

    MenuWidget *menu_widget = create<MenuWidget>(new_window);
    MenuWidgetItem *project_menu = menu_widget->add_menu("Project", 0);
    MenuWidgetItem *window_menu = menu_widget->add_menu("Window", 0);
    MenuWidgetItem *help_menu = menu_widget->add_menu("Help", 0);

    MenuWidgetItem *exit_menu = project_menu->add_menu("Exit", 1, alt_shortcut(VirtKeyF4));
    MenuWidgetItem *new_window_menu = window_menu->add_menu("New Window", 0, no_shortcut());
    MenuWidgetItem *report_bug_menu = help_menu->add_menu("Report a Bug", 0, shortcut(VirtKeyF1));

    exit_menu->set_activate_handler(exit_handler, this);
    new_window_menu->set_activate_handler(new_window_handler, this);
    report_bug_menu->set_activate_handler(report_bug_handler, this);

    ResourcesTreeWidget *resources_tree = create<ResourcesTreeWidget>(new_window);

    TrackEditorWidget *track_editor = create<TrackEditorWidget>(new_window, project);

    GridLayoutWidget *grid_layout = create<GridLayoutWidget>(new_window);
    grid_layout->padding = 0;
    grid_layout->spacing = 0;
    grid_layout->add_widget(resources_tree, 0, 0, HAlignLeft, VAlignTop);
    grid_layout->add_widget(track_editor, 0, 1, HAlignLeft, VAlignTop);

    GridLayoutWidget *main_grid_layout = create<GridLayoutWidget>(new_window);
    main_grid_layout->padding = 0;
    main_grid_layout->spacing = 0;
    main_grid_layout->add_widget(menu_widget, 0, 0, HAlignLeft, VAlignTop);
    main_grid_layout->add_widget(grid_layout, 1, 0, HAlignLeft, VAlignTop);
    new_window->set_main_widget(main_grid_layout);
}

GenesisEditor::~GenesisEditor() {
    project_close(project);
    genesis_destroy_context(_genesis_context);
}

void GenesisEditor::exec() {
    _gui->exec();
}
