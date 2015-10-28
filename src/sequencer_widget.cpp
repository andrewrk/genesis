#include "sequencer_widget.hpp"
#include "scroll_bar_widget.hpp"
#include "color.hpp"
#include "gui_window.hpp"
#include "label.hpp"

/*
    * low pitch
-1= G 3
0 = A 4 (440 hz)
1 = B 4
2 = C 4
3 = D
4 = E
5 = F
6 = G
* high pitch
*/
static const char *note_names[] = {
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
};

static const int begin_note_index = -4 * 7;
static const int end_note_index =  4 * 7;

static void scroll_callback(Event, void *userdata) {
    SequencerWidget *sequencer_widget = (SequencerWidget *)userdata;
    sequencer_widget->update_model();
}

SequencerWidget::SequencerWidget(GuiWindow *window, Project *project) :
    Widget(window)
{
    this->project = project;
    grid_row_color = parse_color("#565656");
    note_label_color = color_fg_text();

    grid_bg.set_scheme(SunkenBoxSchemeSunkenBorders);

    vert_scroll_bar = create<ScrollBarWidget>(gui_window, ScrollBarLayoutVert);
    vert_scroll_bar->parent_widget = this;
    horiz_scroll_bar = create<ScrollBarWidget>(gui_window, ScrollBarLayoutHoriz);
    horiz_scroll_bar->parent_widget = this;

    vert_scroll_bar->events.attach_handler(EventScrollValueChange, scroll_callback, this);
}

SequencerWidget::~SequencerWidget() {
    destroy(vert_scroll_bar, 1);
    destroy(horiz_scroll_bar, 1);
}

void SequencerWidget::draw(const glm::mat4 &projection) {
    bg.draw(gui_window, projection);
    grid_bg.draw(gui_window, projection);

    horiz_scroll_bar->draw(projection);
    vert_scroll_bar->draw(projection);

    for (int i = 0; i < grid_rows.length(); i += 1) {
        SequencerWidgetGridRow *grid_row = &grid_rows.at(i);

        gui_window->fill_rect(grid_row_color, projection * grid_row->model);

        grid_row->label->draw(projection * grid_row->label_model, note_label_color);
    }
}

void SequencerWidget::update_model() {
    bg.update(this, 0, 0, width, height);

    horiz_scroll_bar->left = left;
    horiz_scroll_bar->top = top;
    horiz_scroll_bar->width = width;
    horiz_scroll_bar->height = horiz_scroll_bar->min_height();
    //horiz_scroll_bar->min_value = 0;
    //horiz_scroll_bar->max_value = max(0, full_width - body_width);
    //horiz_scroll_bar->set_handle_ratio(body_width, full_width);
    //horiz_scroll_bar->set_value(horiz_scroll_bar->value);
    horiz_scroll_bar->on_resize();

    int timeline_top = horiz_scroll_bar->height;
    int timeline_height = 24;
    int timeline_bottom = timeline_top + timeline_height;

    int grid_height = height - timeline_bottom;

    int grid_cell_height = 16;
    int full_height = (end_note_index - begin_note_index) * grid_cell_height;
    int available_height = grid_height;

    vert_scroll_bar->left = left + width - vert_scroll_bar->min_width();
    vert_scroll_bar->top = top + timeline_bottom;
    vert_scroll_bar->width = vert_scroll_bar->min_width();
    vert_scroll_bar->height = grid_height;
    vert_scroll_bar->min_value = 0;
    vert_scroll_bar->max_value = max(0, full_height - available_height);
    vert_scroll_bar->set_handle_ratio(available_height, full_height);
    vert_scroll_bar->set_value(vert_scroll_bar->value);
    vert_scroll_bar->on_resize();

    int key_width = 32;
    int grid_width = width - key_width - vert_scroll_bar->width;
    grid_bg.update(this, key_width, timeline_bottom, grid_width, grid_height);

    int label_padding = 4;

    int grid_top = timeline_bottom;
    int grid_bottom = grid_top + grid_height;

    // Iterate over all the notes. If a note is visible, then prepare to display it.
    int next_grid_row = 0;
    for (int note_index = begin_note_index; note_index < end_note_index; note_index += 1) {
        int absolute_row_index = (end_note_index - begin_note_index) -
            (note_index - begin_note_index) - 1;
        int row_top = timeline_bottom + absolute_row_index * grid_cell_height - vert_scroll_bar->value;
        int row_bottom = row_top + grid_cell_height;

        bool visible = (row_bottom >= grid_top && row_top < grid_bottom);
        if (!visible)
            continue;

        SequencerWidgetGridRow *grid_row;
        if (next_grid_row < grid_rows.length()) {
            grid_row = &grid_rows.at(next_grid_row);
        } else {
            ok_or_panic(grid_rows.add_one());
            grid_row = &grid_rows.last();
            grid_row->label = create<Label>(gui);
        }
        next_grid_row += 1;

        grid_row->model = transform2d(0, row_top,
                grid_width + key_width, 1.0f);
        String row_text;
        get_note_index_text(note_index, row_text);
        grid_row->label->set_text(row_text);
        grid_row->label->update();
        int label_top = row_top + grid_cell_height / 2 - grid_row->label->height() / 2;
        int label_left = key_width - grid_row->label->width() - label_padding;
        grid_row->label_model = transform2d(label_left, label_top);
    }

    while (grid_rows.length() > next_grid_row) {
        SequencerWidgetGridRow *grid_row = &grid_rows.last();
        deinit_grid_row(grid_row);
        grid_rows.pop();
    }
}

void SequencerWidget::deinit_grid_row(SequencerWidgetGridRow *grid_row) {
    destroy(grid_row->label, 1);
}

void SequencerWidget::get_note_index_text(int offset, String &text) {
    int note_name_index = euclidean_mod(offset, 7);
    assert(note_name_index >= 0);
    assert(note_name_index < array_length(note_names));
    const char *note_name = note_names[note_name_index];
    int note_number = (offset - begin_note_index) / 7;

    ByteBuffer buf;
    buf.format("%s %d", note_name, note_number);
    text = buf;
}

void SequencerWidget::on_mouse_move(const MouseEvent *event) {
    // otherwise, forward to vert scroll bar
    if (forward_mouse_event(vert_scroll_bar, event))
        return;
}

void SequencerWidget::on_mouse_wheel(const MouseWheelEvent *event) {
    {
        float range = vert_scroll_bar->max_value - vert_scroll_bar->min_value;
        vert_scroll_bar->set_value(vert_scroll_bar->value - event->wheel_y * range * 0.18f * vert_scroll_bar->handle_ratio);
    }
    update_model();
}
