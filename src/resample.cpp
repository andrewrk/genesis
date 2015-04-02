#include "resample.hpp"
#include "libav.hpp"
#include "audio_file.hpp"
#include <assert.h>

static const int BYTES_PER_SAMPLE = 4; // float samples

struct ResampleContext {
    AVAudioResampleContext *avr;
    bool in_connected;
    bool out_connected;
};

static void resample_destroy(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    if (resample_context) {
        avresample_free(&resample_context->avr);
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
    resample_context->avr = avresample_alloc_context();
    if (!resample_context->avr) {
        resample_destroy(node);
        return GenesisErrorNoMem;
    }
    return 0;
}

static int port_connected(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    if (!resample_context->in_connected || !resample_context->out_connected)
        return 0;

    struct GenesisPort *audio_in_port = node->ports[0];
    struct GenesisPort *audio_out_port = node->ports[1];

    uint64_t in_channel_layout = channel_layout_to_libav(genesis_audio_port_channel_layout(audio_in_port));
    uint64_t out_channel_layout = channel_layout_to_libav(genesis_audio_port_channel_layout(audio_out_port));

    int in_sample_rate = genesis_audio_port_sample_rate(audio_in_port);
    int out_sample_rate = genesis_audio_port_sample_rate(audio_out_port);

    int err = 0;
    err = err || av_opt_set_int(resample_context->avr, "in_channel_layout",  in_channel_layout,  0);
    err = err || av_opt_set_int(resample_context->avr, "out_channel_layout", out_channel_layout, 0);
    err = err || av_opt_set_int(resample_context->avr, "in_sample_rate",     in_sample_rate,     0);
    err = err || av_opt_set_int(resample_context->avr, "out_sample_rate",    out_sample_rate,    0);
    err = err || av_opt_set_int(resample_context->avr, "in_sample_fmt",      AV_SAMPLE_FMT_FLT,  0);
    err = err || av_opt_set_int(resample_context->avr, "out_sample_fmt",     AV_SAMPLE_FMT_FLT,  0);
    if (err)
        return GenesisErrorInvalidState;

    err = avresample_open(resample_context->avr);
    if (err)
        return GenesisErrorInvalidState;

    return 0;
}

static void port_disconnected(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    avresample_close(resample_context->avr);
}

static void resample_seek(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    avresample_close(resample_context->avr);
    int err = avresample_open(resample_context->avr);
    assert(!err);
}

static void resample_run(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    struct GenesisPort *audio_in_port = node->ports[0];
    struct GenesisPort *audio_out_port = node->ports[1];

    int output_byte_count = genesis_audio_out_port_free_count(audio_out_port);
    int input_byte_count = genesis_audio_in_port_fill_count(audio_in_port);

    int output_samples_count = output_byte_count / BYTES_PER_SAMPLE;
    int input_samples_count = input_byte_count / BYTES_PER_SAMPLE;

    uint8_t *out_buf = (uint8_t *)genesis_audio_out_port_write_ptr(audio_out_port);
    uint8_t *in_buf = (uint8_t *)genesis_audio_in_port_read_ptr(audio_in_port);

    int samples_written = avresample_convert(resample_context->avr,
            &out_buf, output_byte_count, output_samples_count,
            &in_buf, input_byte_count, input_samples_count);

    genesis_audio_in_port_advance_read_ptr(audio_in_port, input_samples_count * BYTES_PER_SAMPLE);
    genesis_audio_out_port_advance_write_ptr(audio_out_port, samples_written * BYTES_PER_SAMPLE);
}

static int in_connect(struct GenesisPort *port, struct GenesisPort *other_port) {
    struct GenesisNode *node = genesis_port_node(port);
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    resample_context->in_connected = true;
    return port_connected(node);
}

static int out_connect(struct GenesisPort *port, struct GenesisPort *other_port) {
    struct GenesisNode *node = genesis_port_node(port);
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    resample_context->out_connected = true;
    return port_connected(node);
}

static void in_disconnect(struct GenesisPort *port, struct GenesisPort *other_port) {
    struct GenesisNode *node = genesis_port_node(port);
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    resample_context->in_connected = false;
    port_disconnected(node);
}

static void out_disconnect(struct GenesisPort *port, struct GenesisPort *other_port) {
    struct GenesisNode *node = genesis_port_node(port);
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    resample_context->out_connected = false;
    port_disconnected(node);
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

    genesis_port_descriptor_set_connect_callback(audio_in_port, in_connect);
    genesis_port_descriptor_set_connect_callback(audio_out_port, out_connect);
    genesis_port_descriptor_set_disconnect_callback(audio_in_port, in_disconnect);
    genesis_port_descriptor_set_disconnect_callback(audio_out_port, out_disconnect);

    const struct GenesisChannelLayout *mono_layout =
        genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);

    genesis_audio_port_descriptor_set_channel_layout(audio_in_port, mono_layout, false, -1);
    genesis_audio_port_descriptor_set_sample_rate(audio_in_port, 48000, false, -1);

    genesis_audio_port_descriptor_set_channel_layout(audio_out_port, mono_layout, false, -1);
    genesis_audio_port_descriptor_set_sample_rate(audio_out_port, 48000, false, -1);

    return 0;
}
