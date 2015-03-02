#include "audio_edit_widget.hpp"
#include "byte_buffer.hpp"
#include "color.hpp"

#include <stdint.h>

static AudioFile *create_empty_audio_file() {
    AudioFile *audio_file = create<AudioFile>();
    audio_file->channels.resize(2);
    audio_file->channel_layout = genesis_get_channel_layout(ChannelLayoutIdStereo);
    audio_file->sample_rate = 48000;
    audio_file->export_sample_format = {SampleFormatInt32, false};
    audio_file->export_bit_rate = 320 * 1000;
    return audio_file;
}

AudioEditWidget::AudioEditWidget(Gui *gui) :
    _widget(Widget {
        destructor,
        draw,
        left,
        top,
        width,
        height,
        on_mouse_move,
        on_mouse_out,
        on_mouse_over,
        on_gain_focus,
        on_lose_focus,
        on_text_input,
        on_key_event,
        -1,
        true,
    }),
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
    _audio_file(create_empty_audio_file()),
    _scroll_x(0),
    _frames_per_pixel(1.0f),
    _select_down(false),
    _playback_select_down(false)
{
    init_selection(_selection);
    init_selection(_playback_selection);
}

AudioEditWidget::~AudioEditWidget() {
    destroy_audio_file();
    destroy_all_ui();
}

void AudioEditWidget::destroy_audio_file() {
    if (!_audio_file)
        return;
    destroy(_audio_file, 1);
    _audio_file = NULL;
}

void AudioEditWidget::destroy_all_ui() {
    for (long i = 0; i < _channel_list.length(); i += 1) {
        destroy_per_channel_data(_channel_list.at(i));
    }
    _channel_list.clear();
}

void AudioEditWidget::destroy_per_channel_data(PerChannelData *per_channel_data) {
    destroy(per_channel_data->waveform_texture, 1);
    _gui->destroy_widget(&per_channel_data->channel_name_widget->_widget);
    destroy(per_channel_data, 1);
}

void AudioEditWidget::load_file(const ByteBuffer &file_path) {
    AudioFile *audio_file = create_empty_audio_file();
    audio_file_load(file_path, audio_file);
    edit_audio_file(audio_file);
}

static String audio_file_channel_name(const AudioFile *audio_file, int index) {
    if (audio_file->channel_layout) {
        if (index < audio_file->channel_layout->channels.length())
            return genesis_get_channel_name(audio_file->channel_layout->channels.at(index));
        else
            return ByteBuffer::format("Channel %d (extra)", index + 1);
    } else {
        return ByteBuffer::format("Channel %d", index + 1);
    }
}

void AudioEditWidget::edit_audio_file(AudioFile *audio_file) {
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
    TextWidget *text_widget = create<TextWidget>(_gui);
    text_widget->set_text(channel_name);
    text_widget->set_background_on(false);
    text_widget->set_text_interaction(false);

    PerChannelData *per_channel_data = create<PerChannelData>();
    per_channel_data->channel_name_widget = text_widget;
    per_channel_data->waveform_texture = create<AlphaTexture>(_gui);

    return per_channel_data;
}

void AudioEditWidget::draw(const glm::mat4 &projection) {
    _gui->fill_rect(glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), _left, _top, _width, _height);

    _gui->fill_rect(_timeline_bg_color, _left + timeline_left(), _top + timeline_top(),
            timeline_width(), _timeline_height);

    for (long i = 0; i < _channel_list.length(); i += 1) {
        PerChannelData *per_channel_data = _channel_list.at(i);

        glm::mat4 mvp = projection * per_channel_data->waveform_model;
        per_channel_data->waveform_texture->draw(_waveform_fg_color, mvp);

        TextWidget *channel_name_widget = per_channel_data->channel_name_widget;
        if (channel_name_widget->_widget._is_visible)
            channel_name_widget->draw(projection);
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

    int sel_start_x = pos_at_frame(start);
    int sel_end_x = pos_at_frame(end);
    for (long i = 0; i < _channel_list.length(); i += 1) {
        PerChannelData *per_channel_data = _channel_list.at(i);

        if (!_selection.channels.at(i))
            continue;

        _gui->fill_rect(_waveform_sel_bg_color,
                _left + per_channel_data->left + sel_start_x, _top + per_channel_data->top,
                sel_end_x - sel_start_x, per_channel_data->height);
    }

    long playback_start = _playback_selection.start;
    long playback_end = _playback_selection.end;
    if (playback_start == playback_end)
        playback_end += 1;
    int playback_sel_start_x = timeline_pos_at_frame(playback_start);
    int playback_sel_end_x = timeline_pos_at_frame(playback_end);
    _gui->fill_rect(_timeline_sel_bg_color,
            _left + timeline_left() + playback_sel_start_x, _top + timeline_top(),
            playback_sel_end_x - playback_sel_start_x, _timeline_height);


    // draw the wave texture again with a stencil
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilMask(0x00);

    for (long i = 0; i < _channel_list.length(); i += 1) {
        PerChannelData *per_channel_data = _channel_list.at(i);

        if (!_selection.channels.at(i))
            continue;

        glm::mat4 mvp = projection * per_channel_data->waveform_model;
        per_channel_data->waveform_texture->draw(_waveform_sel_fg_color, mvp);
    }

    glDisable(GL_STENCIL_TEST);
}

int AudioEditWidget::wave_start_left() const {
    return _padding_left;
}

int AudioEditWidget::wave_width() const {
    return _width - _padding_left - _padding_right;
}

long AudioEditWidget::frame_at_pos(int x) {
    float percent_x = (x - wave_start_left() + _scroll_x) / (float)wave_width();
    percent_x = clamp(0.0f, percent_x, 1.0f);
    long frame_at_end = _frames_per_pixel * wave_width();
    long frame_count = get_display_frame_count();
    return min((long)(frame_at_end * percent_x), frame_count);
}

int AudioEditWidget::pos_at_frame(long frame) {
    return frame / _frames_per_pixel - _scroll_x;
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
        int max_scroll_x = max(0, get_full_wave_width() - wave_width());
        _scroll_x = clamp(0, _scroll_x, max_scroll_x);
        update_model();
    }
}

void AudioEditWidget::on_mouse_move(const MouseEvent *event) {
    switch (event->action) {
        case MouseActionDown:
            if (event->button != MouseButtonLeft)
                break;
            {
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
                    _playback_selection.start = frame;
                    _playback_selection.end = frame;
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
            if (!event->buttons.left)
                break;
            if (_select_down) {
                _selection.end = frame_at_pos(event->x);
                scroll_cursor_into_view();
            } else if (_playback_select_down) {
                _playback_selection.end = timeline_frame_at_pos(event->x);
                scroll_playback_cursor_into_view();
            }
            break;
        case MouseActionDbl:
            break;
    }
}

void AudioEditWidget::on_mouse_out(const MouseEvent *event) {
    SDL_SetCursor(_gui->_cursor_default);
}

void AudioEditWidget::on_mouse_over(const MouseEvent *event) {
    SDL_SetCursor(_gui->_cursor_ibeam);
}

void AudioEditWidget::on_gain_focus() {

}

void AudioEditWidget::on_lose_focus() {

}

void AudioEditWidget::on_text_input(const TextInputEvent *event) {

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
    }
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
    return _padding_top;
}

int AudioEditWidget::timeline_left() const {
    return _padding_left;
}

int AudioEditWidget::timeline_width() const {
    return _width - _padding_right - timeline_left();
}

void AudioEditWidget::update_model() {
    ByteBuffer pixels;
    int top = timeline_top() + _timeline_height + _margin;

    for (long i = 0; i < _channel_list.length(); i += 1) {
        PerChannelData *per_channel_data = _channel_list.at(i);
        per_channel_data->left = wave_start_left();
        per_channel_data->top = top;

        List<double> *samples = &_audio_file->channels.at(i).samples;

        TextWidget *text_widget = per_channel_data->channel_name_widget;
        text_widget->set_pos(_left + wave_start_left(), _top + top);

        int width = wave_width();
        int height = _channel_edit_height;
        per_channel_data->width = width;
        per_channel_data->height = _channel_edit_height;

        pixels.resize(width * height);
        for (int x = 0; x < width; x += 1) {
            long start_frame = x * _frames_per_pixel;
            if (start_frame < samples->length()) {
                long end_frame = min(
                        (long)((x + 1) * _frames_per_pixel),
                        samples->length());
                double min_sample =  1.0;
                double max_sample = -1.0;
                for (long frame = start_frame; frame < end_frame; frame += 1) {
                    double sample = samples->at(frame);
                    min_sample = min(min_sample, sample);
                    max_sample = max(max_sample, sample);
                }

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
