#ifndef AUDIO_EDIT_WIDGET
#define AUDIO_EDIT_WIDGET

#include "widget.hpp"
#include "byte_buffer.hpp"
#include "text_widget.hpp"
#include "alpha_texture.hpp"
#include "genesis.h"
#include "audio_file.hpp"

class Gui;
class AudioEditWidget {
public:
    Widget _widget;

    AudioEditWidget(Gui *gui);
    AudioEditWidget(const AudioEditWidget &copy) = delete;
    AudioEditWidget &operator=(const AudioEditWidget &copy) = delete;

    ~AudioEditWidget();
    void draw(const glm::mat4 &projection);
    int left() const { return _left; }
    int top() const { return _top; }
    int width() const { return _width; }
    int height() const { return _height; }
    void on_mouse_move(const MouseEvent *event);
    void on_mouse_out(const MouseEvent *event);
    void on_mouse_over(const MouseEvent *event);
    void on_gain_focus();
    void on_lose_focus();
    void on_text_input(const TextInputEvent *event);
    void on_key_event(const KeyEvent *event);


    void load_file(const ByteBuffer &file_path);
    void edit_audio_file(AudioFile *audio_file);
    void set_pos(int left, int top);
    void set_size(int width, int height);

    void save_as(const ByteBuffer &file_path, ExportSampleFormat export_sample_format);

private:
    Gui *_gui;

    int _left;
    int _top;
    int _width;
    int _height;

    int _padding_left;
    int _padding_right;
    int _padding_top;
    //int _padding_bottom;

    int _channel_edit_height;
    int _margin;
    int _timeline_height;

    glm::vec4 _waveform_fg_color;
    glm::vec4 _waveform_bg_color;
    glm::vec4 _waveform_sel_bg_color;
    glm::vec4 _waveform_sel_fg_color;
    glm::vec4 _timeline_bg_color;
    glm::vec4 _timeline_fg_color;
    glm::vec4 _timeline_sel_bg_color;
    glm::vec4 _timeline_sel_fg_color;

    AudioFile *_audio_file;

    struct PerChannelData {
        TextWidget *channel_name_widget;
        AlphaTexture *waveform_texture;
        glm::mat4 waveform_model;
        int left;
        int top;
        int width;
        int height;
    };

    List<PerChannelData*> _channel_list;

    struct Selection {
        List<bool> channels;
        long start;
        long end;
    };

    struct CursorPosition {
        long channel;
        long frame;
    };

    Selection _selection;
    Selection _playback_selection;
    int _scroll_x; // in pixels
    double _frames_per_pixel;
    bool _select_down;


    void update_model();

    void destroy_audio_file();
    void destroy_all_ui();
    void destroy_per_channel_data(PerChannelData *per_channel_data);
    PerChannelData *create_per_channel_data(int index);

    void init_selection(Selection &selection);
    bool get_frame_and_channel(int x, int y, CursorPosition *out);
    long frame_at_pos(int x);
    int pos_at_frame(long frame);
    int wave_start_left() const;
    int wave_width() const;
    void scroll_cursor_into_view();
    void zoom_100();
    long get_display_frame_count() const;
    int get_full_wave_width() const;
    void delete_selection();
    void clamp_selection();
    void get_order_correct_selection(const Selection *selection, long *start, long *end);

    // widget methods
    static void destructor(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->~AudioEditWidget();
    }
    static void draw(Widget *widget, const glm::mat4 &projection) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->draw(projection);
    }
    static int left(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->left();
    }
    static int top(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->top();
    }
    static int width(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->width();
    }
    static int height(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->height();
    }
    static void on_mouse_move(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_mouse_move(event);
    }
    static void on_mouse_out(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_mouse_out(event);
    }
    static void on_mouse_over(Widget *widget, const MouseEvent *event) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_mouse_over(event);
    }
    static void on_gain_focus(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_gain_focus();
    }
    static void on_lose_focus(Widget *widget) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_lose_focus();
    }
    static void on_text_input(Widget *widget, const TextInputEvent *event) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_text_input(event);
    }
    static void on_key_event(Widget *widget, const KeyEvent *event) {
        return (reinterpret_cast<AudioEditWidget*>(widget))->on_key_event(event);
    }
};

#endif
