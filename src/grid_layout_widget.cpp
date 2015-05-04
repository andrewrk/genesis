#include "grid_layout_widget.hpp"
#include "gui_window.hpp"

void GridLayoutWidget::draw(const glm::mat4 &projection) {
    for (int row = 0; row < rows(); row += 1) {
        List<Cell> *r = &cells.at(row);
        for (int col = 0; col < cols(); col += 1) {
            Widget *widget = r->at(col).widget;
            if (widget && widget->is_visible)
                widget->draw(projection);
        }
    }
}

void GridLayoutWidget::add_widget(Widget *widget, int row, int col, HAlign h_align, VAlign v_align) {
    assert(!widget->parent_widget); // widget already has parent
    ensure_size(row + 1, col + 1);
    widget->parent_widget = this;
    widget->layout_row = row;
    widget->layout_col = col;
    Cell *cell = &cells.at(row).at(col);
    cell->widget = widget;
    cell->h_align = h_align;
    cell->v_align = v_align;
    on_size_hints_changed();
}

void GridLayoutWidget::remove_widget(Widget *widget) {
    if (widget->layout_row == -1)
        return;

    Cell *cell = &cells.at(widget->layout_row).at(widget->layout_col);
    cell->widget = nullptr;

    widget->layout_row = -1;
    reduce_size();
    on_size_hints_changed();
}

int GridLayoutWidget::min_width() const {
    int max_min_row_width = 0;
    for (int row = 0; row < rows(); row += 1) {
        int row_width = get_row_min_width(row);
        max_min_row_width = max(max_min_row_width, row_width);
    }
    return max_min_row_width;
}

bool GridLayoutWidget::expanding_x() const {
    // if and only if any widget has max_width -1
    for (int row = 0; row < rows(); row += 1) {
        if (get_row_max_width(row) == -1)
            return true;
    }
    return false;
}

int GridLayoutWidget::max_width() const {
    // if any widget has max_width -1, then return -1
    // otherwise, return the max max row width
    int max_max_row_width = 0;
    for (int row = 0; row < rows(); row += 1) {
        int row_width = get_row_max_width(row);
        if (row_width == -1)
            return -1;
        max_max_row_width = max(max_max_row_width, row_width);
    }
    return max_max_row_width;
}

int GridLayoutWidget::min_height() const {
    int max_min_col_height = 0;
    for (int col = 0; col < cols(); col += 1) {
        int col_height = get_col_min_height(col);
        max_min_col_height = max(max_min_col_height, col_height);
    }
    return max_min_col_height;
}

bool GridLayoutWidget::expanding_y() const {
    // if and only if any widget has max_height -1
    for (int col = 0; col < cols(); col += 1) {
        if (get_col_max_height(col) == -1)
            return true;
    }
    return false;
}

int GridLayoutWidget::max_height() const {
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
}

void GridLayoutWidget::on_resize() {
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

void GridLayoutWidget::ensure_size(int row_count, int col_count) {
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

void GridLayoutWidget::reduce_size() {
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

int GridLayoutWidget::get_row_min_width(int row) const {
    // return the min widths of the widgets added together with spacing
    // in between
    const List<Cell> *r = &cells.at(row);
    int total = 0;
    for (int col = 0; col < cols(); col += 1) {
        const Cell *cell = &r->at(col);
        if (!cell->widget)
            continue;
        total += cell->widget->min_width();
    }
    return total + padding * 2 + spacing * (cols() - 1);
}

int GridLayoutWidget::get_row_max_width(int row) const {
    // if any widget has max width -1, return -1
    // otherwise, return all the max widths added together with spacing
    // in between
    const List<Cell> *r = &cells.at(row);
    int total = 0;
    for (int col = 0; col < cols(); col += 1) {
        const Cell *cell = &r->at(col);
        if (!cell->widget)
            return -1;
        int widget_max_width = cell->widget->max_width();
        if (widget_max_width == -1)
            return -1;
        total += widget_max_width;
    }
    return total + padding * 2 + spacing * (cols() - 1);
}

int GridLayoutWidget::get_row_min_height(int row) const {
    // return the largest min height
    // don't take spacing into account
    int biggest = 0;
    for (int col = 0; col < cols(); col += 1) {
        const Cell *cell = &cells.at(row).at(col);
        if (!cell->widget)
            continue;
        biggest = max(biggest, cell->widget->min_height());
    }
    return biggest;
}

int GridLayoutWidget::get_row_max_height(int row) const {
    // if any widget has max height -1, return -1
    // otherwise, return the largest max height
    int biggest = 0;
    for (int col = 0; col < cols(); col += 1) {
        const Cell *cell = &cells.at(row).at(col);
        if (!cell->widget)
            return -1;
        int widget_max_height = cell->widget->max_height();
        if (widget_max_height == -1)
            return -1;
        biggest = max(biggest, widget_max_height);
    }
    return biggest;
}

int GridLayoutWidget::get_col_min_width(int col) const {
    // return the largest min width
    // don't take spacing into account
    int biggest = 0;
    for (int row = 0; row < rows(); row += 1) {
        const Cell *cell = &cells.at(row).at(col);
        if (!cell->widget)
            continue;
        biggest = max(biggest, cell->widget->min_width());
    }
    return biggest;
}

int GridLayoutWidget::get_col_max_width(int col) const {
    // if any widget has max width -1, return -1
    // otherwise, return the largest max width
    // don't take spacing into account
    int biggest = 0;
    for (int row = 0; row < rows(); row += 1) {
        const Cell *cell = &cells.at(row).at(col);
        if (!cell->widget)
            return -1;
        int widget_max_width = cell->widget->max_width();
        if (widget_max_width == -1)
            return -1;
        biggest = max(biggest, widget_max_width);
    }
    return biggest;
}

int GridLayoutWidget::get_col_min_height(int col) const {
    // return the min heights of the widgets added together with spacing
    // in between
    int total = 0;
    for (int row = 0; row < rows(); row += 1) {
        const Cell *cell = &cells.at(row).at(col);
        total += cell->widget->min_height();
    }
    return total + padding * 2 + spacing * (rows() - 1);
}

int GridLayoutWidget::get_col_max_height(int col) const {
    // if any widget has max width -1, return -1
    // otherwise, return all the max widths added together with spacing
    // in between
    int total = 0;
    for (int row = 0; row < rows(); row += 1) {
        const Cell *cell = &cells.at(row).at(col);
        if (!cell->widget)
            return -1;
        int widget_max_height = cell->widget->max_height();
        if (widget_max_height == -1)
            return -1;
        total += widget_max_height;
    }
    return total + padding * 2 + spacing * (rows() - 1);
}

void GridLayoutWidget::layout_x() {
    int available_width = width - padding * 2 - spacing * (cols() - 1);

    if (col_props.resize(cols()))
        panic("out of memory");

    bool expanding = expanding_x();
    for (int col = 0; col < cols(); col += 1) {
        ColRowInfo *col_info = &col_props.at(col);
        col_info->done = false;
        col_info->min_size = get_col_min_width(col);
        col_info->max_size = get_col_max_width(col);
    }
    int not_done_count = cols();

outer:
    while (not_done_count > 0) {
        // iterate over the columns and see if any columns complain
        // about getting a fair share
        int each_col_amt = available_width / not_done_count;
        for (int col = 0; col < cols(); col += 1) {
            ColRowInfo *col_info = &col_props.at(col);
            if (col_info->done)
                continue;
            if (expanding && col_info->max_size != -1 && each_col_amt > col_info->max_size) {
                col_info->size = col_info->max_size;
                col_info->done = true;
                available_width -= col_info->size;
                not_done_count -= 1;
                goto outer;
            } else if (each_col_amt < col_info->min_size) {
                col_info->size = col_info->min_size;
                col_info->done = true;
                available_width -= col_info->size;
                not_done_count -= 1;
                goto outer;
            } else {
                col_info->size = each_col_amt;
            }
        }
        // everybody is happy
        break;
    }

    // determine each column's left
    int next_left = padding;
    for (int col = 0; col < cols(); col += 1) {
        ColRowInfo *col_info = &col_props.at(col);
        col_info->start = next_left;
        next_left += col_info->size + spacing;
    }

    // now we know each column's left and width. iterate over every widget
    // and set the width and left properties
    for (int row = 0; row < rows(); row += 1) {
        for (int col = 0; col < cols(); col += 1) {
            Cell *cell = &cells.at(row).at(col);
            Widget *widget = cell->widget;
            if (!widget)
                continue;
            ColRowInfo *col_info = &col_props.at(col);
            int cell_width = col_info->size;
            int widget_min_width = widget->min_width();
            int widget_max_width = widget->max_width();
            widget->width = max(cell_width, widget_min_width);
            if (widget_max_width >= 0)
                widget->width = min(widget->width, widget_max_width);
            switch (cell->h_align) {
                case HAlignLeft:
                    widget->left = left + col_info->start;
                    break;
                case HAlignRight:
                    widget->left = left + col_info->start + cell_width - widget->width;
                    break;
                case HAlignCenter:
                    widget->left = left + col_info->start + cell_width / 2 - widget->width / 2;
                    break;
            }
        }
    }
}

void GridLayoutWidget::layout_y() {
    int available_height = height - padding * 2 - spacing * (rows() - 1);

    if (row_props.resize(rows()))
        panic("out of memory");

    bool expanding = expanding_y();
    for (int row = 0; row < rows(); row += 1) {
        ColRowInfo *row_info = &row_props.at(row);
        row_info->done = false;
        row_info->min_size = get_row_min_height(row);
        row_info->max_size = get_row_max_height(row);
    }
    int not_done_count = rows();

outer:
    while (not_done_count > 0) {
        // iterate over the rows and see if any rows complain
        // about getting a fair share
        int each_row_amt = available_height / not_done_count;
        for (int row = 0; row < rows(); row += 1) {
            ColRowInfo *row_info = &row_props.at(row);
            if (row_info->done)
                continue;
            if (expanding && row_info->max_size != -1 && each_row_amt > row_info->max_size) {
                row_info->size = row_info->max_size;
                row_info->done = true;
                available_height -= row_info->size;
                not_done_count -= 1;
                goto outer;
            } else if (each_row_amt < row_info->min_size) {
                row_info->size = row_info->min_size;
                row_info->done = true;
                available_height -= row_info->size;
                not_done_count -= 1;
                goto outer;
            } else {
                row_info->size = each_row_amt;
            }
        }
        // everybody is happy
        break;
    }

    // determine each row's top
    int next_top = padding;
    for (int row = 0; row < rows(); row += 1) {
        ColRowInfo *row_info = &row_props.at(row);
        row_info->start = next_top;
        next_top += row_info->size + spacing;
    }

    // now we know each row's top and height. iterate over every widget
    // and set the height and top properties
    for (int row = 0; row < rows(); row += 1) {
        ColRowInfo *row_info = &row_props.at(row);
        for (int col = 0; col < cols(); col += 1) {
            Cell *cell = &cells.at(row).at(col);
            Widget *widget = cell->widget;
            if (!widget)
                continue;
            int cell_height = row_info->size;
            int widget_min_height = widget->min_height();
            int widget_max_height = widget->max_height();
            widget->height = max(cell_height, widget_min_height);
            if (widget_max_height >= 0)
                widget->height = min(widget->height, widget_max_height);
            switch (cell->v_align) {
                case VAlignTop:
                    widget->top = top + row_info->start;
                    break;
                case VAlignBottom:
                    widget->top = top + row_info->start + cell_height - widget->height;
                    break;
                case VAlignCenter:
                    widget->top = top + row_info->start + cell_height / 2 - widget->height / 2;
                    break;
            }
        }
    }
}

void GridLayoutWidget::on_mouse_move(const MouseEvent *event) {
    MouseEvent mouse_event = *event;
    mouse_event.x += left;
    mouse_event.y += top;
    for (int row = 0; row < rows(); row += 1) {
        List<Cell> *r = &cells.at(row);
        for (int col = 0; col < cols(); col += 1) {
            Widget *widget = r->at(col).widget;
            if (!widget)
                continue;
            if (!widget->is_visible)
                continue;
            if (gui_window->try_mouse_move_event_on_widget(widget, &mouse_event))
                return;
        }
    }
}
