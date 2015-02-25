#include "audio_file.hpp"
#include "libav.hpp"

#include <groove/groove.h>
#include <stdint.h>


static const int supported_sample_rates[] = {
    8000,
    16000,
    32000,
    44100,
    48000,
    96000,
};

static void import_buffer_uint8(const GrooveBuffer *buffer, AudioFile *audio_file) {
    uint8_t *ptr = buffer->data[0];
    double min = 0.0;
    double max = (double)UINT8_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (int frame = 0; frame < buffer->frame_count; frame += 1) {
        for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 1) {
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
        for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 2) {
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
        for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 4) {
            int32_t *sample = reinterpret_cast<int32_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / half_range - 1.0;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

static void import_buffer_float(const GrooveBuffer *buffer, AudioFile *audio_file) {
    uint8_t *ptr = buffer->data[0];
    for (int frame = 0; frame < buffer->frame_count; frame += 1) {
        for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 4) {
            float *sample = reinterpret_cast<float*>(ptr);
            double dbl_sample = *sample;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

static void import_buffer_double(const GrooveBuffer *buffer, AudioFile *audio_file) {
    uint8_t *ptr = buffer->data[0];
    for (int frame = 0; frame < buffer->frame_count; frame += 1) {
        for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 8) {
            double *sample = reinterpret_cast<double*>(ptr);
            audio_file->channels.at(ch).samples.append(*sample);
        }
    }
}

static void import_buffer_uint8_planar(const GrooveBuffer *buffer, AudioFile *audio_file) {
    double min = 0.0;
    double max = (double)UINT8_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1) {
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
    for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1) {
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
    for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1) {
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
    for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1) {
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
    for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1) {
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
    int index = sample_fmt + 1;
    if (index < 0 || (size_t)index >= array_length(sample_format_names))
        panic("invalid sample format");
    fprintf(stderr, "sample format: %s\n", sample_format_names[index]);
}

void audio_file_load(const ByteBuffer &file_path, AudioFile *audio_file) {
    GrooveFile *file = groove_file_open(file_path.raw());
    if (!file)
        panic("groove_file_open error");

    GrooveTag *tag = NULL;
    audio_file->tags.clear();
    while ((tag = groove_file_metadata_get(file, "", tag, 0))) {
        bool ok;
        String value(groove_tag_value(tag), &ok);
        audio_file->tags.put(groove_tag_key(tag), value);
    }

    GrooveAudioFormat audio_format;
    groove_file_audio_format(file, &audio_format);

    debug_print_sample_format(audio_format.sample_fmt);

    audio_file->channel_layout = genesis_from_libav_channel_layout(audio_format.channel_layout);
    audio_file->sample_rate = audio_format.sample_rate;
    int channel_count = groove_channel_layout_count(audio_format.channel_layout);

    audio_file->channels.resize(channel_count);
    for (size_t i = 0; i < audio_file->channels.length(); i += 1) {
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
}

static void get_format_and_codec(const char *format_short_name, const char *codec_short_name,
        const char *filename, AVOutputFormat **oformat, AVCodec **codec)
{
    *codec = NULL;
    *oformat = av_guess_format(format_short_name, filename, NULL);
    if (!*oformat)
        return;
    // av_guess_codec ignores mime_type, filename, and codec_short_name. see
    // https://bugzilla.libav.org/show_bug.cgi?id=580
    // because of this we do a workaround to return the correct codec based on
    // the codec_short_name.
    if (codec_short_name) {
        *codec = avcodec_find_encoder_by_name(codec_short_name);
        if (!*codec) {
            const AVCodecDescriptor *desc = avcodec_descriptor_get_by_name(codec_short_name);
            if (desc)
                *codec = avcodec_find_encoder(desc->id);
        }
    }
    if (!*codec) {
        enum AVCodecID codec_id = av_guess_codec(*oformat, codec_short_name,
                filename, NULL, AVMEDIA_TYPE_AUDIO);
        *codec = avcodec_find_encoder(codec_id);
        if (!*codec)
            return;
    }
}

static bool codec_supports_sample_rate(const AVCodec *codec, int sample_rate) {
    if (!codec->supported_samplerates)
        return true;

    const int *p = codec->supported_samplerates;
    while (*p) {
        if (*p == sample_rate)
            return true;

        p += 1;
    }

    return false;
}

void get_supported_sample_rates(const char *format_short_name,
        const char *codec_short_name, const char *filename, List<int> &out)
{
    out.clear();

    AVOutputFormat *oformat;
    AVCodec *codec;
    get_format_and_codec(format_short_name, codec_short_name, filename, &oformat, &codec);
    if (!oformat || !codec)
        panic("could not find codec");

    for (size_t i = 0; i < array_length(supported_sample_rates); i += 1) {
        int sample_rate = supported_sample_rates[i];
        if (codec_supports_sample_rate(codec, sample_rate))
            out.append(sample_rate);
    }
}

static bool codec_supports_sample_format(const AVCodec *codec, SampleFormat format) {
    if (!codec->sample_fmts)
        return true;

    const enum AVSampleFormat *p = (enum AVSampleFormat*) codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == AV_SAMPLE_FMT_U8 && format == SampleFormatUInt8)
            return true;

        if (*p == AV_SAMPLE_FMT_S16 && format == SampleFormatInt16)
            return true;

        if (*p == AV_SAMPLE_FMT_S32 && format == SampleFormatInt32)
            return true;

        if (*p == AV_SAMPLE_FMT_FLT && format == SampleFormatFloat)
            return true;

        if (*p == AV_SAMPLE_FMT_DBL && format == SampleFormatDouble)
            return true;

        p += 1;
    }

    return false;
}

void get_supported_sample_formats(const char *format_short_name,
        const char *codec_short_name, const char *filename, List<SampleFormat> &out)
{
    out.clear();

    AVOutputFormat *oformat;
    AVCodec *codec;
    get_format_and_codec(format_short_name, codec_short_name, filename, &oformat, &codec);
    if (!oformat || !codec)
        panic("could not find codec");

    if (codec_supports_sample_format(codec, SampleFormatUInt8))
        out.append(SampleFormatUInt8);

    if (codec_supports_sample_format(codec, SampleFormatInt16))
        out.append(SampleFormatInt16);

    if (codec_supports_sample_format(codec, SampleFormatInt32))
        out.append(SampleFormatInt32);

    if (codec_supports_sample_format(codec, SampleFormatFloat))
        out.append(SampleFormatFloat);

    if (codec_supports_sample_format(codec, SampleFormatDouble))
        out.append(SampleFormatDouble);
}

static int abs_diff(int a, int b) {
    int n = a - b;
    return n >= 0 ? n : -n;
}

static uint64_t closest_supported_channel_layout(AVCodec *codec, uint64_t target) {
    // if we can, return exact match. otherwise, return the layout with the
    // minimum number of channels >= target

    if (!codec->channel_layouts)
        return target;

    int target_count = av_get_channel_layout_nb_channels(target);
    const uint64_t *p = codec->channel_layouts;
    uint64_t best = *p;
    int best_count = av_get_channel_layout_nb_channels(best);
    while (*p) {
        if (*p == target)
            return target;

        int count = av_get_channel_layout_nb_channels(*p);
        if ((best_count < target_count && count > best_count) ||
            (count >= target_count &&
             abs_diff(target_count, count) < abs_diff(target_count, best_count)))
        {
            best_count = count;
            best = *p;
        }

        p += 1;
    }

    return best;
}

static AVSampleFormat to_libav_sample_format(SampleFormat format) {
    switch (format) {
        case SampleFormatUInt8:  return AV_SAMPLE_FMT_U8;
        case SampleFormatInt16:  return AV_SAMPLE_FMT_S16;
        case SampleFormatInt32:  return AV_SAMPLE_FMT_S32;
        case SampleFormatFloat:  return AV_SAMPLE_FMT_FLT;
        case SampleFormatDouble: return AV_SAMPLE_FMT_DBL;
    }
    panic("unrecognized sample format");
}

static void write_sample_uint8(double sample, uint8_t *ptr) {
    *ptr = (uint8_t)((sample * 127.5) + 127.5);
}

static void write_sample_int16(double sample, uint8_t *ptr) {
    int16_t *int_ptr = reinterpret_cast<int16_t*>(ptr);
    *int_ptr = (int16_t)(sample * 32767.0);
}

static void write_sample_int32(double sample, uint8_t *ptr) {
    int32_t *int_ptr = reinterpret_cast<int32_t*>(ptr);
    *int_ptr = (int32_t)(sample * 2147483647.0);
}

static void write_sample_float(double sample, uint8_t *ptr) {
    float *float_ptr = reinterpret_cast<float*>(ptr);
    *float_ptr = (float)sample;
}

static void write_sample_double(double sample, uint8_t *ptr) {
    double *double_ptr = reinterpret_cast<double*>(ptr);
    *double_ptr = sample;
}

void audio_file_save(const ByteBuffer &file_path, const char *format_short_name,
        const char *codec_short_name, const AudioFile *audio_file)
{
    AVCodec *codec;
    AVOutputFormat *oformat;
    get_format_and_codec(format_short_name, codec_short_name, file_path.raw(), &oformat, &codec);

    uint64_t out_channel_layout = closest_supported_channel_layout(
            codec, genesis_to_libav_channel_layout(audio_file->channel_layout));

    AVFormatContext *fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx)
        panic("unable to allocate format context");

    int err = avio_open(&fmt_ctx->pb, file_path.raw(), AVIO_FLAG_WRITE);
    if (err < 0) {
        char buf[256];
        av_strerror(err, buf, sizeof(buf));
        panic("could not open %s: %s", file_path.raw(), buf);
    }

    fmt_ctx->oformat = oformat;

    AVStream *stream = avformat_new_stream(fmt_ctx, codec);
    if (!stream)
        panic("unable to create output stream");

    AVCodecContext *codec_ctx = stream->codec;
    codec_ctx->bit_rate = audio_file->export_bit_rate;
    codec_ctx->sample_fmt = to_libav_sample_format(audio_file->export_sample_format);
    codec_ctx->sample_rate = audio_file->sample_rate;
    codec_ctx->channel_layout = out_channel_layout;
    codec_ctx->channels = audio_file->channels.length();
    codec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    err = avcodec_open2(codec_ctx, codec, NULL);
    if (err < 0) {
        char buf[256];
        av_strerror(err, buf, sizeof(buf));
        panic("unable to open codec: %s", buf);
    }

    // copy metadata to format context
    av_dict_free(&fmt_ctx->metadata);
    auto it = audio_file->tags.entry_iterator();
    for (;;) {
        auto *entry = it.next();
        if (!entry)
            break;
        av_dict_set(&fmt_ctx->metadata, entry->key.raw(), entry->value.encode().raw(),
                AV_DICT_IGNORE_SUFFIX);
    }

    err = avformat_write_header(fmt_ctx, NULL);
    if (err < 0)
        panic("error writing header");


    AVFrame *frame = av_frame_alloc();
    if (!frame)
        panic("error allocating frame");

    int buffer_size = av_samples_get_buffer_size(NULL, codec_ctx->channels,
            codec_ctx->frame_size, codec_ctx->sample_fmt, 0);
    int bytes_per_sample = av_get_bytes_per_sample(codec_ctx->sample_fmt);
    uint8_t *buffer = allocate<uint8_t>(buffer_size);

    err = avcodec_fill_audio_frame(frame, codec_ctx->channels, codec_ctx->sample_fmt,
            buffer, buffer_size, 0);
    if (err < 0)
        panic("error setting up audio frame");

    // set up write_frames fn pointer
    void (*write_sample)(double, uint8_t *);

    switch (audio_file->export_sample_format) {
        case SampleFormatUInt8:
            write_sample = write_sample_uint8;;
            break;
        case SampleFormatInt16:
            write_sample = write_sample_int16;
            break;
        case SampleFormatInt32:
            write_sample = write_sample_int32;
            break;
        case SampleFormatFloat:
            write_sample = write_sample_float;
            break;
        case SampleFormatDouble:
            write_sample = write_sample_double;
            break;
    }

    AVPacket pkt;
    size_t start = 0;
    uint8_t *buffer_ptr = buffer;
    for (;;) {
        av_init_packet(&pkt);
        pkt.data = NULL; // packet data will be allocated by the encoder
        pkt.size = 0;

        int frame_count = buffer_size / bytes_per_sample / codec_ctx->channels;
        size_t end = start + frame_count;
        for (size_t i = start; i < end; i += 1) {
            for (size_t ch = 0; ch < audio_file->channels.length(); ch += 1) {
                write_sample(audio_file->channels.at(ch).samples.at(i), buffer_ptr);
                buffer_ptr += bytes_per_sample;
            }
        }
        start = end;

        int got_packet = 0;
        err = avcodec_encode_audio2(codec_ctx, &pkt, frame, &got_packet);
        if (err < 0) {
            char buf[256];
            av_strerror(err, buf, sizeof(buf));
            panic("error encoding audio frame: %s", buf);
        }
        if (got_packet) {
            err = av_write_frame(fmt_ctx, &pkt);
            if (err < 0)
                panic("error writing frame");
            av_free_packet(&pkt);
        }
    }
    for (;;) {
        int got_packet = 0;
        err = avcodec_encode_audio2(codec_ctx, &pkt, NULL, &got_packet);
        if (err < 0) {
            char buf[256];
            av_strerror(err, buf, sizeof(buf));
            panic("error encoding audio frame: %s", buf);
        }
        if (got_packet) {
            err = av_write_frame(fmt_ctx, &pkt);
            if (err < 0)
                panic("error writing frame");
            av_free_packet(&pkt);
        } else {
            break;
        }
    }
    for (;;) {
        err = av_write_frame(fmt_ctx, NULL);
        if (err < 0) {
            panic("error flushing format");
        } else if (err == 1) {
            break;
        }
    }

    destroy(buffer, buffer_size);
    av_frame_free(&frame);

    err = av_write_trailer(fmt_ctx);
    if (err < 0)
        panic("error writing trailer");

    avformat_free_context(fmt_ctx); // this frees stream too
    avcodec_close(codec_ctx);
}

void audio_file_init(void) {
    avcodec_register_all();
    av_register_all();
}
