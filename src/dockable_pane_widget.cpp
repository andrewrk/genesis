#include "dockable_pane_widget.hpp"
#include "gui_window.hpp"
#include "color.hpp"
#include "tab_widget.hpp"
#include "drag_event.hpp"
#include "gui.hpp"
#include "label.hpp"

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
    resize_down = false;
    resize_down_pos = 0;
    auto_hide_tabs = false;
    insert_tab_arrow = false;
    insert_tab_arrow_color = color_attention_overlay();
    show_drop_lines = false;

    for (int i = 0; i < array_length(drop_area_labels); i += 1) {
        drop_area_labels[i] = create<Label>(gui);
    }
    drop_area_labels[0]->set_text("Dock North");
    drop_area_labels[1]->set_text("Dock East");
    drop_area_labels[2]->set_text("Dock South");
    drop_area_labels[3]->set_text("Dock West");
    drop_area_labels[4]->set_text("Add as Tab");
    for (int i = 0; i < array_length(drop_area_labels); i += 1) {
        drop_area_labels[i]->update();
    }
}

DockAreaWidget::~DockAreaWidget() {
    for (int i = 0; i < array_length(drop_area_labels); i += 1) {
        destroy(drop_area_labels[i], 1);
    }
    reset_state();
}

void DockAreaWidget::draw(const glm::mat4 &projection) {
    switch (layout) {
        case DockAreaLayoutTabs:
            if (tab_widget) {
                tab_widget->draw(projection);

                if (insert_tab_arrow) {
                    gui->draw_image_color(gui_window, gui->img_arrow_down,
                            projection * insert_arrow_model, insert_tab_arrow_color);
                }

                if (show_drop_lines) {
                    for (int i = 0; i < array_length(drop_lines); i += 1) {
                        gui_window->fill_rect(insert_tab_arrow_color, projection * drop_lines[i]);
                    }
                    for (int i = 0; i < array_length(drop_area_labels); i += 1) {
                        drop_area_labels[i]->draw(projection * drop_area_label_models[i], insert_tab_arrow_color);
                    }
                    if (drop_area_icon) {
                        gui->draw_image_color(gui_window, drop_area_icon,
                                projection * drop_area_icon_model, insert_tab_arrow_color);
                    }
                }
            }
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

void DockAreaWidget::transfer_state(DockAreaWidget *source, DockAreaWidget *dest) {
    dest->layout = source->layout;
    dest->child_a = source->child_a; source->child_a = nullptr;
    dest->child_b = source->child_b; source->child_b = nullptr;
    dest->split_ratio = source->split_ratio;
    dest->tab_widget = source->tab_widget; source->tab_widget = nullptr;
}

DockAreaWidget * DockAreaWidget::transfer_state_to_new_child() {
    assert((layout == DockAreaLayoutTabs && tab_widget) ||
            (layout != DockAreaLayoutTabs && child_a && child_b));
    DockAreaWidget *child = create<DockAreaWidget>(gui_window);
    transfer_state(this, child);
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

static void on_tab_widget_drag_tab(TabWidgetTab *tab,
        TabWidget *tab_widget, const MouseEvent *event)
{
    DockAreaWidget *dock_area = (DockAreaWidget *) tab_widget->parent_widget;
    DragData *drag_data = ok_mem(create_zero<DragData>());
    drag_data->drag_type = DragTypeViewTab;
    drag_data->ptr = tab->widget;
    drag_data->destruct = nullptr;

    dock_area->gui_window->start_drag(event, drag_data);
}

static void on_tab_widget_drag_event(TabWidget *tab_widget, const DragEvent *event) {
    DockAreaWidget *dock_area = (DockAreaWidget *) tab_widget->parent_widget;
    dock_area->handle_tab_drag(event);
}

void DockAreaWidget::on_drag(const DragEvent *event) {
    switch (layout) {
        case DockAreaLayoutTabs:
            if (tab_widget)
                forward_drag_event(tab_widget, event);
            return;
        case DockAreaLayoutHoriz:
        case DockAreaLayoutVert:
            if (forward_drag_event(child_a, event))
                return;
            if (forward_drag_event(child_b, event))
                return;
            return;
    }
}

void DockAreaWidget::clear_drag_ui() {
    insert_tab_arrow = false;
    show_drop_lines = false;
}

void DockAreaWidget::handle_tab_drag(const DragEvent *event) {
    static const int insert_arrow_width = 16;
    static const int insert_arrow_height = 16;
    static const int drop_icon_width = 16;
    static const int drop_icon_height = 16;

    clear_drag_ui();

    if (event->action == DragActionOut)
        return;

    if (layout != DockAreaLayoutTabs)
        return;
    if (event->drag_data->drag_type != DragTypeViewTab) 
        return;
    if (!tab_widget)
        return;

    int insert_index = tab_widget->get_insert_index_at(event->mouse_event.x, event->mouse_event.y);
    if (insert_index >= 0) {
        if (event->action == DragActionMove) {
            insert_tab_arrow = true;
            int tab_x, tab_y;
            tab_widget->get_tab_pos(insert_index, &tab_x, &tab_y);
            int arrow_icon_left = tab_x - insert_arrow_width / 2;
            int arrow_icon_top = tab_y - insert_arrow_height;
            float scale_x = insert_arrow_width / (float)gui->img_arrow_down->width;
            float scale_y = insert_arrow_height / (float)gui->img_arrow_down->height;
            insert_arrow_model = transform2d(arrow_icon_left, arrow_icon_top, scale_x, scale_y);
        } else if (event->action == DragActionDrop) {
            DockablePaneWidget *pane = (DockablePaneWidget *)event->drag_data->ptr;
            TabWidget *pane_parent = (TabWidget *)pane->parent_widget;
            int source_index = pane_parent->get_widget_index(pane);

            GuiWindow *local_gui_window = gui_window;
            if (pane_parent == tab_widget) {
                tab_widget->move_tab(source_index, insert_index);
            } else {
                pane_parent->remove_tab(source_index);
                tab_widget->insert_tab(pane, pane->title, insert_index);
                tab_widget->select_index(insert_index);
                if (pane_parent->tab_count() == 0) {
                    DockAreaWidget *dock_area = (DockAreaWidget *)pane_parent->parent_widget;
                    dock_area->collapse();
                }
            }
            local_gui_window->events.trigger(EventPerspectiveChange);
            return; // function must return now since dock_area->collapse() might have destroyed us
        }
    } else {
        show_drop_lines = true;

        int available_width = tab_widget->width;
        int available_height = tab_widget->height - tab_widget->widget_top;

        center_drop_width = available_width / 3;
        center_drop_height = available_height / 3;
        center_drop_left = available_width / 2 - center_drop_width / 2;
        center_drop_top = tab_widget->widget_top + available_height / 2 - center_drop_height / 2;
        center_drop_right = center_drop_left + center_drop_width;
        center_drop_bottom = center_drop_top + center_drop_height;

        drop_lines[0] = transform2d(center_drop_left, tab_widget->widget_top, 1, available_height);
        drop_lines[1] = transform2d(center_drop_right, tab_widget->widget_top, 1, available_height);
        drop_lines[2] = transform2d(0, center_drop_top, available_width, 1);
        drop_lines[3] = tab_widget->transform2d(0, center_drop_bottom, available_width, 1);

        drop_area_label_models[0] = transform2d(
                center_drop_left + center_drop_width / 2 - drop_area_labels[0]->width() / 2,
                tab_widget->widget_top + (center_drop_top - tab_widget->widget_top) / 2
                    - drop_area_labels[0]->height() / 2);
        drop_area_label_models[1] = transform2d(
                center_drop_right + (available_width - center_drop_right) / 2 - drop_area_labels[1]->width() / 2,
                center_drop_top + center_drop_height / 2 - drop_area_labels[1]->height() / 2);
        drop_area_label_models[2] = transform2d(
                center_drop_left + center_drop_width / 2 - drop_area_labels[2]->width() / 2,
                center_drop_bottom + (height - center_drop_bottom) / 2 - drop_area_labels[2]->height() / 2);
        drop_area_label_models[3] = transform2d(
                center_drop_left / 2 - drop_area_labels[3]->width() / 2,
                center_drop_top + center_drop_height / 2 - drop_area_labels[3]->height() / 2);
        drop_area_label_models[4] = transform2d(
                center_drop_left + center_drop_width / 2 - drop_area_labels[4]->width() / 2,
                center_drop_top + center_drop_height / 2 - drop_area_labels[4]->height() / 2);

        bool left = event->mouse_event.x < center_drop_left;
        bool right = event->mouse_event.x > center_drop_right;
        bool top = event->mouse_event.y < center_drop_top;
        bool bottom = event->mouse_event.y > center_drop_bottom;

        drop_area_icon = nullptr;
        int icon_left = center_drop_left + center_drop_width / 2 - drop_icon_width / 2;
        int icon_top = center_drop_top + center_drop_height / 2 - drop_icon_height / 2 + drop_area_labels[4]->height();
        if (top && !left && !right) {
            drop_area_icon = gui->img_arrow_up;
            icon_top -= center_drop_height;
        } else if (right && !top && !bottom) {
            drop_area_icon = gui->img_arrow_right;
            icon_left += center_drop_width;
        } else if (bottom && !left && !right) {
            drop_area_icon = gui->img_arrow_down;
            icon_top += center_drop_height;
        } else if (left && !top && !bottom) {
            drop_area_icon = gui->img_arrow_left;
            icon_left -= center_drop_width;
        } else if (!left && !right && !top && !bottom) {
            drop_area_icon = gui->img_entry_dir;
        }
        if (drop_area_icon) {
            float scale_x = drop_icon_width / (float)drop_area_icon->width;
            float scale_y = drop_icon_height / (float)drop_area_icon->height;
            drop_area_icon_model = transform2d(icon_left, icon_top, scale_x, scale_y);
        }

        if (event->action == DragActionDrop) {
            if (!drop_area_icon)
                return;

            GuiWindow *local_gui_window = gui_window;
            DockablePaneWidget *pane = (DockablePaneWidget *)event->drag_data->ptr;
            TabWidget *pane_parent = (TabWidget *)pane->parent_widget;
            int source_index = pane_parent->get_widget_index(pane);

            if (drop_area_icon == gui->img_entry_dir) {
                if (pane_parent == tab_widget) {
                    tab_widget->move_tab(source_index, tab_widget->tab_count());
                } else {
                    pane_parent->remove_tab(source_index);
                    tab_widget->insert_tab(pane, pane->title, tab_widget->tab_count());
                    tab_widget->select_index(tab_widget->tab_count() - 1);
                    if (pane_parent->tab_count() == 0) {
                        DockAreaWidget *dock_area = (DockAreaWidget *)pane_parent->parent_widget;
                        dock_area->collapse();
                    }
                }
                local_gui_window->events.trigger(EventPerspectiveChange);
                return;
            }

            pane_parent->remove_tab(source_index);

            if (drop_area_icon == gui->img_arrow_up) {
                add_top_pane(pane);
            } else if (drop_area_icon == gui->img_arrow_right) {
                add_right_pane(pane);
            } else if (drop_area_icon == gui->img_arrow_down) {
                add_bottom_pane(pane);
            } else if (drop_area_icon == gui->img_arrow_left) {
                add_left_pane(pane);
            }

            if (pane_parent->tab_count() == 0) {
                DockAreaWidget *pane_parent_dock_area = (DockAreaWidget *)pane_parent->parent_widget;
                pane_parent_dock_area->collapse();
            }
            local_gui_window->events.trigger(EventPerspectiveChange);
        }
    }
}

void DockAreaWidget::add_tab_widget(DockablePaneWidget *pane) {
    assert(!pane->parent_widget);
    assert(layout == DockAreaLayoutTabs);
    if (!tab_widget) {
        tab_widget = create<TabWidget>(gui_window);
        tab_widget->parent_widget = this;
        tab_widget->set_auto_hide(auto_hide_tabs);
        tab_widget->on_drag_tab = on_tab_widget_drag_tab;
        tab_widget->on_drag_event = on_tab_widget_drag_event;
    }
    tab_widget->add_widget(pane, pane->title);
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

void DockAreaWidget::add_tab_pane(DockablePaneWidget *pane) {
    assert(layout == DockAreaLayoutTabs);
    add_tab_widget(pane);
}

void DockAreaWidget::on_mouse_move(const MouseEvent *event) {
    MouseEvent mouse_event = *event;
    mouse_event.x += left;
    mouse_event.y += top;

    if (resize_down) {
        if (event->action == MouseActionUp && event->button == MouseButtonLeft) {
            resize_down = false;
            gui_window->events.trigger(EventPerspectiveChange);
            return;
        }
        if (layout == DockAreaLayoutHoriz) {
            int delta = event->x - resize_down_pos;
            int available_width = width - split_area_size;
            split_ratio = clamp(0.0f, (resize_down_ratio * available_width + delta) / (float)available_width, 1.0f);
            update_model();
        } else if (layout == DockAreaLayoutVert) {
            int delta = event->y - resize_down_pos;
            int available_height = height - split_area_size;
            split_ratio = clamp(0.0f, (resize_down_ratio * available_height + delta) / (float)available_height, 1.0f);
            update_model();
        }
    }

    bool mouse_over_resize = false;
    if (layout == DockAreaLayoutHoriz) {
        int start = width * split_ratio - split_area_size / 2;
        int end = start + split_area_size;
        if (event->x >= start && event->x < end) {
            gui_window->set_cursor_hresize();
            mouse_over_resize = true;
            if (event->action == MouseActionDown && event->button == MouseButtonLeft) {
                resize_down = true;
                resize_down_pos = event->x;
                resize_down_ratio = split_ratio;
            }
        }
    } else if (layout == DockAreaLayoutVert) {
        int start = height * split_ratio - split_area_size / 2;
        int end = start + split_area_size;
        if (event->y >= start && event->y < end) {
            gui_window->set_cursor_vresize();
            mouse_over_resize = true;
            if (event->action == MouseActionDown && event->button == MouseButtonLeft) {
                resize_down = true;
                resize_down_pos = event->y;
                resize_down_ratio = split_ratio;
            }
        }
    }
    if (mouse_over_resize) {
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
                split_border_end_model = transform2d(child_b->left - left - 1, 0, 1, height);
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
                split_border_end_model = transform2d(0, child_b->top - top - 1, width, 1);
                break;
            }
    }
}

void DockAreaWidget::reset_state() {
    switch (layout) {
    case DockAreaLayoutTabs:
        if (tab_widget) {
            tab_widget->parent_widget = nullptr;
            destroy(tab_widget, 1);
            tab_widget = nullptr;
        }
        assert(!child_a);
        assert(!child_b);
        break;
    case DockAreaLayoutHoriz:
    case DockAreaLayoutVert:
        assert(!tab_widget);
        destroy(child_a, 1);
        destroy(child_b, 1);
        child_a = nullptr;
        child_b = nullptr;
        break;
    }
    layout = DockAreaLayoutTabs;
    split_ratio = 0.50f;
}

void DockAreaWidget::set_auto_hide_tabs(bool value) {
    switch (layout) {
    case DockAreaLayoutTabs:
        if (tab_widget)
            tab_widget->set_auto_hide(value);
        break;
    case DockAreaLayoutHoriz:
    case DockAreaLayoutVert:
        assert(child_a);
        assert(child_b);

        child_a->set_auto_hide_tabs(value);
        child_b->set_auto_hide_tabs(value);
        break;
    }
}

void DockAreaWidget::collapse() {
    reset_state();
    DockAreaWidget *parent = (DockAreaWidget *)parent_widget;
    assert(parent);
    assert(parent->layout != DockAreaLayoutTabs);
    if (parent->child_a == this) {
        parent->remove_a();
    } else {
        assert(parent->child_b == this);
        parent->remove_b();
    }
}

void DockAreaWidget::remove_a() {
    assert(child_a->layout == DockAreaLayoutTabs);
    assert(child_a->tab_widget == nullptr);
    assert(tab_widget == nullptr);

    DockAreaWidget *old_child_a = child_a;
    DockAreaWidget *old_child_b = child_b;
    transfer_state(child_b, this);
    if (tab_widget)
        tab_widget->parent_widget = this;
    if (child_a)
        child_a->parent_widget = this;
    if (child_b)
        child_b->parent_widget = this;

    old_child_a->parent_widget = nullptr;
    old_child_b->parent_widget = nullptr;
    destroy(old_child_a, 1);
    destroy(old_child_b, 1);

    update_model();
}

void DockAreaWidget::remove_b() {
    assert(child_b->layout == DockAreaLayoutTabs);
    assert(child_b->tab_widget == nullptr);
    assert(tab_widget == nullptr);

    DockAreaWidget *old_child_a = child_a;
    DockAreaWidget *old_child_b = child_b;
    transfer_state(child_a, this);
    if (tab_widget)
        tab_widget->parent_widget = this;
    if (child_a)
        child_a->parent_widget = this;
    if (child_b)
        child_b->parent_widget = this;

    old_child_a->parent_widget = nullptr;
    old_child_b->parent_widget = nullptr;

    destroy(old_child_a, 1);
    destroy(old_child_b, 1);


    update_model();
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

void DockablePaneWidget::on_drag(const DragEvent *event) {
    forward_drag_event(child, event);
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

