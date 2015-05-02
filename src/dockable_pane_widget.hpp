#ifndef DOCKABLE_PANE_WIDGET
#define DOCKABLE_PANE_WIDGET

#include "widget.hpp"
#include "tab_widget.hpp"

#include <assert.h>

class DockablePaneWidget;

struct DockablePaneTab {
    DockablePaneWidget *pane;
};

enum DockAreaLayout {
    DockAreaLayoutTabs,
    DockAreaLayoutHoriz,
    DockAreaLayoutVert,
};

class DockAreaWidget : public Widget {
public:
    DockAreaWidget(GuiWindow *window) : Widget(window) {
        layout = DockAreaLayoutTabs;
        child_a = nullptr;
        child_b = nullptr;
        tab_widget = nullptr;
        split_ratio = 0.50f;
    }
    ~DockAreaWidget() override { }

    void draw(const glm::mat4 &projection) override;
    void on_mouse_move(const MouseEvent *event) override;
    void on_resize() override { update_model(); }

    void add_left_pane(DockablePaneWidget *pane);
    void add_right_pane(DockablePaneWidget *pane);
    void add_top_pane(DockablePaneWidget *pane);
    void add_bottom_pane(DockablePaneWidget *pane);

    void add_left_dock_area(DockAreaWidget *dock_area);
    void add_right_dock_area(DockAreaWidget *dock_area);
    void add_top_dock_area(DockAreaWidget *dock_area);
    void add_bottom_dock_area(DockAreaWidget *dock_area);

    DockAreaLayout layout;
    DockAreaWidget *child_a; // left/top
    DockAreaWidget *child_b; // right/bottom
    TabWidget *tab_widget; // if layout == DockAreaLayoutTabs
    float split_ratio;

    DockAreaWidget * transfer_state_to_new_child();
    void add_tab_widget(DockablePaneWidget *pane);
    DockAreaWidget *create_dock_area_for_pane(DockablePaneWidget *pane);
    void update_model();
};

class DockablePaneWidget : public Widget {
public:
    DockablePaneWidget(Widget *child, const String &title);
    ~DockablePaneWidget() override {}

    void draw(const glm::mat4 &projection) override;
    void on_mouse_move(const MouseEvent *event) override;
    void on_resize() override;

    Widget *child;
    String title;
};

#endif
