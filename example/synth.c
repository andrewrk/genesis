#include "genesis.h"
#include <stdio.h>

static void run(struct GenesisPort *port) {
    //struct GenesisNode *node = genesis_port_node(port);
    fprintf(stderr, "TODO: hold down A note\n");
    /*
    for (;;) {
        int *buffer;
        int size;
        genesis_node_get_note_buffer(node, &buffer, &size);
        if (size == 0)
            break;
        for (int i = 0; i < size; i += 1) {
            buffer[i] = 440;
        }
    }
    */
}

int main(int argc, char **argv) {
    struct GenesisContext *context = genesis_create_context();
    if (!context) {
        fprintf(stderr, "unable to create context\n");
        return 1;
    }

    struct GenesisNodeDescriptor *synth_descr = genesis_node_descriptor_find(context, "synth");
    if (!synth_descr) {
        fprintf(stderr, "unable to find synth\n");
        return 1;
    }
    fprintf(stderr, "synth: %s\n", genesis_node_descriptor_description(synth_descr));

    struct GenesisNode *synth_node = genesis_node_descriptor_create_node(synth_descr);
    if (!synth_node) {
        fprintf(stderr, "unable to create synth node\n");
        return 1;
    }

    // block until we have audio devices list
    genesis_refresh_audio_devices(context);

    int playback_device_index = genesis_get_default_playback_device_index(context);
    if (playback_device_index < 0) {
        fprintf(stderr, "error getting device list\n");
        return 1;
    }

    struct GenesisAudioDevice *out_device = genesis_get_audio_device(context, playback_device_index);
    if (!out_device) {
        fprintf(stderr, "error getting playback device\n");
        return 1;
    }

    struct GenesisNodeDescriptor *playback_node_descr = genesis_audio_device_create_node_descriptor(out_device);
    if (!playback_node_descr) {
        fprintf(stderr, "unable to get node info for output device\n");
        return 1;
    }

    struct GenesisNode *playback_node = genesis_node_descriptor_create_node(playback_node_descr);
    if (!playback_node) {
        fprintf(stderr, "unable to create out node\n");
        return 1;
    }

    int audio_out_port_index = genesis_node_descriptor_find_port_index(synth_descr, "audio_out");
    if (audio_out_port_index < 0) {
        fprintf(stderr, "unable to find audio_out port\n");
        return 1;
    }

    struct GenesisPort *audio_out_port = genesis_node_port(synth_node, audio_out_port_index);
    if (!audio_out_port) {
        fprintf(stderr, "expected to find audio_out port\n");
        return 1;
    }

    int audio_in_port_index = genesis_node_descriptor_find_port_index(playback_node_descr, "audio_in");
    if (audio_in_port_index < 0) {
        fprintf(stderr, "unable to find audio_in port\n");
        return 1;
    }

    struct GenesisPort *audio_in_port = genesis_node_port(playback_node, audio_in_port_index);
    if (!audio_in_port) {
        fprintf(stderr, "expected to find audio_in port\n");
        return 1;
    }

    enum GenesisError err = genesis_connect_ports(audio_out_port, audio_in_port);
    if (err) {
        fprintf(stderr, "unable to connect audio ports: %s\n", genesis_error_string(err));
        return 1;
    }

    struct GenesisNodeDescriptor *input_node_descr = genesis_create_node_descriptor(context, 1);
    if (!input_node_descr) {
        fprintf(stderr, "unable to create input node descriptor\n");
        return 1;
    }

    struct GenesisPortDescriptor *input_notes_port_descr = genesis_node_descriptor_create_port(
            input_node_descr, 0, GenesisPortTypeNotesOut);

    genesis_port_descriptor_set_run_callback(input_notes_port_descr, run);

    for (;;)
        genesis_wait_events(context);
}
