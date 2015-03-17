#include "genesis.h"

static void run(struct GenesisNode *node) {
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
}

int main(int argc, char * argv[]) {
    genesis_init();

    struct GenesisNodeDescriptor *synth_descr = genesis_node_descriptor_find("synth");
    if (!synth_descr) {
        fprintf(stderr, "unable to find synth\n");
        return 1;
    }
    fprintf(stderr, "synth: %s\n", synth_descr->description);

    struct GenesisNode *synth_node = genesis_create_node(synth_descr);
    if (!synth_node) {
        fprintf(stderr, "unable to create synth node\n");
        return 1;
    }

    struct GenesisOutputDevice *out_device = genesis_get_default_output_device();
    if (!out_device) {
        fprintf(stderr, "unable to get default output device\n");
        return 1;
    }

    struct GenesisNodeDescriptor *out_node_info = genesis_get_output_device_node_info(out_device);
    if (!out_node_info) {
        fprintf(stderr, "unable to get node info for output device\n");
        return 1;
    }

    struct GenesisNode *out_node = genesis_create_node(out_node_info);
    if (!out_node) {
        fprintf(stderr, "unable to create out node\n");
        return 1;
    }

    enum GenesisError err = genesis_connect_nodes(synth_node, out_node);
    if (err) {
        fprintf(stderr, "unable to connect nodes: %s", genesis_error_string(err));
        return 1;
    }

    struct GenesisNodeDescriptor *input_node_info = genesis_create_node_info();
    if (!input_node_info) {
        fprintf(stderr, "unable to create node info\n");
        return 1;
    }
    input_node_info->run = run;
}
