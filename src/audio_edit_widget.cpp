#include "audio_edit_widget.hpp"
#include "byte_buffer.hpp"
#include "color.hpp"

#include <stdint.h>
#include <errno.h>

static void noop(void *arg) {}

static void audio_file_node_seek(struct GenesisNode *node) {
    // TODO
}

static void audio_file_node_run(struct GenesisNode *node) {
    // TODO
    /*
void AudioEditWidget::playback_thread() {
    while (!_playback_thread_join_flag) {
        _playback_mutex.lock();

        _audio_file_mutex.lock();

        int free_byte_count = _playback_device->writable_size();
        int bytes_per_frame = get_bytes_per_frame(SampleFormatInt32, _audio_file->channels.length());
        int free_frame_count = free_byte_count / bytes_per_frame;
        if (_playback_active) {
            long playback_range_start, playback_range_end;
            calculate_playback_range(&playback_range_start, &playback_range_end);

            while (free_frame_count > 0) {
                char *buffer_ptr;
                int buffer_size = free_frame_count * bytes_per_frame;
                _playback_device->begin_write(&buffer_ptr, &buffer_size);
                float *samples = reinterpret_cast<float*>(buffer_ptr);
                int this_buffer_frame_count = buffer_size / bytes_per_frame;
                free_frame_count -= this_buffer_frame_count;

                clamp_playback_write_head(playback_range_start, playback_range_end);
                int channel_count = _audio_file->channels.length();
                for (int i = 0; i < this_buffer_frame_count; i += 1) {
                    for (int ch = 0; ch < channel_count; ch += 1) {
                        float sample = _audio_file->channels.at(ch).samples.at(_playback_write_head);
                        samples[i * channel_count + ch] = sample;
                    }
                    _playback_write_head += 1;
                    clamp_playback_write_head(playback_range_start, playback_range_end);
                }
                _playback_device->write(buffer_ptr, this_buffer_frame_count * bytes_per_frame);
            }

            _audio_file_mutex.unlock();

            _playback_cursor_frame = _playback_write_head -
                (_playback_device_latency * _playback_device_sample_rate);
        } else {
            _audio_file_mutex.unlock();

            // write zeroes
            while (free_frame_count > 0) {
                char *buffer_ptr;
                int buffer_size = free_frame_count * bytes_per_frame;
                _playback_device->begin_write(&buffer_ptr, &buffer_size);
                int this_buffer_frame_count = buffer_size / bytes_per_frame;
                free_frame_count -= this_buffer_frame_count;
                memset(buffer_ptr, 0, this_buffer_frame_count * bytes_per_frame);
                _playback_device->write(buffer_ptr, this_buffer_frame_count * bytes_per_frame);
            }
        }
        _playback_cond.timed_wait(&_playback_mutex, _playback_thread_wakeup_timeout);
        _playback_mutex.unlock();
    }
}
void AudioEditWidget::recording_thread() {
    while (!_recording_thread_join_flag) {
        _recording_mutex.lock();

        _audio_file_mutex.lock();
        for (;;) {
            const char *data;
            int byte_count;
            _recording_device->peek(&data, &byte_count);

            if (byte_count > 0) {
                int bytes_per_frame = get_bytes_per_frame(SampleFormatFloat, _audio_file->channels.length());
                int frame_count = byte_count / bytes_per_frame;
                if (frame_count * bytes_per_frame != byte_count)
                    panic("partial frame");
                int channel_count = _audio_file->channels.length();
                if (data) {
                    fprintf(stderr, "got %d frames\n", frame_count);
                    const float *sample_ptr = reinterpret_cast<const float*>(data);
                    for (int i = 0; i < frame_count; i += 1) {
                        for (int ch = 0; ch < channel_count; ch += 1) {
                            float sample = *sample_ptr;
                            _audio_file->channels.at(ch).samples.append(sample);
                            sample_ptr += 1;
                        }
                    }
                } else {
                    // there's a hole. fill with silence
                    fprintf(stderr, "recording: dropped %d frames\n", frame_count);
                    for (int i = 0; i < frame_count; i += 1) {
                        for (int ch = 0; ch < channel_count; ch += 1) {
                            _audio_file->channels.at(ch).samples.append(0.0f);
                        }
                    }
                }
                _recording_device->drop();
                schedule_update_model();
            } else {
                break;
            }
        }
        _audio_file_mutex.unlock();

        _recording_cond.timed_wait(&_recording_mutex, _recording_thread_wakeup_timeout);
        _recording_mutex.unlock();
    }
}

*/

}

static void static_on_devices_change(void *arg) {
    return reinterpret_cast<AudioEditWidget*>(arg)->on_devices_change();
}

static void static_on_playback_index_changed(SelectWidget *select_widget) {
    return static_cast<AudioEditWidget*>(select_widget->_userdata)->on_playback_index_changed();
}

AudioEditWidget::AudioEditWidget(GuiWindow *gui_window, Gui *gui, GenesisContext *genesis_context) :
    Widget(),
    _genesis_context(genesis_context),
    _gui_window(gui_window),
    _gui(gui),
    _left(0),
    _top(0),
    _width(500),
    _height(300),
    _padding_left(4),
    _padding_right(4),
    _padding_top(4),
    //_padding_bottom(4),
    _channel_edit_height(100),
    _margin(2),
    _timeline_height(16),
    _waveform_fg_color(0.25882f, 0.43137f, 0.772549, 1.0f),
    _waveform_bg_color(0.0f, 0.0f, 0.0f, 0.0f),
    _waveform_sel_bg_color(0.2f, 0.2314f, 0.4784f, 1.0f),
    _waveform_sel_fg_color(0.6745f, 0.69f, 0.8157f, 1.0f),
    _timeline_bg_color(parse_color("#979AA5")),
    _timeline_fg_color(0.851f, 0.867f, 0.914f, 1.0f),
    _timeline_sel_bg_color(parse_color("#4D505C")),
    _timeline_sel_fg_color(0.851f, 0.867f, 0.914f, 1.0f),
    _playback_cursor_color(0.0f, 0.0f, 0.0f, 1.0f),
    _scroll_x(0),
    _frames_per_pixel(1.0f),
    _select_down(false),
    _playback_select_down(false),
    _playback_write_head(0),
    _playback_active(false),
    _playback_cursor_frame(0),
    _audio_file_node_descr(nullptr),
    _playback_node_descr(nullptr),
    _recording_node_descr(nullptr),
    _audio_in_port_descr(nullptr),
    _audio_out_port_descr(nullptr),
    _audio_file_node(nullptr),
    _playback_node(nullptr),
    _recording_node(nullptr),
    _initialized_default_device_indexes(false),
    _want_update_model(false)
{
    if (_playback_mutex.error())
        panic("mutex init error");

    _audio_file = create_empty_audio_file();

    init_selection(_selection);
    init_selection(_playback_selection);

    _select_playback_device = create<SelectWidget>(gui_window, gui);
    _select_playback_device->_userdata = this;
    _select_playback_device->set_on_selected_index_change(static_on_playback_index_changed);

    _select_recording_device = create<SelectWidget>(gui_window, gui);
    _select_recording_device->_userdata = this;

    genesis_set_audio_device_callback(_genesis_context, static_on_devices_change, this);

    _audio_file_node_descr = genesis_create_node_descriptor(_genesis_context, 2,
            "audio_file", "Listen to the audio file you are editing.");
    if (!_audio_file_node_descr)
        panic("out of memory");
    genesis_node_descriptor_set_userdata(_audio_file_node_descr, this);
    genesis_node_descriptor_set_run_callback(_audio_file_node_descr, audio_file_node_run);
    genesis_node_descriptor_set_seek_callback(_audio_file_node_descr, audio_file_node_seek);

    _audio_out_port_descr = genesis_node_descriptor_create_port(
            _audio_file_node_descr, 0, GenesisPortTypeAudioOut, "audio_out");
    _audio_in_port_descr = genesis_node_descriptor_create_port(
            _audio_file_node_descr, 1, GenesisPortTypeAudioOut, "audio_in");
    if (!_audio_out_port_descr || !_audio_in_port_descr)
        panic("out of memory");
}

AudioEditWidget::~AudioEditWidget() {
    genesis_set_audio_device_callback(_genesis_context, noop, nullptr);

    close_recording_device();
    close_playback_device();
    destroy_audio_file();
    destroy_all_ui();

    _gui_window->destroy_widget(_select_recording_device);
    _gui_window->destroy_widget(_select_playback_device);
}

GenesisAudioFile *AudioEditWidget::create_empty_audio_file() {
    GenesisAudioFile *audio_file = genesis_audio_file_create(_genesis_context);
    if (!audio_file)
        panic("out of memory");
    return audio_file;
}

void AudioEditWidget::destroy_audio_file() {
    if (_audio_file) {
        destroy(_audio_file, 1);
        _audio_file = NULL;
    }
}

void AudioEditWidget::on_devices_change() {
    _select_playback_device->clear();
    _select_recording_device->clear();

    int audio_count = genesis_get_audio_device_count(_genesis_context);
    if (audio_count < 0)
        panic("no audio devices found");

    int default_playback_index = genesis_get_default_playback_device_index(_genesis_context);
    int default_recording_index = genesis_get_default_recording_device_index(_genesis_context);

    for (int i = 0; i < audio_count; i += 1) {
        struct GenesisAudioDevice *device = genesis_get_audio_device(_genesis_context, i);
        if (genesis_audio_device_purpose(device) == GenesisAudioDevicePurposePlayback) {
            _select_playback_device->append_choice(genesis_audio_device_description(device));
        } else {
            _select_recording_device->append_choice(genesis_audio_device_description(device));
        }
    }
    if (!_initialized_default_device_indexes) {
        _select_playback_device->select_index(default_playback_index);
        _select_recording_device->select_index(default_recording_index);

        _initialized_default_device_indexes = true;
    }

    open_playback_device();
    schedule_update_model();
}

void AudioEditWidget::destroy_all_ui() {
    for (int i = 0; i < _channel_list.length(); i += 1) {
        destroy_per_channel_data(_channel_list.at(i));
    }
    _channel_list.clear();
}

void AudioEditWidget::destroy_per_channel_data(PerChannelData *per_channel_data) {
    destroy(per_channel_data->waveform_texture, 1);
    _gui_window->destroy_widget(per_channel_data->channel_name_widget);
    destroy(per_channel_data, 1);
}

void AudioEditWidget::load_file(const ByteBuffer &file_path) {
    GenesisAudioFile *audio_file;
    int err = genesis_audio_file_load(_genesis_context, file_path.raw(), &audio_file);
    if (err)
        panic("error loading file: %s", genesis_error_string(err));

    edit_audio_file(audio_file);
}

void AudioEditWidget::close_playback_device() {
    if (_playback_node) {
        genesis_stop_pipeline(_genesis_context);
        genesis_node_destroy(_playback_node);
        _playback_node = nullptr;
        genesis_node_descriptor_destroy(_playback_node_descr);
        _playback_node_descr = nullptr;
    }
}

static String audio_file_channel_name(const GenesisAudioFile *audio_file, int index) {
    if (index < audio_file->channel_layout.channel_count)
        return genesis_get_channel_name(audio_file->channel_layout.channels[index]);
    else
        return ByteBuffer::format("Channel %d (extra)", index + 1);
}

void AudioEditWidget::edit_audio_file(GenesisAudioFile *audio_file) {
    close_playback_device();
    close_recording_device();
    destroy_audio_file();
    _audio_file = audio_file;

    destroy_all_ui();
    for (int i = 0; i < _audio_file->channels.length(); i += 1) {
        PerChannelData *per_channel_data = create_per_channel_data(i);
        if (_channel_list.append(per_channel_data))
            panic("out of memory");
    }
    init_selection(_selection);
    init_selection(_playback_selection);

    _display_frame_count = calc_display_frame_count();

    zoom_100();

    open_playback_device();
}

void AudioEditWidget::calculate_playback_range(long *start, long *end) {
    if (_playback_selection.start == _playback_selection.end) {
        *start = 0;
        *end = _display_frame_count;
    } else {
        if (_playback_selection.start > _playback_selection.end) {
            *start = _playback_selection.end;
            *end = _playback_selection.start;
        } else {
            *start = _playback_selection.start;
            *end = _playback_selection.end;
        }
        *start = max(0L, *start);
        *end = min(_display_frame_count, *end);
    }
}

void AudioEditWidget::clamp_playback_write_head(long start, long end) {
    _playback_write_head = start + euclidean_mod((_playback_write_head - start), end - start);
}

void AudioEditWidget::flush_events() {
    if (_want_update_model) {
        _want_update_model = false;
        update_model();
    }
}

void AudioEditWidget::schedule_update_model() {
    _want_update_model = true;
}

void AudioEditWidget::open_playback_device() {
    if (_playback_node)
        return;

    // TODO: this selected index is off because the select boxes are split
    GenesisAudioDevice *selected_playback_device = genesis_get_audio_device(
            _genesis_context, _select_playback_device->selected_index());
    if (!selected_playback_device)
        panic("unable to get playback device %d", _select_playback_device->selected_index());

    int err = genesis_audio_device_create_node_descriptor(selected_playback_device, &_playback_node_descr);
    if (err)
        panic("error instantiating playback device: %s", genesis_error_string(err));

    genesis_audio_port_descriptor_set_channel_layout(_audio_out_port_descr,
            genesis_audio_file_channel_layout(_audio_file), true, -1);

    genesis_audio_port_descriptor_set_sample_rate(_audio_out_port_descr,
            genesis_audio_file_sample_rate(_audio_file), true, -1);

    _audio_file_node = genesis_node_descriptor_create_node(_audio_file_node_descr);
    if (!_audio_file_node)
        panic("error creating audio file node");

    _playback_node = genesis_node_descriptor_create_node(_playback_node_descr);
    if (!_playback_node)
        panic("error creating playback node");

    err = genesis_connect_audio_nodes(_audio_file_node, _playback_node);
    if (err)
        panic("unable to connect audio ports: %s\n", genesis_error_string(err));
}

long AudioEditWidget::calc_display_frame_count() const {
    if (!_audio_file)
        return 0;
    if (_audio_file->channels.length() == 0)
        return 0;
    int largest = 0;
    for (int i = 0; i < _audio_file->channels.length(); i += 1) {
        largest = max(largest, _audio_file->channels.at(i).samples.length());
    }
    return largest;
}

void AudioEditWidget::zoom_100() {
    _frames_per_pixel = _display_frame_count / (double)wave_width();
    schedule_update_model();
}

void AudioEditWidget::init_selection(Selection &selection) {
    selection.start = 0;
    selection.end = 0;
    if (selection.channels.resize(_audio_file->channels.length()))
        panic("out of memory");
    for (int i = 0; i < selection.channels.length(); i += 1) {
        selection.channels.at(i) = true;
    }
}

AudioEditWidget::PerChannelData *AudioEditWidget::create_per_channel_data(int i) {
    String channel_name = audio_file_channel_name(_audio_file, i);
    TextWidget *text_widget = create<TextWidget>(_gui_window, _gui);
    text_widget->set_text(channel_name);
    text_widget->set_background_on(false);
    text_widget->set_text_interaction(false);

    PerChannelData *per_channel_data = create<PerChannelData>();
    per_channel_data->channel_name_widget = text_widget;
    per_channel_data->waveform_texture = create<AlphaTexture>(_gui);

    return per_channel_data;
}

int AudioEditWidget::clamp_in_wave_x(int x) {
    return clamp(wave_start_left(), x, wave_start_left() + wave_width() - 1);
}

void AudioEditWidget::draw(GuiWindow *window, const glm::mat4 &projection) {
    window->fill_rect(glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), _left, _top, _width, _height);

    _select_playback_device->draw(window, projection);
    _select_recording_device->draw(window, projection);

    window->fill_rect(_timeline_bg_color, _left + timeline_left(), _top + timeline_top(),
            timeline_width(), _timeline_height);

    for (int i = 0; i < _channel_list.length(); i += 1) {
        PerChannelData *per_channel_data = _channel_list.at(i);

        glm::mat4 mvp = projection * per_channel_data->waveform_model;
        per_channel_data->waveform_texture->draw(window, _waveform_fg_color, mvp);

        TextWidget *channel_name_widget = per_channel_data->channel_name_widget;
        if (channel_name_widget->_is_visible)
            channel_name_widget->draw(window, projection);
    }

    long start = _selection.start;
    long end = _selection.end;
    if (start == end)
        end += 1;

    // fill selection rectangle and stencil at the same time
    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    glClear(GL_STENCIL_BUFFER_BIT);

    int sel_start_x = clamp_in_wave_x(pos_at_frame(start));
    int sel_end_x = clamp_in_wave_x(pos_at_frame(end));
    for (int i = 0; i < _channel_list.length(); i += 1) {
        PerChannelData *per_channel_data = _channel_list.at(i);

        if (!_selection.channels.at(i))
            continue;

        window->fill_rect(_waveform_sel_bg_color,
                _left + sel_start_x, _top + per_channel_data->top,
                sel_end_x - sel_start_x, per_channel_data->height);
    }

    long playback_start = _playback_selection.start;
    long playback_end = _playback_selection.end;
    if (playback_start == playback_end)
        playback_end += 1;
    int playback_sel_start_x = clamp_in_wave_x(timeline_pos_at_frame(playback_start));
    int playback_sel_end_x = clamp_in_wave_x(timeline_pos_at_frame(playback_end));
    window->fill_rect(_timeline_sel_bg_color,
            _left + playback_sel_start_x, _top + timeline_top(),
            playback_sel_end_x - playback_sel_start_x, _timeline_height);


    // draw the wave texture again with a stencil
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilMask(0x00);

    for (int i = 0; i < _channel_list.length(); i += 1) {
        PerChannelData *per_channel_data = _channel_list.at(i);

        if (!_selection.channels.at(i))
            continue;

        glm::mat4 mvp = projection * per_channel_data->waveform_model;
        per_channel_data->waveform_texture->draw(window, _waveform_sel_fg_color, mvp);
    }

    glDisable(GL_STENCIL_TEST);

    int cursor_pos_x = pos_at_frame(_playback_cursor_frame);
    if (cursor_pos_x >= wave_start_left() && cursor_pos_x < wave_start_left() + wave_width()) {
        window->fill_rect(_playback_cursor_color,
                _left + cursor_pos_x, _top + timeline_top(), 2, _timeline_height);
    }

}

int AudioEditWidget::wave_start_left() const {
    return _padding_left;
}

int AudioEditWidget::wave_width() const {
    return _width - _padding_left - _padding_right;
}

long AudioEditWidget::frame_at_pos(int x) {
    long frame = (x - wave_start_left() + _scroll_x) * _frames_per_pixel;
    return clamp(0L, frame, _display_frame_count);
}

int AudioEditWidget::pos_at_frame(long frame) {
    return frame / _frames_per_pixel - _scroll_x + wave_start_left();
}

long AudioEditWidget::timeline_frame_at_pos(int x) {
    return frame_at_pos(x);
}

int AudioEditWidget::timeline_pos_at_frame(long frame) {
    return pos_at_frame(frame);
}

bool AudioEditWidget::get_frame_and_channel(int x, int y, CursorPosition *out) {
    for (int channel = 0; channel < _channel_list.length(); channel += 1) {
        PerChannelData *per_channel_data = _channel_list.at(channel);
        if (x >= per_channel_data->left &&
            x < per_channel_data->left + per_channel_data->width &&
            y >= per_channel_data->top &&
            y < per_channel_data->top + per_channel_data->height)
        {
            out->frame = frame_at_pos(x);
            out->channel = channel;
            return true;
        }
    }
    return false;
}

long AudioEditWidget::get_timeline_frame(int x, int y) {
    if (x >= timeline_left() && x < timeline_left() + timeline_width() &&
        y >= timeline_top() && y < timeline_top() + _timeline_height)
    {
        return timeline_frame_at_pos(x);
    } else {
        return -1;
    }
}

int AudioEditWidget::get_full_wave_width() const {
    return _display_frame_count / _frames_per_pixel;
}

void AudioEditWidget::scroll_cursor_into_view() {
    return scroll_frame_into_view(_selection.end);
}

void AudioEditWidget::scroll_playback_cursor_into_view() {
    return scroll_frame_into_view(_playback_selection.end);
}

void AudioEditWidget::scroll_frame_into_view(long frame) {
    int x = pos_at_frame(frame);
    bool scrolled = false;

    int cursor_too_far_right = x - wave_width();
    if (cursor_too_far_right > 0) {
        _scroll_x += cursor_too_far_right;
        scrolled = true;
    }

    int cursor_too_far_left = -x + wave_start_left();
    if (cursor_too_far_left > 0) {
        _scroll_x -= cursor_too_far_left;
        scrolled = true;
    }

    if (scrolled) {
        clamp_scroll_x();
        schedule_update_model();
    }
}

void AudioEditWidget::clamp_scroll_x() {
    int max_scroll_x = max(0, get_full_wave_width() - wave_width());
    _scroll_x = clamp(0, _scroll_x, max_scroll_x);
}

void AudioEditWidget::set_playback_selection(long start, long end) {
    _playback_mutex.lock();

    _playback_selection.start = start;
    _playback_selection.end = end;

    _playback_mutex.unlock();
}

void AudioEditWidget::on_mouse_move(const MouseEvent *event) {
    if (_gui_window->try_mouse_move_event_on_widget(_select_playback_device, event))
        return;
    if (_gui_window->try_mouse_move_event_on_widget(_select_recording_device, event))
        return;

    switch (event->action) {
        case MouseActionDown:
            if (event->button == MouseButtonLeft) {
                CursorPosition pos;
                if (get_frame_and_channel(event->x, event->y, &pos)) {
                    _selection.start = pos.frame;
                    _selection.end = pos.frame;
                    _selection.channels.fill(true);
                    _select_down = true;
                    scroll_cursor_into_view();
                    break;
                }
                long frame = get_timeline_frame(event->x, event->y);
                if (frame >= 0) {
                    set_playback_selection(frame, frame);
                    _playback_select_down = true;
                    scroll_playback_cursor_into_view();
                    break;
                }
            }
            break;
        case MouseActionUp:
            if (event->button != MouseButtonLeft)
                break;

            if (_select_down)
                _select_down = false;

            if (_playback_select_down)
                _playback_select_down = false;

            break;
        case MouseActionMove:
            {
                CursorPosition pos;
                if (get_timeline_frame(event->x, event->y) != -1 ||
                   get_frame_and_channel(event->x, event->y, &pos))
                {
                    _gui_window->set_cursor_beam();
                } else {
                    _gui_window->set_cursor_default();
                }
                if (event->buttons.left) {
                    if (_select_down) {
                        _selection.end = frame_at_pos(event->x);
                        scroll_cursor_into_view();
                    } else if (_playback_select_down) {
                        set_playback_selection(_playback_selection.start, timeline_frame_at_pos(event->x));
                        scroll_playback_cursor_into_view();
                    }
                }
            }
            break;
    }
}

void AudioEditWidget::change_zoom_mouse_anchor(double new_frames_per_pixel, int anchor_pixel_x) {
    change_zoom_frame_anchor(new_frames_per_pixel, frame_at_pos(anchor_pixel_x));
}

void AudioEditWidget::change_zoom_frame_anchor(double new_frames_per_pixel, long anchor_frame) {
    int old_x = pos_at_frame(anchor_frame);
    _frames_per_pixel = new_frames_per_pixel;
    int new_x = pos_at_frame(anchor_frame);
    _scroll_x += (new_x - old_x);
    clamp_scroll_x();
    schedule_update_model();
}

void AudioEditWidget::on_mouse_wheel(const MouseWheelEvent *event) {
    if (key_mod_ctrl(event->modifiers)) {
        if (event->wheel_y > 0) {
            change_zoom_mouse_anchor(_frames_per_pixel * 0.8, event->x);
        } else if (event->wheel_y < 0) {
            change_zoom_mouse_anchor(_frames_per_pixel * 1.2, event->x);
        }
    }
}

void AudioEditWidget::on_mouse_out(const MouseEvent *event) {
    _gui_window->set_cursor_default();
}

void AudioEditWidget::on_key_event(const KeyEvent *event) {
    if (event->action != KeyActionDown)
        return;

    switch (event->virt_key) {
        default:
            break;
        case VirtKeyDelete:
            delete_selection();
            break;
        case VirtKeySpace:
            toggle_play();
            break;
        case VirtKeyEnter:
            restart_play();
            break;
        case VirtKey0:
            zoom_100();
            break;
        case VirtKeyRight:
            scroll_by(32);
            break;
        case VirtKeyLeft:
            scroll_by(-32);
            break;
        case VirtKeyR:
            if (key_mod_only_ctrl(event->modifiers)) {
                if (_recording_node) {
                    close_recording_device();
                } else {
                    open_recording_device();
                }
            }
            break;
    }
}

void AudioEditWidget::open_recording_device() {
    if (_recording_node)
        return;

    // TODO: this selected index is off because the select boxes are split
    GenesisAudioDevice *selected_recording_device = genesis_get_audio_device(
            _genesis_context, _select_recording_device->selected_index());

    if (!selected_recording_device)
        panic("unable to get recording device %d", _select_recording_device->selected_index());

    GenesisAudioFile *audio_file = genesis_audio_file_create(_genesis_context);
    if (genesis_audio_file_set_channel_layout(audio_file,
            genesis_audio_device_channel_layout(selected_recording_device)))
    {
        panic("out of memory");
    }
    genesis_audio_file_set_sample_rate(audio_file,
            genesis_audio_device_sample_rate(selected_recording_device));

    genesis_audio_port_descriptor_set_channel_layout(_audio_in_port_descr,
            genesis_audio_device_channel_layout(selected_recording_device), true, -1);

    genesis_audio_port_descriptor_set_sample_rate(_audio_in_port_descr,
            genesis_audio_device_sample_rate(selected_recording_device), true, -1);

    edit_audio_file(audio_file);

    _frames_per_pixel = 500.0;

    int err = genesis_audio_device_create_node_descriptor(selected_recording_device,
            &_recording_node_descr);
    if (err)
        panic("error creating recording node descriptor: %s", genesis_error_string(err));

    _recording_node = genesis_node_descriptor_create_node(_recording_node_descr);
    if (!_recording_node)
        panic("unable to create recording node");

    err = genesis_connect_audio_nodes(_recording_node, _audio_file_node);
    if (err)
        panic("error connecting audio nodes: %s", genesis_error_string(err));
}

void AudioEditWidget::close_recording_device() {
    if (_recording_node) {
        genesis_stop_pipeline(_genesis_context);
        genesis_node_destroy(_recording_node);
        _recording_node = nullptr;
        genesis_node_descriptor_destroy(_recording_node_descr);
        _recording_node_descr = nullptr;
    }
}

void AudioEditWidget::scroll_by(int x) {
    _scroll_x += x;
    clamp_scroll_x();
    schedule_update_model();
}

void AudioEditWidget::clear_playback_buffer() {
    panic("TODO: clear_playback_buffer");
}

void AudioEditWidget::toggle_play() {
    if (!_playback_node)
        return;

    _playback_mutex.lock();

    _playback_active = !_playback_active;
    clear_playback_buffer();

    _playback_mutex.unlock();
}

void AudioEditWidget::restart_play() {
    if (!_playback_node)
        return;

    _playback_mutex.lock();

    _playback_write_head = _playback_selection.start;
    _playback_active = true;
    clear_playback_buffer();

    _playback_mutex.unlock();
}

void AudioEditWidget::get_order_correct_selection(const Selection *selection,
        long *start, long *end)
{
    if (selection->start > selection->end) {
        *start = selection->end;
        *end = selection->start;
    } else {
        *start = selection->start;
        *end = selection->end;
    }
}

void AudioEditWidget::delete_selection() {
    long start, end;
    get_order_correct_selection(&_selection, &start, &end);

    _audio_file_mutex.lock();
    for (int i = 0; i < _audio_file->channels.length(); i += 1) {
        if (start >= _audio_file->channels.at(i).samples.length())
            continue;
        _audio_file->channels.at(i).samples.remove_range(start, end);
    }

    _selection.start = min(_selection.start, _selection.end);
    _selection.end = _selection.start;
    clamp_selection();
    _audio_file_mutex.unlock();

    schedule_update_model();
}

void AudioEditWidget::set_pos(int left, int top) {
    _left = left;
    _top = top;
    schedule_update_model();
}

void AudioEditWidget::set_size(int width, int height) {
    _width = width;
    _height = height;
    schedule_update_model();
}

int AudioEditWidget::timeline_top() const {
    int button_row_height = max(_select_playback_device->height(), _select_recording_device->height());
    return _padding_top + button_row_height + _margin;
}

int AudioEditWidget::timeline_left() const {
    return _padding_left;
}

int AudioEditWidget::timeline_width() const {
    return _width - _padding_right - timeline_left();
}

void AudioEditWidget::update_model() {
    _select_playback_device->set_pos(_left + _margin, _top + _margin);
    _select_recording_device->set_pos(
            _select_playback_device->left() + _select_playback_device->width() + _margin * 2,
            _select_playback_device->top());

    ByteBuffer pixels;
    int top = timeline_top() + _timeline_height + _margin;

    _audio_file_mutex.lock();
    _display_frame_count = calc_display_frame_count();

    for (int i = 0; i < _channel_list.length(); i += 1) {
        PerChannelData *per_channel_data = _channel_list.at(i);
        per_channel_data->left = wave_start_left();
        per_channel_data->top = top;

        List<float> *samples = &_audio_file->channels.at(i).samples;

        TextWidget *text_widget = per_channel_data->channel_name_widget;
        text_widget->set_pos(_left + wave_start_left(), _top + top);

        int width = wave_width();
        int height = _channel_edit_height;
        per_channel_data->width = width;
        per_channel_data->height = _channel_edit_height;

        pixels.resize(width * height);
        for (int x = 0; x < width; x += 1) {
            long start_frame = (x + _scroll_x) * _frames_per_pixel;
            if (start_frame < samples->length()) {
                long end_frame = min(
                        (x + _scroll_x + 1) * _frames_per_pixel,
                        samples->length());
                float min_sample =  1.0f;
                float max_sample = -1.0f;
                for (long frame = start_frame; frame < end_frame; frame += 1) {
                    float sample = samples->at(frame);
                    min_sample = min(min_sample, sample);
                    max_sample = max(max_sample, sample);
                }
                min_sample = clamp(-1.0f, min_sample, 1.0f);
                max_sample = clamp(-1.0f, max_sample, 1.0f);

                int y_min = (min_sample + 1.0) / 2.0 * height;
                int y_max = (max_sample + 1.0) / 2.0 * height;
                int y = 0;

                // top bg
                for (; y < y_min; y += 1) {
                    pixels.raw()[y * width + x] = 0;
                }
                // top and bottom wave
                for (; y <= y_max; y += 1) {
                    pixels.raw()[y * width + x] = 255;
                }
                // bottom bg
                for (; y < height; y += 1) {
                    pixels.raw()[y * width + x] = 0;
                }
            } else {
                // the waveform does not go into this column
                for (int y = 0; y < height; y += 1) {
                    pixels.raw()[y * width + x] = 0;
                }
            }
        }
        per_channel_data->waveform_texture->send_pixels(pixels, width, height);
        per_channel_data->waveform_model = glm::scale(
                    glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(_left + _padding_left, _top + top, 0.0f)),
                    glm::vec3(
                        per_channel_data->waveform_texture->_width,
                        per_channel_data->waveform_texture->_height,
                        1.0f));

        top += _channel_edit_height + _margin;
    }

    _audio_file_mutex.unlock();
}

void AudioEditWidget::clamp_selection() {
    _selection.start = min(_selection.start, _display_frame_count);
    _selection.end = min(_selection.end, _display_frame_count);
}

void AudioEditWidget::save_as(const ByteBuffer &file_path, GenesisExportFormat *export_format) {
    int sample_rate = genesis_audio_file_sample_rate(_audio_file);
    if (!genesis_audio_file_codec_supports_sample_rate(export_format->codec, sample_rate))
        panic("unsupported sample rate");
    export_format->sample_rate = sample_rate;

    _audio_file_mutex.lock();

    int err = genesis_audio_file_export(_audio_file, file_path.raw(), export_format);
    if (err != GenesisErrorNone)
        panic("error saving file: %s", genesis_error_string(err));

    _audio_file_mutex.unlock();
}

void AudioEditWidget::on_playback_index_changed() {
    close_playback_device();
    open_playback_device();
}
