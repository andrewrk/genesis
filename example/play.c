#include "genesis.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// play the input file over the default playback device

struct PlayChannelContext {
    struct GenesisAudioFileIterator iter;
    long offset;
};

struct PlayContext {
    volatile bool running;
    struct PlayChannelContext channel_context[GENESIS_MAX_CHANNELS];
    long frame_index;
    long frame_count;
    struct GenesisContext *context;
};

static void on_underrun(void *userdata) {
    static int underrun_count = 0;
    fprintf(stderr, "buffer underrun %d\n", ++underrun_count);
}

static void audio_file_node_run(struct GenesisNode *node) {
    const struct GenesisNodeDescriptor *node_descriptor = genesis_node_descriptor(node);
    struct PlayContext *play_context = (struct PlayContext *)genesis_node_descriptor_userdata(node_descriptor);
    if (!play_context->running)
        return;

    struct GenesisPort *audio_out_port = genesis_node_port(node, 0);

    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);
    const struct SoundIoChannelLayout *channel_layout = genesis_audio_port_channel_layout(audio_out_port);
    int channel_count = channel_layout->channel_count;
    float *out_samples = (float *)genesis_audio_out_port_write_ptr(audio_out_port);

    int audio_file_frames_left = play_context->frame_count - play_context->frame_index;
    bool end_detected = audio_file_frames_left <= 0;
    int output_end = (output_frame_count < audio_file_frames_left) ?
        output_frame_count : audio_file_frames_left;

    for (int ch = 0; ch < channel_count; ch += 1) {
        struct PlayChannelContext *channel_context = &play_context->channel_context[ch];
        for (int frame_offset = 0; frame_offset < output_end; frame_offset += 1) {
            if (channel_context->offset >= channel_context->iter.end) {
                genesis_audio_file_iterator_next(&channel_context->iter);
                channel_context->offset = 0;
            }

            out_samples[channel_count * frame_offset + ch] =
                channel_context->iter.ptr[channel_context->offset];

            channel_context->offset += 1;
        }
    }

    genesis_audio_out_port_advance_write_ptr(audio_out_port, output_end);
    play_context->frame_index += output_end;

    if (end_detected) {
        play_context->running = false;
        genesis_wakeup(play_context->context);
    }
}

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s [options] audio_file\n"
            "Options:\n"
            "  [--backend dummy|alsa|pulseaudio|jack|coreaudio|wasapi]\n"
            "  [--device id]\n"
            "  [--raw]\n"
            "  [--latency seconds]\n"
            "  [--force-resample]\n"
            , exe);
    return 1;
}

static void print_channel_and_rate(const struct SoundIoChannelLayout *layout, int sample_rate) {
    if (layout->name)
        fprintf(stderr, "%d Hz (%s)", sample_rate, layout->name);
    else
        fprintf(stderr, "%d Hz (%d channel)", sample_rate, layout->channel_count);
}

int main(int argc, char **argv) {
    char *exe = argv[0];
    bool raw = false;
    enum SoundIoBackend backend = SoundIoBackendNone;
    char *device_id = NULL;
    double latency = 0.0;
    const char *input_filename = NULL;
    bool force_resample = false;

    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            if (strcmp(arg, "--raw") == 0) {
                raw = true;
            } else if (strcmp(arg, "--force-resample") == 0) {
                force_resample = true;
            } else {
                i += 1;
                if (i >= argc) {
                    return usage(exe);
                } else if (strcmp(arg, "--backend") == 0) {
                    if (strcmp(argv[i], "dummy") == 0) {
                        backend = SoundIoBackendDummy;
                    } else if (strcmp(argv[i], "alsa") == 0) {
                        backend = SoundIoBackendAlsa;
                    } else if (strcmp(argv[i], "pulseaudio") == 0) {
                        backend = SoundIoBackendPulseAudio;
                    } else if (strcmp(argv[i], "jack") == 0) {
                        backend = SoundIoBackendJack;
                    } else if (strcmp(argv[i], "coreaudio") == 0) {
                        backend = SoundIoBackendCoreAudio;
                    } else if (strcmp(argv[i], "wasapi") == 0) {
                        backend = SoundIoBackendWasapi;
                    } else {
                        fprintf(stderr, "Invalid backend: %s\n", argv[i]);
                        return 1;
                    }
                } else if (strcmp(arg, "--device") == 0) {
                    device_id = argv[i];
                } else if (strcmp(arg, "--latency") == 0) {
                    latency = atof(argv[i]);
                } else {
                    return usage(exe);
                }
            }
        } else if (!input_filename) {
            input_filename = arg;
        } else {
            return usage(exe);
        }
    }

    if (!input_filename)
        return usage(exe);

    struct GenesisContext *context;
    int err = genesis_create_context(&context);
    if (err) {
        fprintf(stderr, "unable to create context: %s\n", genesis_strerror(err));
        return 1;
    }

    if (latency > 0.0)
        genesis_set_latency(context, latency);
    genesis_set_underrun_callback(context, on_underrun, NULL);

    struct GenesisAudioFile *audio_file;
    err = genesis_audio_file_load(context, input_filename, &audio_file);
    if (err) {
        fprintf(stderr, "unable to load audio file: %s\n", genesis_strerror(err));
        return 1;
    }
    const struct SoundIoChannelLayout *channel_layout = genesis_audio_file_channel_layout(audio_file);
    int sample_rate = genesis_audio_file_sample_rate(audio_file);

    // block until we have audio devices list
    genesis_flush_events(context);

    struct SoundIoDevice *out_device;
    if (backend == SoundIoBackendNone) {
        out_device = genesis_get_default_output_device(context);
    } else {
        out_device = genesis_find_output_device(context, backend, device_id, raw);
    }

    if (!out_device) {
        fprintf(stderr, "error getting playback device\n");
        return 1;
    }

    struct GenesisNodeDescriptor *playback_node_descr;
    err = genesis_audio_device_create_node_descriptor(context, out_device, &playback_node_descr);
    if (err) {
        fprintf(stderr, "unable to get node info for output device: %s\n", genesis_strerror(err));
        return 1;
    }

    struct GenesisNode *playback_node = genesis_node_descriptor_create_node(playback_node_descr);
    if (!playback_node) {
        fprintf(stderr, "unable to create out node\n");
        return 1;
    }

    // create audio file player node
    struct GenesisNodeDescriptor *audio_file_node_descr = genesis_create_node_descriptor(
            context, 1, "audio_file", "Audio file playback.");
    if (!audio_file_node_descr) {
        fprintf(stderr, "unable to create node descriptor\n");
        return 1;
    }

    struct PlayContext play_context;
    play_context.context = context;
    play_context.running = true;
    play_context.frame_count = genesis_audio_file_frame_count(audio_file);
    play_context.frame_index = 0;
    for (int ch = 0; ch < channel_layout->channel_count; ch += 1) {
        struct PlayChannelContext *channel_context = &play_context.channel_context[ch];
        channel_context->offset = 0;
        channel_context->iter = genesis_audio_file_iterator(audio_file, ch, 0);
    }

    genesis_node_descriptor_set_userdata(audio_file_node_descr, &play_context);
    genesis_node_descriptor_set_run_callback(audio_file_node_descr, audio_file_node_run);

    struct GenesisPortDescriptor *audio_out_port_descr = genesis_node_descriptor_create_port(
            audio_file_node_descr, 0, GenesisPortTypeAudioOut, "audio_out");

    if (!audio_out_port_descr) {
        fprintf(stderr, "unable to create audio out port descriptor\n");
        return 1;
    }

    genesis_audio_port_descriptor_set_channel_layout(audio_out_port_descr, channel_layout, true, -1);
    genesis_audio_port_descriptor_set_sample_rate(audio_out_port_descr, sample_rate, true, -1);

    struct GenesisNode *audio_file_node = genesis_node_descriptor_create_node(audio_file_node_descr);
    if (!audio_file_node) {
        fprintf(stderr, "unable to create audio file node\n");
        return 1;
    }

    bool try_with_resample;
    if (force_resample) {
        try_with_resample = true;
    } else {
        if ((err = genesis_connect_audio_nodes(audio_file_node, playback_node))) {
            if (err == GenesisErrorIncompatibleChannelLayouts ||
                err == GenesisErrorIncompatibleSampleRates)
            {
                try_with_resample = true;
            } else {
                fprintf(stderr, "unable to connect audio nodes: %s\n", genesis_strerror(err));
                return 1;
            }
        } else {
            try_with_resample = false;
        }
    }
    if (try_with_resample) {
        struct GenesisNodeDescriptor *resample_descr = genesis_node_descriptor_find(context, "resample");
        if (!resample_descr) {
            fprintf(stderr, "unable to find resampler\n");
            return 1;
        }
        struct GenesisNode *resample_node = genesis_node_descriptor_create_node(resample_descr);
        if (!resample_node) {
            fprintf(stderr, "error creating resample node\n");
            return 1;
        }

        if ((err = genesis_connect_audio_nodes(resample_node, playback_node))) {
            fprintf(stderr, "error connecting resampler to playback device: %s\n", genesis_strerror(err));
            return 1;
        }

        if ((err = genesis_connect_audio_nodes(audio_file_node, resample_node))) {
            fprintf(stderr, "error connecting audio file node to resampler: %s\n", genesis_strerror(err));
            return 1;
        }
    }

    struct GenesisPort *audio_in_port = genesis_node_port(playback_node, 0);
    const struct SoundIoChannelLayout *device_layout = genesis_audio_port_channel_layout(audio_in_port);
    int device_rate = genesis_audio_port_sample_rate(audio_in_port);

    fprintf(stderr, "%s ", input_filename);
    print_channel_and_rate(channel_layout, sample_rate);
    fprintf(stderr, " -> ");

    if (try_with_resample) {
        fprintf(stderr, "resample -> ");
    }

    fprintf(stderr, "%s %s ", soundio_backend_name(out_device->soundio->current_backend), out_device->name);
    print_channel_and_rate(device_layout, device_rate);
    fprintf(stderr, "\n");

    soundio_device_unref(out_device);

    err = genesis_start_pipeline(context, 0.0);
    if (err) {
        fprintf(stderr, "unable to start pipeline: %s\n", genesis_strerror(err));
        return 1;
    }

    while (play_context.running)
        genesis_wait_events(context);

    genesis_stop_pipeline(context);

    genesis_audio_file_destroy(audio_file);
    genesis_destroy_context(context);
}
