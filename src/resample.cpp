#include "resample.hpp"
#include "audio_file.hpp"
#include "util.hpp"

static const double PI = 3.14159265358979323846;
static const double transition_band_hz = 800.0;

static const double lfe_mix_level = 1.0;
static const double surround_mix_level = 1.0;

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

    float channel_matrix[GenesisChannelIdCount][GenesisChannelIdCount];
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

static inline float get_channel_value(float *samples, ResampleContext *resample_context,
        const struct GenesisChannelLayout *in_layout,
        const struct GenesisChannelLayout *out_layout,
        int in_frame_index, int out_channel_index)
{
    float sum = 0.0f;
    for (int in_ch = 0; in_ch < in_layout->channel_count; in_ch += 1) {
        float in_sample = samples[in_frame_index * in_layout->channel_count + in_ch];
        sum += resample_context->channel_matrix[out_channel_index][in_ch] * in_sample;
    }
    return sum;
}

static void resample_run(struct GenesisNode *node) {
    struct ResampleContext *resample_context = (struct ResampleContext *)node->userdata;
    struct GenesisPort *audio_in_port = node->ports[0];
    struct GenesisPort *audio_out_port = node->ports[1];

    int input_frame_count = genesis_audio_in_port_fill_count(audio_in_port);
    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);

    const struct GenesisChannelLayout * in_channel_layout = genesis_audio_port_channel_layout(audio_in_port);
    const struct GenesisChannelLayout * out_channel_layout = genesis_audio_port_channel_layout(audio_out_port);

    int out_channel_count = out_channel_layout->channel_count;

    float *in_buf = genesis_audio_in_port_read_ptr(audio_in_port);
    float *out_buf = genesis_audio_out_port_write_ptr(audio_out_port);

    if (!resample_context->impulse_response) {
        int frame_count = min(input_frame_count, output_frame_count);
        for (int frame = 0; frame < frame_count; frame += 1) {
            for (int ch = 0; ch < out_channel_count; ch += 1) {
                out_buf[frame * out_channel_count + ch] = get_channel_value(in_buf,
                        resample_context, in_channel_layout, out_channel_layout, frame, ch);
            }
        }
        genesis_audio_in_port_advance_read_ptr(audio_in_port, frame_count);
        genesis_audio_out_port_advance_write_ptr(audio_out_port, frame_count);
        return;
    }

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
                float over_value = get_channel_value(in_buf, resample_context,
                        in_channel_layout, out_channel_layout, in_index, ch);
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

    int in_sample_rate = genesis_audio_port_sample_rate(audio_in_port);
    int out_sample_rate = genesis_audio_port_sample_rate(audio_out_port);

    int gcd = greatest_common_denominator(in_sample_rate, out_sample_rate);
    resample_context->upsample_factor = out_sample_rate / gcd;
    resample_context->downsample_factor = in_sample_rate / gcd;

    resample_context->oversampled_rate = in_sample_rate * resample_context->upsample_factor;

    if (in_sample_rate == out_sample_rate) {
        destroy(resample_context->impulse_response, resample_context->impulse_response_size);
        resample_context->impulse_response = nullptr;
    } else {
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
    }

    // set up channel matrix
    const struct GenesisChannelLayout * in_channel_layout = genesis_audio_port_channel_layout(audio_in_port);
    const struct GenesisChannelLayout * out_channel_layout = genesis_audio_port_channel_layout(audio_out_port);
    memset(resample_context->channel_matrix, 0, sizeof(resample_context->channel_matrix));

    int in_contains[GenesisChannelIdCount];
    int out_contains[GenesisChannelIdCount];
    for (int id = 0; id < GenesisChannelIdCount; id += 1) {
        in_contains[id] = genesis_channel_layout_find_channel(in_channel_layout, (GenesisChannelId)id);
        out_contains[id] = genesis_channel_layout_find_channel(out_channel_layout, (GenesisChannelId)id);
    }

    bool unaccounted[GenesisChannelIdCount];
    for (int id = 0; id < GenesisChannelIdCount; id += 1) {
        unaccounted[id] = in_contains[id] >= 0 && out_contains[id] == -1;
    }

    // route matching channel ids
    for (int id = 0; id < GenesisChannelIdCount; id += 1) {
        if (in_contains[id] >= 0 && out_contains[id] >= 0)
            resample_context->channel_matrix[out_contains[id]][in_contains[id]] = 1.0;
    }

    // mix front center to left/right
    if (unaccounted[GenesisChannelIdFrontCenter]) {
        if (out_contains[GenesisChannelIdFrontLeft] >= 0 && out_contains[GenesisChannelIdFrontRight] >= 0) {
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontLeft]][in_contains[GenesisChannelIdFrontCenter]] += M_SQRT1_2;
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontRight]][in_contains[GenesisChannelIdFrontCenter]] += M_SQRT1_2;
            unaccounted[GenesisChannelIdFrontCenter] = false;
        }
    }

    // mix back left/right to back center, side or front
    if (unaccounted[GenesisChannelIdBackLeft] && unaccounted[GenesisChannelIdBackRight]) {
        if (out_contains[GenesisChannelIdBackCenter] >= 0) {
            resample_context->channel_matrix[out_contains[GenesisChannelIdBackCenter]][in_contains[GenesisChannelIdBackLeft]] += M_SQRT1_2;
            resample_context->channel_matrix[out_contains[GenesisChannelIdBackCenter]][in_contains[GenesisChannelIdBackRight]] += M_SQRT1_2;
            unaccounted[GenesisChannelIdBackLeft] = false;
            unaccounted[GenesisChannelIdBackRight] = false;
        } else if (out_contains[GenesisChannelIdSideLeft] >= 0 &&
                   out_contains[GenesisChannelIdSideRight] >= 0)
        {
            if (in_contains[GenesisChannelIdSideLeft] >= 0 &&
                in_contains[GenesisChannelIdSideRight] >= 0)
            {
                resample_context->channel_matrix[out_contains[GenesisChannelIdSideLeft]][in_contains[GenesisChannelIdBackLeft]] += M_SQRT1_2;
                resample_context->channel_matrix[out_contains[GenesisChannelIdSideRight]][in_contains[GenesisChannelIdBackRight]] += M_SQRT1_2;
            } else {
                resample_context->channel_matrix[out_contains[GenesisChannelIdSideLeft]][in_contains[GenesisChannelIdBackLeft]] += 1.0;
                resample_context->channel_matrix[out_contains[GenesisChannelIdSideRight]][in_contains[GenesisChannelIdBackRight]] += 1.0;
            }
            unaccounted[GenesisChannelIdBackLeft] = false;
            unaccounted[GenesisChannelIdBackRight] = false;
        } else if (out_contains[GenesisChannelIdFrontLeft] >= 0 &&
                   out_contains[GenesisChannelIdFrontRight] >= 0)
        {
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontLeft]][in_contains[GenesisChannelIdBackLeft]] += surround_mix_level * M_SQRT1_2;
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontRight]][in_contains[GenesisChannelIdBackRight]] += surround_mix_level * M_SQRT1_2;
            unaccounted[GenesisChannelIdBackLeft] = false;
            unaccounted[GenesisChannelIdBackRight] = false;
        } else if (out_contains[GenesisChannelIdFrontCenter] >= 0) {
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontCenter]][in_contains[GenesisChannelIdBackLeft]] += surround_mix_level * M_SQRT1_2;
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontCenter]][in_contains[GenesisChannelIdBackRight]] += surround_mix_level * M_SQRT1_2;
            unaccounted[GenesisChannelIdBackLeft] = false;
            unaccounted[GenesisChannelIdBackRight] = false;
        }
    }

    // mix side left/right into back or front
    if (unaccounted[GenesisChannelIdSideLeft] && unaccounted[GenesisChannelIdSideRight]) {
        if (out_contains[GenesisChannelIdBackLeft] >= 0 &&
            out_contains[GenesisChannelIdBackRight] >= 0)
        {
            // if back channels do not exist in the input, just copy side
            // channels to back channels, otherwise mix side into back
            if (in_contains[GenesisChannelIdBackLeft] >= 0 &&
                in_contains[GenesisChannelIdBackRight] >= 0)
            {
                resample_context->channel_matrix[out_contains[GenesisChannelIdBackLeft]][in_contains[GenesisChannelIdSideLeft]] += M_SQRT1_2;
                resample_context->channel_matrix[out_contains[GenesisChannelIdBackRight]][in_contains[GenesisChannelIdSideRight]] += M_SQRT1_2;
            } else {
                resample_context->channel_matrix[out_contains[GenesisChannelIdBackLeft]][in_contains[GenesisChannelIdSideLeft]] += 1.0;
                resample_context->channel_matrix[out_contains[GenesisChannelIdBackRight]][in_contains[GenesisChannelIdSideRight]] += 1.0;
            }
            unaccounted[GenesisChannelIdSideLeft] = false;
            unaccounted[GenesisChannelIdSideRight] = false;
        } else if (out_contains[GenesisChannelIdBackCenter] >= 0) {
            resample_context->channel_matrix[out_contains[GenesisChannelIdBackCenter]][in_contains[GenesisChannelIdSideLeft]] += M_SQRT1_2;
            resample_context->channel_matrix[out_contains[GenesisChannelIdBackCenter]][in_contains[GenesisChannelIdSideRight]] += M_SQRT1_2;
            unaccounted[GenesisChannelIdSideLeft] = false;
            unaccounted[GenesisChannelIdSideRight] = false;
        } else if (out_contains[GenesisChannelIdFrontLeft] >= 0 &&
                   out_contains[GenesisChannelIdFrontRight] >= 0)
        {
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontLeft]][in_contains[GenesisChannelIdSideLeft]] += surround_mix_level;
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontRight]][in_contains[GenesisChannelIdSideRight]] += surround_mix_level;
            unaccounted[GenesisChannelIdSideLeft] = false;
            unaccounted[GenesisChannelIdSideRight] = false;
        } else if (out_contains[GenesisChannelIdFrontCenter] >= 0) {
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontCenter]][in_contains[GenesisChannelIdSideLeft]] += surround_mix_level;
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontCenter]][in_contains[GenesisChannelIdSideRight]] += surround_mix_level;
            unaccounted[GenesisChannelIdSideLeft] = false;
            unaccounted[GenesisChannelIdSideRight] = false;
        }
    }

    // mix left of center/right of center into front left/right or center
    if (unaccounted[GenesisChannelIdFrontLeftOfCenter] &&
        unaccounted[GenesisChannelIdFrontRightOfCenter])
    {
        if (out_contains[GenesisChannelIdFrontLeft] >= 0 && out_contains[GenesisChannelIdFrontRight] >= 0) {
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontLeft]][in_contains[GenesisChannelIdFrontLeftOfCenter]] += 1.0;
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontRight]][in_contains[GenesisChannelIdFrontRightOfCenter]] += 1.0;
            unaccounted[GenesisChannelIdFrontLeftOfCenter] = false;
            unaccounted[GenesisChannelIdFrontRightOfCenter] = false;
        } else if (out_contains[GenesisChannelIdFrontCenter] >= 0) {
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontCenter]][in_contains[GenesisChannelIdFrontLeftOfCenter]] += M_SQRT1_2;
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontCenter]][in_contains[GenesisChannelIdFrontRightOfCenter]] += M_SQRT1_2;
            unaccounted[GenesisChannelIdFrontLeftOfCenter] = false;
            unaccounted[GenesisChannelIdFrontRightOfCenter] = false;
        }
    }

    // mix LFE into front left/right or center
    if (unaccounted[GenesisChannelIdLowFrequency]) {
        if (out_contains[GenesisChannelIdFrontCenter] >= 0) {
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontCenter]][in_contains[GenesisChannelIdLowFrequency]] += lfe_mix_level;
            unaccounted[GenesisChannelIdLowFrequency] = false;
        } else if (out_contains[GenesisChannelIdFrontLeft] >= 0 &&
                out_contains[GenesisChannelIdFrontRight] >= 0)
        {
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontLeft]][in_contains[GenesisChannelIdLowFrequency]] += lfe_mix_level * M_SQRT1_2;
            resample_context->channel_matrix[out_contains[GenesisChannelIdFrontRight]][in_contains[GenesisChannelIdLowFrequency]] += lfe_mix_level * M_SQRT1_2;
            unaccounted[GenesisChannelIdLowFrequency] = false;
        }
    }

    // make sure all input channels are accounted for
    for (int id = 0; id < GenesisChannelIdCount; id += 1) {
        if (unaccounted[id]) {
            fprintf(stderr, "unaccounted: %s\n", genesis_get_channel_name((GenesisChannelId)id));
            return GenesisErrorUnimplemented;
        }
    }

    // normalize per channel
    for (int out_ch = 0; out_ch < out_channel_layout->channel_count; out_ch += 1) {
        double sum = 0.0;
        for (int in_ch = 0; in_ch < in_channel_layout->channel_count; in_ch += 1) {
            sum += resample_context->channel_matrix[out_ch][in_ch];
        }
        if (sum > 0.0) {
            for (int in_ch = 0; in_ch < in_channel_layout->channel_count; in_ch += 1) {
                resample_context->channel_matrix[out_ch][in_ch] *= 1.0 / sum;
            }
        }
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
