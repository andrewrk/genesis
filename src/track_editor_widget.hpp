#ifndef TRACK_EDITOR_WIDGET_HPP
#define TRACK_EDITOR_WIDGET_HPP

#include "widget.hpp"
#include "sunken_box.hpp"

struct Project;
struct Track;
class Label;
class MenuWidgetItem;
class ScrollBarWidget;
struct DraggedSampleFile;
struct AudioAsset;
struct AudioClip;

class TrackEditorWidget : public Widget {
public:
    TrackEditorWidget(GuiWindow *gui_window, Project *project);
    ~TrackEditorWidget() override;

    void draw(const glm::mat4 &projection) override;
    void on_resize() override { update_model(); }
    void on_mouse_move(const MouseEvent *event) override;
    void on_mouse_wheel(const MouseWheelEvent *event) override;
    void on_drag(const DragEvent *) override;


    Project *project;

    int padding_top;
    int padding_bottom;
    int padding_left;
    int padding_right;
    int timeline_height;
    int track_head_width;
    int track_height;
    int head_left;
    int body_left;

    int track_name_label_padding_left;
    int track_name_label_padding_top;

    glm::vec4 track_name_color;
    glm::vec4 track_main_bg_color;
    glm::vec4 timeline_bg_color;

    glm::vec4 dark_border_color;
    glm::vec4 light_border_color;

    SunkenBox timeline_bg;
    glm::mat4 stencil_model;

    struct DisplayTrack;
    struct GuiTrack {
        Track *track;
        DisplayTrack *display_track;
        int left;
        int right;
        int top;
        int bottom;
    };

    struct DisplayTrack {
        GuiTrack *gui_track;
        SunkenBox head_bg;
        SunkenBox body_bg;
        glm::mat4 border_top_model;
        glm::mat4 border_bottom_model;
        Label *track_name_label;
        glm::mat4 track_name_label_model;
        // adjusted for scroll position
        int top;
        int bottom;
    };

    List<GuiTrack *> gui_tracks;
    List<DisplayTrack *> display_tracks;
    int display_track_count;

    MenuWidgetItem *track_context_menu;
    GuiTrack *menu_track;
    ScrollBarWidget *vert_scroll_bar;
    ScrollBarWidget *horiz_scroll_bar;

    void update_model();
    GuiTrack *create_gui_track();
    void destroy_gui_track(GuiTrack *gui_track);
    void right_click_track_head(GuiTrack *gui_track, int x, int y);
    void clear_track_context_menu();
    GuiTrack *get_track_body_at(int x, int y);
    GuiTrack *get_track_head_at(int x, int y);
    void on_drag_sample_file(DraggedSampleFile *dragged_sample_file, const DragEvent *event);
    void on_drag_audio_asset(AudioAsset *audio_asset, const DragEvent *event);
    void on_drag_audio_clip(AudioClip *audio_clip, const DragEvent *event);
    void refresh_tracks();
    DisplayTrack * create_display_track(GuiTrack *gui_track);
    void destroy_display_track(DisplayTrack *display_track);
};

#endif
