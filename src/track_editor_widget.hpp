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
struct AudioClipSegment;
class SpritesheetImage;

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

    int timeline_bottom;
    int timeline_height;
    int track_head_width;
    int track_height;
    int head_left;
    int body_left;
    int track_area_bottom;

    int track_name_label_padding_left;
    int track_name_label_padding_top;

    glm::vec4 track_name_color;
    glm::vec4 track_main_bg_color;
    glm::vec4 timeline_bg_color;

    glm::vec4 dark_border_color;
    glm::vec4 light_border_color;

    SunkenBox timeline_bg;
    glm::mat4 track_area_stencil;
    glm::mat4 widget_stencil;

    const SpritesheetImage *play_head_icon;
    glm::vec4 play_head_color;
    glm::mat4 play_head_icon_model;
    glm::mat4 play_head_model;
    glm::vec4 timeline_bottom_border_color;
    glm::mat4 timeline_bottom_border_model;

    double pixels_per_whole_note;

    struct DisplayAudioClipSegment;
    struct GuiAudioClipSegment {
        AudioClipSegment *segment;
        DisplayAudioClipSegment *display_segment;
        int left;
        int right;
        int top;
        int bottom;
    };

    struct DisplayAudioClipSegment {
        GuiAudioClipSegment *gui_segment;
        SunkenBox title_bar;
        SunkenBox body;
        Label *label;
        glm::mat4 label_model;

        // adjusted for scroll position
        int left;
        int right;
        int top;
        int bottom;
    };

    struct DisplayTrack;
    struct GuiTrack {
        Track *track;
        DisplayTrack *display_track;
        List<GuiAudioClipSegment *> gui_audio_clip_segments;
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

        List<DisplayAudioClipSegment *> display_audio_clip_segments;
        int display_audio_clip_segment_count;

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

    bool scrub_mouse_down;

    void update_model();
    void update_play_head_model();
    GuiTrack *create_gui_track();
    DisplayTrack * create_display_track(GuiTrack *gui_track);
    DisplayAudioClipSegment * create_display_audio_clip_segment(
            DisplayTrack *display_track, GuiAudioClipSegment *gui_audio_clip_segment);
    GuiAudioClipSegment * create_gui_audio_clip_segment();
    void destroy_gui_track(GuiTrack *gui_track);
    void right_click_track_head(GuiTrack *gui_track, int x, int y);
    void clear_track_context_menu();
    GuiTrack *get_track_body_at(int x, int y);
    GuiTrack *get_track_head_at(int x, int y);
    void on_drag_sample_file(DraggedSampleFile *dragged_sample_file, const DragEvent *event);
    void on_drag_audio_asset(AudioAsset *audio_asset, const DragEvent *event);
    void on_drag_audio_clip(AudioClip *audio_clip, const DragEvent *event);
    void refresh_tracks();
    void destroy_display_track(DisplayTrack *display_track);
    void destroy_display_audio_clip_segment(DisplayAudioClipSegment *display_audio_clip_segment);
    void destroy_gui_audio_clip_segment(GuiAudioClipSegment *gui_audio_clip_segment);
    int whole_note_to_pixel(double whole_note_pos);
    double pixel_to_whole_note(int pixel_x);
    void scrub(const MouseEvent *event);
};

#endif
