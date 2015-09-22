#include "synth.hpp"

static const float PI = 3.14159265358979323846;

struct SynthNoteState {
    float velocity;
    float seconds_offset;
};

struct SynthContext {
    float seconds_offset;
    SynthNoteState notes_on[GENESIS_NOTES_COUNT];
    float pitch;
};

static void synth_destroy(struct GenesisNode *node) {
    SynthContext *synth_context = (SynthContext*)node->userdata;
    destroy(synth_context, 1);
}

static int synth_create(struct GenesisNode *node) {
    SynthContext *synth_context = create_zero<SynthContext>();
    node->userdata = synth_context;
    if (!node->userdata) {
        synth_destroy(node);
        return GenesisErrorNoMem;
    }
    return 0;
}

static void synth_seek(struct GenesisNode *node) {
    // do nothing
}

static void synth_run(struct GenesisNode *node) {
    struct SynthContext *synth_context = (struct SynthContext*)node->userdata;
    struct GenesisPort *events_in_port = genesis_node_port(node, 0);
    struct GenesisPort *audio_out_port = genesis_node_port(node, 1);

    int event_count;
    double time_available;
    // TODO compute time_requested instead of hardcoding 999.0
    genesis_events_in_port_fill_count(events_in_port, 999.0, &event_count, &time_available);
    GenesisMidiEvent *event = genesis_events_in_port_read_ptr(events_in_port);
    for (int i = 0; i < event_count; i += 1) {
        switch (event->event_type) {
            case GenesisMidiEventTypeNoteOn:
                {
                    SynthNoteState *note_state = &synth_context->notes_on[event->data.note_data.note];
                    note_state->velocity = event->data.note_data.velocity;
                    note_state->seconds_offset = 0.0f;
                    break;
                }
            case GenesisMidiEventTypeNoteOff:
                synth_context->notes_on[event->data.note_data.note].velocity = 0.0f;
                break;
            case GenesisMidiEventTypePitch:
                synth_context->pitch = event->data.pitch_data.pitch;
                break;
        }
        event += 1;
    }
    genesis_events_in_port_advance_read_ptr(events_in_port, event_count, time_available);

    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);
    int bytes_per_frame = genesis_audio_port_bytes_per_frame(audio_out_port);

    float float_sample_rate = genesis_audio_port_sample_rate(audio_out_port);
    float seconds_per_frame = 1.0f / float_sample_rate;

    float *write_ptr_start = genesis_audio_out_port_write_ptr(audio_out_port);
    // clear everything to 0
    memset(write_ptr_start, 0, output_frame_count * bytes_per_frame);

    float divisor = 0.0f;
    for (int note = 0; note < GENESIS_NOTES_COUNT; note += 1) {
        if (synth_context->notes_on[note].velocity > 0.0f)
            divisor += 1.0f;
    }
    float one_over_notes_count = 1.0f / divisor;
    const SoundIoChannelLayout *channel_layout = genesis_audio_port_channel_layout(audio_out_port);
    for (int note = 0; note < GENESIS_NOTES_COUNT; note += 1) {
        SynthNoteState *note_state = &synth_context->notes_on[note];
        float note_value = note_state->velocity;
        if (note_value == 0.0f)
            continue;

        float *ptr = write_ptr_start;

        // 69 is A 440
        float pitch = (synth_context->pitch != 0.0f) ?
            (440.0f * powf(2.0f, (note - 69.0f) / 12.0f + synth_context->pitch)) :
            genesis_midi_note_to_pitch(note);
        float radians_per_second = pitch * 2.0f * PI;
        for (int frame = 0; frame < output_frame_count; frame += 1) {
            float sample = sinf((note_state->seconds_offset + frame * seconds_per_frame) * radians_per_second);
            for (int channel = 0; channel < channel_layout->channel_count; channel += 1) {
                *ptr += sample * note_value * one_over_notes_count;
                ptr += 1;
            }
        }
        note_state->seconds_offset += seconds_per_frame * output_frame_count;
    }

    genesis_audio_out_port_advance_write_ptr(audio_out_port, output_frame_count);
}

int create_synth_descriptor(GenesisContext *context) {
    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 2, "synth", "A single oscillator");
    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    genesis_node_descriptor_set_run_callback(node_descr, synth_run);
    genesis_node_descriptor_set_create_callback(node_descr, synth_create);
    genesis_node_descriptor_set_destroy_callback(node_descr, synth_destroy);
    genesis_node_descriptor_set_seek_callback(node_descr, synth_seek);

    struct GenesisPortDescriptor *events_port = genesis_node_descriptor_create_port(
            node_descr, 0, GenesisPortTypeEventsIn, "events_in");
    struct GenesisPortDescriptor *audio_port = genesis_node_descriptor_create_port(
            node_descr, 1, GenesisPortTypeAudioOut, "audio_out");

    if (!events_port || !audio_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    genesis_audio_port_descriptor_set_channel_layout(audio_port,
        soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono), false, -1);

    genesis_audio_port_descriptor_set_sample_rate(audio_port, 48000, false, -1);

    return 0;
}
