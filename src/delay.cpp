#include "delay.hpp"

static const int MAX_DELAY_FRAMES = 96000;

struct DelayContext {
    float *delayed_frames;
    int delayed_frames_capacity;
    int channel_count;
    int frame_offset;
    float delay_length_notes; // in whole notes
    int delay_length_frames;
};

static void delay_destroy(struct GenesisNode *node) {
    struct DelayContext *delay_context = (struct DelayContext *)node->userdata;
    if (delay_context) {
        destroy(delay_context->delayed_frames, delay_context->delayed_frames_capacity);
        destroy(delay_context, 1);
    }
}

static int delay_create(struct GenesisNode *node) {
    struct DelayContext *delay_context = create_zero<DelayContext>();
    node->userdata = delay_context;
    if (!delay_context) {
        delay_destroy(node);
        return GenesisErrorNoMem;
    }
    delay_context->delay_length_notes = 1.0f;
    return 0;
}

static int delay_port_connect(struct GenesisPort *port) {
    struct GenesisNode *node = port->node;
    struct GenesisPort *audio_in_port = node->ports[0];
    if (port != audio_in_port)
        return 0;
    struct DelayContext *delay_context = (struct DelayContext *)node->userdata;
    struct GenesisContext *context = node->descriptor->context;

    delay_context->channel_count = genesis_audio_port_channel_layout(audio_in_port)->channel_count;
    int new_capacity = delay_context->channel_count * MAX_DELAY_FRAMES;
    if (new_capacity != delay_context->delayed_frames_capacity) {
        float *new_frames = reallocate_safe<float>(
                delay_context->delayed_frames, delay_context->delayed_frames_capacity, new_capacity);
        if (!new_frames)
            return GenesisErrorNoMem;

        delay_context->delayed_frames = new_frames;
        delay_context->delayed_frames_capacity = new_capacity;
    }

    delay_context->delay_length_frames = genesis_whole_notes_to_frames(
            context, delay_context->delay_length_notes, genesis_audio_port_sample_rate(audio_in_port));

    return 0;
}

static void delay_seek(struct GenesisNode *node) {
    struct DelayContext *delay_context = (struct DelayContext *)node->userdata;
    delay_context->frame_offset = 0;
    memset(delay_context->delayed_frames, 0, delay_context->delayed_frames_capacity);
}

static void delay_run(struct GenesisNode *node) {
    struct DelayContext *delay_context = (struct DelayContext *)node->userdata;
    struct GenesisPort *audio_in_port = node->ports[0];
    struct GenesisPort *audio_out_port = node->ports[1];

    int free_byte_count = genesis_audio_out_port_free_count(audio_out_port);
    int available_byte_count = genesis_audio_in_port_fill_count(audio_in_port);

    int target_write_bytes = min(free_byte_count, available_byte_count);
    int bytes_per_frame = genesis_audio_port_bytes_per_frame(audio_out_port);
    int free_frame_count = target_write_bytes / bytes_per_frame;
    int write_size = free_frame_count * bytes_per_frame;

    float *in_buf = (float*)genesis_audio_in_port_read_ptr(audio_in_port);
    float *out_buf = (float*)genesis_audio_out_port_write_ptr(audio_out_port);
    for (int frame = 0; frame < free_frame_count; frame += 1) {
        for (int channel = 0; channel < delay_context->channel_count; channel += 1) {
            int sample_index = frame * delay_context->channel_count + channel;
            float in_sample = in_buf[sample_index];
            int delay_sample_index = delay_context->frame_offset * delay_context->channel_count + channel;
            out_buf[sample_index] = in_sample + 0.50f * delay_context->delayed_frames[delay_sample_index];

            delay_context->delayed_frames[delay_sample_index] =
                delay_context->delayed_frames[delay_sample_index] * 0.50f + in_sample;
        }
        delay_context->frame_offset = (delay_context->frame_offset + 1) % delay_context->delay_length_frames;
    }

    genesis_audio_out_port_advance_write_ptr(audio_out_port, write_size);
    genesis_audio_in_port_advance_read_ptr(audio_in_port, write_size);
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
    node_descr->port_connect = delay_port_connect;

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
