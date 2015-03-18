#include "genesis.h"
#include <stdio.h>

int main(int argc, char **argv) {
    struct GenesisContext *context = genesis_create_context();
    if (!context) {
        fprintf(stderr, "unable to create context\n");
        return 1;
    }

    int count = genesis_audio_device_count(context);
    if (count < 0) {
        fprintf(stderr, "unable to find devices\n");
        return 1;
    }

    int default_playback = genesis_audio_device_default_playback(context);
    int default_recording = genesis_audio_device_default_recording(context);
    for (int i = 0; i < count; i += 1) {
        struct GenesisAudioDevice *device = genesis_audio_device_get(context, i);
        const char *purpose_str;
        const char *default_str;
        if (genesis_audio_device_purpose(device) == GenesisAudioDevicePurposePlayback) {
            purpose_str = "playback";
            default_str = (i == default_playback) ? " (default)" : "";
        } else {
            purpose_str = "recording";
            default_str = (i == default_recording) ? " (default)" : "";
        }
        const char *description = genesis_audio_device_description(device);
        fprintf(stderr, "%s device: %s%s\n", purpose_str, description, default_str);
    }
    fprintf(stderr, "%d devices found\n", count);

    genesis_destroy_context(context);
}
