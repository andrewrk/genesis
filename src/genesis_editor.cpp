#include "genesis_editor.hpp"
#include "list.hpp"
#include "gui_window.hpp"
#include "genesis.h"
#include "grid_layout_widget.hpp"
#include "menu_widget.hpp"
#include "resources_tree_widget.hpp"

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
    _resource_bundle("resources.bundle")
{
    int err = genesis_create_context(&_genesis_context);
    if (err)
        panic("unable to create genesis context: %s", genesis_error_string(err));

    _gui = create<Gui>(_genesis_context, &_resource_bundle);

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

    GridLayoutWidget *grid_layout = create<GridLayoutWidget>(new_window);
    grid_layout->padding = 0;
    grid_layout->spacing = 0;
    grid_layout->add_widget(menu_widget, 0, 0, HAlignLeft, VAlignTop);
    grid_layout->add_widget(resources_tree, 1, 0, HAlignLeft, VAlignTop);
    new_window->set_main_widget(grid_layout);
}

GenesisEditor::~GenesisEditor() {
    genesis_destroy_context(_genesis_context);
}

void GenesisEditor::exec() {
    _gui->exec();
}

void GenesisEditor::edit_file(const char *filename) {
    fprintf(stderr, "edit file %s\n", filename);
}
