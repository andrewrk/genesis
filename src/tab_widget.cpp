#include "tab_widget.hpp"
#include "color.hpp"
#include "gui_window.hpp"
#include "label.hpp"

static void destroy_tab(TabWidgetTab *tab) {
    if (tab) {
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
    show_tab_bar(true)
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

void TabWidget::on_mouse_move(const MouseEvent *event) {
    if (!current_tab)
        return;

    if (show_tab_bar && event->action == MouseActionDown && event->button == MouseButtonLeft) {
        TabWidgetTab *clicked_tab = get_tab_at(event->x, event->y);
        if (clicked_tab) {
            current_index = clicked_tab->index;
            update_model();
            return;
        }
    }

    MouseEvent mouse_event = *event;
    mouse_event.x += left;
    mouse_event.y += top;

    gui_window->try_mouse_move_event_on_widget(current_tab->widget, &mouse_event);
}

void TabWidget::change_current_index(int direction) {
    current_index = euclidean_mod(current_index + direction, tabs.length());
    update_model();
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
    assert(!widget->parent_widget); // widget already has parent
    widget->parent_widget = this;
    TabWidgetTab *tab = create<TabWidgetTab>();
    tab->widget = widget;
    tab->label = create<Label>(gui);
    tab->label->set_text(title);
    tab->label->update();
    if (current_index < 0)
        current_index = 0;
    ok_or_panic(tabs.append(tab));
    update_model();
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
