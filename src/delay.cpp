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

static int delay_port_connect(struct GenesisPort *audio_in_port, struct GenesisPort *other_port) {
    struct GenesisNode *node = audio_in_port->node;
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
    struct GenesisPort *audio_in_port = genesis_node_port(node, 0);
    struct GenesisPort *audio_out_port = genesis_node_port(node, 1);

    int input_frame_count = genesis_audio_in_port_fill_count(audio_in_port);
    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);
    int frame_count = min(input_frame_count, output_frame_count);

    float *in_buf = genesis_audio_in_port_read_ptr(audio_in_port);
    float *out_buf = genesis_audio_out_port_write_ptr(audio_out_port);
    for (int frame = 0; frame < frame_count; frame += 1) {
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

    genesis_audio_in_port_advance_read_ptr(audio_in_port, frame_count);
    genesis_audio_out_port_advance_write_ptr(audio_out_port, frame_count);
}

int create_delay_descriptor(GenesisContext *context) {
    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 2, "delay", "Simple delay filter.");
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

    int target_sample_rate = genesis_get_sample_rate(context);

    genesis_port_descriptor_set_connect_callback(audio_in_port, delay_port_connect);

    genesis_audio_port_descriptor_set_channel_layout(audio_in_port,
        soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono), false, -1);

    genesis_audio_port_descriptor_set_sample_rate(audio_in_port, target_sample_rate, false, -1);


    genesis_audio_port_descriptor_set_channel_layout(audio_out_port,
        soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono), true, 0);

    genesis_audio_port_descriptor_set_sample_rate(audio_out_port, target_sample_rate, true, 0);

    return 0;
}
