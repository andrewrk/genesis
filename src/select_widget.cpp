#include "select_widget.hpp"
#include "gui_window.hpp"
#include "gui.hpp"
#include "color.hpp"
#include "menu_widget.hpp"

static void on_select_choice(void *userdata) {
    SelectWidgetItem *item = (SelectWidgetItem *)userdata;
    SelectWidget *select_widget = item->parent;
    select_widget->selected_index = item->index;
    select_widget->events.trigger(EventSelectedIndexChanged);
}

static void on_context_menu_destroy(ContextMenuWidget *context_menu_widget) {
    SelectWidget *select_widget = (SelectWidget *)context_menu_widget->userdata;

    select_widget->context_menu_open = false;
}

SelectWidget::~SelectWidget() {
    destroy(context_menu, 1);
}

SelectWidget::SelectWidget(GuiWindow *gui_window) :
    Widget(gui_window),
    label(gui_window->gui)
{
    arrow_icon_img = gui->img_caret_down;
    padding_left = 4;
    padding_right = 4;
    padding_top = 4;
    padding_bottom = 4;
    icon_spacing = 4;
    hovering = false;
    text_color = color_fg_text();
    selected_index = -1;

    context_menu = create<MenuWidgetItem>(gui_window);
    context_menu_open = false;

    update_model();
}

void SelectWidget::draw(const glm::mat4 &projection) {
    bg.draw(gui_window, projection);
    if (selected_index >= 0)
        label.draw(projection * label_model, text_color);

    gui->draw_image_color(gui_window, arrow_icon_img, projection * arrow_icon_model, text_color);
}

void SelectWidget::update_model() {
    bg.set_scheme(SunkenBoxSchemeRaisedBorders);
    bg.update(this, 0, 0, width, height);

    label_model = transform2d(padding_left, padding_top);

    float icon_h = label.height();
    float icon_w = icon_h;
    float scale_x = ((float)icon_w) / ((float)arrow_icon_img->width);
    float scale_y = ((float)icon_h) / ((float)arrow_icon_img->height);
    arrow_icon_model = transform2d(width - padding_right - icon_w - icon_spacing, padding_top, scale_x, scale_y);
}

void SelectWidget::clear() {
    items.clear();
    context_menu->clear();
    clear_context_menu();
}

void SelectWidget::append_choice(const String &choice) {
    ok_or_panic(items.add_one());
    SelectWidgetItem *item = &items.last();
    item->parent = this;
    item->name = choice;
    item->menu_item = context_menu->add_menu(choice, -1, no_shortcut());
    set_activate_handlers();
    clear_context_menu();
}


void SelectWidget::set_activate_handlers() {
    for (int i = 0; i < items.length(); i += 1) {
        SelectWidgetItem *item = &items.at(i);
        item->index = i;
        item->menu_item->set_activate_handler(on_select_choice, item);
    }
}

void SelectWidget::clear_context_menu() {
    if (context_menu_open) {
        gui_window->destroy_context_menu();
    }
}

void SelectWidget::select_index(int index) {
    selected_index = index;
    if (selected_index >= 0) {
        SelectWidgetItem *item = &items.at(selected_index);
        label.set_text(item->name);
        label.update();
        update_model();
    }
}

void SelectWidget::on_mouse_move(const MouseEvent *event) {
    switch (event->action) {
        case MouseActionMove:
            break;
        case MouseActionDown:
            {
                context_menu_open = true;
                ContextMenuWidget *context_menu_widget = pop_context_menu(context_menu, 0, 0, width, height);
                context_menu_widget->userdata = this;
                context_menu_widget->on_destroy = on_context_menu_destroy;
                break;
            }
        case MouseActionUp:
            break;
    }
}

void SelectWidget::on_mouse_out(const MouseEvent *event) {
    hovering = false;
}

void SelectWidget::on_mouse_over(const MouseEvent *event) {
    hovering = true;
}

int SelectWidget::min_height() const {
    return padding_top + label.height() + padding_bottom;
}

int SelectWidget::max_height() const {
    return min_height();
}

int SelectWidget::min_width() const {
    return padding_left + label.width() + icon_spacing + label.height() + padding_right;
}
