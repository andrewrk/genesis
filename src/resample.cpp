#include "resample.hpp"
#include "audio_file.hpp"
#include "util.hpp"

static const double PI = 3.14159265358979323846;
static const double transition_band_hz = 800.0;

struct ResampleContext {
    bool in_connected;
    bool out_connected;

    long in_offset;
    long out_offset;

    int upsample_factor;
    int downsample_factor;

    float *impulse_response;
    int impulse_response_size;

    long oversampled_rate;
};

static double sinc(double x) {
    return (x == 0.0) ? 1.0 : (sinf(PI * x) / (PI * x));
}

static double blackman_window(double n, double size) {
    return 0.42 -
        0.50 * cos(2.0 * PI * n / (size - 1.0)) +
        0.08 * cos(4.0 * PI * n / (size - 1.0));
}

static void resample_destroy(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    if (resample_context) {
        destroy(resample_context->impulse_response, resample_context->impulse_response_size);
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
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    resample_context->in_offset = 0;
    resample_context->out_offset = 0;
}

static void resample_run(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    struct GenesisPort *audio_in_port = node->ports[0];
    struct GenesisPort *audio_out_port = node->ports[1];

    int input_frame_count = genesis_audio_in_port_fill_count(audio_in_port);
    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);

    const struct GenesisChannelLayout * in_channel_layout = genesis_audio_port_channel_layout(audio_in_port);
    const struct GenesisChannelLayout * out_channel_layout = genesis_audio_port_channel_layout(audio_out_port);

    if (!genesis_channel_layout_equal(in_channel_layout, out_channel_layout))
        panic("TODO channel layout remapping not implemented");

    int in_channel_count = in_channel_layout->channel_count;
    int out_channel_count = out_channel_layout->channel_count;

    float *in_buf = genesis_audio_in_port_read_ptr(audio_in_port);
    float *out_buf = genesis_audio_out_port_write_ptr(audio_out_port);

    // pretend that we have upsampled the input to the oversample rate by adding zeroes
    // now low-pass filter using our impulse response window

    int window_size = resample_context->impulse_response_size;
    int half_window_size = window_size / 2;

    // compute available input start and end in oversample coordinates
    long over_input_start = resample_context->in_offset * resample_context->upsample_factor;
    long over_input_end = over_input_start + input_frame_count * resample_context->upsample_factor;

    // compute available output start and end in oversample coordinates
    long over_output_start = resample_context->out_offset * resample_context->downsample_factor;
    long over_output_end = over_output_start + output_frame_count * resample_context->downsample_factor;

    if (over_input_end - half_window_size < over_output_start)
        return;

    long over_actual_output_end = min(over_output_end, over_input_end - half_window_size);
    if (over_actual_output_end - over_output_start < 0)
        return;

    // actual output frame count in output sample rate coordinates
    int out_frame_count = (over_actual_output_end - over_output_start) / resample_context->downsample_factor;

    // actual input start and end in oversample coordinates
    long over_actual_input_start = over_output_start - half_window_size;
    long next_over_actual_input_start = over_actual_output_end - half_window_size;
    long over_in_frame_count = next_over_actual_input_start - over_actual_input_start;
    int in_frame_count = over_in_frame_count / resample_context->upsample_factor;

    for (int frame = 0; frame < out_frame_count; frame += 1) {
        long over_frame = (resample_context->out_offset + frame) * resample_context->downsample_factor;
        // calculate this oversampled frame
        float total[GENESIS_MAX_CHANNELS] = {0.0f};

        int impulse_start = resample_context->upsample_factor - (((over_frame - half_window_size) +
                (resample_context->upsample_factor * half_window_size)) % resample_context->upsample_factor);
        for (int impulse_i = impulse_start; impulse_i < window_size;
                impulse_i += resample_context->upsample_factor)
        {
            long over_in_index = over_frame + (impulse_i - half_window_size);
            int in_index = (over_in_index / resample_context->upsample_factor) - resample_context->in_offset;
            if (in_index < 0)
                continue;
            for (int ch = 0; ch < out_channel_count; ch += 1) {
                float over_value = in_buf[in_index * in_channel_count + ch];
                total[ch] += resample_context->impulse_response[window_size - impulse_i - 1] * over_value;
            }
        }
        for (int ch = 0; ch < out_channel_count; ch += 1) {
            int out_sample_index = frame * out_channel_count + ch;
            out_buf[out_sample_index] = total[ch];
        }
    }

    genesis_audio_in_port_advance_read_ptr(audio_in_port, in_frame_count);
    genesis_audio_out_port_advance_write_ptr(audio_out_port, out_frame_count);

    resample_context->in_offset += in_frame_count;
    resample_context->out_offset += out_frame_count;

    if (resample_context->in_offset > resample_context->oversampled_rate &&
        resample_context->out_offset > resample_context->oversampled_rate)
    {
        resample_context->in_offset = resample_context->in_offset % resample_context->oversampled_rate;
        resample_context->out_offset = resample_context->out_offset % resample_context->oversampled_rate;
    }
}

static int port_connected(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    if (!resample_context->in_connected || !resample_context->out_connected)
        return 0;

    struct GenesisPort *audio_in_port = node->ports[0];
    struct GenesisPort *audio_out_port = node->ports[1];

    //const struct GenesisChannelLayout * in_channel_layout = genesis_audio_port_channel_layout(audio_in_port);
    //const struct GenesisChannelLayout * out_channel_layout = genesis_audio_port_channel_layout(audio_out_port);

    int in_sample_rate = genesis_audio_port_sample_rate(audio_in_port);
    int out_sample_rate = genesis_audio_port_sample_rate(audio_out_port);

    int gcd = greatest_common_denominator(in_sample_rate, out_sample_rate);
    resample_context->upsample_factor = out_sample_rate / gcd;
    resample_context->downsample_factor = in_sample_rate / gcd;

    resample_context->oversampled_rate = in_sample_rate * resample_context->upsample_factor;

    double cutoff_freq_hz = min(in_sample_rate, out_sample_rate) / 2.0;
    double cutoff_freq_float = cutoff_freq_hz / (double)resample_context->oversampled_rate;

    double transition_band = transition_band_hz / resample_context->oversampled_rate;
    int window_size = ceil(4.0 / transition_band);
    window_size += !(window_size % 2);

    float *new_impulse_response = reallocate_safe(resample_context->impulse_response,
            resample_context->impulse_response_size, window_size);

    if (!new_impulse_response)
        return GenesisErrorNoMem;

    resample_context->impulse_response = new_impulse_response;
    resample_context->impulse_response_size = window_size;

    // create impulse response by sampling the sinc function and then
    // multiplying by the blackman window
    for (int i = 0; i < window_size; i += 1) {
        // sample the sinc function
        double sinc_sample = sinc(2.0 * cutoff_freq_float * (i - (window_size - 1) / 2.0));
        double sample = sinc_sample * blackman_window(i, window_size);
        resample_context->impulse_response[i] = sample;
    }

    return 0;
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
}

static void out_disconnect(struct GenesisPort *port, struct GenesisPort *other_port) {
    struct GenesisNode *node = genesis_port_node(port);
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    resample_context->out_connected = false;
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
