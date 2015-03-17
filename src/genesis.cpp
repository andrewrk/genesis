#include "genesis.h"
#include "audio_file.hpp"
#include "list.hpp"

struct GenesisContext {
    List<GenesisNodeDescriptor> node_descriptors;
};

struct GenesisContext *genesis_create_context(void) {
    audio_file_init();

    GenesisContext *context = create<GenesisContext>();

    context->node_descriptors.resize(1);
    GenesisNodeDescriptor *node_descr = &context->node_descriptors.at(0);
    node_descr->name = "synth";
    node_descr->port_count = 2;
    node_descr->port_descriptors[0].name = "notes_in";
    node_descr->port_descriptors[0].port_type = GenesisPortTypeNotesIn;

    node_descr->port_descriptors[1].name = "audio_out";
    node_descr->port_descriptors[1].port_type = GenesisPortTypeAudioOut;
    node_descr->port_descriptors[1].data.audio_out.channel_layout =
        *genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
    node_descr->port_descriptors[1].data.audio_out.channel_config = GenesisChannelConfigFixed;

    return context;
}

void genesis_destroy_context(struct GenesisContext *context) {
    destroy(context, 0);
}

struct GenesisNodeDescriptor *genesis_node_descriptor_find(
        GenesisContext *context, const char *name)
{
    for (int i = 0; i < context->node_descriptors.length(); i += 1) {
        GenesisNodeDescriptor *descr = &context->node_descriptors.at(i);
        if (strcmp(descr->name, name) == 0)
            return descr;
    }
    return nullptr;
}
