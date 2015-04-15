#ifndef VERTICAL_LAYOUT_HPP
#define VERTICAL_LAYOUT_HPP

#include "widget.hpp"

class VerticalLayoutWidget : public Widget {
public:
    VerticalLayoutWidget(GuiWindow *gui_window, int margin, int padding) :
        Widget(gui_window),
        margin(margin),
        padding(padding)
    {
    }
    ~VerticalLayoutWidget() override { }

    void draw(const glm::mat4 &projection) override {
        for (int i = 0; i < widgets.length(); i += 1) {
            Widget *widget = widgets.at(i);
            if (widget->is_visible)
                widget->draw(projection);
        }
    }

    void add_widget(Widget *widget) {
        if (widgets.append(widget))
            panic("out of memory");
    }

    void remove_widget(Widget *widget) override {
        if (widget->set_index == -1)
            return;

        int index = widget->set_index;
        widget->set_index = -1;

        widgets.swap_remove(index);
        if (index < widgets.length())
            widgets.at(index)->set_index = index;
    }

    void on_resize() override {
        layout_x();
        layout_y();
    }

    void layout_x() {
        int leftover_width = width;
        for (int i = 0; i < widgets.length(); i += 1) {
            Widget *widget = widgets.at(i);
            int widget_min_width = widget->min_width();
            if (widget_min_width >= 0) {
                int margin_value = margin * (i != widgets.length() - 1);
                leftover_width -= widget_min_width - margin_value;
            }
        }
        if (leftover_width <= 0) {
            int next_x = left;
            // everybody gets their minimum width
            for (int i = 0; i < widgets.length(); i += 1) {
                Widget *widget = widgets.at(i);
                widget->left = next_x;
                widget->width = widget->min_width();
                widget->on_resize();
                next_x += widget->width + margin;
            }
            return;
        }
        panic("TODO");
    }

    void layout_y() {
        panic("TODO");
    }

    int min_width() const override {
        // return the largest min width
        int largest = -1;
        for (int i = 0; i < widgets.length(); i += 1) {
            Widget *widget = widgets.at(i);
            int widget_min_width = widget->min_width();
            if (widget_min_width > largest)
                largest = widget_min_width;
        }
        return (largest == -1) ? -1 : (largest + padding * 2);
    }

    int max_width() const override {
        // return the largest max width
        int largest = 0;
        for (int i = 0; i < widgets.length(); i += 1) {
            Widget *widget = widgets.at(i);
            int widget_max_width = widget->max_width();
            if (widget_max_width == -1)
                return -1;
            largest = max(widget_max_width, largest);
        }
        return largest + padding * 2;
    }

    int min_height() const override {
        // return the min heights of the widgets added together with margin
        // in between
        int total = 0;
        for (int i = 0; i < widgets.length(); i += 1) {
            Widget *widget = widgets.at(i);
            int widget_min_height = widget->min_height();
            if (widget_min_height >= 0) {
                int margin_value = margin * (i != widgets.length() - 1);
                total += widget_min_height + margin_value;
            }
        }
        return (total == 0) ? 0 : (total + padding * 2);
    }

    int max_height() const override {
        // return the max heights of the widgets added together with margin
        // in between
        int total = 0;
        for (int i = 0; i < widgets.length(); i += 1) {
            Widget *widget = widgets.at(i);
            int widget_max_height = widget->max_height();
            if (widget_max_height == -1)
                return -1;
            int margin_value = margin * (i != widgets.length() - 1);
            total += widget_max_height + margin_value;
        }
        return total + padding * 2;
    }

    List<Widget *> widgets;
    int margin;
    int padding;
};

#endif
