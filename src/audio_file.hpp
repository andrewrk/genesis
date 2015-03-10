#ifndef AUDIO_FILE_HPP
#define AUDIO_FILE_HPP

#include "list.hpp"
#include "hash_map.hpp"
#include "string.hpp"
#include "channel_layouts.hpp"
#include "sample_format.hpp"

struct ExportSampleFormat {
    SampleFormat sample_format;
    bool planar;
};

struct Channel {
    // samples are always stored as 48000, 32-bit float in memory.
    List<float> samples;
};

struct AudioFile {
    List<Channel> channels;
    const ChannelLayout *channel_layout;
    int sample_rate;
    // export_* used when saving audio file to disk
    ExportSampleFormat export_sample_format;
    int export_bit_rate;
    HashMap<ByteBuffer, String, ByteBuffer::hash> tags;
};

void audio_file_load(const ByteBuffer &file_path, AudioFile *audio_file);
void audio_file_save(const ByteBuffer &file_path, const char *format_short_name,
        const char *codec_short_name, const AudioFile *audio_file);

void audio_file_get_supported_sample_rates(const char *format_short_name,
        const char *codec_short_name, const char *filename, List<int> &out);
void audio_file_get_supported_sample_formats(const char *format_short_name,
        const char *codec_short_name, const char *filename, List<ExportSampleFormat> &out);

bool codec_supports_sample_rate(const char *format_short_name,
        const char *codec_short_name, const char *filename, int sample_rate);

bool codec_supports_sample_format(const char *format_short_name,
        const char *codec_short_name, const char *filename, ExportSampleFormat format);

void audio_file_init(void);

#endif
