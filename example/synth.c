#include "genesis.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// hooks default MIDI keyboard up to a simple synth

static int usage(char *exe) {
    fprintf(stderr, "Usage: %s [options]\n"
            "Options:\n"
            "  [--backend dummy|alsa|pulseaudio|jack|coreaudio|wasapi]\n"
            "  [--device id]\n"
            "  [--raw]\n"
            "  [--latency seconds]\n"
            , exe);
    return 1;
}

static void on_underrun(void *userdata) {
    static int underrun_count = 0;
    fprintf(stderr, "buffer underrun %d\n", ++underrun_count);
}

int main(int argc, char **argv) {
    char *exe = argv[0];
    bool raw = false;
    enum SoundIoBackend backend = SoundIoBackendNone;
    char *device_id = NULL;
    double latency = 0.0;

    for (int i = 1; i < argc; i += 1) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            if (strcmp(arg, "--raw") == 0) {
                raw = true;
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
        } else {
            return usage(exe);
        }
    }

    struct GenesisContext *context;
    int err = genesis_create_context(&context);
    if (err) {
        fprintf(stderr, "unable to create context: %s\n", genesis_strerror(err));
        return 1;
    }

    if (latency > 0.0)
        genesis_set_latency(context, latency);
    genesis_set_underrun_callback(context, on_underrun, NULL);

    struct GenesisNodeDescriptor *synth_descr = genesis_node_descriptor_find(context, "synth");
    if (!synth_descr) {
        fprintf(stderr, "unable to find synth\n");
        return 1;
    }
    fprintf(stderr, "synth: %s\n", genesis_node_descriptor_description(synth_descr));

    struct GenesisNode *synth_node = genesis_node_descriptor_create_node(synth_descr);
    if (!synth_node) {
        fprintf(stderr, "unable to create synth node\n");
        return 1;
    }

    // block until we have devices list
    genesis_flush_events(context);
    genesis_refresh_midi_devices(context);

    struct SoundIoDevice *out_device;

    if (backend != SoundIoBackendNone) {
        out_device = genesis_find_output_device(context, backend, device_id, raw);
    } else {
        out_device = genesis_get_default_output_device(context);
    }

    if (!out_device) {
        fprintf(stderr, "error getting playback device\n");
        return 1;
    }

    fprintf(stderr, "Audio Output Device: %s (%s)\n", out_device->name,
            soundio_backend_name(out_device->soundio->current_backend));

    struct GenesisNodeDescriptor *playback_node_descr;
    err = genesis_audio_device_create_node_descriptor(context, out_device, &playback_node_descr);
    if (err) {
        fprintf(stderr, "unable to get node info for output device: %s\n", genesis_strerror(err));
        return 1;
    }
    soundio_device_unref(out_device);

    struct GenesisNode *playback_node = genesis_node_descriptor_create_node(playback_node_descr);
    if (!playback_node) {
        fprintf(stderr, "unable to create out node\n");
        return 1;
    }

    int audio_out_port_index = genesis_node_descriptor_find_port_index(synth_descr, "audio_out");
    if (audio_out_port_index < 0) {
        fprintf(stderr, "unable to find audio_out port\n");
        return 1;
    }

    struct GenesisPort *audio_out_port = genesis_node_port(synth_node, audio_out_port_index);
    if (!audio_out_port) {
        fprintf(stderr, "expected to find audio_out port\n");
        return 1;
    }

    int audio_in_port_index = genesis_node_descriptor_find_port_index(playback_node_descr, "audio_in");
    if (audio_in_port_index < 0) {
        fprintf(stderr, "unable to find audio_in port\n");
        return 1;
    }

    struct GenesisPort *audio_in_port = genesis_node_port(playback_node, audio_in_port_index);
    if (!audio_in_port) {
        fprintf(stderr, "expected to find audio_in port\n");
        return 1;
    }

    err = genesis_connect_ports(audio_out_port, audio_in_port);
    if (err) {
        fprintf(stderr, "unable to connect audio ports: %s\n", genesis_strerror(err));
        return 1;
    }

    int midi_device_index = genesis_get_default_midi_device_index(context);
    if (midi_device_index < 0) {
        fprintf(stderr, "error getting midi device list\n");
        return 1;
    }

    struct GenesisMidiDevice *midi_device = genesis_get_midi_device(context, midi_device_index);
    if (!midi_device) {
        fprintf(stderr, "error getting midi device\n");
        return 1;
    }

    fprintf(stderr, "MIDI Device: %s\n", genesis_midi_device_description(midi_device));

    struct GenesisNodeDescriptor *midi_node_descr;
    err = genesis_midi_device_create_node_descriptor(midi_device, &midi_node_descr);
    if (err) {
        fprintf(stderr, "unable to create input node descriptor: %s\n", genesis_strerror(err));
        return 1;
    }
    genesis_midi_device_unref(midi_device);

    struct GenesisNode *midi_node = genesis_node_descriptor_create_node(midi_node_descr);
    if (!playback_node) {
        fprintf(stderr, "unable to create midi node\n");
        return 1;
    }

    int events_out_port_index = genesis_node_descriptor_find_port_index(midi_node_descr, "events_out");
    if (events_out_port_index < 0) {
        fprintf(stderr, "unable to find events_out port\n");
        return 1;
    }

    struct GenesisPort *events_out_port = genesis_node_port(midi_node, events_out_port_index);
    if (!events_out_port) {
        fprintf(stderr, "expected to find events_out port\n");
        return 1;
    }

    int events_in_port_index = genesis_node_descriptor_find_port_index(synth_descr, "events_in");
    if (events_in_port_index < 0) {
        fprintf(stderr, "unable to find events_in port\n");
        return 1;
    }

    struct GenesisPort *events_in_port = genesis_node_port(synth_node, events_in_port_index);
    if (!events_in_port) {
        fprintf(stderr, "expected to find events_in port\n");
        return 1;
    }

    err = genesis_connect_ports(events_out_port, events_in_port);
    if (err) {
        fprintf(stderr, "unable to connect audio ports: %s\n", genesis_strerror(err));
        return 1;
    }

    if ((err = genesis_start_pipeline(context, 0.0))) {
        fprintf(stderr, "unable to start audio pipeline: %s\n", genesis_strerror(err));
        return 1;
    }

    for (;;)
        genesis_wait_events(context);
}
