#include "genesis.h"
#include <stdio.h>
#include <stdlib.h>

// adds a delay effect to the default recording device and plays it over
// default playback device

static void connect_audio_nodes(struct GenesisNode *source, struct GenesisNode *dest) {
    int err = genesis_connect_audio_nodes(source, dest);
    if (err) {
        fprintf(stderr, "unable to connect audio ports: %s\n", genesis_error_string(err));
        fprintf(stderr, "%s -> %s\n",
                genesis_node_descriptor_name(genesis_node_descriptor(source)),
                genesis_node_descriptor_name(genesis_node_descriptor(dest)));
        exit(1);
    }
}

int main(int argc, char **argv) {
    struct GenesisContext *context;
    int err = genesis_create_context(&context);
    if (err) {
        fprintf(stderr, "unable to create context: %s\n", genesis_error_string(err));
        return 1;
    }

    // block until we have audio devices list
    genesis_refresh_audio_devices(context);

    int playback_device_index = genesis_get_default_playback_device_index(context);
    int recording_device_index = genesis_get_default_recording_device_index(context);
    if (playback_device_index < 0 || recording_device_index < 0) {
        fprintf(stderr, "error getting audio device list\n");
        return 1;
    }

    struct GenesisAudioDevice *out_device = genesis_get_audio_device(context, playback_device_index);
    struct GenesisAudioDevice *in_device = genesis_get_audio_device(context, recording_device_index);
    if (!out_device || !in_device) {
        fprintf(stderr, "error getting playback or recording device\n");
        return 1;
    }

    struct GenesisNodeDescriptor *playback_node_descr;
    err = genesis_audio_device_create_node_descriptor(out_device, &playback_node_descr);
    if (err) {
        fprintf(stderr, "unable to get node info for output device: %s\n", genesis_error_string(err));
        return 1;
    }

    struct GenesisNode *playback_node = genesis_node_descriptor_create_node(playback_node_descr);
    if (!playback_node) {
        fprintf(stderr, "unable to create out node\n");
        return 1;
    }

    struct GenesisNodeDescriptor *recording_node_descr;
    err = genesis_audio_device_create_node_descriptor(in_device, &recording_node_descr);
    if (err) {
        fprintf(stderr, "unable to get node info for output device: %s\n", genesis_error_string(err));
        return 1;
    }

    genesis_audio_device_unref(in_device);
    genesis_audio_device_unref(out_device);

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

    connect_audio_nodes(recording_node, delay_node);
    connect_audio_nodes(delay_node, playback_node);

    genesis_start_pipeline(context);

    for (;;)
        genesis_wait_events(context);
}
