#ifndef GRID_LAYOUT_HPP
#define GRID_LAYOUT_HPP

#include "widget.hpp"

enum HAlign {
    HAlignLeft,
    HAlignRight,
    HAlignCenter,
};

enum VAlign {
    VAlignTop,
    VAlignBottom,
    VAlignCenter,
};

class GridLayoutWidget : public Widget {
public:
    GridLayoutWidget(GuiWindow *gui_window) :
        Widget(gui_window),
        spacing(4),
        padding(4)
    {
    }
    ~GridLayoutWidget() override { }

    int rows() const {
        return cells.length();
    }

    int cols() const {
        return (cells.length() > 0) ? cells.at(0).length() : 0;
    }

    void draw(const glm::mat4 &projection) override {
        for (int row = 0; row < rows(); row += 1) {
            List<Cell> *r = &cells.at(row);
            for (int col = 0; col < cols(); col += 1) {
                Widget *widget = r->at(col).widget;
                if (widget && widget->is_visible)
                    widget->draw(projection);
            }
        }
    }

    void add_widget(Widget *widget, int row, int col, HAlign h_align, VAlign v_align) {
        if (widget->parent_widget)
            panic("widget already has parent");
        if (widget->set_index >= 0)
            panic("widget already attached to window");
        ensure_size(row, col);
        widget->parent_widget = this;
        widget->layout_row = row;
        widget->layout_col = col;
        Cell *cell = &cells.at(row).at(col);
        cell->widget = widget;
        cell->h_align = h_align;
        cell->v_align = v_align;
        on_size_hints_changed();
    }

    void remove_widget(Widget *widget) override {
        if (widget->layout_row == -1)
            return;

        Cell *cell = &cells.at(widget->layout_row).at(widget->layout_col);
        cell->widget = nullptr;

        widget->layout_row = -1;
        reduce_size();
        on_size_hints_changed();
    }

    int min_width() const override {
        int max_min_row_width = 0;
        for (int row = 0; row < rows(); row += 1) {
            int row_width = get_row_min_width(row);
            max_min_row_width = max(max_min_row_width, row_width);
        }
        return max_min_row_width;
    };

    int max_width() const override {
        // if any widget has max_width -1, then return -1.
        // otherwise, return the max max row width
        int max_max_row_width = 0;
        for (int row = 0; row < rows(); row += 1) {
            int row_width = get_row_max_width(row);
            if (row_width == -1)
                return -1;
            max_max_row_width = max(max_max_row_width, row_width);
        }
        return max_max_row_width;
    };

    int min_height() const override {
        int max_min_col_height;
        for (int col = 0; col < cols(); col += 1) {
            int col_height = get_col_min_height(col);
            max_min_col_height = max(max_min_col_height, col_height);
        }
        return max_min_col_height;
    };

    int max_height() const override {
        // if any widget has max_height -1, then return -1.
        // otherwise, return the max max col height
        int max_max_col_height = 0;
        for (int col = 0; col < cols(); col += 1) {
            int col_height = get_col_max_height(col);
            if (col_height == -1)
                return -1;
            max_max_col_height = max(max_max_col_height, col_height);
        }
        return max_max_col_height;
    };

    void on_resize() override {
        layout_x();
        layout_y();
        for (int row = 0; row < rows(); row += 1) {
            List<Cell> *r = &cells.at(row);
            for (int col = 0; col < cols(); col += 1) {
                Widget *widget = r->at(col).widget;
                if (widget)
                    widget->on_resize();
            }
        }
    }

    struct Cell {
        Widget *widget;
        HAlign h_align;
        VAlign v_align;
    };

    int spacing;
    int padding;
    List<List<Cell>> cells;

    void ensure_size(int row_count, int col_count) {
        if (rows() < row_count) {
            int old_length = rows();
            if (cells.resize(row_count))
                panic("out of memory");
            for (int row = old_length; row < row_count; row += 1) {
                List<Cell> *r = &cells.at(row);
                if (r->resize(col_count))
                    panic("out of memory");
                for (int col = 0; col < col_count; col += 1) {
                    r->at(col).widget = nullptr;
                }
            }
        }
        for (int row = 0; row < rows(); row += 1) {
            List<Cell> *r = &cells.at(row);
            if (r->length() < col_count) {
                int old_length = r->length();
                if (r->resize(col_count))
                    panic("out of memory");
                for (int col = old_length; col < col_count; col += 1) {
                    r->at(col).widget = nullptr;
                }
            }
        }
    }

    void reduce_size() {
        int last_row = rows() - 1;
        bool all_cols_empty = true;
        int row;
        for (row = last_row; row >= 0; row -= 1) {
            List<Cell> *r = &cells.at(row);
            for (int col = 0; col < r->length(); col += 1) {
                if (r->at(col).widget) {
                    all_cols_empty = false;
                    break;
                }
            }
            if (!all_cols_empty)
                break;
        }
        int row_shave_size = (last_row - row) + all_cols_empty;
        if (row_shave_size > 0) {
            if (cells.resize(rows() - row_shave_size))
                panic("out of memory");
        }

        int last_col = cells.at(0).length() - 1;
        bool all_rows_empty = true;
        int col;
        for (col = last_col; col >= 0; col -= 1) {
            for (int row = 0; row < rows(); row += 1) {
                Cell *cell = &cells.at(row).at(col);
                if (cell->widget) {
                    all_rows_empty = false;
                    break;
                }
            }
            if (!all_rows_empty)
                break;
        }
        int col_shave_size = (last_col - col) + all_rows_empty;
        if (col_shave_size > 0) {
            for (int row = 0; row < rows(); row += 1) {
                List<Cell> *r = &cells.at(row);
                if (r->resize(r->length() - col_shave_size))
                    panic("out of memory");
            }
        }
    }

    int get_row_min_width(int row) const {
        // return the min widths of the widgets added together with spacing
        // in between
        const List<Cell> *r = &cells.at(row);
        int total = 0;
        bool first = true;
        for (int col = 0; col < r->length(); col += 1) {
            const Cell *cell = &r->at(col);
            int widget_min_width = cell->widget->min_width();
            if (widget_min_width >= 0) {
                int spacing_value = spacing * !first;
                total += spacing_value + widget_min_width;
                first = false;
            }
        }
        return (total == 0) ? 0 : (total + padding * 2);
    }

    int get_row_max_width(int row) const {
        // if any widget has max width -1, return -1
        // otherwise, return all the max widths added together with spacing
        // in between
        const List<Cell> *r = &cells.at(row);
        int total = 0;
        bool first = true;
        for (int col = 0; col < r->length(); col += 1) {
            const Cell *cell = &r->at(col);
            int widget_max_width = cell->widget->max_width();
            if (widget_max_width == -1)
                return -1;
            if (widget_max_width > 0) {
                int spacing_value = spacing * !first;
                total += spacing_value + widget_max_width;
                first = false;
            }
        }
        return (total == 0) ? 0 : (total + padding * 2);
    }

    int get_col_min_height(int col) const {
        // return the min heights of the widgets added together with spacing
        // in between
        int total = 0;
        bool first = true;
        for (int row = 0; row < rows(); row += 1) {
            const Cell *cell = &cells.at(row).at(col);
            int widget_min_height = cell->widget->min_height();
            if (widget_min_height > 0) {
                int spacing_value = spacing * !first;
                total += spacing_value + widget_min_height;
                first = false;
            }
        }
        return (total == 0) ? 0 : (total + padding * 2);
    }

    int get_col_max_height(int col) const {
        // if any widget has max width -1, return -1
        // otherwise, return all the max widths added together with spacing
        // in between
        int total = 0;
        bool first = true;
        for (int row = 0; row < rows(); row += 1) {
            const Cell *cell = &cells.at(row).at(col);
            int widget_max_height = cell->widget->max_height();
            if (widget_max_height == -1)
                return -1;
            if (widget_max_height > 0) {
                int spacing_value = spacing * !first;
                total += spacing_value + widget_max_height;
                first = false;
            }
        }
        return (total == 0) ? 0 : (total + padding * 2);
    }

    void layout_x() {
        panic("TODO");
    }

    void layout_y() {
        panic("TODO");
    }
};

#endif
