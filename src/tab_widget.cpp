#include "tab_widget.hpp"
#include "color.hpp"
#include "gui_window.hpp"
#include "label.hpp"

static void destroy_tab(TabWidgetTab *tab) {
    if (tab) {
        if (tab->widget)
            tab->widget->parent_widget = nullptr;
        destroy(tab->label, 1);
        destroy(tab, 1);
    }
}

TabWidget::TabWidget(GuiWindow *window) :
    Widget(window),
    current_index(-1),
    current_tab(nullptr),
    tab_border_color(color_light_border()),
    tab_height(20),
    bg_color(color_light_bg()),
    tab_bg_color(color_dark_bg_inactive()),
    tab_text_color(color_fg_text()),
    tab_selected_bg_color(color_dark_bg()),
    padding_left(2),
    padding_right(2),
    padding_top(2),
    padding_bottom(0),
    tab_spacing(4),
    title_padding_left(6),
    title_padding_right(6),
    widget_top(0),
    auto_hide(false),
    show_tab_bar(true),
    on_drag_tab(nullptr)
{
}

TabWidget::~TabWidget() {
    for (int i = 0; i < tabs.length(); i += 1) {
        TabWidgetTab *tab = tabs.at(i);
        destroy_tab(tab);
    }
}

void TabWidget::draw(const glm::mat4 &projection) {
    if (!current_tab)
        return;

    if (show_tab_bar) {
        gui_window->fill_rect(bg_color, projection * tab_bg_model);
        gui_window->fill_rect(tab_border_color, projection * tab_bottom_line_model);

        for (int i = 0; i < tabs.length(); i += 1) {
            TabWidgetTab *tab = tabs.at(i);
            glm::vec4 this_bg_color = tab->is_current ? tab_selected_bg_color : tab_bg_color;
            gui_window->fill_rect(this_bg_color, projection * tab->bg_model);
            gui_window->fill_rect(tab_border_color, projection * tab->left_line_model);
            gui_window->fill_rect(tab_border_color, projection * tab->right_line_model);
            gui_window->fill_rect(tab_border_color, projection * tab->top_line_model);
            tab->label->draw(projection * tab->label_model, tab_text_color);
        }
    }

    current_tab->widget->draw(projection);
};

void TabWidget::get_tab_pos(int index, int *x, int *y) {
    if (index <= 0) {
        TabWidgetTab *tab = tabs.at(0);
        *x = tab->left;
    } else if (index >= tabs.length()) {
        TabWidgetTab *tab = tabs.last();
        *x = tab->right;
    } else {
        TabWidgetTab *prev_tab = tabs.at(index - 1);
        TabWidgetTab *tab = tabs.at(index);
        *x = prev_tab->right + (tab->left - prev_tab->right) / 2;
    }
    *y = widget_top;
}

int TabWidget::get_insert_index_at(int x, int y) {
    if (y < 0 || y >= widget_top)
        return -1;

    if (x < 0)
        return 0;

    for (int i = 0; i < tabs.length(); i += 1) {
        TabWidgetTab *tab = tabs.at(i);
        if (x < tab->left + (tab->right - tab->left) / 2)
            return i;
    }

    return tabs.length();
}

TabWidgetTab *TabWidget::get_tab_at(int x, int y) {
    if (y < 0 || y >= widget_top)
        return nullptr;

    for (int i = 0; i < tabs.length(); i += 1) {
        TabWidgetTab *tab = tabs.at(i);
        if (x >= tab->left && x < tab->right)
            return tab;
    }

    return nullptr;
}

void TabWidget::select_index(int index) {
    assert(index >= 0);
    assert(index < tabs.length());
    current_index = index;
    update_model();
}

void TabWidget::on_mouse_move(const MouseEvent *event) {
    assert(current_tab);

    if (show_tab_bar && event->action == MouseActionDown && event->button == MouseButtonLeft) {
        TabWidgetTab *clicked_tab = get_tab_at(event->x, event->y);
        if (clicked_tab) {
            select_index(clicked_tab->index);
            if (on_drag_tab) {
                MouseEvent mouse_event = *event;
                mouse_event.x += left;
                mouse_event.y += top;
                on_drag_tab(clicked_tab, this, &mouse_event);
            }
            return;
        }
    }

    MouseEvent mouse_event = *event;
    mouse_event.x += left;
    mouse_event.y += top;

    gui_window->try_mouse_move_event_on_widget(current_tab->widget, &mouse_event);
}

void TabWidget::on_drag(const DragEvent *event) {
    assert(current_tab);

    if (on_drag_event) {
        on_drag_event(this, event);
    } else if (event->mouse_event.y >= widget_top) {
        forward_drag_event(current_tab->widget, event);
    }
}

void TabWidget::change_current_index(int direction) {
    select_index(euclidean_mod(current_index + direction, tabs.length()));
}

bool TabWidget::on_key_event(const KeyEvent *event) {
    if (event->action == KeyActionDown) {
        if (key_sequence_match(ctrl_shortcut(VirtKeyPageUp), event)) {
            change_current_index(-1);
            return true;
        } else if (key_sequence_match(ctrl_shortcut(VirtKeyPageDown), event)) {
            change_current_index(1);
            return true;
        }
    }
    return false;
}

void TabWidget::add_widget(Widget *widget, const String &title) {
    insert_tab(widget, title, tabs.length());
}

void TabWidget::update_model() {
    show_tab_bar = !auto_hide || tabs.length() >= 2;
    if (show_tab_bar) {
        widget_top = padding_top + tab_height;
    } else {
        widget_top = padding_top;
    }

    tab_bg_model = transform2d(0, 0, width, height);
    tab_bottom_line_model = transform2d(0, widget_top - 1, width, 1);

    int next_left = padding_left;
    for (int i = 0; i < tabs.length(); i += 1) {
        TabWidgetTab *tab = tabs.at(i);
        tab->index = i;
        tab->is_current = (i == current_index);
        tab->left = next_left;
        tab->right = tab->left + title_padding_left + tab->label->width() + title_padding_right;
        next_left = tab->right + tab_spacing;

        int tab_top = padding_top;
        tab->label_model = transform2d(
                tab->left + title_padding_left, tab_top + tab_height / 2 - tab->label->height() / 2);
        tab->left_line_model = transform2d(tab->left, tab_top, 1, tab_height);
        tab->right_line_model = transform2d(tab->right - 1, tab_top, 1, tab_height);
        tab->top_line_model = transform2d(tab->left, tab_top, tab->right - tab->left, 1);

        int extra_height = tab->is_current ? 0 : -1;
        tab->bg_model = transform2d(tab->left, tab_top, tab->right - tab->left, tab_height + extra_height);
    }

    if (tabs.length() == 0) {
        current_tab = nullptr;
    } else {
        current_tab = tabs.at(current_index);
        current_tab->widget->left = left;
        current_tab->widget->top = top + widget_top;
        current_tab->widget->width = width;
        current_tab->widget->height = height - widget_top;
        current_tab->widget->on_resize();
    }
}

void TabWidget::select_widget(Widget *widget) {
    int index = get_widget_index(widget);
    assert(index >= 0);
    select_index(index);
}

int TabWidget::get_widget_index(Widget *widget) {
    for (int i = 0; i < tabs.length(); i += 1) {
        TabWidgetTab *tab = tabs.at(i);
        if (tab->widget == widget)
            return i;
    }
    return -1;
}

void TabWidget::move_tab(int source_index, int dest_index) {
    assert(dest_index >= 0);
    assert(dest_index <= tabs.length());

    TabWidgetTab *target_tab = tabs.at(source_index);

    tabs.remove_range(source_index, source_index + 1);
    if (source_index < dest_index)
        dest_index -= 1;

    ok_or_panic(tabs.insert_space(dest_index, 1));

    tabs.at(dest_index) = target_tab;
    current_index = dest_index;
    clamp_current_index();
}

void TabWidget::remove_tab(int index) {
    TabWidgetTab *tab = tabs.at(index);
    tabs.remove_range(index, index + 1);
    tab->widget->parent_widget = nullptr;
    destroy(tab->label, 1);
    destroy(tab, 1);
    tab = nullptr;
    clamp_current_index();
}

void TabWidget::insert_tab(Widget *widget, const String &title, int dest_index) {
    assert(!widget->parent_widget); // widget already has parent
    widget->parent_widget = this;

    TabWidgetTab *tab = create<TabWidgetTab>();
    tab->widget = widget;
    tab->label = create<Label>(gui);
    tab->label->set_text(title);
    tab->label->update();

    ok_or_panic(tabs.insert_space(dest_index, 1));
    tabs.at(dest_index) = tab;
    clamp_current_index();
}

void TabWidget::clamp_current_index() {
    current_index = (tabs.length() == 0) ? -1 : clamp(0, current_index, tabs.length() - 1);
    update_model();
}
