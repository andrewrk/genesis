#include "audio_file.hpp"
#include "libav.hpp"

#include <stdint.h>

static const ExportSampleFormat prioritized_export_formats[] = {
    {SampleFormatInt32, false},
    {SampleFormatFloat, false},
    {SampleFormatInt16, false},
    {SampleFormatDouble, false},
    {SampleFormatUInt8, false},

    {SampleFormatInt32, true},
    {SampleFormatFloat, true},
    {SampleFormatInt16, true},
    {SampleFormatDouble, true},
    {SampleFormatUInt8, true},
};

static const int prioritized_sample_rates[] = {
    48000,
    44100,
    96000,
    32000,
    16000,
    8000,
};

static void import_frame_uint8(const AVFrame *avframe, AudioFile *audio_file) {
    uint8_t *ptr = avframe->extended_data[0];
    double min = 0.0;
    double max = (double)UINT8_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (int frame = 0; frame < avframe->nb_samples; frame += 1) {
        for (long ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 1) {
            uint8_t sample = *ptr;
            double dbl_sample = (((double)sample) - min) / half_range - 1.0;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

static void import_frame_int16(const AVFrame *avframe, AudioFile *audio_file) {
    uint8_t *ptr = avframe->extended_data[0];
    double min = (double)INT16_MIN;
    double max = (double)INT16_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (int frame = 0; frame < avframe->nb_samples; frame += 1) {
        for (long ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 2) {
            int16_t *sample = reinterpret_cast<int16_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / half_range - 1.0;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

static void import_frame_int32(const AVFrame *avframe, AudioFile *audio_file) {
    uint8_t *ptr = avframe->extended_data[0];
    double min = (double)INT32_MIN;
    double max = (double)INT32_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (int frame = 0; frame < avframe->nb_samples; frame += 1) {
        for (long ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 4) {
            int32_t *sample = reinterpret_cast<int32_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / half_range - 1.0;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

static void import_frame_float(const AVFrame *avframe, AudioFile *audio_file) {
    uint8_t *ptr = avframe->extended_data[0];
    for (int frame = 0; frame < avframe->nb_samples; frame += 1) {
        for (long ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 4) {
            float *sample = reinterpret_cast<float*>(ptr);
            double dbl_sample = *sample;
            audio_file->channels.at(ch).samples.append(dbl_sample);
        }
    }
}

static void import_frame_double(const AVFrame *avframe, AudioFile *audio_file) {
    uint8_t *ptr = avframe->extended_data[0];
    for (int frame = 0; frame < avframe->nb_samples; frame += 1) {
        for (long ch = 0; ch < audio_file->channels.length(); ch += 1, ptr += 8) {
            double *sample = reinterpret_cast<double*>(ptr);
            audio_file->channels.at(ch).samples.append(*sample);
        }
    }
}

static void import_frame_uint8_planar(const AVFrame *avframe, AudioFile *audio_file) {
    double min = 0.0;
    double max = (double)UINT8_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = avframe->extended_data[ch];
        Channel *channel = &audio_file->channels.at(ch);
        for (int frame = 0; frame < avframe->nb_samples; frame += 1, ptr += 1) {
            uint8_t sample = *ptr;
            double dbl_sample = (((double)sample) - min) / half_range - 1.0;
            channel->samples.append(dbl_sample);
        }
    }
}

static void import_frame_int16_planar(const AVFrame *avframe, AudioFile *audio_file) {
    double min = (double)INT16_MIN;
    double max = (double)INT16_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = avframe->extended_data[ch];
        Channel *channel = &audio_file->channels.at(ch);
        for (int frame = 0; frame < avframe->nb_samples; frame += 1, ptr += 2) {
            int16_t *sample = reinterpret_cast<int16_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / half_range - 1.0;
            channel->samples.append(dbl_sample);
        }
    }
}

static void import_frame_int32_planar(const AVFrame *avframe, AudioFile *audio_file) {
    double min = (double)INT32_MIN;
    double max = (double)INT32_MAX;
    double half_range = max / 2.0 - min / 2.0;
    for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = avframe->extended_data[ch];
        Channel *channel = &audio_file->channels.at(ch);
        for (int frame = 0; frame < avframe->nb_samples; frame += 1, ptr += 4) {
            int32_t *sample = reinterpret_cast<int32_t*>(ptr);
            double dbl_sample = (((double)*sample) - min) / half_range - 1.0;
            channel->samples.append(dbl_sample);
        }
    }
}

static void import_frame_float_planar(const AVFrame *avframe, AudioFile *audio_file) {
    for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = avframe->extended_data[ch];
        Channel *channel = &audio_file->channels.at(ch);
        for (int frame = 0; frame < avframe->nb_samples; frame += 1, ptr += 4) {
            float *sample = reinterpret_cast<float*>(ptr);
            double dbl_sample = *sample;
            channel->samples.append(dbl_sample);
        }
    }
}

static void import_frame_double_planar(const AVFrame *avframe, AudioFile *audio_file) {
    for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ptr = avframe->extended_data[ch];
        Channel *channel = &audio_file->channels.at(ch);
        for (int frame = 0; frame < avframe->nb_samples; frame += 1, ptr += 8) {
            double *sample = reinterpret_cast<double*>(ptr);
            channel->samples.append(*sample);
        }
    }
}

static ExportSampleFormat from_libav_sample_format(AVSampleFormat format) {
    switch (format) {
        default:
            panic("invalid sample format");
        case AV_SAMPLE_FMT_U8:
            return {SampleFormatUInt8, false};
        case AV_SAMPLE_FMT_U8P:
            return {SampleFormatUInt8, true};
        case AV_SAMPLE_FMT_S16:
            return {SampleFormatInt16, false};
        case AV_SAMPLE_FMT_S16P:
            return {SampleFormatInt16, true};
        case AV_SAMPLE_FMT_S32:
            return {SampleFormatInt32, false};
        case AV_SAMPLE_FMT_S32P:
            return {SampleFormatInt32, true};
        case AV_SAMPLE_FMT_FLT:
            return {SampleFormatFloat, false};
        case AV_SAMPLE_FMT_FLTP:
            return {SampleFormatFloat, true};
        case AV_SAMPLE_FMT_DBL:
            return {SampleFormatDouble, false};
        case AV_SAMPLE_FMT_DBLP:
            return {SampleFormatDouble, true};
    }
}

static int decode_interrupt_cb(void *ctx) {
    return 0;
}

static int decode_frame(AudioFile *audio_file, AVPacket *pkt,
        AVCodecContext *codec_ctx, AVFrame *in_frame,
        void (*import_frame)(const AVFrame *, AudioFile *))
{
    AVPacket pkt_temp = *pkt;
    bool new_packet = true;
    int decoded_byte_count = 0;
    while (pkt_temp.size > 0 || (!pkt_temp.data && new_packet)) {
        new_packet = false;
        int got_frame;
        int len1 = avcodec_decode_audio4(codec_ctx, in_frame, &got_frame, &pkt_temp);
        if (len1 < 0) {
            char buf[256];
            av_strerror(len1, buf, sizeof(buf));
            fprintf(stderr, "error decoding audio: %s\n", buf);
            return -1;
        }
        pkt_temp.data += len1;
        pkt_temp.size -= len1;

        if (!got_frame) {
            // stop sending empty packets if the decoder is finished
            if (!pkt_temp.data && (codec_ctx->codec->capabilities & CODEC_CAP_DELAY))
                break;
            continue;
        }

        import_frame(in_frame, audio_file);
        decoded_byte_count += in_frame->nb_samples *
            av_get_bytes_per_sample((AVSampleFormat)in_frame->format) *
            av_get_channel_layout_nb_channels(in_frame->channel_layout);
    }

    return decoded_byte_count;
}

static GenesisChannelId from_libav_channel_id(uint64_t libav_channel_id) {
    switch (libav_channel_id) {
        case AV_CH_FRONT_LEFT:            return GenesisChannelIdFrontLeft;
        case AV_CH_FRONT_RIGHT:           return GenesisChannelIdFrontRight;
        case AV_CH_FRONT_CENTER:          return GenesisChannelIdFrontCenter;
        case AV_CH_LOW_FREQUENCY:         return GenesisChannelIdLowFrequency;
        case AV_CH_BACK_LEFT:             return GenesisChannelIdBackLeft;
        case AV_CH_BACK_RIGHT:            return GenesisChannelIdBackRight;
        case AV_CH_FRONT_LEFT_OF_CENTER:  return GenesisChannelIdFrontLeftOfCenter;
        case AV_CH_FRONT_RIGHT_OF_CENTER: return GenesisChannelIdFrontRightOfCenter;
        case AV_CH_BACK_CENTER:           return GenesisChannelIdBackCenter;
        case AV_CH_SIDE_LEFT:             return GenesisChannelIdSideLeft;
        case AV_CH_SIDE_RIGHT:            return GenesisChannelIdSideRight;
        case AV_CH_TOP_CENTER:            return GenesisChannelIdTopCenter;
        case AV_CH_TOP_FRONT_LEFT:        return GenesisChannelIdTopFrontLeft;
        case AV_CH_TOP_FRONT_CENTER:      return GenesisChannelIdTopFrontCenter;
        case AV_CH_TOP_FRONT_RIGHT:       return GenesisChannelIdTopFrontRight;
        case AV_CH_TOP_BACK_LEFT:         return GenesisChannelIdTopBackLeft;
        case AV_CH_TOP_BACK_CENTER:       return GenesisChannelIdTopBackCenter;
        case AV_CH_TOP_BACK_RIGHT:        return GenesisChannelIdTopBackRight;

        default: return GenesisChannelIdInvalid;
    }
}

static enum GenesisError channel_layout_init_from_libav(uint64_t libav_channel_layout,
        GenesisChannelLayout *layout)
{
    int channel_count = av_get_channel_layout_nb_channels(libav_channel_layout);
    if (layout->channel_count > GENESIS_MAX_CHANNELS)
        return GenesisErrorMaxChannelsExceeded;

    layout->channel_count = channel_count;
    for (int i = 0; i < layout->channel_count; i += 1) {
        uint64_t libav_channel_id = av_channel_layout_extract_channel(libav_channel_layout, i);
        GenesisChannelId channel_id = from_libav_channel_id(libav_channel_id);
        layout->channels[i] = channel_id;
    }

    int builtin_layout_count = genesis_channel_layout_builtin_count();
    for (int i = 0; i < builtin_layout_count; i += 1) {
        const GenesisChannelLayout *builtin_layout = genesis_channel_layout_get_builtin(i);
        if (genesis_channel_layout_equal(builtin_layout, layout)) {
            layout->name = builtin_layout->name;
            return GenesisErrorNone;
        }
    }

    layout->name = nullptr;
    return GenesisErrorNone;
}


void audio_file_load(const ByteBuffer &file_path, AudioFile *audio_file) {
    AVFormatContext *ic = avformat_alloc_context();
    if (!ic)
        panic("unable to create format context");

    ic->interrupt_callback.callback = decode_interrupt_cb;
    ic->interrupt_callback.opaque = NULL;

    int err = avformat_open_input(&ic, file_path.raw(), NULL, NULL);
    if (err < 0) {
        char buf[256];
        av_strerror(err, buf, sizeof(buf));
        panic("unable to open %s: %s", file_path.raw(), buf);
    }

    err = avformat_find_stream_info(ic, NULL);
    if (err < 0) {
        char buf[256];
        av_strerror(err, buf, sizeof(buf));
        panic("unable to find stream in %s: %s", file_path.raw(), buf);
    }

    // set all streams to discard. in a few lines here we will find the audio
    // stream and cancel discarding it
    for (long i = 0; i < ic->nb_streams; i += 1)
        ic->streams[i]->discard = AVDISCARD_ALL;

    AVCodec *decoder = NULL;
    int audio_stream_index = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
            -1, -1, &decoder, 0);
    if (audio_stream_index < 0)
        panic("no audio stream found");
    if (!decoder)
        panic("no decoder found");

    AVStream *audio_st = ic->streams[audio_stream_index];
    audio_st->discard = AVDISCARD_DEFAULT;

    AVCodecContext *codec_ctx = audio_st->codec;
    err = avcodec_open2(codec_ctx, decoder, NULL);
    if (err < 0) {
        char buf[256];
        av_strerror(err, buf, sizeof(buf));
        panic("unable to open decoder: %s", buf);
    }

    if (!codec_ctx->channel_layout)
        codec_ctx->channel_layout = av_get_default_channel_layout(codec_ctx->channels);
    if (!codec_ctx->channel_layout)
        panic("unable to guess channel layout");

    // copy the audio stream metadata to the context metadata
    av_dict_copy(&ic->metadata, audio_st->metadata, 0);

    AVDictionaryEntry *tag = NULL;
    audio_file->tags.clear();
    while ((tag = av_dict_get(ic->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        bool ok;
        String value(tag->value, &ok);
        audio_file->tags.put(tag->key, value);
    }

    GenesisError genesis_err = channel_layout_init_from_libav(codec_ctx->channel_layout,
           &audio_file->channel_layout);
    if (genesis_err)
        panic("unable to determine channel layout: %s", genesis_error_string(genesis_err));
    audio_file->sample_rate = codec_ctx->sample_rate;
    audio_file->export_sample_format = from_libav_sample_format(codec_ctx->sample_fmt);
    audio_file->export_bit_rate = 320 * 1000;
    long channel_count = audio_file->channel_layout.channel_count;

    void (*import_frame)(const AVFrame *, AudioFile *);
    switch (codec_ctx->sample_fmt) {
        default:
            panic("unrecognized sample format");
            break;
        case AV_SAMPLE_FMT_U8:
            import_frame = import_frame_uint8;
            break;
        case AV_SAMPLE_FMT_S16:
            import_frame = import_frame_int16;
            break;
        case AV_SAMPLE_FMT_S32:
            import_frame = import_frame_int32;
            break;
        case AV_SAMPLE_FMT_FLT:
            import_frame = import_frame_float;
            break;
        case AV_SAMPLE_FMT_DBL:
            import_frame = import_frame_double;
            break;

        case AV_SAMPLE_FMT_U8P:
            import_frame = import_frame_uint8_planar;
            break;
        case AV_SAMPLE_FMT_S16P:
            import_frame = import_frame_int16_planar;
            break;
        case AV_SAMPLE_FMT_S32P:
            import_frame = import_frame_int32_planar;
            break;
        case AV_SAMPLE_FMT_FLTP:
            import_frame = import_frame_float_planar;
            break;
        case AV_SAMPLE_FMT_DBLP:
            import_frame = import_frame_double_planar;
            break;
    }

    audio_file->channels.resize(channel_count);
    for (long i = 0; i < audio_file->channels.length(); i += 1) {
        audio_file->channels.at(i).samples.clear();
    }

    AVFrame *in_frame = av_frame_alloc();
    if (!in_frame)
        panic("unable to allocate frame");

    AVPacket pkt;
    memset(&pkt, 0, sizeof(AVPacket));

    for (;;) {
        err = av_read_frame(ic, &pkt);
        if (err == AVERROR_EOF) {
            break;
        } else if (err < 0) {
            char buf[256];
            av_strerror(err, buf, sizeof(buf));
            panic("unable to read frame: %s", buf);
        }
        decode_frame(audio_file, &pkt, codec_ctx, in_frame, import_frame);
        av_free_packet(&pkt);
    }

    // flush
    for (;;) {
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        pkt.stream_index = audio_stream_index;
        if (decode_frame(audio_file, &pkt, codec_ctx, in_frame, import_frame) <= 0)
            break;
    }

    av_frame_free(&in_frame);
    avcodec_close(codec_ctx);
    avformat_close_input(&ic);
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

static bool libav_codec_supports_sample_rate(const AVCodec *codec, int sample_rate) {
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

void audio_file_get_supported_sample_rates(const char *format_short_name,
        const char *codec_short_name, const char *filename, List<int> &out)
{
    out.clear();

    AVOutputFormat *oformat;
    AVCodec *codec;
    get_format_and_codec(format_short_name, codec_short_name, filename, &oformat, &codec);
    if (!oformat || !codec)
        panic("could not find codec");

    for (long i = 0; i < array_length(prioritized_sample_rates); i += 1) {
        int sample_rate = prioritized_sample_rates[i];
        if (libav_codec_supports_sample_rate(codec, sample_rate))
            out.append(sample_rate);
    }
}

static bool libav_codec_supports_sample_format(const AVCodec *codec, ExportSampleFormat format) {
    if (!codec->sample_fmts)
        return true;

    const enum AVSampleFormat *p = (enum AVSampleFormat*) codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (format.planar && format.sample_format == SampleFormatUInt8 && *p == AV_SAMPLE_FMT_U8P)
            return true;
        if (format.planar && format.sample_format == SampleFormatInt16 && *p == AV_SAMPLE_FMT_S16P)
            return true;
        if (format.planar && format.sample_format == SampleFormatInt32 && *p == AV_SAMPLE_FMT_S32P)
            return true;
        if (format.planar && format.sample_format == SampleFormatFloat && *p == AV_SAMPLE_FMT_FLTP)
            return true;
        if (format.planar && format.sample_format == SampleFormatDouble && *p == AV_SAMPLE_FMT_DBLP)
            return true;

        if (!format.planar && format.sample_format == SampleFormatUInt8 && *p == AV_SAMPLE_FMT_U8)
            return true;
        if (!format.planar && format.sample_format == SampleFormatInt16 && *p == AV_SAMPLE_FMT_S16)
            return true;
        if (!format.planar && format.sample_format == SampleFormatInt32 && *p == AV_SAMPLE_FMT_S32)
            return true;
        if (!format.planar && format.sample_format == SampleFormatFloat && *p == AV_SAMPLE_FMT_FLT)
            return true;
        if (!format.planar && format.sample_format == SampleFormatDouble && *p == AV_SAMPLE_FMT_DBL)
            return true;

        p += 1;
    }

    return false;
}

void audio_file_get_supported_sample_formats(const char *format_short_name,
        const char *codec_short_name, const char *filename, List<ExportSampleFormat> &out)
{
    out.clear();

    AVOutputFormat *oformat;
    AVCodec *codec;
    get_format_and_codec(format_short_name, codec_short_name, filename, &oformat, &codec);
    if (!oformat || !codec)
        panic("could not find codec");

    for (long i = 0; i < array_length(prioritized_export_formats); i += 1) {
        if (libav_codec_supports_sample_format(codec, prioritized_export_formats[i]))
            out.append(prioritized_export_formats[i]);
    }
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

static AVSampleFormat to_libav_sample_format(ExportSampleFormat format) {
    if (format.planar) {
        switch (format.sample_format) {
            case SampleFormatUInt8:  return AV_SAMPLE_FMT_U8P;
            case SampleFormatInt16:  return AV_SAMPLE_FMT_S16P;
            case SampleFormatInt32:  return AV_SAMPLE_FMT_S32P;
            case SampleFormatFloat:  return AV_SAMPLE_FMT_FLTP;
            case SampleFormatDouble: return AV_SAMPLE_FMT_DBLP;
            case SampleFormatInvalid: panic("invalid sample format");
        }
    } else {
        switch (format.sample_format) {
            case SampleFormatUInt8:  return AV_SAMPLE_FMT_U8;
            case SampleFormatInt16:  return AV_SAMPLE_FMT_S16;
            case SampleFormatInt32:  return AV_SAMPLE_FMT_S32;
            case SampleFormatFloat:  return AV_SAMPLE_FMT_FLT;
            case SampleFormatDouble: return AV_SAMPLE_FMT_DBL;
            case SampleFormatInvalid: panic("invalid sample format");
        }
    }
    panic("unrecognized sample format");
}

static void write_frames_uint8_planar(const AudioFile *audio_file,
        long start, long end, uint8_t *buffer, AVFrame *frame)
{
    for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
        uint8_t *ch_buf = frame->extended_data[ch];
        for (long i = start; i < end; i += 1) {
            float sample = audio_file->channels.at(ch).samples.at(i);
            *ch_buf = (uint8_t)((sample * 127.5) + 127.5);
            ch_buf += 1;
        }
    }
}

static void write_frames_int16_planar(const AudioFile *audio_file,
        long start, long end, uint8_t *buffer, AVFrame *frame)
{
    for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
        int16_t *ch_buf = reinterpret_cast<int16_t*>(frame->extended_data[ch]);
        for (long i = start; i < end; i += 1) {
            float sample = audio_file->channels.at(ch).samples.at(i);
            *ch_buf = (int16_t)(sample * 32767.0);
            ch_buf += 1;
        }
    }
}

static void write_frames_int32_planar(const AudioFile *audio_file,
        long start, long end, uint8_t *buffer, AVFrame *frame)
{
    for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
        int32_t *ch_buf = reinterpret_cast<int32_t*>(frame->extended_data[ch]);
        for (long i = start; i < end; i += 1) {
            float sample = audio_file->channels.at(ch).samples.at(i);
            *ch_buf = (int32_t)(sample * 2147483647.0);
            ch_buf += 1;
        }
    }
}

static void write_frames_float_planar(const AudioFile *audio_file,
        long start, long end, uint8_t *buffer, AVFrame *frame)
{
    for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
        float *ch_buf = reinterpret_cast<float*>(frame->extended_data[ch]);
        for (long i = start; i < end; i += 1) {
            float sample = audio_file->channels.at(ch).samples.at(i);
            *ch_buf = (float)sample;
            ch_buf += 1;
        }
    }
}

static void write_frames_double_planar(const AudioFile *audio_file,
        long start, long end, uint8_t *buffer, AVFrame *frame)
{
    for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
        double *ch_buf = reinterpret_cast<double*>(frame->extended_data[ch]);
        for (long i = start; i < end; i += 1) {
            float sample = audio_file->channels.at(ch).samples.at(i);
            *ch_buf = sample;
            ch_buf += 1;
        }
    }
}

static void write_frames_uint8(const AudioFile *audio_file,
        long start, long end, uint8_t *buffer, AVFrame *)
{
    for (long i = start; i < end; i += 1) {
        for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
            float sample = audio_file->channels.at(ch).samples.at(i);

            *buffer = (uint8_t)((sample * 127.5) + 127.5);

            buffer += 1;
        }
    }
}

static void write_frames_int16(const AudioFile *audio_file,
        long start, long end, uint8_t *buffer, AVFrame *)
{
    for (long i = start; i < end; i += 1) {
        for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
            float sample = audio_file->channels.at(ch).samples.at(i);

            int16_t *int_ptr = reinterpret_cast<int16_t*>(buffer);
            *int_ptr = (int16_t)(sample * 32767.0);

            buffer += 2;
        }
    }
}

static void write_frames_int32(const AudioFile *audio_file,
        long start, long end, uint8_t *buffer, AVFrame *)
{
    for (long i = start; i < end; i += 1) {
        for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
            float sample = audio_file->channels.at(ch).samples.at(i);

            int32_t *int_ptr = reinterpret_cast<int32_t*>(buffer);
            *int_ptr = (int32_t)(sample * 2147483647.0);

            buffer += 4;
        }
    }
}

static void write_frames_float(const AudioFile *audio_file,
        long start, long end, uint8_t *buffer, AVFrame *)
{
    for (long i = start; i < end; i += 1) {
        for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
            float sample = audio_file->channels.at(ch).samples.at(i);

            float *float_ptr = reinterpret_cast<float*>(buffer);
            *float_ptr = (float)sample;

            buffer += 4;
        }
    }
}

static void write_frames_double(const AudioFile *audio_file,
        long start, long end, uint8_t *buffer, AVFrame *)
{
    for (long i = start; i < end; i += 1) {
        for (long ch = 0; ch < audio_file->channels.length(); ch += 1) {
            float sample = audio_file->channels.at(ch).samples.at(i);
            double *double_ptr = reinterpret_cast<double*>(buffer);
            *double_ptr = sample;
            buffer += 8;
        }
    }
}

static uint64_t to_libav_channel_id(enum GenesisChannelId channel_id) {
    switch (channel_id) {
    case GenesisChannelIdInvalid: panic("invalid channel id");
    case GenesisChannelIdFrontLeft: return AV_CH_FRONT_LEFT;
    case GenesisChannelIdFrontRight: return AV_CH_FRONT_RIGHT;
    case GenesisChannelIdFrontCenter: return AV_CH_FRONT_CENTER;
    case GenesisChannelIdLowFrequency: return AV_CH_LOW_FREQUENCY;
    case GenesisChannelIdBackLeft: return AV_CH_BACK_LEFT;
    case GenesisChannelIdBackRight: return AV_CH_BACK_RIGHT;
    case GenesisChannelIdFrontLeftOfCenter: return AV_CH_FRONT_LEFT_OF_CENTER;
    case GenesisChannelIdFrontRightOfCenter: return AV_CH_FRONT_RIGHT_OF_CENTER;
    case GenesisChannelIdBackCenter: return AV_CH_BACK_CENTER;
    case GenesisChannelIdSideLeft: return AV_CH_SIDE_LEFT;
    case GenesisChannelIdSideRight: return AV_CH_SIDE_RIGHT;
    case GenesisChannelIdTopCenter: return AV_CH_TOP_CENTER;
    case GenesisChannelIdTopFrontLeft: return AV_CH_TOP_FRONT_LEFT;
    case GenesisChannelIdTopFrontCenter: return AV_CH_TOP_FRONT_CENTER;
    case GenesisChannelIdTopFrontRight: return AV_CH_TOP_FRONT_RIGHT;
    case GenesisChannelIdTopBackLeft: return AV_CH_TOP_BACK_LEFT;
    case GenesisChannelIdTopBackCenter: return AV_CH_TOP_BACK_CENTER;
    case GenesisChannelIdTopBackRight: return AV_CH_TOP_BACK_RIGHT;
    }
    panic("invalid channel id");
}


static uint64_t channel_layout_to_libav(const GenesisChannelLayout *channel_layout) {
    uint64_t result = 0;
    for (int i = 0; i < channel_layout->channel_count; i += 1) {
        GenesisChannelId channel_id = channel_layout->channels[i];
        result |= to_libav_channel_id(channel_id);
    }
    return result;
}

void audio_file_save(const ByteBuffer &file_path, const char *format_short_name,
        const char *codec_short_name, const AudioFile *audio_file)
{
    AVCodec *codec;
    AVOutputFormat *oformat;
    get_format_and_codec(format_short_name, codec_short_name, file_path.raw(), &oformat, &codec);

    uint64_t target_channel_layout = channel_layout_to_libav(&audio_file->channel_layout);
    uint64_t out_channel_layout = closest_supported_channel_layout(codec, target_channel_layout);

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

    int buffer_size;
    if (codec_ctx->frame_size) {
        buffer_size = av_samples_get_buffer_size(NULL, codec_ctx->channels,
            codec_ctx->frame_size, codec_ctx->sample_fmt, 0);
        if (buffer_size < 0) {
            char buf[256];
            av_strerror(err, buf, sizeof(buf));
            panic("error determining buffer size: %s", buf);
        }
    } else {
        buffer_size = 16 * 1024;
    }
    int bytes_per_sample = av_get_bytes_per_sample(codec_ctx->sample_fmt);
    int frame_count = buffer_size / bytes_per_sample / codec_ctx->channels;

    frame->pts = 0;
    frame->nb_samples = frame_count;
    frame->format = codec_ctx->sample_fmt;
    frame->channel_layout = codec_ctx->channel_layout;

    uint8_t *buffer = allocate<uint8_t>(buffer_size);

    err = avcodec_fill_audio_frame(frame, codec_ctx->channels, codec_ctx->sample_fmt,
            buffer, buffer_size, 0);
    if (err < 0) {
        char buf[256];
        av_strerror(err, buf, sizeof(buf));
        panic("error setting up audio frame: %s", buf);
    }

    void (*write_frames)(const AudioFile *, long, long, uint8_t *, AVFrame *);

    if (audio_file->export_sample_format.planar) {
        switch (audio_file->export_sample_format.sample_format) {
            default:
                panic("unknown sample format");
            case SampleFormatUInt8:
                write_frames = write_frames_uint8_planar;
                break;
            case SampleFormatInt16:
                write_frames = write_frames_int16_planar;
                break;
            case SampleFormatInt32:
                write_frames = write_frames_int32_planar;
                break;
            case SampleFormatFloat:
                write_frames = write_frames_float_planar;
                break;
            case SampleFormatDouble:
                write_frames = write_frames_double_planar;
                break;
        }
    } else {
        switch (audio_file->export_sample_format.sample_format) {
            default:
                panic("unknown sample format");
            case SampleFormatUInt8:
                write_frames = write_frames_uint8;
                break;
            case SampleFormatInt16:
                write_frames = write_frames_int16;
                break;
            case SampleFormatInt32:
                write_frames = write_frames_int32;
                break;
            case SampleFormatFloat:
                write_frames = write_frames_float;
                break;
            case SampleFormatDouble:
                write_frames = write_frames_double;
                break;
        }
    }

    long source_frame_count = audio_file->channels.at(0).samples.length();

    AVPacket pkt;
    long start = 0;
    for (;;) {
        av_init_packet(&pkt);
        pkt.data = NULL; // packet data will be allocated by the encoder
        pkt.size = 0;

        long end = min(start + frame_count, source_frame_count);
        write_frames(audio_file, start, end, buffer, frame);
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
        if (end == source_frame_count)
            break;

        frame->pts += frame_count;
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

    avio_closep(&fmt_ctx->pb);
    avcodec_close(codec_ctx);
    avformat_free_context(fmt_ctx);
}

void audio_file_init(void) {
    avcodec_register_all();
    av_register_all();
}

bool codec_supports_sample_rate(const char *format_short_name,
        const char *codec_short_name, const char *filename, int sample_rate)
{
    AVCodec *codec;
    AVOutputFormat *oformat;
    get_format_and_codec(format_short_name, codec_short_name, filename, &oformat, &codec);
    return libav_codec_supports_sample_rate(codec, sample_rate);
}

bool codec_supports_sample_format(const char *format_short_name,
        const char *codec_short_name, const char *filename, ExportSampleFormat format)
{
    AVCodec *codec;
    AVOutputFormat *oformat;
    get_format_and_codec(format_short_name, codec_short_name, filename, &oformat, &codec);
    return libav_codec_supports_sample_format(codec, format);
}

