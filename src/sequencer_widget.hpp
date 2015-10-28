#ifndef GENESIS_SEQUENCER_WIDGET_HPP
#define GENESIS_SEQUENCER_WIDGET_HPP

#include "widget.hpp"
#include "sunken_box.hpp"

class GuiWindow;
struct Project;
class ScrollBarWidget;
class Label;

struct SequencerWidgetGridRow {
    Label *label;
    glm::mat4 label_model;
    glm::mat4 model;
};

class SequencerWidget : public Widget {
public:
    SequencerWidget(GuiWindow *window, Project *project);
    ~SequencerWidget() override;

    void draw(const glm::mat4 &projection) override;
    void on_resize() override { update_model(); }
    void on_mouse_move(const MouseEvent *event) override;
    void on_mouse_wheel(const MouseWheelEvent *event) override;

    Project *project;
    SunkenBox bg;
    SunkenBox grid_bg;
    ScrollBarWidget *vert_scroll_bar;
    ScrollBarWidget *horiz_scroll_bar;

    glm::vec4 grid_row_color;
    glm::vec4 note_label_color;

    List<SequencerWidgetGridRow> grid_rows;

    void update_model();

    void deinit_grid_row(SequencerWidgetGridRow *grid_row);
    void get_note_index_text(int note_index, String &text);
};

#endif

