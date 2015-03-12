#ifndef AUDIO_EDIT_WIDGET
#define AUDIO_EDIT_WIDGET

#include "widget.hpp"
#include "byte_buffer.hpp"
#include "text_widget.hpp"
#include "alpha_texture.hpp"
#include "genesis.h"
#include "audio_file.hpp"
#include "select_widget.hpp"
#include "threads.hpp"

#include <atomic>
using std::atomic_bool;
using std::atomic_long;

class Gui;
class AudioHardware;
class OpenPlaybackDevice;
class OpenRecordingDevice;

class AudioEditWidget : public Widget {
public:
    AudioEditWidget(GuiWindow *gui_window, Gui *gui, AudioHardware *audio_hardware);
    ~AudioEditWidget() override;

    void draw(GuiWindow *window, const glm::mat4 &projection) override;
    int left() const override { return _left; }
    int top() const override { return _top; }
    int width() const override { return _width; }
    int height() const override { return _height; }
    void on_mouse_move(const MouseEvent *event) override;
    void on_mouse_out(const MouseEvent *event) override;
    void on_key_event(const KeyEvent *event) override;
    void on_mouse_wheel(const MouseWheelEvent *event) override;
    void flush_events() override;



    void load_file(const ByteBuffer &file_path);
    void edit_audio_file(AudioFile *audio_file);
    void set_pos(int left, int top);
    void set_size(int width, int height);

    void save_as(const ByteBuffer &file_path, ExportSampleFormat export_sample_format);

private:
    GuiWindow *_gui_window;
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
    glm::vec4 _playback_cursor_color;

    AudioFile *_audio_file; // protected by mutex

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
    Selection _playback_selection; // protected by mutex
    int _scroll_x; // number of pixels scrolled to the right
    double _frames_per_pixel;
    bool _select_down;
    bool _playback_select_down;

    AudioHardware *_audio_hardware;
    OpenPlaybackDevice *_playback_device;
    Thread _playback_thread;
    Mutex _playback_mutex;
    MutexCond _playback_cond;
    atomic_bool _playback_thread_join_flag;
    long _playback_write_head; // protected by mutex
    bool _playback_active; // protected by mutex
    double _playback_thread_wakeup_timeout;
    atomic_long _playback_cursor_frame;
    double _playback_device_latency;
    int _playback_device_sample_rate;

    SelectWidget *_select_playback_device;
    SelectWidget *_select_recording_device;

    OpenRecordingDevice *_recording_device;
    Thread _recording_thread;
    bool _recording_thread_created;
    Mutex _recording_mutex;
    MutexCond _recording_cond;
    atomic_bool _recording_thread_join_flag;
    double _recording_thread_wakeup_timeout;

    // kept in sync with _select_playback_device options
    List<const AudioDevice*> _playback_device_list;
    // kept in sync with _select_recording_device options
    List<const AudioDevice*> _recording_device_list;

    bool _initialized_default_device_indexes;

    atomic_bool _want_update_model;


    void update_model();

    void destroy_audio_file();
    void destroy_all_ui();
    void destroy_per_channel_data(PerChannelData *per_channel_data);
    PerChannelData *create_per_channel_data(int index);

    void init_selection(Selection &selection);
    bool get_frame_and_channel(int x, int y, CursorPosition *out);
    long get_timeline_frame(int x, int y);
    int wave_start_left() const;
    int wave_width() const;
    void scroll_cursor_into_view();
    void scroll_playback_cursor_into_view();
    void scroll_frame_into_view(long frame);
    void zoom_100();
    long get_display_frame_count() const;
    int get_full_wave_width() const;
    void delete_selection();
    void clamp_selection();
    void get_order_correct_selection(const Selection *selection, long *start, long *end);
    int timeline_left() const;
    int timeline_top() const;
    int timeline_width() const;

    long frame_at_pos(int x);
    int pos_at_frame(long frame);

    long timeline_frame_at_pos(int x);
    int timeline_pos_at_frame(long frame);

    void toggle_play();
    void restart_play();

    void close_playback_device();
    void open_playback_device();

    void playback_thread();
    void recording_thread();

    void clamp_playback_write_head(long start, long end);
    void calculate_playback_range(long *start, long *end);
    void clear_playback_buffer();
    void set_playback_selection(long start, long end);
    void clamp_scroll_x();
    void change_zoom_mouse_anchor(double new_frames_per_pixel, int anchor_pixel_x);
    void change_zoom_frame_anchor(double new_frames_per_pixel, long anchor_frame);
    void scroll_by(int x);
    int clamp_in_wave_x(int x);

    void on_devices_change();
    void open_recording_device();
    void close_recording_device();

    void on_playback_index_changed();

    static void static_playback_thread(void *arg) {
        return reinterpret_cast<AudioEditWidget*>(arg)->playback_thread();
    }

    static void static_recording_thread(void *arg) {
        return reinterpret_cast<AudioEditWidget*>(arg)->recording_thread();
    }

    static void static_on_devices_change(AudioHardware *audio_hardware) {
        return static_cast<AudioEditWidget*>(audio_hardware->_userdata)->on_devices_change();
    }

    static void static_on_playback_index_changed(SelectWidget *select_widget) {
        return static_cast<AudioEditWidget*>(select_widget->_userdata)->on_playback_index_changed();
    }

    AudioEditWidget(const AudioEditWidget &copy) = delete;
    AudioEditWidget &operator=(const AudioEditWidget &copy) = delete;
};

#endif
