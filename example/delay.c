#include "genesis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// adds a delay effect to the default recording device and plays it over
// default playback device

static void connect_audio_nodes(struct GenesisNode *source, struct GenesisNode *dest) {
    int err = genesis_connect_audio_nodes(source, dest);
    if (err) {
        fprintf(stderr, "unable to connect audio ports: %s\n", genesis_strerror(err));
        fprintf(stderr, "%s -> %s\n",
                genesis_node_descriptor_name(genesis_node_descriptor(source)),
                genesis_node_descriptor_name(genesis_node_descriptor(dest)));
        exit(1);
    }
}

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s [--no-delay]\n", exe);
    return 1;
}

static void on_underrun(void *userdata) {
    static int underrun_count = 0;
    fprintf(stderr, "buffer underrun %d\n", ++underrun_count);
}

int main(int argc, char **argv) {
    bool no_delay = false;

    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            arg += 2;
            if (strcmp(arg, "no-delay") == 0) {
                no_delay = true;
            } else {
                return usage(argv[0]);
            }
        } else {
            return usage(argv[0]);
        }
    }

    struct GenesisContext *context;
    int err = genesis_create_context(&context);
    if (err) {
        fprintf(stderr, "unable to create context: %s\n", genesis_strerror(err));
        return 1;
    }

    genesis_set_underrun_callback(context, on_underrun, NULL);

    // block until we have audio devices list
    genesis_flush_events(context);

    int playback_device_index = genesis_default_output_device_index(context);
    int recording_device_index = genesis_default_input_device_index(context);
    if (playback_device_index < 0 || recording_device_index < 0) {
        fprintf(stderr, "error getting audio device list\n");
        return 1;
    }

    struct SoundIoDevice *out_device = genesis_get_output_device(context, playback_device_index);
    struct SoundIoDevice *in_device = genesis_get_input_device(context, recording_device_index);
    if (!out_device || !in_device) {
        fprintf(stderr, "error getting playback or recording device\n");
        return 1;
    }

    struct GenesisNodeDescriptor *playback_node_descr;
    err = genesis_audio_device_create_node_descriptor(context, out_device, &playback_node_descr);
    if (err) {
        fprintf(stderr, "unable to get node info for output device: %s\n", genesis_strerror(err));
        return 1;
    }

    struct GenesisNode *playback_node = genesis_node_descriptor_create_node(playback_node_descr);
    if (!playback_node) {
        fprintf(stderr, "unable to create out node\n");
        return 1;
    }

    struct GenesisNodeDescriptor *recording_node_descr;
    err = genesis_audio_device_create_node_descriptor(context, in_device, &recording_node_descr);
    if (err) {
        fprintf(stderr, "unable to get node info for output device: %s\n", genesis_strerror(err));
        return 1;
    }

    soundio_device_unref(in_device);
    soundio_device_unref(out_device);

    struct GenesisNode *recording_node = genesis_node_descriptor_create_node(recording_node_descr);
    if (!recording_node) {
        fprintf(stderr, "unable to create out node\n");
        return 1;
    }

    struct GenesisNodeDescriptor *delay_descr = genesis_node_descriptor_find(context, "delay");
    if (!delay_descr) {
        fprintf(stderr, "unable to find delay filter\n");
        return 1;
    }
    fprintf(stderr, "delay: %s\n", genesis_node_descriptor_description(delay_descr));

    struct GenesisNode *delay_node = genesis_node_descriptor_create_node(delay_descr);
    if (!delay_node) {
        fprintf(stderr, "unable to create delay node\n");
        return 1;
    }

    if (no_delay) {
        connect_audio_nodes(recording_node, playback_node);
    } else {
        connect_audio_nodes(recording_node, delay_node);
        connect_audio_nodes(delay_node, playback_node);
    }

    genesis_start_pipeline(context, 0.0);

    for (;;)
        genesis_wait_events(context);
}
