#include "mixer_node.hpp"

struct DescriptorContext {
    int input_port_count;
};

struct MixerContext {
    int input_port_count;
    float **read_ptrs;
};

static void mixer_destroy(struct GenesisNode *node) {
    struct MixerContext *mixer_context = (struct MixerContext *)node->userdata;
    if (mixer_context) {
        if (mixer_context->read_ptrs)
            destroy(mixer_context->read_ptrs, mixer_context->input_port_count);
        destroy(mixer_context, 1);
    }
}

static void destroy_node_descriptor(struct GenesisNodeDescriptor *node_descr) {
    if (node_descr) {
        DescriptorContext *descr_context = (DescriptorContext*)node_descr->userdata;
        destroy(descr_context, 1);
    }
}

static int mixer_create(struct GenesisNode *node) {
    const GenesisNodeDescriptor *node_descr = genesis_node_descriptor(node);
    DescriptorContext *descr_context = (DescriptorContext *)genesis_node_descriptor_userdata(node_descr);
    struct MixerContext *mixer_context = create_zero<MixerContext>();
    node->userdata = mixer_context;
    if (!mixer_context) {
        mixer_destroy(node);
        return GenesisErrorNoMem;
    }
    mixer_context->input_port_count = descr_context->input_port_count;
    mixer_context->read_ptrs = allocate_zero<float *>(mixer_context->input_port_count);
    if (!mixer_context->read_ptrs) {
        mixer_destroy(node);
        return GenesisErrorNoMem;
    }

    return 0;
}

static void mixer_run(struct GenesisNode *node) {
    struct MixerContext *mixer_context = (struct MixerContext *)node->userdata;
    struct GenesisPort *audio_out_port = genesis_node_port(node, 0);

    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);
    const struct SoundIoChannelLayout * out_channel_layout = genesis_audio_port_channel_layout(audio_out_port);
    int channel_count = out_channel_layout->channel_count;

    int min_frame_count = output_frame_count;
    for (int i = 0; i < mixer_context->input_port_count; i += 1) {
        GenesisPort *audio_in_port = genesis_node_port(node, i + 1);
        mixer_context->read_ptrs[i] = genesis_audio_in_port_read_ptr(audio_in_port);
        int input_frame_count = genesis_audio_in_port_fill_count(audio_in_port);
        min_frame_count = min(min_frame_count, input_frame_count);
    }

    float *out_ptr = genesis_audio_out_port_write_ptr(audio_out_port);
    for (int frame = 0; frame < min_frame_count; frame += 1) {
        float total[GENESIS_MAX_CHANNELS] = {0.0f};
        for (int port_i = 0; port_i < mixer_context->input_port_count; port_i += 1) {
            for (int ch = 0; ch < channel_count; ch += 1) {
                total[ch] += mixer_context->read_ptrs[port_i][0];
                mixer_context->read_ptrs[port_i] += 1;
            }
        }

        for (int ch = 0; ch < channel_count; ch += 1) {
            out_ptr[0] = total[ch];
            out_ptr += 1;
        }
    }

    genesis_audio_out_port_advance_write_ptr(audio_out_port, min_frame_count);
    for (int i = 0; i < mixer_context->input_port_count; i += 1) {
        GenesisPort *audio_in_port = genesis_node_port(node, i + 1);
        genesis_audio_in_port_advance_read_ptr(audio_in_port, min_frame_count);
    }
}

int create_mixer_descriptor(GenesisContext *context, int input_port_count, GenesisNodeDescriptor **out) {
    *out = nullptr;

    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(
            context, input_port_count + 1, "mixer", "Audio mixer.");
    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    DescriptorContext *descr_context = create_zero<DescriptorContext>();
    if (!descr_context) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    descr_context->input_port_count = input_port_count;
    genesis_node_descriptor_set_userdata(node_descr, descr_context);
    node_descr->destroy_descriptor = destroy_node_descriptor;

    genesis_node_descriptor_set_run_callback(node_descr, mixer_run);
    genesis_node_descriptor_set_create_callback(node_descr, mixer_create);
    genesis_node_descriptor_set_destroy_callback(node_descr, mixer_destroy);

    struct GenesisPortDescriptor *audio_out_port = genesis_node_descriptor_create_port(
            node_descr, 0, GenesisPortTypeAudioOut, "audio_out");
    if (!audio_out_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    genesis_audio_port_descriptor_set_channel_layout(audio_out_port,
        soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono), false, -1);

    genesis_audio_port_descriptor_set_sample_rate(audio_out_port, 48000, false, -1);

    for (int i = 0; i < input_port_count; i += 1) {
        char name[256];
        sprintf(name, "audio_in_%d", i);
        struct GenesisPortDescriptor *audio_in_port = genesis_node_descriptor_create_port(
            node_descr, i + 1, GenesisPortTypeAudioIn, name);
        if (!audio_in_port) {
            genesis_node_descriptor_destroy(node_descr);
            return GenesisErrorNoMem;
        }
        genesis_audio_port_descriptor_set_channel_layout(audio_in_port,
            soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono), true, 0);

        genesis_audio_port_descriptor_set_sample_rate(audio_in_port, 48000, true, 0);
    }

    *out = node_descr;

    return 0;
}
