#include "dockable_pane_widget.hpp"
#include "gui_window.hpp"

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
            assert(child_a);
            assert(child_b);

            child_a->left = left;
            child_a->top = top;
            child_a->width = width * split_ratio;
            child_a->height = height;

            child_b->left = child_a->left + child_a->width;
            child_b->top = top;
            child_b->width = width - child_a->width;
            child_b->height = height;

            child_a->on_resize();
            child_b->on_resize();
            break;
        case DockAreaLayoutVert:
            assert(child_a);
            assert(child_b);

            child_a->left = left;
            child_a->top = top;
            child_a->width = width;
            child_a->height = height * split_ratio;

            child_b->left = left;
            child_b->top = child_a->top + child_a->height;
            child_b->width = width;
            child_b->height = height - child_a->height;

            child_a->on_resize();
            child_b->on_resize();
            break;
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
