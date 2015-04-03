#include "resample.hpp"
#include "audio_file.hpp"
#include <assert.h>

struct ResampleContext {
    long in_offset;
    long out_offset;
};

static void resample_destroy(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    if (resample_context) {
        destroy(resample_context, 1);
    }
}

static int resample_create(struct GenesisNode *node) {
    ResampleContext *resample_context = create_zero<ResampleContext>();
    node->userdata = resample_context;
    if (!resample_context) {
        resample_destroy(node);
        return GenesisErrorNoMem;
    }
    return 0;
}

static void resample_seek(struct GenesisNode *node) {
    ResampleContext *resample_context = create_zero<ResampleContext>();
    resample_context->in_offset = 0;
    resample_context->out_offset = 0;
}

static void resample_run(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    struct GenesisPort *audio_in_port = node->ports[0];
    struct GenesisPort *audio_out_port = node->ports[1];

    int output_byte_count = genesis_audio_out_port_free_count(audio_out_port);
    int input_byte_count = genesis_audio_in_port_fill_count(audio_in_port);

    int output_bytes_per_frame = genesis_audio_port_bytes_per_frame(audio_out_port);
    int input_bytes_per_frame = genesis_audio_port_bytes_per_frame(audio_in_port);

    int output_frame_count = output_byte_count / output_bytes_per_frame;
    int input_frame_count = input_byte_count / input_bytes_per_frame;

    const struct GenesisChannelLayout * out_channel_layout = genesis_audio_port_channel_layout(audio_out_port);
    const struct GenesisChannelLayout * in_channel_layout = genesis_audio_port_channel_layout(audio_in_port);

    int out_sample_rate = genesis_audio_port_sample_rate(audio_out_port);
    int in_sample_rate = genesis_audio_port_sample_rate(audio_in_port);

    float *out_buf = (float *)genesis_audio_out_port_write_ptr(audio_out_port);
    float *in_buf = (float *)genesis_audio_in_port_read_ptr(audio_in_port);

    if (!genesis_channel_layout_equal(out_channel_layout, in_channel_layout))
        panic("TODO channel layout remapping not implemented");

    int in_frame = 0;
    for (long out_frame = 0; out_frame < output_frame_count; out_frame += 1) {
        long out_by_in_rate = (resample_context->out_offset + out_frame) * in_sample_rate;
        long total_in_frame = out_by_in_rate / out_sample_rate;
        long remainder = out_by_in_rate % out_sample_rate;
        float interp = remainder / (float)out_sample_rate;
        in_frame = total_in_frame - resample_context->in_offset;
        if (in_frame + 1 >= input_frame_count) {
            output_frame_count = out_frame;
            break;
        }
        for (int ch = 0; ch < out_channel_layout->channel_count; ch += 1) {
            long in_sample_index = in_frame * in_channel_layout->channel_count + ch;
            float sample = in_buf[in_sample_index] * (1.0f - interp) + in_buf[in_sample_index + 1] * interp;
            long out_sample_index = out_frame * out_channel_layout->channel_count + ch;
            out_buf[out_sample_index] = sample;
        }
    }
    resample_context->out_offset += output_frame_count;
    resample_context->in_offset += in_frame;

    genesis_audio_in_port_advance_read_ptr(audio_in_port, in_frame * input_bytes_per_frame);
    genesis_audio_out_port_advance_write_ptr(audio_out_port, output_frame_count * output_bytes_per_frame);

    if (resample_context->in_offset > in_sample_rate * out_sample_rate &&
        resample_context->out_offset > in_sample_rate * out_sample_rate)
    {
        resample_context->in_offset = resample_context->in_offset % (in_sample_rate * out_sample_rate);
        resample_context->out_offset = resample_context->out_offset % (in_sample_rate * out_sample_rate);
    }
}

int create_resample_descriptor(GenesisContext *context) {
    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 2,
            "resample", "Resample audio and remap channel layouts.");

    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    genesis_node_descriptor_set_run_callback(node_descr, resample_run);
    genesis_node_descriptor_set_create_callback(node_descr, resample_create);
    genesis_node_descriptor_set_destroy_callback(node_descr, resample_destroy);
    genesis_node_descriptor_set_seek_callback(node_descr, resample_seek);

    struct GenesisPortDescriptor *audio_in_port = genesis_node_descriptor_create_port(
            node_descr, 0, GenesisPortTypeAudioIn, "audio_in");
    struct GenesisPortDescriptor *audio_out_port = genesis_node_descriptor_create_port(
            node_descr, 1, GenesisPortTypeAudioOut, "audio_out");

    if (!audio_in_port || !audio_out_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    const struct GenesisChannelLayout *mono_layout =
        genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);

    genesis_audio_port_descriptor_set_channel_layout(audio_in_port, mono_layout, false, -1);
    genesis_audio_port_descriptor_set_sample_rate(audio_in_port, 48000, false, -1);

    genesis_audio_port_descriptor_set_channel_layout(audio_out_port, mono_layout, false, -1);
    genesis_audio_port_descriptor_set_sample_rate(audio_out_port, 48000, false, -1);

    return 0;
}
