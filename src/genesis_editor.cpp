#include "genesis_editor.hpp"
#include "list.hpp"
#include "gui_window.hpp"
#include "genesis.h"
#include "grid_layout_widget.hpp"
#include "menu_widget.hpp"

GenesisEditor::GenesisEditor() :
    _resource_bundle("resources.bundle"),
    _find_file_widget(NULL)
{
    int err = genesis_create_context(&_genesis_context);
    if (err)
        panic("unable to create genesis context: %s", genesis_error_string(err));

    _gui = create<Gui>(_genesis_context, &_resource_bundle);

    _gui_window = _gui->create_window(true);
    _gui_window->_userdata = this;
    _gui_window->set_on_close_event(static_on_close_event);

    MenuWidget *menu_widget = create<MenuWidget>(_gui_window);
    MenuWidgetItem *project_menu = menu_widget->add_menu("Project", 0);
    MenuWidgetItem *help_menu = menu_widget->add_menu("Help", 0);

    project_menu->add_menu("Exit", 1, MenuWidget::alt_shortcut(VirtKeyF4));

    help_menu->add_menu("Report a Bug", 0, MenuWidget::no_shortcut());

    GridLayoutWidget *grid_layout = create<GridLayoutWidget>(_gui_window);
    grid_layout->padding = 0;
    grid_layout->spacing = 0;
    grid_layout->add_widget(menu_widget, 0, 0, HAlignLeft, VAlignTop);
    _gui_window->set_main_widget(grid_layout);
}

GenesisEditor::~GenesisEditor() {
    genesis_destroy_context(_genesis_context);
}

void GenesisEditor::on_close_event(GuiWindow *window) {
    _gui->destroy_window(_gui_window);
}

void GenesisEditor::exec() {
    _gui->exec();
}

void GenesisEditor::edit_file(const char *filename) {
    fprintf(stderr, "edit file %s\n", filename);
}
