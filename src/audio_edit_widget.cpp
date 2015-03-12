#include "audio_edit_widget.hpp"
#include "byte_buffer.hpp"
#include "color.hpp"
#include "audio_hardware.hpp"

#include <stdint.h>
#include <errno.h>

static AudioFile *create_empty_audio_file() {
    AudioFile *audio_file = create<AudioFile>();
    audio_file->channels.resize(2);
    audio_file->channel_layout = *genesis_get_channel_layout(ChannelLayoutIdStereo);
    audio_file->sample_rate = 48000;
    audio_file->export_sample_format = {SampleFormatInt32, false};
    audio_file->export_bit_rate = 320 * 1000;
    return audio_file;
}

AudioEditWidget::AudioEditWidget(GuiWindow *gui_window, Gui *gui, AudioHardware *audio_hardware) :
    Widget(),
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
    _audio_file(create_empty_audio_file()),
    _scroll_x(0),
    _frames_per_pixel(1.0f),
    _select_down(false),
    _playback_select_down(false),
    _audio_hardware(audio_hardware),
    _playback_device(NULL),
    _playback_thread_join_flag(false),
    _playback_write_head(0),
    _playback_active(false),
    _playback_cursor_frame(0),
    _recording_device(NULL),
    _recording_thread_join_flag(false),
    _initialized_default_device_indexes(false),
    _want_update_model(false)
{
    init_selection(_selection);
    init_selection(_playback_selection);

    _select_playback_device = create<SelectWidget>(gui_window, gui);
    _select_playback_device->_userdata = this;
    _select_playback_device->set_on_selected_index_change(static_on_playback_index_changed);

    _select_recording_device = create<SelectWidget>(gui_window, gui);
    _select_recording_device->_userdata = this;

    _audio_hardware->_userdata = this;
    _audio_hardware->set_on_devices_change(static_on_devices_change);
    on_devices_change();
}

AudioEditWidget::~AudioEditWidget() {
    _audio_hardware->clear_on_devices_change();

    close_recording_device();
    close_playback_device();
    destroy_audio_file();
    destroy_all_ui();

    _gui_window->destroy_widget(_select_recording_device);
    _gui_window->destroy_widget(_select_playback_device);
}

void AudioEditWidget::destroy_audio_file() {
    if (_audio_file) {
        destroy(_audio_file, 1);
        _audio_file = NULL;
    }
}

void AudioEditWidget::on_devices_change() {
    const AudioDevicesInfo *info = _audio_hardware->devices_info();

    _select_playback_device->clear();
    _select_recording_device->clear();
    _playback_device_list.clear();
    _recording_device_list.clear();

    int default_playback_index = 0;
    int default_recording_index = 0;

    if (!info) {
        update_model();
        return;
    }

    for (int i = 0; i < info->devices.length(); i += 1) {
        const AudioDevice *audio_device = &info->devices.at(i);

        if (audio_device->purpose == AudioDevicePurposePlayback) {
            int this_index = _playback_device_list.length();

            _select_playback_device->append_choice(audio_device->description);
            _playback_device_list.append(audio_device);

            if (info->default_output_index == i)
                default_playback_index = this_index;
        } else {
            int this_index = _recording_device_list.length();

            _select_recording_device->append_choice(audio_device->description);
            _recording_device_list.append(audio_device);

            if (info->default_input_index == i)
                default_recording_index = this_index;
        }
    }
    if (!_initialized_default_device_indexes) {
        _select_playback_device->select_index(default_playback_index);
        _select_recording_device->select_index(default_recording_index);

        _initialized_default_device_indexes = true;
    }

    open_playback_device();
    update_model();
}

void AudioEditWidget::destroy_all_ui() {
    for (long i = 0; i < _channel_list.length(); i += 1) {
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
    AudioFile *audio_file = create_empty_audio_file();
    audio_file_load(file_path, audio_file);
    edit_audio_file(audio_file);
}

void AudioEditWidget::close_playback_device() {
    if (_playback_thread.running()) {
        _playback_thread_join_flag = true;
        _playback_mutex.lock();
        _playback_cond.signal();
        _playback_mutex.unlock();
        _playback_thread.join();
    }

    if (_playback_device) {
        destroy(_playback_device, 1);
        _playback_device = nullptr;
    }
}

static String audio_file_channel_name(const AudioFile *audio_file, int index) {
    if (index < audio_file->channel_layout.channels.length())
        return genesis_get_channel_name(audio_file->channel_layout.channels.at(index));
    else
        return ByteBuffer::format("Channel %d (extra)", index + 1);
}

void AudioEditWidget::edit_audio_file(AudioFile *audio_file) {
    close_playback_device();
    destroy_audio_file();
    _audio_file = audio_file;

    destroy_all_ui();
    for (long i = 0; i < _audio_file->channels.length(); i += 1) {
        PerChannelData *per_channel_data = create_per_channel_data(i);
        _channel_list.append(per_channel_data);
    }
    init_selection(_selection);
    init_selection(_playback_selection);

    zoom_100();

    open_playback_device();
}

void AudioEditWidget::calculate_playback_range(long *start, long *end) {
    long audio_file_frame_count = get_display_frame_count();
    if (_playback_selection.start == _playback_selection.end) {
        *start = 0;
        *end = audio_file_frame_count;
    } else {
        if (_playback_selection.start > _playback_selection.end) {
            *start = _playback_selection.end;
            *end = _playback_selection.start;
        } else {
            *start = _playback_selection.start;
            *end = _playback_selection.end;
        }
        *start = max(0L, *start);
        *end = min(audio_file_frame_count, *end);
    }
}

void AudioEditWidget::clamp_playback_write_head(long start, long end) {
    _playback_write_head = start + euclidean_mod((_playback_write_head - start), end - start);
}

void AudioEditWidget::playback_thread() {
    while (!_playback_thread_join_flag) {
        _playback_mutex.lock();

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

            _playback_cursor_frame = _playback_write_head -
                (_playback_device_latency * _playback_device_sample_rate);
        } else {
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

void AudioEditWidget::flush_events() {
    if (_want_update_model) {
        _want_update_model = false;
        update_model();
    }
}

void AudioEditWidget::recording_thread() {
    while (!_recording_thread_join_flag) {
        _recording_mutex.lock();

        int bytes_per_frame = get_bytes_per_frame(SampleFormatFloat, _audio_file->channels.length());
        const char *data;
        int byte_count;
        _recording_device->peek(&data, &byte_count);
        int frame_count = byte_count / bytes_per_frame;
        // TODO handle the case where buffer contains part of a frame
        /*
        if (frame_count * bytes_per_frame != byte_count)
            panic("partial frame");
            */
        int channel_count = _audio_file->channels.length();
        if (!data && byte_count > 0) {
            // there's a hole. fill with silence
            fprintf(stderr, "recording: dropped %d frames\n", frame_count);
            for (int i = 0; i < frame_count; i += 1) {
                for (int ch = 0; ch < channel_count; ch += 1) {
                    _audio_file->channels.at(ch).samples.append(0.0f);
                }
            }
            _recording_device->drop();
            _want_update_model = true;
        } else if (data) {
            fprintf(stderr, "got %d frames\n", frame_count);
            const float *sample_ptr = reinterpret_cast<const float*>(data);
            for (int i = 0; i < frame_count; i += 1) {
                for (int ch = 0; ch < channel_count; ch += 1) {
                    float sample = *sample_ptr;
                    _audio_file->channels.at(ch).samples.append(sample);
                    sample_ptr += 1;
                }
            }
            _recording_device->drop();
            _want_update_model = true;
        }

        _recording_cond.timed_wait(&_recording_mutex, _recording_thread_wakeup_timeout);
        _recording_mutex.unlock();
    }
}

void AudioEditWidget::open_playback_device() {
    if (_playback_device || _playback_device_list.length() == 0)
        return;

    _playback_device_latency = 0.008;
    _playback_device_sample_rate = _audio_file->sample_rate;

    const AudioDevice *selected_playback_device = _playback_device_list.at(_select_playback_device->selected_index());

    bool ok;
    _playback_device = create<OpenPlaybackDevice>(_audio_hardware,
            selected_playback_device->name.encode().raw(),
            &_audio_file->channel_layout, SampleFormatFloat, _playback_device_latency,
            _audio_file->sample_rate, &ok);

    if (!ok)
        panic("could not open playback device");

    _playback_thread_join_flag = false;
    _playback_thread_wakeup_timeout = _playback_device_latency / 10.0;
    _playback_thread.start(static_playback_thread, this);
}

long AudioEditWidget::get_display_frame_count() const {
    if (!_audio_file)
        return 0;
    if (_audio_file->channels.length() == 0)
        return 0;
    long largest = 0;
    for (long i = 0; i < _audio_file->channels.length(); i += 1) {
        largest = max(largest, _audio_file->channels.at(i).samples.length());
    }
    return largest;
}

void AudioEditWidget::zoom_100() {
    _frames_per_pixel = get_display_frame_count() / (double)wave_width();
    update_model();
}

void AudioEditWidget::init_selection(Selection &selection) {
    selection.start = 0;
    selection.end = 0;
    selection.channels.resize(_audio_file->channels.length());
    for (long i = 0; i < selection.channels.length(); i += 1) {
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

    for (long i = 0; i < _channel_list.length(); i += 1) {
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
    for (long i = 0; i < _channel_list.length(); i += 1) {
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

    for (long i = 0; i < _channel_list.length(); i += 1) {
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
    long frame_count = get_display_frame_count();
    return clamp(0L, frame, frame_count);
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
    for (long channel = 0; channel < _channel_list.length(); channel += 1) {
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
    return get_display_frame_count() / _frames_per_pixel;
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
        update_model();
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
    update_model();
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
                if (_recording_device) {
                    close_recording_device();
                } else {
                    open_recording_device();
                }
            }
            break;
    }
}

void AudioEditWidget::open_recording_device() {
    if (_recording_device || _recording_device_list.length() == 0)
        return;

    const AudioDevice *selected_recording_device = _recording_device_list.at(_select_recording_device->selected_index());

    AudioFile *audio_file = create<AudioFile>();
    audio_file->channels.resize(selected_recording_device->channel_layout.channels.length());
    audio_file->channel_layout = selected_recording_device->channel_layout;
    audio_file->sample_rate = selected_recording_device->default_sample_rate;
    audio_file->export_sample_format = {SampleFormatInt32, false};
    audio_file->export_bit_rate = 320 * 1000;

    edit_audio_file(audio_file);

    _frames_per_pixel = 500.0;

    double recording_latency = 0.17;

    bool ok;
    _recording_device = create<OpenRecordingDevice>(_audio_hardware,
            selected_recording_device->name.encode().raw(),
            &selected_recording_device->channel_layout, SampleFormatFloat,
            recording_latency, selected_recording_device->default_sample_rate, &ok);

    if (!ok)
        panic("could not open recording device");

    _recording_thread_join_flag = false;
    _recording_thread_wakeup_timeout = recording_latency / 10.0;
    _recording_thread.start(static_recording_thread, this);
}

void AudioEditWidget::close_recording_device() {
    if (_recording_thread.running()) {
        _recording_thread_join_flag = true;
        _recording_mutex.lock();
        _recording_cond.signal();
        _recording_mutex.unlock();
        _recording_thread.join();
    }

    if (_recording_device) {
        destroy(_recording_device, 1);
        _recording_device = nullptr;
    }
}

void AudioEditWidget::scroll_by(int x) {
    _scroll_x += x;
    clamp_scroll_x();
    update_model();
}

void AudioEditWidget::clear_playback_buffer() {
    _playback_device->clear_buffer();
    _playback_cond.signal();
}

void AudioEditWidget::toggle_play() {
    if (!_playback_device)
        return;

    _playback_mutex.lock();

    _playback_active = !_playback_active;
    clear_playback_buffer();

    _playback_mutex.unlock();
}

void AudioEditWidget::restart_play() {
    if (!_playback_device)
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
    for (long i = 0; i < _audio_file->channels.length(); i += 1) {
        if (start >= _audio_file->channels.at(i).samples.length())
            continue;
        _audio_file->channels.at(i).samples.remove_range(start, end);
    }
    _selection.start = min(_selection.start, _selection.end);
    _selection.end = _selection.start;
    clamp_selection();
    update_model();
}

void AudioEditWidget::set_pos(int left, int top) {
    _left = left;
    _top = top;
    update_model();
}

void AudioEditWidget::set_size(int width, int height) {
    _width = width;
    _height = height;
    update_model();
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

    for (long i = 0; i < _channel_list.length(); i += 1) {
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
                        (long)((x + _scroll_x + 1) * _frames_per_pixel),
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
}

void AudioEditWidget::clamp_selection() {
    long frame_count = get_display_frame_count();
    _selection.start = min(_selection.start, frame_count);
    _selection.end = min(_selection.end, frame_count);
}

void AudioEditWidget::save_as(const ByteBuffer &file_path,
        ExportSampleFormat export_sample_format)
{
    _audio_file->export_sample_format = export_sample_format;
    audio_file_save(file_path, NULL, NULL, _audio_file);
}

void AudioEditWidget::on_playback_index_changed() {
    close_playback_device();
    open_playback_device();
}
