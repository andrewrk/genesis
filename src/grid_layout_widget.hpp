#ifndef GRID_LAYOUT_HPP
#define GRID_LAYOUT_HPP

#include "widget.hpp"
#include "list.hpp"

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

    void draw(const glm::mat4 &projection) override;
    void add_widget(Widget *widget, int row, int col, HAlign h_align, VAlign v_align);
    void remove_widget(Widget *widget) override;

    int min_width() const override;
    int max_width() const override;
    int min_height() const override;
    int max_height() const override;

    void on_resize() override;

    void on_mouse_move(const MouseEvent *) override;

    struct Cell {
        Widget *widget;
        HAlign h_align;
        VAlign v_align;
    };

    int spacing;
    int padding;
    List<List<Cell>> cells;

    struct ColRowInfo {
        bool done;
        int min_size; // min_width/min_height
        int max_size; // max_width/max_height
        int size;     // width/height
        int start;    // left/top
    };
    List<ColRowInfo> col_props;
    List<ColRowInfo> row_props;

    void ensure_size(int row_count, int col_count);
    void reduce_size();

    int get_row_min_width(int row) const;
    int get_row_max_width(int row) const;
    int get_row_min_height(int row) const;
    int get_row_max_height(int row) const;
    int get_col_min_width(int col) const;
    int get_col_max_width(int col) const;
    int get_col_min_height(int col) const;

    int get_col_max_height(int col) const;

    void layout_x();
    void layout_y();
};

#endif
