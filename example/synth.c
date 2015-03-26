#include "genesis.h"
#include <stdio.h>

int main(int argc, char **argv) {
    struct GenesisContext *context;
    int err = genesis_create_context(&context);
    if (err) {
        fprintf(stderr, "unable to create context: %s\n", genesis_error_string(err));
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
    genesis_refresh_midi_devices(context);

    int playback_device_index = genesis_get_default_playback_device_index(context);
    if (playback_device_index < 0) {
        fprintf(stderr, "error getting audio device list\n");
        return 1;
    }

    struct GenesisAudioDevice *out_device = genesis_get_audio_device(context, playback_device_index);
    if (!out_device) {
        fprintf(stderr, "error getting playback device\n");
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

    err = genesis_connect_ports(audio_out_port, audio_in_port);
    if (err) {
        fprintf(stderr, "unable to connect audio ports: %s\n", genesis_error_string(err));
        return 1;
    }

    int midi_device_index = genesis_get_default_midi_device_index(context);
    if (midi_device_index < 0) {
        fprintf(stderr, "error getting midi device list\n");
        return 1;
    }

    struct GenesisMidiDevice *midi_device = genesis_get_midi_device(context, midi_device_index);
    if (!midi_device) {
        fprintf(stderr, "error getting midi device\n");
        return 1;
    }

    struct GenesisNodeDescriptor *midi_node_descr;
    err = genesis_midi_device_create_node_descriptor(midi_device, &midi_node_descr);
    if (err) {
        fprintf(stderr, "unable to create input node descriptor: %s\n", genesis_error_string(err));
        return 1;
    }

    struct GenesisNode *midi_node = genesis_node_descriptor_create_node(midi_node_descr);
    if (!playback_node) {
        fprintf(stderr, "unable to create midi node\n");
        return 1;
    }

    int notes_out_port_index = genesis_node_descriptor_find_port_index(midi_node_descr, "notes_out");
    if (notes_out_port_index < 0) {
        fprintf(stderr, "unable to find notes_out port\n");
        return 1;
    }

    struct GenesisPort *notes_out_port = genesis_node_port(midi_node, notes_out_port_index);
    if (!notes_out_port) {
        fprintf(stderr, "expected to find notes_out port\n");
        return 1;
    }

    int notes_in_port_index = genesis_node_descriptor_find_port_index(synth_descr, "notes_in");
    if (notes_in_port_index < 0) {
        fprintf(stderr, "unable to find notes_in port\n");
        return 1;
    }

    struct GenesisPort *notes_in_port = genesis_node_port(synth_node, notes_in_port_index);
    if (!notes_in_port) {
        fprintf(stderr, "expected to find notes_in port\n");
        return 1;
    }

    err = genesis_connect_ports(notes_out_port, notes_in_port);
    if (err) {
        fprintf(stderr, "unable to connect audio ports: %s\n", genesis_error_string(err));
        return 1;
    }

    genesis_start_pipeline(context);

    for (;;)
        genesis_wait_events(context);
}
