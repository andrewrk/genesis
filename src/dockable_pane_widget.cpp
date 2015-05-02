#include "dockable_pane_widget.hpp"
#include "gui_window.hpp"
#include "color.hpp"
#include "tab_widget.hpp"

DockAreaWidget::DockAreaWidget(GuiWindow *window) :
    Widget(window)
{
    layout = DockAreaLayoutTabs;
    child_a = nullptr;
    child_b = nullptr;
    tab_widget = nullptr;
    split_ratio = 0.50f;
    split_area_size = 4;
    light_border_color = color_light_border();
    dark_border_color = color_dark_border();
}

void DockAreaWidget::draw(const glm::mat4 &projection) {
    switch (layout) {
        case DockAreaLayoutTabs:
            if (tab_widget)
                tab_widget->draw(projection);
            break;
        case DockAreaLayoutHoriz:
        case DockAreaLayoutVert:
            assert(child_a);
            assert(child_b);
            child_a->draw(projection);
            child_b->draw(projection);
            gui_window->fill_rect(light_border_color, projection * split_border_start_model);
            gui_window->fill_rect(dark_border_color, projection * split_border_end_model);
            break;
    }
}

DockAreaWidget * DockAreaWidget::transfer_state_to_new_child() {
    assert((layout == DockAreaLayoutTabs && tab_widget) ||
            (layout != DockAreaLayoutTabs && child_a && child_b));
    DockAreaWidget *child = create<DockAreaWidget>(gui_window);
    child->layout = layout;
    child->child_a = child_a;
    child->child_b = child_b;
    child->tab_widget = tab_widget;
    child->parent_widget = this;
    if (child->child_a)
        child->child_a->parent_widget = child;
    if (child->child_b)
        child->child_b->parent_widget = child;
    if (child->tab_widget)
        child->tab_widget->parent_widget = child;
    child->update_model();
    return child;
}

void DockAreaWidget::add_tab_widget(DockablePaneWidget *pane) {
    assert(!pane->parent_widget);
    assert(layout == DockAreaLayoutTabs);
    tab_widget = create<TabWidget>(gui_window);
    tab_widget->parent_widget = this;
    tab_widget->add_widget(pane, pane->title);
    tab_widget->set_auto_hide(true);
}

DockAreaWidget *DockAreaWidget::create_dock_area_for_pane(DockablePaneWidget *pane) {
    DockAreaWidget *dock_area = create<DockAreaWidget>(gui_window);
    dock_area->layout = DockAreaLayoutTabs;
    dock_area->add_tab_widget(pane);
    return dock_area;
}

void DockAreaWidget::add_left_dock_area(DockAreaWidget *dock_area) {
    assert(!dock_area->parent_widget);
    dock_area->parent_widget = this;

    DockAreaWidget *child = transfer_state_to_new_child();
    layout = DockAreaLayoutHoriz;
    child_a = dock_area;
    child_b = child;
    tab_widget = nullptr;
    update_model();
}

void DockAreaWidget::add_right_dock_area(DockAreaWidget *dock_area) {
    assert(!dock_area->parent_widget);
    dock_area->parent_widget = this;

    DockAreaWidget *child = transfer_state_to_new_child();
    layout = DockAreaLayoutHoriz;
    child_a = child;
    child_b = dock_area;
    tab_widget = nullptr;
    update_model();
}

void DockAreaWidget::add_top_dock_area(DockAreaWidget *dock_area) {
    assert(!dock_area->parent_widget);
    dock_area->parent_widget = this;

    DockAreaWidget *child = transfer_state_to_new_child();
    layout = DockAreaLayoutVert;
    child_a = dock_area;
    child_b = child;
    tab_widget = nullptr;
    update_model();
}

void DockAreaWidget::add_bottom_dock_area(DockAreaWidget *dock_area) {
    assert(!dock_area->parent_widget);
    dock_area->parent_widget = this;

    DockAreaWidget *child = transfer_state_to_new_child();
    layout = DockAreaLayoutVert;
    child_a = child;
    child_b = dock_area;
    tab_widget = nullptr;
    update_model();
}

void DockAreaWidget::add_left_pane(DockablePaneWidget *pane) {
    if (layout == DockAreaLayoutTabs && !tab_widget) {
        add_tab_widget(pane);
        return;
    }
    add_left_dock_area(create_dock_area_for_pane(pane));
}

void DockAreaWidget::add_right_pane(DockablePaneWidget *pane) {
    if (layout == DockAreaLayoutTabs && !tab_widget) {
        add_tab_widget(pane);
        return;
    }
    add_right_dock_area(create_dock_area_for_pane(pane));
}

void DockAreaWidget::add_top_pane(DockablePaneWidget *pane) {
    if (layout == DockAreaLayoutTabs && !tab_widget) {
        add_tab_widget(pane);
        return;
    }
    add_top_dock_area(create_dock_area_for_pane(pane));
}

void DockAreaWidget::add_bottom_pane(DockablePaneWidget *pane) {
    if (layout == DockAreaLayoutTabs && !tab_widget) {
        add_tab_widget(pane);
        return;
    }
    add_bottom_dock_area(create_dock_area_for_pane(pane));
}

void DockAreaWidget::on_mouse_move(const MouseEvent *event) {
    MouseEvent mouse_event = *event;
    mouse_event.x += left;
    mouse_event.y += top;

    bool mouse_over_resize = false;
    if (layout == DockAreaLayoutHoriz) {
        int start = width * split_ratio - split_area_size / 2;
        int end = start + split_area_size;
        if (event->x >= start && event->x < end) {
            gui_window->set_cursor_hresize();
            mouse_over_resize = true;
        }
    } else if (layout == DockAreaLayoutVert) {
        int start = height * split_ratio - split_area_size / 2;
        int end = start + split_area_size;
        if (event->y >= start && event->y < end) {
            gui_window->set_cursor_vresize();
            mouse_over_resize = true;
        }
    }
    if (mouse_over_resize) {
        // TODO
        return;
    } else {
        gui_window->set_cursor_default();
    }

    if (tab_widget && gui_window->try_mouse_move_event_on_widget(tab_widget, &mouse_event))
        return;

    if (child_a && gui_window->try_mouse_move_event_on_widget(child_a, &mouse_event))
        return;

    if (child_b && gui_window->try_mouse_move_event_on_widget(child_b, &mouse_event))
        return;
}

void DockAreaWidget::update_model() {
    switch (layout) {
        case DockAreaLayoutTabs:
            if (tab_widget) {
                tab_widget->left = left;
                tab_widget->top = top;
                tab_widget->width = width;
                tab_widget->height = height;
                tab_widget->on_resize();
            }
            break;
        case DockAreaLayoutHoriz:
            {
                assert(child_a);
                assert(child_b);

                int available_width = width - split_area_size;

                child_a->left = left;
                child_a->top = top;
                child_a->width = available_width * split_ratio;
                child_a->height = height;

                child_b->left = child_a->left + child_a->width + split_area_size;
                child_b->top = top;
                child_b->width = available_width - child_a->width;
                child_b->height = height;

                child_a->on_resize();
                child_b->on_resize();

                split_border_start_model = transform2d(child_a->width, 0, 1, height);
                split_border_end_model = transform2d(child_b->left - 1, 0, 1, height);
                break;
            }
        case DockAreaLayoutVert:
            {
                assert(child_a);
                assert(child_b);

                int available_height = height - split_area_size;

                child_a->left = left;
                child_a->top = top;
                child_a->width = width;
                child_a->height = available_height * split_ratio;

                child_b->left = left;
                child_b->top = child_a->top + child_a->height + split_area_size;
                child_b->width = width;
                child_b->height = available_height - child_a->height;

                child_a->on_resize();
                child_b->on_resize();

                split_border_start_model = transform2d(0, child_a->height, width, 1);
                split_border_end_model = transform2d(0, child_b->top - 1, width, 1);
                break;
            }
    }
}

DockablePaneWidget::DockablePaneWidget(Widget *child, const String &title) :
    Widget(child->gui_window),
    child(child),
    title(title)
{
    assert(!child->parent_widget);
    child->parent_widget = this;
}

void DockablePaneWidget::draw(const glm::mat4 &projection) {
    child->draw(projection);
}

void DockablePaneWidget::on_mouse_move(const MouseEvent *event) {
    MouseEvent mouse_event = *event;
    mouse_event.x += left;
    mouse_event.y += top;

    gui_window->try_mouse_move_event_on_widget(child, &mouse_event);
}

void DockablePaneWidget::on_resize() {
    child->left = left;
    child->top = top;
    child->width = width;
    child->height = height;
    child->on_resize();
}
