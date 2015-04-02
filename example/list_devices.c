#include "genesis.h"

#include <stdio.h>
#include <string.h>

// list or keep a watch on audio and midi devices

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s [--watch]\n", exe);
    return 1;
}

static int list_devices(struct GenesisContext *context) {
    genesis_refresh_audio_devices(context);
    genesis_refresh_midi_devices(context);
    int audio_count = genesis_get_audio_device_count(context);
    if (audio_count < 0) {
        fprintf(stderr, "unable to find audio devices\n");
        return 1;
    }
    int midi_count = genesis_get_midi_device_count(context);
    if (midi_count < 0) {
        fprintf(stderr, "unable to find midi devices\n");
        return 1;
    }

    int default_playback = genesis_get_default_playback_device_index(context);
    int default_recording = genesis_get_default_recording_device_index(context);
    int default_midi = genesis_get_default_midi_device_index(context);
    for (int i = 0; i < audio_count; i += 1) {
        struct GenesisAudioDevice *device = genesis_get_audio_device(context, i);
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
        int sample_rate = genesis_audio_device_sample_rate(device);
        fprintf(stderr, "%s device: %d Hz %s%s\n", purpose_str, sample_rate, description, default_str);
    }
    for (int i = 0; i < midi_count; i += 1) {
        struct GenesisMidiDevice *device = genesis_get_midi_device(context, i);
        const char *default_str = (i == default_midi) ? " (default)" : "";
        const char *description = genesis_midi_device_description(device);
        fprintf(stderr, "controller device: %s%s\n", description, default_str);
    }
    fprintf(stderr, "%d devices found\n", audio_count + midi_count);
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

    struct GenesisContext *context;
    int err = genesis_create_context(&context);
    if (err) {
        fprintf(stderr, "unable to create context: %s\n", genesis_error_string(err));
        return 1;
    }

    if (watch) {
        genesis_set_audio_device_callback(context, on_devices_change, context);
        genesis_set_midi_device_callback(context, on_devices_change, context);
        for (;;) {
            genesis_wait_events(context);
        }
    } else {
        int err = list_devices(context);
        genesis_destroy_context(context);
        return err;
    }
}
