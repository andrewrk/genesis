#include "genesis.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// print the list of supported input and export formats, codecs, sample formats, and sample rates

int main(int argc, char **argv) {
    struct GenesisContext *context = genesis_create_context();
    if (!context) {
        fprintf(stderr, "unable to create context\n");
        return 1;
    }
    fprintf(stderr, "Import:\n");
    int in_format_count = genesis_in_format_count(context);
    for (int format_index = 0; format_index < in_format_count; format_index += 1) {
        struct GenesisAudioFileFormat *format = genesis_in_format_index(context, format_index);
        fprintf(stderr, "format: %s\n", genesis_audio_file_format_description(format));
        int codec_count = genesis_audio_file_format_codec_count(format);
        for (int codec_index = 0; codec_index < codec_count; codec_index += 1) {
            struct GenesisAudioFileCodec *codec = genesis_audio_file_format_codec_index(format, codec_index);
            fprintf(stderr, "  %s\n", genesis_audio_file_codec_description(codec));
        }
    }

    fprintf(stderr, "\nExport:\n");
    int out_format_count = genesis_out_format_count(context);
    for (int format_index = 0; format_index < out_format_count; format_index += 1) {
        struct GenesisAudioFileFormat *format = genesis_out_format_index(context, format_index);
        fprintf(stderr, "format: %s\n", genesis_audio_file_format_description(format));
        int codec_count = genesis_audio_file_format_codec_count(format);
        for (int codec_index = 0; codec_index < codec_count; codec_index += 1) {
            struct GenesisAudioFileCodec *codec = genesis_audio_file_format_codec_index(format, codec_index);
            fprintf(stderr, "  %s\n", genesis_audio_file_codec_description(codec));
        }
    }

    genesis_destroy_context(context);
}
