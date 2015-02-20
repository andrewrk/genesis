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
    for (int i = 0; i < _channel_name_widgets.length(); i += 1) {
        TextWidget *text_widget = _channel_name_widgets.at(i);
        _gui->destroy_widget(&text_widget->_widget);
    }
    _channel_name_widgets.clear();
}

static void import_buffer_int32(const GrooveBuffer *buffer, AudioFile *audio_file) {
    uint8_t *ptr = buffer->data[0];
    double min = (double)INT32_MIN;
    double max = (double)INT32_MAX;
    double range = max - min;
    for (int frame = 0; frame < buffer->frame_count; frame += 1) {
        for (int ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 4) {
            int32_t *sample = reinterpret_cast<int32_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / range;
            audio_file->channels.at(ch).samples.append(dbl_sample);
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

static void import_buffer_int16_planar(const GrooveBuffer *buffer, AudioFile *audio_file) {
    double min = (double)INT16_MIN;
    double max = (double)INT16_MAX;
    double range = max - min;
    for (int ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = buffer->data[ch];
        for (int frame = 0; frame < buffer->frame_count; frame += 1, ptr += 2) {
            int16_t *sample = reinterpret_cast<int16_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / range;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

void AudioEditWidget::load_file(const ByteBuffer &file_path) {
    AudioFile *audio_file = create_empty_audio_file();

    GrooveFile *file = groove_file_open(file_path.raw());
    if (!file)
        panic("groove_file_open error");

    GrooveAudioFormat audio_format;
    groove_file_audio_format(file, &audio_format);

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
                panic("unsupported sample format: u8");
                break;
            case GROOVE_SAMPLE_FMT_S16:         /* signed 16 bits */
                panic("unsupported sample format: s16");
                break;
            case GROOVE_SAMPLE_FMT_S32:         /* signed 32 bits */
                import_buffer_int32(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_FLT:         /* float (32 bits) */
                panic("unsupported sample format: float");
                break;
            case GROOVE_SAMPLE_FMT_DBL:         /* double (64 bits) */
                panic("unsupported sample format: double");
                break;

            case GROOVE_SAMPLE_FMT_U8P:         /* unsigned 8 bits, planar */
                panic("unsupported sample format: u8 planar");
                break;
            case GROOVE_SAMPLE_FMT_S16P:        /* signed 16 bits, planar */
                import_buffer_int16_planar(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_S32P:        /* signed 32 bits, planar */
                panic("unsupported sample format: s32 planar");
                break;
            case GROOVE_SAMPLE_FMT_FLTP:        /* float (32 bits), planar */
                import_buffer_float_planar(buffer, audio_file);
                break;
            case GROOVE_SAMPLE_FMT_DBLP:         /* double (64 bits), planar */
                panic("unsupported sample format: double planar");
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
        String channel_name = audio_file_channel_name(_audio_file, i);
        TextWidget *text_widget = create<TextWidget>(_gui);
        text_widget->set_text(channel_name);
        text_widget->set_background_on(false);
        text_widget->set_text_interaction(false);
        _channel_name_widgets.append(text_widget);
    }
    update_model();
}

void AudioEditWidget::draw(const glm::mat4 &projection) {
    _gui->fill_rect(glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), _left, _top, _width, _height);

    for (int i = 0; i < _channel_name_widgets.length(); i += 1) {
        TextWidget *text_widget = _channel_name_widgets.at(i);
        if (text_widget->_widget._is_visible)
            text_widget->draw(projection);
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

void AudioEditWidget::update_model() {
    int y = _top + _padding_top;
    for (int i = 0; i < _channel_name_widgets.length(); i += 1) {
        TextWidget *text_widget = _channel_name_widgets.at(i);
        text_widget->set_pos(_left + _padding_left, y);
        y += text_widget->height();
    }
}
