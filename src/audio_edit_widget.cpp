#include "audio_edit_widget.hpp"
#include "byte_buffer.hpp"

#include <groove/groove.h>
#include <stdint.h>

static AudioFile *create_empty_audio_file() {
    AudioFile *audio_file = create<AudioFile>();
    audio_file->channels.resize(2);
    audio_file->channel_layout = genesis_get_channel_layout(ChannelLayoutIdStereo);
    audio_file->channels.at(0).sample_rate = 44100;
    audio_file->channels.at(1).sample_rate = 44100;
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
    _padding_bottom(4),
    _channel_edit_height(100),
    _margin(2),
    _waveform_fg_color(0.25882f, 0.43137f, 0.772549, 1.0f),
    _waveform_bg_color(0.0f, 0.0f, 0.0f, 0.0f),
    _audio_file(create_empty_audio_file())
{
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
    for (int i = 0; i < _channel_list.length(); i += 1) {
        destroy_per_channel_data(_channel_list.at(i));
    }
    _channel_list.clear();
}

void AudioEditWidget::destroy_per_channel_data(PerChannelData *per_channel_data) {
    destroy(per_channel_data->waveform_texture, 1);
    _gui->destroy_widget(&per_channel_data->channel_name_widget->_widget);
    destroy(per_channel_data, 1);
}

static void import_buffer_uint8(const GrooveBuffer *buffer, AudioFile *audio_file) {
    uint8_t *ptr = buffer->data[0];
    double min = 0.0;
    double max = (double)UINT8_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (int frame = 0; frame < buffer->frame_count; frame += 1) {
        for (int ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 1) {
            uint8_t sample = *ptr;
            double dbl_sample = (((double)sample) - min) / half_range - 1.0;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

static void import_buffer_int16(const GrooveBuffer *buffer, AudioFile *audio_file) {
    uint8_t *ptr = buffer->data[0];
    double min = (double)INT16_MIN;
    double max = (double)INT16_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (int frame = 0; frame < buffer->frame_count; frame += 1) {
        for (int ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 2) {
            int16_t *sample = reinterpret_cast<int16_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / half_range - 1.0;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

static void import_buffer_int32(const GrooveBuffer *buffer, AudioFile *audio_file) {
    uint8_t *ptr = buffer->data[0];
    double min = (double)INT32_MIN;
    double max = (double)INT32_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (int frame = 0; frame < buffer->frame_count; frame += 1) {
        for (int ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 4) {
            int32_t *sample = reinterpret_cast<int32_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / half_range - 1.0;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

static void import_buffer_float(const GrooveBuffer *buffer, AudioFile *audio_file) {
    uint8_t *ptr = buffer->data[0];
    for (int frame = 0; frame < buffer->frame_count; frame += 1) {
        for (int ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 4) {
            float *sample = reinterpret_cast<float*>(ptr);
            double dbl_sample = *sample;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

static void import_buffer_double(const GrooveBuffer *buffer, AudioFile *audio_file) {
    uint8_t *ptr = buffer->data[0];
    for (int frame = 0; frame < buffer->frame_count; frame += 1) {
        for (int ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 8) {
            double *sample = reinterpret_cast<double*>(ptr);
            audio_file->channels.at(ch).samples.append(*sample);
        }
    }
}

static void import_buffer_uint8_planar(const GrooveBuffer *buffer, AudioFile *audio_file) {
    double min = 0.0;
    double max = (double)UINT8_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (int ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = buffer->data[ch];
        Channel *channel = &audio_file->channels.at(ch);
        for (int frame = 0; frame < buffer->frame_count; frame += 1, ptr += 1) {
            uint8_t sample = *ptr;
            double dbl_sample = (((double)sample) - min) / half_range - 1.0;
            channel->samples.append(dbl_sample);
        }
    }
}

static void import_buffer_int16_planar(const GrooveBuffer *buffer, AudioFile *audio_file) {
    double min = (double)INT16_MIN;
    double max = (double)INT16_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (int ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = buffer->data[ch];
        Channel *channel = &audio_file->channels.at(ch);
        for (int frame = 0; frame < buffer->frame_count; frame += 1, ptr += 2) {
            int16_t *sample = reinterpret_cast<int16_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / half_range - 1.0;
            channel->samples.append(dbl_sample);
        }
    }
}

static void import_buffer_int32_planar(const GrooveBuffer *buffer, AudioFile *audio_file) {
    double min = (double)INT32_MIN;
    double max = (double)INT32_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (int ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = buffer->data[ch];
        Channel *channel = &audio_file->channels.at(ch);
        for (int frame = 0; frame < buffer->frame_count; frame += 1, ptr += 4) {
            int32_t *sample = reinterpret_cast<int32_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / half_range - 1.0;
            channel->samples.append(dbl_sample);
        }
    }
}

static void import_buffer_float_planar(const GrooveBuffer *buffer, AudioFile *audio_file) {
    for (int ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = buffer->data[ch];
        Channel *channel = &audio_file->channels.at(ch);
        for (int frame = 0; frame < buffer->frame_count; frame += 1, ptr += 4) {
            float *sample = reinterpret_cast<float*>(ptr);
            double dbl_sample = *sample;
            channel->samples.append(dbl_sample);
        }
    }
}

static void import_buffer_double_planar(const GrooveBuffer *buffer, AudioFile *audio_file) {
    for (int ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = buffer->data[ch];
        Channel *channel = &audio_file->channels.at(ch);
        for (int frame = 0; frame < buffer->frame_count; frame += 1, ptr += 8) {
            double *sample = reinterpret_cast<double*>(ptr);
            channel->samples.append(*sample);
        }
    }
}

static const char *sample_format_names[] = {
    "none",
    "unsigned 8 bits",
    "signed 16 bits",
    "signed 32 bits",
    "float (32 bits)",
    "double (64 bits)",
    "unsigned 8 bits, planar",
    "signed 16 bits, planar",
    "signed 32 bits, planar",
    "float (32 bits), planar",
    "double (64 bits), planar",
};

static void debug_print_sample_format(enum GrooveSampleFormat sample_fmt) {
    size_t index = sample_fmt + 1;
    if (index < 0 || index >= array_length(sample_format_names))
        panic("invalid sample format");
    fprintf(stderr, "sample format: %s\n", sample_format_names[index]);
}

void AudioEditWidget::load_file(const ByteBuffer &file_path) {
    AudioFile *audio_file = create_empty_audio_file();

    GrooveFile *file = groove_file_open(file_path.raw());
    if (!file)
        panic("groove_file_open error");

    GrooveAudioFormat audio_format;
    groove_file_audio_format(file, &audio_format);

    debug_print_sample_format(audio_format.sample_fmt);

    audio_file->channel_layout = genesis_from_groove_channel_layout(audio_format.channel_layout);
    int channel_count = groove_channel_layout_count(audio_format.channel_layout);

    audio_file->channels.resize(channel_count);
    for (int i = 0; i < audio_file->channels.length(); i += 1) {
        audio_file->channels.at(i).sample_rate = audio_format.sample_rate;
        audio_file->channels.at(i).samples.clear();
    }

    GroovePlaylist *playlist = groove_playlist_create();
    GrooveSink *sink = groove_sink_create();
    if (!playlist || !sink)
        panic("out of memory");

    sink->audio_format = audio_format;
    sink->disable_resample = 1;

    int err = groove_sink_attach(sink, playlist);
    if (err)
        panic("error attaching sink");

    GroovePlaylistItem *item = groove_playlist_insert(
            playlist, file, 1.0, 1.0, NULL);
    if (!item)
        panic("out of memory");

    GrooveBuffer *buffer;
    while (groove_sink_buffer_get(sink, &buffer, 1) == GROOVE_BUFFER_YES) {
        if (buffer->format.sample_rate != audio_format.sample_rate) {
            panic("non-consistent sample rate: %d -> %d",
                    audio_format.sample_rate, buffer->format.sample_rate );
        }
        if (buffer->format.channel_layout != audio_format.channel_layout)
            panic("non-consistent channel layout");
        switch (buffer->format.sample_fmt) {
            default:
                panic("unrecognized sample format");
                break;
            case GROOVE_SAMPLE_FMT_U8:          /* unsigned 8 bits */
                import_buffer_uint8(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_S16:         /* signed 16 bits */
                import_buffer_int16(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_S32:         /* signed 32 bits */
                import_buffer_int32(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_FLT:         /* float (32 bits) */
                import_buffer_float(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_DBL:         /* double (64 bits) */
                import_buffer_double(buffer, audio_file);
                break;

            case GROOVE_SAMPLE_FMT_U8P:         /* unsigned 8 bits, planar */
                import_buffer_uint8_planar(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_S16P:        /* signed 16 bits, planar */
                import_buffer_int16_planar(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_S32P:        /* signed 32 bits, planar */
                import_buffer_int32_planar(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_FLTP:        /* float (32 bits), planar */
                import_buffer_float_planar(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_DBLP:         /* double (64 bits), planar */
                import_buffer_double_planar(buffer, audio_file);
                break;
        }
    }

    groove_sink_detach(sink);
    groove_playlist_clear(playlist);
    groove_playlist_destroy(playlist);
    groove_file_close(file);

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
    for (int i = 0; i < _audio_file->channels.length(); i += 1) {
        PerChannelData *per_channel_data = create_per_channel_data(i);
        _channel_list.append(per_channel_data);
    }
    update_model();
}

AudioEditWidget::PerChannelData *AudioEditWidget::create_per_channel_data(int i) {
    String channel_name = audio_file_channel_name(_audio_file, i);
    TextWidget *text_widget = create<TextWidget>(_gui);
    text_widget->set_text(channel_name);
    text_widget->set_background_on(false);
    text_widget->set_text_interaction(false);

    PerChannelData *per_channel_data = create<PerChannelData>();
    per_channel_data->channel_name_widget = text_widget;
    per_channel_data->waveform_texture = create<Texture>(_gui);

    return per_channel_data;
}

void AudioEditWidget::draw(const glm::mat4 &projection) {
    _gui->fill_rect(glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), _left, _top, _width, _height);

    for (int i = 0; i < _channel_list.length(); i += 1) {
        PerChannelData *per_channel_data = _channel_list.at(i);

        glm::mat4 mvp = projection * per_channel_data->waveform_model;
        per_channel_data->waveform_texture->draw(mvp);

        TextWidget *channel_name_widget = per_channel_data->channel_name_widget;
        if (channel_name_widget->_widget._is_visible)
            channel_name_widget->draw(projection);
    }
}

void AudioEditWidget::on_mouse_move(const MouseEvent *event) {

}

void AudioEditWidget::on_mouse_out(const MouseEvent *event) {

}

void AudioEditWidget::on_mouse_over(const MouseEvent *event) {

}

void AudioEditWidget::on_gain_focus() {

}

void AudioEditWidget::on_lose_focus() {

}

void AudioEditWidget::on_text_input(const TextInputEvent *event) {

}

void AudioEditWidget::on_key_event(const KeyEvent *event) {

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

static void vec4_color_to_uint8(const glm::vec4 &rgba, uint8_t *out) {
    out[0] = (uint8_t)(out[0] * 255.0f);
    out[1] = (uint8_t)(out[1] * 255.0f);
    out[2] = (uint8_t)(out[2] * 255.0f);
    out[3] = (uint8_t)(out[3] * 255.0f);
}

void AudioEditWidget::update_model() {
    ByteBuffer pixels;
    int top = _top + _padding_top;

    uint8_t color_fg[4];
    uint8_t color_bg[4];
    
    vec4_color_to_uint8(_waveform_fg_color, color_fg);
    vec4_color_to_uint8(_waveform_bg_color, color_bg);

    for (int i = 0; i < _channel_list.length(); i += 1) {
        PerChannelData *per_channel_data = _channel_list.at(i);
        List<double> *samples = &_audio_file->channels.at(i).samples;

        TextWidget *text_widget = per_channel_data->channel_name_widget;
        text_widget->set_pos(_left + _padding_left, top);

        int width = _width;
        int height = _channel_edit_height;
        int pitch = width * 4;
        double frames_per_pixel = samples->length() / width;
        pixels.resize(_height * pitch);
        for (int x = 0; x < width; x += 1) {
            double min_sample =  1.0;
            double max_sample = -1.0;
            for (int off = 0; off < frames_per_pixel; off += 1) {
                double sample = samples->at(x * frames_per_pixel + off);
                min_sample = min(min_sample, sample);
                max_sample = max(max_sample, sample);
            }

            int y_min = (min_sample + 1.0) / 2.0 * height;
            int y_max = (max_sample + 1.0) / 2.0 * height;
            int y = 0;

            // top bg
            for (; y < y_min; y += 1) {
                memcpy(pixels.raw() + (4 * (y * width + x)), color_bg, 4);
            }
            // top and bottom wave
            for (; y <= y_max; y += 1) {
                memcpy(pixels.raw() + (4 * (y * width + x)), color_fg, 4);
            }
            // bottom bg
            for (; y < height; y += 1) {
                memcpy(pixels.raw() + (4 * (y * width + x)), color_bg, 4);
            }
        }
        per_channel_data->waveform_texture->send_pixels(pixels, width, height);
        per_channel_data->waveform_model = glm::scale(
                    glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(_left + _padding_left, top, 0.0f)),
                    glm::vec3(
                        per_channel_data->waveform_texture->_width,
                        per_channel_data->waveform_texture->_height,
                        1.0f));

        top += _channel_edit_height + _margin;
    }
}
