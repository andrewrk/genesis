#include "genesis.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// print the list of supported input and export formats, codecs, sample formats, and sample rates

int main(int argc, char **argv) {
    fprintf(stderr, "libgenesis version %s\n", genesis_version_string());

    struct GenesisContext *context;
    int err = genesis_create_context(&context);
    if (err) {
        fprintf(stderr, "unable to create genesis context: %s\n", genesis_strerror(err));
        return 1;
    }
    fprintf(stdout, "\nImport:\n");
    int in_format_count = genesis_in_format_count(context);
    for (int format_index = 0; format_index < in_format_count; format_index += 1) {
        struct GenesisAudioFileFormat *format = genesis_in_format_index(context, format_index);
        fprintf(stdout, "format: %s (%s)\n",
                genesis_audio_file_format_description(format), genesis_audio_file_format_name(format));
        int codec_count = genesis_audio_file_format_codec_count(format);
        for (int codec_index = 0; codec_index < codec_count; codec_index += 1) {
            struct GenesisAudioFileCodec *codec = genesis_audio_file_format_codec_index(format, codec_index);
            fprintf(stdout, "  %s\n", genesis_audio_file_codec_description(codec));
        }
    }

    fprintf(stdout, "\nExport:\n");
    int out_format_count = genesis_out_format_count(context);
    for (int format_index = 0; format_index < out_format_count; format_index += 1) {
        struct GenesisRenderFormat *format = genesis_out_format_index(context, format_index);
        fprintf(stdout, "  %s\n", genesis_render_format_description(format));
    }

    genesis_destroy_context(context);
}
