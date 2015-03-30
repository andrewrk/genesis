#include "delay.hpp"

static void delay_destroy(struct GenesisNode *node) {
    // TODO
}

static int delay_create(struct GenesisNode *node) {
    // TODO
    return 0;
}

static void delay_seek(struct GenesisNode *node) {
    // TODO
}

static void delay_run(struct GenesisNode *node) {
    // TODO
}

int create_delay_descriptor(GenesisContext *context) {
    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 2);
    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->run = delay_run;
    node_descr->create = delay_create;
    node_descr->destroy = delay_destroy;
    node_descr->seek = delay_seek;

    node_descr->name = strdup("delay");
    node_descr->description = strdup("Simple delay filter.");
    if (!node_descr->name || !node_descr->description) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    GenesisAudioPortDescriptor *audio_in_port = create_zero<GenesisAudioPortDescriptor>();
    GenesisAudioPortDescriptor *audio_out_port = create_zero<GenesisAudioPortDescriptor>();

    node_descr->port_descriptors.at(0) = &audio_in_port->port_descriptor;
    node_descr->port_descriptors.at(1) = &audio_out_port->port_descriptor;

    if (!audio_in_port || !audio_out_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    audio_in_port->port_descriptor.port_type = GenesisPortTypeAudioIn;
    audio_in_port->port_descriptor.name = strdup("audio_in");
    if (!audio_in_port->port_descriptor.name) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }
    audio_in_port->channel_layout =
        *genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
    audio_in_port->channel_layout_fixed = false;
    audio_in_port->same_channel_layout_index = -1;
    audio_in_port->sample_rate = 48000;
    audio_in_port->sample_rate_fixed = false;
    audio_in_port->same_sample_rate_index = -1;


    audio_out_port->port_descriptor.port_type = GenesisPortTypeAudioOut;
    audio_out_port->port_descriptor.name = strdup("audio_out");
    if (!audio_out_port->port_descriptor.name) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }
    audio_out_port->channel_layout =
        *genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
    audio_out_port->channel_layout_fixed = true;
    audio_out_port->same_channel_layout_index = 0;
    audio_out_port->sample_rate = 48000;
    audio_out_port->sample_rate_fixed = true;
    audio_out_port->same_sample_rate_index = 0;

    return 0;
}
