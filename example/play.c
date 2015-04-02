#include "genesis.h"
#include <stdio.h>

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s inputfile\n", exe);
    return 1;
}

static int report_error(enum GenesisError err) {
    fprintf(stderr, "Error: %s\n", genesis_error_string(err));
    return 1;
}

int main(int argc, char **argv) {
    struct GenesisContext *context;
    int err = genesis_create_context(&context);
    if (err)
        return report_error(err);

    const char *input_filename = NULL;
    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            return usage(argv[0]);
        } else if (!input_filename) {
            input_filename = arg;
        } else {
            return usage(argv[0]);
        }
    }

    if (!input_filename)
        return usage(argv[0]);

    struct GenesisAudioFile *audio_file;
    err = genesis_audio_file_load(context, input_filename, &audio_file);
    if (err)
        return report_error(err);

    const struct GenesisChannelLayout *channel_layout = genesis_audio_file_channel_layout(audio_file);
    if (channel_layout->name)
        fprintf(stderr, "Channels: %d (%s)\n", channel_layout->channel_count, channel_layout->name);
    else
        fprintf(stderr, "Channels: %d\n", channel_layout->channel_count);

    int sample_rate = genesis_audio_file_sample_rate(audio_file);
    fprintf(stderr, "Sample Rate: %d Hz\n", sample_rate);

    genesis_audio_file_destroy(audio_file);
    genesis_destroy_context(context);
}
