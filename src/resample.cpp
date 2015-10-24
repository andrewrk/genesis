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

    long over_offset;

    long upsample_factor;
    long downsample_factor;

    float *impulse_response;
    int impulse_response_size;

    long oversampled_rate;

    float channel_matrix[GENESIS_CHANNEL_ID_COUNT][GENESIS_CHANNEL_ID_COUNT];
};

static double sinc(double x) {
    return (x == 0.0) ? 1.0 : (sin(PI * x) / (PI * x));
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
    resample_context->over_offset = 0;
}

static float get_channel_value(float *samples, ResampleContext *resample_context,
        const struct SoundIoChannelLayout *in_layout,
        const struct SoundIoChannelLayout *out_layout,
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

    const struct SoundIoChannelLayout * in_channel_layout = genesis_audio_port_channel_layout(audio_in_port);
    const struct SoundIoChannelLayout * out_channel_layout = genesis_audio_port_channel_layout(audio_out_port);

    int out_channel_count = out_channel_layout->channel_count;

    float *in_buf = genesis_audio_in_port_read_ptr(audio_in_port);
    float *out_buf = genesis_audio_out_port_write_ptr(audio_out_port);

    if (!resample_context->impulse_response) {
        // no resampling; only channel remapping
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
    long over_input_start = resample_context->over_offset;
    long over_input_end = over_input_start + input_frame_count * resample_context->upsample_factor;

    // compute available output start and end in oversample coordinates
    long over_output_start = resample_context->over_offset;
    long over_output_end = over_output_start + output_frame_count * resample_context->downsample_factor;

    long over_actual_input_end = over_input_end - half_window_size;

    if (over_actual_input_end <= over_output_start)
        return;

    long over_actual_output_end = min(over_output_end, over_actual_input_end);
    long over_out_count = over_actual_output_end - over_output_start;
    assert(over_out_count > 0);

    // actual output frame count in output sample rate coordinates
    int out_frame_count = over_out_count / resample_context->downsample_factor;
    assert(out_frame_count >= 0);
    if (out_frame_count == 0)
        return;

    int in_frame_count = over_out_count / resample_context->upsample_factor;

    for (long frame = 0; frame < out_frame_count; frame += 1) {
        long over_frame = resample_context->over_offset + (frame * resample_context->downsample_factor);
        // calculate this oversampled frame
        float total[GENESIS_MAX_CHANNELS] = {0.0f};

        int impulse_start = resample_context->upsample_factor - (((over_frame - half_window_size) +
                (resample_context->upsample_factor * half_window_size)) % resample_context->upsample_factor);
        for (int impulse_i = impulse_start; impulse_i < window_size;
                impulse_i += resample_context->upsample_factor)
        {
            long over_in_index = over_frame + (impulse_i - half_window_size);
            int in_index = (over_in_index - resample_context->over_offset) / resample_context->upsample_factor;
            if (in_index < 0)
                continue;
            for (int ch = 0; ch < out_channel_count; ch += 1) {
                float over_value = get_channel_value(in_buf, resample_context,
                        in_channel_layout, out_channel_layout, in_index, ch);
                int impulse_response_index = window_size - impulse_i - 1;
                total[ch] += resample_context->impulse_response[impulse_response_index] * over_value;
            }
        }
        for (int ch = 0; ch < out_channel_count; ch += 1) {
            int out_sample_index = frame * out_channel_count + ch;
            out_buf[out_sample_index] = total[ch];
        }
    }

    genesis_audio_in_port_advance_read_ptr(audio_in_port, in_frame_count);
    genesis_audio_out_port_advance_write_ptr(audio_out_port, out_frame_count);

    resample_context->over_offset = (resample_context->over_offset +
            out_frame_count * resample_context->downsample_factor) % resample_context->oversampled_rate;
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
    const struct SoundIoChannelLayout * in_channel_layout = genesis_audio_port_channel_layout(audio_in_port);
    const struct SoundIoChannelLayout * out_channel_layout = genesis_audio_port_channel_layout(audio_out_port);
    memset(resample_context->channel_matrix, 0, sizeof(resample_context->channel_matrix));

    int in_contains[GENESIS_CHANNEL_ID_COUNT];
    int out_contains[GENESIS_CHANNEL_ID_COUNT];
    for (int id = 0; id < GENESIS_CHANNEL_ID_COUNT; id += 1) {
        in_contains[id] = soundio_channel_layout_find_channel(in_channel_layout, (SoundIoChannelId)id);
        out_contains[id] = soundio_channel_layout_find_channel(out_channel_layout, (SoundIoChannelId)id);
    }

    bool unaccounted[GENESIS_CHANNEL_ID_COUNT];
    for (int id = 0; id < GENESIS_CHANNEL_ID_COUNT; id += 1) {
        unaccounted[id] = in_contains[id] >= 0 && out_contains[id] == -1;
    }

    // route matching channel ids
    for (int id = 0; id < GENESIS_CHANNEL_ID_COUNT; id += 1) {
        if (in_contains[id] >= 0 && out_contains[id] >= 0)
            resample_context->channel_matrix[out_contains[id]][in_contains[id]] = 1.0;
    }

    // mix front center to left/right
    if (unaccounted[SoundIoChannelIdFrontCenter]) {
        if (out_contains[SoundIoChannelIdFrontLeft] >= 0 && out_contains[SoundIoChannelIdFrontRight] >= 0) {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontLeft]][in_contains[SoundIoChannelIdFrontCenter]] += M_SQRT1_2;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontRight]][in_contains[SoundIoChannelIdFrontCenter]] += M_SQRT1_2;
            unaccounted[SoundIoChannelIdFrontCenter] = false;
        }
    }

    // mix back left/right to back center, side or front
    if (unaccounted[SoundIoChannelIdBackLeft] && unaccounted[SoundIoChannelIdBackRight]) {
        if (out_contains[SoundIoChannelIdBackCenter] >= 0) {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdBackCenter]][in_contains[SoundIoChannelIdBackLeft]] += M_SQRT1_2;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdBackCenter]][in_contains[SoundIoChannelIdBackRight]] += M_SQRT1_2;
            unaccounted[SoundIoChannelIdBackLeft] = false;
            unaccounted[SoundIoChannelIdBackRight] = false;
        } else if (out_contains[SoundIoChannelIdSideLeft] >= 0 &&
                   out_contains[SoundIoChannelIdSideRight] >= 0)
        {
            if (in_contains[SoundIoChannelIdSideLeft] >= 0 &&
                in_contains[SoundIoChannelIdSideRight] >= 0)
            {
                resample_context->channel_matrix[out_contains[SoundIoChannelIdSideLeft]][in_contains[SoundIoChannelIdBackLeft]] += M_SQRT1_2;
                resample_context->channel_matrix[out_contains[SoundIoChannelIdSideRight]][in_contains[SoundIoChannelIdBackRight]] += M_SQRT1_2;
            } else {
                resample_context->channel_matrix[out_contains[SoundIoChannelIdSideLeft]][in_contains[SoundIoChannelIdBackLeft]] += 1.0;
                resample_context->channel_matrix[out_contains[SoundIoChannelIdSideRight]][in_contains[SoundIoChannelIdBackRight]] += 1.0;
            }
            unaccounted[SoundIoChannelIdBackLeft] = false;
            unaccounted[SoundIoChannelIdBackRight] = false;
        } else if (out_contains[SoundIoChannelIdFrontLeft] >= 0 &&
                   out_contains[SoundIoChannelIdFrontRight] >= 0)
        {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontLeft]][in_contains[SoundIoChannelIdBackLeft]] += surround_mix_level * M_SQRT1_2;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontRight]][in_contains[SoundIoChannelIdBackRight]] += surround_mix_level * M_SQRT1_2;
            unaccounted[SoundIoChannelIdBackLeft] = false;
            unaccounted[SoundIoChannelIdBackRight] = false;
        } else if (out_contains[SoundIoChannelIdFrontCenter] >= 0) {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontCenter]][in_contains[SoundIoChannelIdBackLeft]] += surround_mix_level * M_SQRT1_2;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontCenter]][in_contains[SoundIoChannelIdBackRight]] += surround_mix_level * M_SQRT1_2;
            unaccounted[SoundIoChannelIdBackLeft] = false;
            unaccounted[SoundIoChannelIdBackRight] = false;
        }
    }

    // mix side left/right into back or front
    if (unaccounted[SoundIoChannelIdSideLeft] && unaccounted[SoundIoChannelIdSideRight]) {
        if (out_contains[SoundIoChannelIdBackLeft] >= 0 &&
            out_contains[SoundIoChannelIdBackRight] >= 0)
        {
            // if back channels do not exist in the input, just copy side
            // channels to back channels, otherwise mix side into back
            if (in_contains[SoundIoChannelIdBackLeft] >= 0 &&
                in_contains[SoundIoChannelIdBackRight] >= 0)
            {
                resample_context->channel_matrix[out_contains[SoundIoChannelIdBackLeft]][in_contains[SoundIoChannelIdSideLeft]] += M_SQRT1_2;
                resample_context->channel_matrix[out_contains[SoundIoChannelIdBackRight]][in_contains[SoundIoChannelIdSideRight]] += M_SQRT1_2;
            } else {
                resample_context->channel_matrix[out_contains[SoundIoChannelIdBackLeft]][in_contains[SoundIoChannelIdSideLeft]] += 1.0;
                resample_context->channel_matrix[out_contains[SoundIoChannelIdBackRight]][in_contains[SoundIoChannelIdSideRight]] += 1.0;
            }
            unaccounted[SoundIoChannelIdSideLeft] = false;
            unaccounted[SoundIoChannelIdSideRight] = false;
        } else if (out_contains[SoundIoChannelIdBackCenter] >= 0) {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdBackCenter]][in_contains[SoundIoChannelIdSideLeft]] += M_SQRT1_2;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdBackCenter]][in_contains[SoundIoChannelIdSideRight]] += M_SQRT1_2;
            unaccounted[SoundIoChannelIdSideLeft] = false;
            unaccounted[SoundIoChannelIdSideRight] = false;
        } else if (out_contains[SoundIoChannelIdFrontLeft] >= 0 &&
                   out_contains[SoundIoChannelIdFrontRight] >= 0)
        {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontLeft]][in_contains[SoundIoChannelIdSideLeft]] += surround_mix_level;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontRight]][in_contains[SoundIoChannelIdSideRight]] += surround_mix_level;
            unaccounted[SoundIoChannelIdSideLeft] = false;
            unaccounted[SoundIoChannelIdSideRight] = false;
        } else if (out_contains[SoundIoChannelIdFrontCenter] >= 0) {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontCenter]][in_contains[SoundIoChannelIdSideLeft]] += surround_mix_level;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontCenter]][in_contains[SoundIoChannelIdSideRight]] += surround_mix_level;
            unaccounted[SoundIoChannelIdSideLeft] = false;
            unaccounted[SoundIoChannelIdSideRight] = false;
        }
    }

    // mix left of center/right of center into front left/right or center
    if (unaccounted[SoundIoChannelIdFrontLeftCenter] &&
        unaccounted[SoundIoChannelIdFrontRightCenter])
    {
        if (out_contains[SoundIoChannelIdFrontLeft] >= 0 && out_contains[SoundIoChannelIdFrontRight] >= 0) {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontLeft]][in_contains[SoundIoChannelIdFrontLeftCenter]] += 1.0;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontRight]][in_contains[SoundIoChannelIdFrontRightCenter]] += 1.0;
            unaccounted[SoundIoChannelIdFrontLeftCenter] = false;
            unaccounted[SoundIoChannelIdFrontRightCenter] = false;
        } else if (out_contains[SoundIoChannelIdFrontCenter] >= 0) {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontCenter]][in_contains[SoundIoChannelIdFrontLeftCenter]] += M_SQRT1_2;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontCenter]][in_contains[SoundIoChannelIdFrontRightCenter]] += M_SQRT1_2;
            unaccounted[SoundIoChannelIdFrontLeftCenter] = false;
            unaccounted[SoundIoChannelIdFrontRightCenter] = false;
        }
    }

    // mix LFE into front left/right or center
    if (unaccounted[SoundIoChannelIdLfe]) {
        if (out_contains[SoundIoChannelIdFrontCenter] >= 0) {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontCenter]][in_contains[SoundIoChannelIdLfe]] += lfe_mix_level;
            unaccounted[SoundIoChannelIdLfe] = false;
        } else if (out_contains[SoundIoChannelIdFrontLeft] >= 0 &&
                out_contains[SoundIoChannelIdFrontRight] >= 0)
        {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontLeft]][in_contains[SoundIoChannelIdLfe]] += lfe_mix_level * M_SQRT1_2;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontRight]][in_contains[SoundIoChannelIdLfe]] += lfe_mix_level * M_SQRT1_2;
            unaccounted[SoundIoChannelIdLfe] = false;
        }
    }

    // mix front left/right to front center
    if (unaccounted[SoundIoChannelIdFrontLeft] && unaccounted[SoundIoChannelIdFrontRight]) {
        if (out_contains[SoundIoChannelIdFrontCenter] >= 0) {
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontCenter]][in_contains[SoundIoChannelIdFrontLeft]] += 0.5;
            resample_context->channel_matrix[out_contains[SoundIoChannelIdFrontCenter]][in_contains[SoundIoChannelIdFrontRight]] += 0.5;
            unaccounted[SoundIoChannelIdFrontLeft] = false;
            unaccounted[SoundIoChannelIdFrontRight] = false;
        }
    }

    // make sure all input channels are accounted for
    for (int id = 0; id < GENESIS_CHANNEL_ID_COUNT; id += 1) {
        if (unaccounted[id]) {
            fprintf(stderr, "unaccounted: %s\n", soundio_get_channel_name((SoundIoChannelId)id));
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

int create_resample_descriptor(GenesisPipeline *pipeline) {
    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(pipeline, 2,
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

    int default_sample_rate = genesis_pipeline_get_sample_rate(pipeline);

    const struct SoundIoChannelLayout *mono_layout =
        soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);

    genesis_audio_port_descriptor_set_channel_layout(audio_in_port, mono_layout, false, -1);
    genesis_audio_port_descriptor_set_sample_rate(audio_in_port, default_sample_rate, false, -1);

    genesis_audio_port_descriptor_set_channel_layout(audio_out_port, mono_layout, false, -1);
    genesis_audio_port_descriptor_set_sample_rate(audio_out_port, default_sample_rate, false, -1);

    return 0;
}
