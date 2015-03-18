#ifndef AUDIO_FILE_HPP
#define AUDIO_FILE_HPP

#include "genesis.h"
#include "list.hpp"
#include "hash_map.hpp"
#include "byte_buffer.hpp"
#include "libav.hpp"

struct Channel {
    List<float> samples;
};

struct GenesisAudioFile {
    List<Channel> channels;
    GenesisChannelLayout channel_layout;
    int sample_rate;
    HashMap<ByteBuffer, ByteBuffer, ByteBuffer::hash> tags;
};

struct GenesisAudioFileCodec {
    AVCodec *codec;
};

struct GenesisAudioFileFormat {
    List<GenesisAudioFileCodec> codecs;
    AVOutputFormat *oformat;
    AVInputFormat *iformat;
};

void audio_file_get_supported_sample_rates(const char *format_short_name,
        const char *codec_short_name, const char *filename, List<int> &out);
void audio_file_get_supported_sample_formats(const char *format_short_name,
        const char *codec_short_name, const char *filename, List<GenesisExportFormat> &out);

bool codec_supports_sample_rate(const char *format_short_name,
        const char *codec_short_name, const char *filename, int sample_rate);

bool codec_supports_sample_format(const char *format_short_name,
        const char *codec_short_name, const char *filename, GenesisExportFormat format);



void audio_file_init(void);
void audio_file_get_out_formats(List<GenesisAudioFileFormat> &formats);
void audio_file_get_in_formats(List<GenesisAudioFileFormat> &formats);

#endif
