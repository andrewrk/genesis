#include "resample.hpp"

int create_resample_descriptor(GenesisContext *context) {
    return 0;
    /*
    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 2, "resample", "Resample audio and remap channel layouts.");
    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    genesis_node_descriptor_set_run_callback(node_descr, delay_run);
    genesis_node_descriptor_set_create_callback(node_descr, delay_create);
    genesis_node_descriptor_set_destroy_callback(node_descr, delay_destroy);
    genesis_node_descriptor_set_seek_callback(node_descr, delay_seek);

    struct GenesisPortDescriptor *audio_in_port = genesis_node_descriptor_create_port(
            node_descr, 0, GenesisPortTypeAudioIn, "audio_in");
    struct GenesisPortDescriptor *audio_out_port = genesis_node_descriptor_create_port(
            node_descr, 1, GenesisPortTypeAudioOut, "audio_out");

    if (!audio_in_port || !audio_out_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    genesis_port_descriptor_set_connect_callback(audio_in_port, delay_port_connect);

    genesis_audio_port_descriptor_set_channel_layout(audio_in_port,
        genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono), false, -1);

    genesis_audio_port_descriptor_set_sample_rate(audio_in_port, 48000, false, -1);


    genesis_audio_port_descriptor_set_channel_layout(audio_out_port,
        genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono), true, 0);

    genesis_audio_port_descriptor_set_sample_rate(audio_out_port, 48000, true, 0);

    return 0;
    */
}
