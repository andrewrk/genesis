#include "genesis.h"

#include <stdio.h>
#include <string.h>

// list or keep a watch on audio devices

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s [--watch]\n", exe);
    return 1;
}

static int list_devices(struct GenesisContext *context) {
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
    return 0;
}

static void on_devices_change(void *userdata) {
    struct GenesisContext *context = userdata;
    fprintf(stderr, "devices changed\n");
    list_devices(context);
}

int main(int argc, char **argv) {
    char *exe = argv[0];
    bool watch = false;

    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (strcmp("--watch", arg) == 0) {
            watch = true;
        } else {
            return usage(exe);
        }
    }

    struct GenesisContext *context = genesis_create_context();
    if (!context) {
        fprintf(stderr, "unable to create context\n");
        return 1;
    }

    if (watch) {
        genesis_audio_device_set_callback(context, on_devices_change, context);
        for (;;) {
            genesis_wait_events(context);
        }
    } else {
        int err = list_devices(context);
        genesis_destroy_context(context);
        return err;
    }
}
