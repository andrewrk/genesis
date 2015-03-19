#include "genesis.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// open an audio file, normalize it, and save it

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s inputfile outputfile\n", exe);
    return 1;
}

static int report_error(enum GenesisError err) {
    fprintf(stderr, "Error: %s\n", genesis_error_string(err));
    return 1;
}

static float clamp(float min_value, float value, float max_value) {
    return fmaxf(fminf(value, max_value), min_value);
}

int main(int argc, char **argv) {
    char *input_filename = NULL;
    char *output_filename = NULL;
    char *format = NULL;
    char *codec_name = NULL;
    int bit_rate_k = 320;

    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            arg += 2;
            if (i + 1 >= argc) {
                return usage(argv[0]);
            } else if (strcmp(arg, "bitrate") == 0) {
                bit_rate_k = atoi(argv[++i]);
            } else if (strcmp(arg, "format") == 0) {
                format = argv[++i];
            } else if (strcmp(arg, "codec") == 0) {
                codec_name = argv[++i];
            } else {
                return usage(argv[0]);
            }
        } else if (!input_filename) {
            input_filename = arg;
        } else if (!output_filename) {
            output_filename = arg;
        } else {
            return usage(argv[0]);
        }
    }

    if (!input_filename || !output_filename)
        return usage(argv[0]);

    struct GenesisContext *context = genesis_create_context();
    if (!context) {
        fprintf(stderr, "unable to create context\n");
        return 1;
    }

    struct GenesisAudioFile *audio_file;
    enum GenesisError err = genesis_audio_file_load(context, input_filename, &audio_file);
    if (err != GenesisErrorNone)
        return report_error(err);

    struct GenesisChannelLayout channel_layout = genesis_audio_file_channel_layout(audio_file);
    if (channel_layout.name)
        fprintf(stderr, "Channels: %d (%s)\n", channel_layout.channel_count, channel_layout.name);
    else
        fprintf(stderr, "Channels: %d\n", channel_layout.channel_count);
    long frame_count = genesis_audio_file_frame_count(audio_file);
    int sample_rate = genesis_audio_file_sample_rate(audio_file);
    double seconds = frame_count / (double)sample_rate;
    fprintf(stderr, "%ld frames (%.2f seconds)\n", frame_count, seconds);

    // calculate the maximum sample value
    float abs_max = 0.0f;
    for (int ch = 0; ch < channel_layout.channel_count; ch += 1) {
        struct GenesisAudioFileIterator it = genesis_audio_file_iterator(audio_file, ch, 0);
        while (it.start < frame_count) {
            for (long frame = it.start, offset = 0; frame < it.end; frame += 1, offset += 1) {
                float sample = it.ptr[offset];
                abs_max = fmaxf(fabsf(sample), abs_max);
            }
            genesis_audio_file_iterator_next(&it);
        }
    }

    if (abs_max == 0.0f) {
        fprintf(stderr, "Audio stream is completely silent.\n");
    } else if (abs_max < 1.0f) {
        float multiplier = 1.0f / abs_max;
        for (int ch = 0; ch < channel_layout.channel_count; ch += 1) {
            struct GenesisAudioFileIterator it = genesis_audio_file_iterator(audio_file, ch, 0);
            while (it.start < frame_count) {
                for (long frame = it.start, offset = 0; frame < it.end; frame += 1, offset += 1) {
                    float sample = it.ptr[offset];
                    it.ptr[offset] = clamp(-1.0f, sample * multiplier, 1.0f);
                }
                genesis_audio_file_iterator_next(&it);
            }
        }
    } else {
        fprintf(stderr, "Audio stream is already normalized.\n");
    }

    struct GenesisExportFormat export_format;
    export_format.bit_rate = bit_rate_k * 1000;
    export_format.codec = genesis_guess_audio_file_codec(context, output_filename, format, codec_name);
    if (!export_format.codec) {
        fprintf(stderr, "unknown export format\n");
        return 1;
    }
    fprintf(stderr, "out codec: %s\n", genesis_audio_file_codec_description(export_format.codec));

    int sample_format_count = genesis_audio_file_codec_sample_format_count(export_format.codec);
    if (sample_format_count <= 0) {
        fprintf(stderr, "unsupported sample format\n");
        return 1;
    }
    export_format.sample_format = genesis_audio_file_codec_sample_format_index(export_format.codec, 0);
    fprintf(stderr, "out sample format: %s\n", genesis_sample_format_string(export_format.sample_format));

    if (!genesis_audio_file_codec_supports_sample_rate(export_format.codec, sample_rate)) {
        fprintf(stderr, "unsupported sample rate\n");
        return 1;
    }
    export_format.sample_rate = sample_rate;
    fprintf(stderr, "out sample rate: %d\n", sample_rate);

    err = genesis_audio_file_export(audio_file, output_filename, &export_format);
    if (err != GenesisErrorNone)
        return report_error(err);

    genesis_audio_file_destroy(audio_file);
    genesis_destroy_context(context);
}
