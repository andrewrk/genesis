#include "synth.hpp"

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
    static const float PI = 3.14159265358979323846;
    struct GenesisEventsPort *events_in_port = (struct GenesisEventsPort *) node->ports[0];
    struct GenesisEventsPort *events_out_port = (struct GenesisEventsPort *) events_in_port->port.input_from;
    struct GenesisPort *audio_out_port = node->ports[1];

    int event_byte_count = events_out_port->event_buffer->fill_count();
    int event_count = event_byte_count / sizeof(GenesisMidiEvent);
    GenesisMidiEvent *event = reinterpret_cast<GenesisMidiEvent *>(events_out_port->event_buffer->read_ptr());
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
    events_out_port->event_buffer->advance_read_ptr(event_byte_count);

    int free_byte_count = genesis_audio_out_port_free_count(audio_out_port);
    int bytes_per_frame = genesis_audio_port_bytes_per_frame(audio_out_port);
    int free_frame_count = free_byte_count / bytes_per_frame;
    int out_buf_size = free_frame_count * bytes_per_frame;

    float float_sample_rate = genesis_audio_port_sample_rate(audio_out_port);
    float seconds_per_frame = 1.0f / float_sample_rate;

    float *write_ptr_start = reinterpret_cast<float*>(genesis_audio_out_port_write_ptr(audio_out_port));
    // clear everything to 0
    memset(write_ptr_start, 0, out_buf_size);

    float divisor = 0.0f;
    for (int note = 0; note < GENESIS_NOTES_COUNT; note += 1) {
        if (synth_context->notes_on[note].velocity > 0.0f)
            divisor += 1.0f;
    }
    float one_over_notes_count = 1.0f / divisor;
    const GenesisChannelLayout *channel_layout = genesis_audio_port_channel_layout(audio_out_port);
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
        for (int frame = 0; frame < free_frame_count; frame += 1) {
            float sample = sinf((note_state->seconds_offset + frame * seconds_per_frame) * radians_per_second);
            for (int channel = 0; channel < channel_layout->channel_count; channel += 1) {
                *ptr += sample * note_value * one_over_notes_count;
                ptr += 1;
            }
        }
        note_state->seconds_offset += seconds_per_frame * free_frame_count;
    }

    genesis_audio_out_port_advance_write_ptr(audio_out_port, out_buf_size);
}

int create_synth_descriptor(GenesisContext *context) {
    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 2);
    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->run = synth_run;
    node_descr->create = synth_create;
    node_descr->destroy = synth_destroy;
    node_descr->seek = synth_seek;

    node_descr->name = strdup("synth");
    node_descr->description = strdup("A single oscillator.");
    if (!node_descr->name || !node_descr->description) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    GenesisEventsPortDescriptor *events_port = create_zero<GenesisEventsPortDescriptor>();
    GenesisAudioPortDescriptor *audio_port = create_zero<GenesisAudioPortDescriptor>();

    node_descr->port_descriptors.at(0) = &events_port->port_descriptor;
    node_descr->port_descriptors.at(1) = &audio_port->port_descriptor;

    if (!events_port || !audio_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    events_port->port_descriptor.port_type = GenesisPortTypeEventsIn;
    events_port->port_descriptor.name = strdup("events_in");
    if (!events_port->port_descriptor.name) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    audio_port->port_descriptor.port_type = GenesisPortTypeAudioOut;
    audio_port->port_descriptor.name = strdup("audio_out");
    if (!events_port->port_descriptor.name) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }
    audio_port->channel_layout =
        *genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
    audio_port->channel_layout_fixed = false;
    audio_port->same_channel_layout_index = -1;
    audio_port->sample_rate = 48000;
    audio_port->sample_rate_fixed = false;
    audio_port->same_sample_rate_index = -1;

    return 0;
}
