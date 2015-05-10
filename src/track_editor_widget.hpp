#ifndef TRACK_EDITOR_WIDGET_HPP
#define TRACK_EDITOR_WIDGET_HPP

#include "widget.hpp"
#include "sunken_box.hpp"

struct Project;
struct Track;
class Label;
class MenuWidgetItem;
class ScrollBarWidget;

class TrackEditorWidget : public Widget {
public:
    TrackEditorWidget(GuiWindow *gui_window, Project *project);
    ~TrackEditorWidget() override;

    void draw(const glm::mat4 &projection) override;
    void on_resize() override { update_model(); }
    void on_mouse_move(const MouseEvent *event) override;


    Project *project;

    int padding_top;
    int padding_bottom;
    int padding_left;
    int padding_right;
    int timeline_height;
    int track_head_width;
    int track_height;

    int track_name_label_padding_left;
    int track_name_label_padding_top;

    glm::vec4 track_name_color;
    glm::vec4 track_head_bg_color;
    glm::vec4 track_main_bg_color;
    glm::vec4 timeline_bg_color;

    glm::vec4 dark_border_color;
    glm::vec4 light_border_color;

    glm::mat4 timeline_model;

    struct GuiTrack {
        Track *track;
        glm::mat4 head_model;
        SunkenBox body_bg;
        glm::mat4 border_top_model;
        glm::mat4 border_bottom_model;

        Label *track_name_label;
        glm::mat4 track_name_label_model;

        int left;
        int top;
        int bottom;
    };
    List<GuiTrack *> tracks;

    MenuWidgetItem *track_context_menu;
    GuiTrack *menu_track;
    ScrollBarWidget *vert_scroll_bar;
    ScrollBarWidget *horiz_scroll_bar;

    void update_model();
    GuiTrack *create_gui_track();
    void destroy_gui_track(GuiTrack *gui_track);
    void right_click_track(GuiTrack *gui_track, int x, int y);
    void clear_track_context_menu();
};

#endif
