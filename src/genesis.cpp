#include "genesis.hpp"
#include "audio_file.hpp"
#include "midi_note_pitch.hpp"

static atomic_flag lib_init_flag = ATOMIC_FLAG_INIT;

static const int BYTES_PER_SAMPLE = 4; // assuming float samples
static const int EVENTS_PER_SECOND_CAPACITY = 16000;
static const double whole_notes_per_second = 140.0 / 60.0;
static const int NOTES_COUNT = 128;

static_assert(NOTES_COUNT == array_length(midi_note_to_pitch), "");

struct SynthNoteState {
    float velocity;
    float seconds_offset;
};

struct SynthContext {
    float seconds_offset;
    SynthNoteState notes_on[NOTES_COUNT];
};

struct PlaybackNodeContext {
    OpenPlaybackDevice *playback_device;
};

struct ControllerNodeContext {
    GenesisMidiDevice *midi_device;
};

static void on_midi_devices_change(MidiHardware *midi_hardware) {
    GenesisContext *context = reinterpret_cast<GenesisContext *>(midi_hardware->userdata);
    if (context->midi_change_callback)
        context->midi_change_callback(context->midi_change_callback_userdata);
}

static void midi_events_signal(MidiHardware *midi_hardware) {
    GenesisContext *context = reinterpret_cast<GenesisContext *>(midi_hardware->userdata);
    genesis_wakeup(context);
}

static void on_devices_change(AudioHardware *audio_hardware) {
    GenesisContext *context = reinterpret_cast<GenesisContext *>(audio_hardware->_userdata);
    if (context->devices_change_callback)
        context->devices_change_callback(context->devices_change_callback_userdata);
}

static void on_audio_hardware_events_signal(AudioHardware *audio_hardware) {
    GenesisContext *context = reinterpret_cast<GenesisContext *>(audio_hardware->_userdata);
    genesis_wakeup(context);
}

double genesis_frames_to_whole_notes(GenesisContext *context, int frames, int frame_rate) {
    double seconds = frames / (double)frame_rate;
    return whole_notes_per_second * seconds;
}

int genesis_whole_notes_to_frames(GenesisContext *context, double whole_notes, int frame_rate) {
    return frame_rate * genesis_whole_notes_to_seconds(context, whole_notes, frame_rate);
}

double genesis_whole_notes_to_seconds(GenesisContext *context, double whole_notes, int frame_rate) {
    return whole_notes / whole_notes_per_second;
}

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
    struct GenesisNotesPort *notes_in_port = (struct GenesisNotesPort *) node->ports[0];
    struct GenesisNotesPort *notes_out_port = (struct GenesisNotesPort *)notes_in_port->port.input_from;
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) node->ports[1];

    int event_byte_count = notes_out_port->event_buffer->fill_count();
    int event_count = event_byte_count / sizeof(GenesisMidiEvent);
    GenesisMidiEvent *event = reinterpret_cast<GenesisMidiEvent *>(notes_out_port->event_buffer->read_ptr());
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
        }
        event += 1;
    }
    notes_out_port->event_buffer->advance_read_ptr(event_byte_count);

    int free_byte_count = audio_out_port->sample_buffer_size - audio_out_port->sample_buffer->fill_count();
    int free_frame_count = free_byte_count / audio_out_port->bytes_per_frame;
    int out_buf_size = free_frame_count * audio_out_port->bytes_per_frame;

    float seconds_per_frame = 1.0f / (float)audio_out_port->sample_rate;

    float *write_ptr_start = reinterpret_cast<float*>(audio_out_port->sample_buffer->write_ptr());
    // clear everything to 0
    memset(write_ptr_start, 0, out_buf_size);

    float divisor = 0.0f;
    for (int note = 0; note < NOTES_COUNT; note += 1) {
        if (synth_context->notes_on[note].velocity > 0.0f)
            divisor += 1.0f;
    }
    float one_over_notes_count = 1.0f / divisor;
    for (int note = 0; note < NOTES_COUNT; note += 1) {
        SynthNoteState *note_state = &synth_context->notes_on[note];
        float note_value = note_state->velocity;
        if (note_value == 0.0f)
            continue;

        float *ptr = write_ptr_start;

        // 69 is A 440
        float pitch = midi_note_to_pitch[note];
        float radians_per_second = pitch * 2.0f * PI;
        for (int frame = 0; frame < free_frame_count; frame += 1) {
            float sample = sinf((note_state->seconds_offset + frame * seconds_per_frame) * radians_per_second);
            for (int channel = 0; channel < audio_out_port->channel_layout.channel_count; channel += 1) {
                *ptr += sample * note_value * one_over_notes_count;
                ptr += 1;
            }
        }
        note_state->seconds_offset += seconds_per_frame * free_frame_count;
    }

    audio_out_port->sample_buffer->advance_write_ptr(out_buf_size);
}

int genesis_create_context(struct GenesisContext **out_context) {
    *out_context = nullptr;

    // only call global initialization once
    if (!lib_init_flag.test_and_set()) {
        audio_file_init();
    }

    GenesisContext *context = create_zero<GenesisContext>();
    if (!context) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }
    context->latency = 0.010; // 10ms

    if (context->events_cond.error() || context->events_mutex.error()) {
        genesis_destroy_context(context);
        return context->events_cond.error() || context->events_mutex.error();
    }

    if (context->thread_pool.resize(Thread::concurrency())) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }

    int err = create_midi_hardware(context, "genesis", midi_events_signal, on_midi_devices_change,
            context, &context->midi_hardware);
    if (err) {
        genesis_destroy_context(context);
        return err;
    }

    err = audio_file_get_out_formats(context->out_formats);
    if (err) {
        genesis_destroy_context(context);
        return err;
    }

    err = audio_file_get_in_formats(context->in_formats);
    if (err) {
        genesis_destroy_context(context);
        return err;
    }

    context->audio_hardware.set_on_devices_change(on_devices_change);
    context->audio_hardware.set_on_events_signal(on_audio_hardware_events_signal);
    context->audio_hardware._userdata = context;

    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 2);
    if (!node_descr) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }

    node_descr->run = synth_run;
    node_descr->create = synth_create;
    node_descr->destroy = synth_destroy;
    node_descr->seek = synth_seek;

    node_descr->name = strdup("synth");
    node_descr->description = strdup("A single oscillator.");
    if (!node_descr->name || !node_descr->description) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }

    GenesisNotesPortDescriptor *notes_port = create_zero<GenesisNotesPortDescriptor>();
    GenesisAudioPortDescriptor *audio_port = create_zero<GenesisAudioPortDescriptor>();

    if (!notes_port || !audio_port) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }

    node_descr->port_descriptors.at(0) = &notes_port->port_descriptor;
    node_descr->port_descriptors.at(1) = &audio_port->port_descriptor;

    notes_port->port_descriptor.port_type = GenesisPortTypeNotesIn;
    notes_port->port_descriptor.name = strdup("notes_in");
    if (!notes_port->port_descriptor.name) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }

    audio_port->port_descriptor.port_type = GenesisPortTypeAudioOut;
    audio_port->port_descriptor.name = strdup("audio_out");
    if (!notes_port->port_descriptor.name) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }
    audio_port->channel_layout =
        *genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
    audio_port->channel_layout_fixed = false;
    audio_port->same_channel_layout_index = -1;
    audio_port->sample_rate = 48000;
    audio_port->sample_rate_fixed = false;
    audio_port->same_sample_rate_index = -1;

    *out_context = context;
    return 0;
}

void genesis_destroy_context(struct GenesisContext *context) {
    if (context) {
        if (context->midi_hardware) {
            destroy_midi_hardware(context->midi_hardware);
        }
        for (int i = 0; i < context->node_descriptors.length(); i += 1) {
            destroy(context->node_descriptors.at(i), 1);
        }
        for (int i = 0; i < context->out_formats.length(); i += 1) {
            destroy(context->out_formats.at(i), 1);
        }
        for (int i = 0; i < context->in_formats.length(); i += 1) {
            destroy(context->in_formats.at(i), 1);
        }
        destroy(context, 1);
    }
}

void genesis_flush_events(struct GenesisContext *context) {
    context->audio_hardware.flush_events();
    midi_hardware_flush_events(context->midi_hardware);
}

void genesis_wait_events(struct GenesisContext *context) {
    genesis_flush_events(context);
    context->events_mutex.lock();
    context->events_cond.wait(&context->events_mutex);
    context->events_mutex.unlock();
}

void genesis_wakeup(struct GenesisContext *context) {
    context->events_mutex.lock();
    context->events_cond.signal();
    context->events_mutex.unlock();
}

struct GenesisNodeDescriptor *genesis_node_descriptor_find(
        GenesisContext *context, const char *name)
{
    for (int i = 0; i < context->node_descriptors.length(); i += 1) {
        GenesisNodeDescriptor *descr = context->node_descriptors.at(i);
        if (strcmp(descr->name, name) == 0)
            return descr;
    }
    return nullptr;
}

const char *genesis_node_descriptor_name(const struct GenesisNodeDescriptor *node_descriptor) {
    return node_descriptor->name;
}

const char *genesis_node_descriptor_description(const struct GenesisNodeDescriptor *node_descriptor) {
    return node_descriptor->description;
}

static GenesisAudioPort *create_audio_port_from_descriptor(GenesisPortDescriptor *port_descriptor) {
    GenesisAudioPort *audio_port = create_zero<GenesisAudioPort>();
    if (!audio_port)
        return nullptr;
    return audio_port;
}

static GenesisNotesPort *create_notes_port_from_descriptor(GenesisPortDescriptor *port_descriptor) {
    GenesisNotesPort *notes_port = create_zero<GenesisNotesPort>();
    if (!notes_port)
        return nullptr;
    return notes_port;
}

static GenesisParamPort *create_param_port_from_descriptor(GenesisPortDescriptor *port_descriptor) {
    GenesisParamPort *param_port = create_zero<GenesisParamPort>();
    if (!param_port)
        return nullptr;
    return param_port;
}

static GenesisPort *create_port_from_descriptor(GenesisPortDescriptor *port_descriptor) {
    GenesisPort *port;
    switch (port_descriptor->port_type) {
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeAudioOut:
            port = &create_audio_port_from_descriptor(port_descriptor)->port;
            break;

        case GenesisPortTypeNotesIn:
        case GenesisPortTypeNotesOut:
            port = &create_notes_port_from_descriptor(port_descriptor)->port;
            break;

        case GenesisPortTypeParamIn:
        case GenesisPortTypeParamOut:
            port = &create_param_port_from_descriptor(port_descriptor)->port;
            break;
    }
    port->descriptor = port_descriptor;
    return port;
}

struct GenesisNode *genesis_node_descriptor_create_node(struct GenesisNodeDescriptor *node_descriptor) {
    GenesisNode *node = create_zero<GenesisNode>();
    if (!node) {
        genesis_node_destroy(node);
        return nullptr;
    }
    node->set_index = -1;
    node->descriptor = node_descriptor;
    node->port_count = node_descriptor->port_descriptors.length();
    node->ports = allocate_zero<GenesisPort*>(node->port_count);
    if (!node->ports) {
        genesis_node_destroy(node);
        return nullptr;
    }
    for (int i = 0; i < node->port_count; i += 1) {
        GenesisPort *port = create_port_from_descriptor(node_descriptor->port_descriptors.at(i));
        if (!port) {
            genesis_node_destroy(node);
            return nullptr;
        }
        port->node = node;
        node->ports[i] = port;
    }
    if (node_descriptor->create(node)) {
        genesis_node_destroy(node);
        return nullptr;
    }
    node->constructed = true;

    int set_index = node_descriptor->context->nodes.length();
    if (node_descriptor->context->nodes.append(node)) {
        genesis_node_destroy(node);
        return nullptr;
    }
    node->set_index = set_index;

    return node;
}

void genesis_node_destroy(struct GenesisNode *node) {
    if (node) {
        if (node->constructed)
            node->descriptor->destroy(node);

        GenesisContext *context = node->descriptor->context;
        if (node->set_index >= 0) {
            context->nodes.swap_remove(node->set_index);
            if (node->set_index < context->nodes.length())
                context->nodes.at(node->set_index)->set_index = node->set_index;
            node->set_index = -1;
        }

        if (node->ports) {
            for (int i = 0; i < node->port_count; i += 1) {
                if (node->ports[i]) {
                    destroy(node->ports[i], 1);
                }
            }
            destroy(node->ports, 1);
        }

        destroy(node, 1);
    }
}

void genesis_refresh_audio_devices(struct GenesisContext *context) {
    context->audio_hardware.block_until_ready();
    context->audio_hardware.flush_events();
    context->audio_hardware.block_until_have_devices();
}

int genesis_get_audio_device_count(struct GenesisContext *context) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware.devices_info();
    if (!audio_device_info)
        return -1;
    return audio_device_info->devices.length();
}

struct GenesisAudioDevice *genesis_get_audio_device(struct GenesisContext *context, int index) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware.devices_info();
    if (!audio_device_info)
        return nullptr;
    if (index < 0 || index >= audio_device_info->devices.length())
        return nullptr;
    return (GenesisAudioDevice *) &audio_device_info->devices.at(index);
}

int genesis_get_default_playback_device_index(struct GenesisContext *context) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware.devices_info();
    if (!audio_device_info)
        return -1;
    return audio_device_info->default_output_index;
}

int genesis_get_default_recording_device_index(struct GenesisContext *context) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware.devices_info();
    if (!audio_device_info)
        return -1;
    return audio_device_info->default_input_index;
}

const char *genesis_audio_device_name(const struct GenesisAudioDevice *audio_device) {
    return audio_device->name.raw();
}

const char *genesis_audio_device_description(const struct GenesisAudioDevice *audio_device) {
    return audio_device->description.raw();
}

enum GenesisAudioDevicePurpose genesis_audio_device_purpose(const struct GenesisAudioDevice *audio_device) {
    return audio_device->purpose;
}

void genesis_set_audio_device_callback(struct GenesisContext *context,
        void (*callback)(void *userdata),
        void *userdata)
{
    context->devices_change_callback_userdata = userdata;
    context->devices_change_callback = callback;
}

int genesis_in_format_count(struct GenesisContext *context) {
    return context->in_formats.length();
}

int genesis_out_format_count(struct GenesisContext *context) {
    return context->out_formats.length();
}

struct GenesisAudioFileFormat *genesis_in_format_index(
        struct GenesisContext *context, int format_index)
{
    return context->in_formats.at(format_index);
}

struct GenesisAudioFileFormat *genesis_out_format_index(
        struct GenesisContext *context, int format_index)
{
    return context->out_formats.at(format_index);
}

struct GenesisAudioFileCodec *genesis_guess_audio_file_codec(
        struct GenesisContext *context, const char *filename_hint,
        const char *format_name, const char *codec_name)
{
    return audio_file_guess_audio_file_codec(context->out_formats, filename_hint, format_name, codec_name);
}

int genesis_audio_file_codec_sample_format_count(const struct GenesisAudioFileCodec *codec) {
    return codec ? codec->prioritized_sample_formats.length() : 0;
}

enum GenesisSampleFormat genesis_audio_file_codec_sample_format_index(
        const struct GenesisAudioFileCodec *codec, int index)
{
    return codec->prioritized_sample_formats.at(index);
}

int genesis_audio_file_codec_sample_rate_count(const struct GenesisAudioFileCodec *codec) {
    return codec ? codec->prioritized_sample_rates.length() : 0;
}

int genesis_audio_file_codec_sample_rate_index(
        const struct GenesisAudioFileCodec *codec, int index)
{
    return codec->prioritized_sample_rates.at(index);
}

static void queue_node_if_ready(GenesisContext *context, GenesisNode *node, bool recursive) {
    if (node->being_processed) {
        // this node is already being processed; no point in
        // queueing it again
        return;
    }
    // make sure all the children of this node are ready
    for (int i = 0; i < node->port_count; i += 1) {
        GenesisPort *port = node->ports[i];
        GenesisPort *child_port = port->input_from;
        if (!child_port || child_port == port)
            continue;
        GenesisNode *child_node = child_port->node;
        if (child_node->being_processed)
            return;

        if (!child_node->data_ready) {
            if (recursive)
                queue_node_if_ready(context, child_node, true);
            return;
        }
    }

    // we know that we want it enqueued. now make sure it only happens once.
    if (!node->being_processed.exchange(true))
        context->task_queue.enqueue(node);
}

static bool fill_playback_buffer(GenesisNode *node, PlaybackNodeContext *playback_node_context,
        OpenPlaybackDevice *playback_device, int requested_byte_count)
{
    GenesisAudioPort *audio_in_port = (GenesisAudioPort *)node->ports[0];
    GenesisAudioPort *audio_out_port = (GenesisAudioPort *)audio_in_port->port.input_from;

    int fill_count = audio_out_port->sample_buffer->fill_count();
    bool need_more_data = false;

    char *buffer;
    for (;;) {
        int byte_count = requested_byte_count;

        if (byte_count > fill_count) {
            byte_count = fill_count;
            requested_byte_count = byte_count;
            need_more_data = true;
            if (byte_count <= 0)
                break;
        }

        playback_device->begin_write(&buffer, &byte_count);
        memcpy(buffer, audio_out_port->sample_buffer->read_ptr(), byte_count);
        playback_device->write(buffer, byte_count);
        audio_out_port->sample_buffer->advance_read_ptr(byte_count);

        requested_byte_count -= byte_count;
        fill_count -= byte_count;
        if (requested_byte_count <= 0)
            break;
    }
    return need_more_data;
}

static void playback_device_run(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    OpenPlaybackDevice *playback_device = playback_node_context->playback_device;
    int writable_size = playback_device->writable_size();
    if (writable_size <= 0)
        return;
    playback_device->lock();
    fill_playback_buffer(node, playback_node_context, playback_device, writable_size);
    playback_device->unlock();
}

static void playback_device_destroy(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    destroy(playback_node_context, 1);
}

static void playback_device_callback(int requested_byte_count, void *userdata) {
    GenesisNode *node = (GenesisNode *)userdata;
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    OpenPlaybackDevice *playback_device = playback_node_context->playback_device;
    GenesisContext *context = node->descriptor->context;

    if (!context->pipeline_running) {
        char *buffer;
        for (;;) {
            int byte_count = requested_byte_count;

            playback_device->begin_write(&buffer, &byte_count);
            memset(buffer, 0, byte_count);
            playback_device->write(buffer, byte_count);

            requested_byte_count -= byte_count;
            if (requested_byte_count <= 0)
                break;
        }
        return;
    }

    bool need_more_data = fill_playback_buffer(node, playback_node_context, playback_device, requested_byte_count);

    if (need_more_data) {
        GenesisAudioPort *audio_in_port = (GenesisAudioPort *)node->ports[0];
        GenesisAudioPort *audio_out_port = (GenesisAudioPort *)audio_in_port->port.input_from;
        GenesisNode *child_node = audio_out_port->port.node;
        child_node->data_ready = false;
        queue_node_if_ready(context, child_node, true);
    }
}

static int playback_device_create(struct GenesisNode *node) {
    GenesisContext *context = node->descriptor->context;

    PlaybackNodeContext *playback_node_context = create_zero<PlaybackNodeContext>();
    if (!playback_node_context) {
        playback_device_destroy(node);
        return GenesisErrorNoMem;
    }
    node->userdata = playback_node_context;

    GenesisAudioDevice *device = (GenesisAudioDevice*)node->descriptor->userdata;

    playback_node_context->playback_device = create_zero<OpenPlaybackDevice>(
            &context->audio_hardware, &device->channel_layout,
            GenesisSampleFormatFloat, context->latency, device->default_sample_rate,
            playback_device_callback, node);
    if (!playback_node_context->playback_device) {
        playback_device_destroy(node);
        return GenesisErrorNoMem;
    }

    int err = playback_node_context->playback_device->start(device->name.raw());
    if (err) {
        playback_device_destroy(node);
        return err;
    }

    return 0;
}

static void playback_device_seek(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    playback_node_context->playback_device->clear_buffer();
}

static void destroy_audio_device_node_descriptor(struct GenesisNodeDescriptor *node_descriptor) {
    GenesisAudioDevice *audio_device = (GenesisAudioDevice*)node_descriptor->userdata;
    destroy_audio_device(audio_device);
}

int genesis_audio_device_create_node_descriptor(struct GenesisAudioDevice *audio_device,
        struct GenesisNodeDescriptor **out)
{
    GenesisContext *context = audio_device->context;

    *out = nullptr;

    if (audio_device->purpose != GenesisAudioDevicePurposePlayback) {
        return GenesisErrorInvalidState;
    }

    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 1);

    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->name = create_formatted_str("playback-device-%s", audio_device->name.raw());
    node_descr->description = create_formatted_str("Playback Device: %s", audio_device->description.raw());

    if (!node_descr->name || !node_descr->description) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->run = playback_device_run;
    node_descr->create = playback_device_create;
    node_descr->destroy = playback_device_destroy;
    node_descr->seek = playback_device_seek;

    node_descr->userdata = duplicate_audio_device(audio_device);
    if (!node_descr->userdata) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }
    node_descr->destroy_descriptor = destroy_audio_device_node_descriptor;

    GenesisAudioPortDescriptor *audio_in_port = create_zero<GenesisAudioPortDescriptor>();
    if (!audio_in_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->port_descriptors.at(0) = &audio_in_port->port_descriptor;

    audio_in_port->port_descriptor.name = strdup("audio_in");
    if (!audio_in_port->port_descriptor.name) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    audio_in_port->port_descriptor.port_type = GenesisPortTypeAudioIn;
    audio_in_port->channel_layout = audio_device->channel_layout;
    audio_in_port->channel_layout_fixed = true;
    audio_in_port->same_channel_layout_index = -1;
    audio_in_port->sample_rate = audio_device->default_sample_rate;
    audio_in_port->sample_rate_fixed = true;
    audio_in_port->same_sample_rate_index = -1;

    *out = node_descr;
    return 0;
}

static void midi_node_on_event(struct GenesisMidiDevice *device, const struct GenesisMidiEvent *event) {
    GenesisNode *node = (GenesisNode *)device->userdata;
    GenesisNotesPort *notes_out_port = (GenesisNotesPort *)node->ports[0];
    int free_count = notes_out_port->event_buffer->free_count();
    int midi_event_size = sizeof(GenesisMidiEvent);
    if (free_count < midi_event_size) {
        fprintf(stderr, "event buffer overflow\n");
        return;
    }
    char *ptr = notes_out_port->event_buffer->write_ptr();
    memcpy(ptr, event, midi_event_size);
    notes_out_port->event_buffer->advance_write_ptr(midi_event_size);
}

static void midi_node_run(struct GenesisNode *node) {
    // do nothing
}

static int midi_node_create(struct GenesisNode *node) {
    GenesisMidiDevice *device = (GenesisMidiDevice*)node->descriptor->userdata;
    device->userdata = node;
    device->on_event = midi_node_on_event;
    return open_midi_device(device);
}

static void midi_node_destroy(struct GenesisNode *node) {
    GenesisMidiDevice *device = (GenesisMidiDevice*)node->descriptor->userdata;
    close_midi_device(device);
    device->userdata = nullptr;
    device->on_event = nullptr;
}

static void midi_node_seek(struct GenesisNode *node) {
    // do nothing
}

static void destroy_midi_device_node_descriptor(struct GenesisNodeDescriptor *node_descriptor) {
    GenesisMidiDevice *midi_device = (GenesisMidiDevice*)node_descriptor->userdata;
    midi_device_unref(midi_device);
}

int genesis_midi_device_create_node_descriptor(
        struct GenesisMidiDevice *midi_device,
        struct GenesisNodeDescriptor **out)
{
    GenesisContext *context = midi_device->midi_hardware->context;
    *out = nullptr;

    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 2);

    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->name = create_formatted_str("midi-device-%s", genesis_midi_device_name(midi_device));
    node_descr->description = create_formatted_str("Midi Device: %s", genesis_midi_device_description(midi_device));

    if (!node_descr->name || !node_descr->description) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->run = midi_node_run;
    node_descr->create = midi_node_create;
    node_descr->destroy = midi_node_destroy;
    node_descr->seek = midi_node_seek;

    node_descr->userdata = midi_device;
    midi_device_ref(midi_device);

    node_descr->destroy_descriptor = destroy_midi_device_node_descriptor;

    GenesisNotesPortDescriptor *notes_out_port = create_zero<GenesisNotesPortDescriptor>();
    if (!notes_out_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->port_descriptors.at(0) = &notes_out_port->port_descriptor;

    notes_out_port->port_descriptor.name = strdup("notes_out");
    if (!notes_out_port->port_descriptor.name) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }
    notes_out_port->port_descriptor.port_type = GenesisPortTypeNotesOut;


    GenesisParamPortDescriptor *param_out_port = create_zero<GenesisParamPortDescriptor>();
    if (!param_out_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->port_descriptors.at(1) = &param_out_port->port_descriptor;

    param_out_port->port_descriptor.name = strdup("param_out");
    if (!param_out_port->port_descriptor.name) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }
    param_out_port->port_descriptor.port_type = GenesisPortTypeParamOut;

    *out = node_descr;
    return 0;
}

static void destroy_audio_port_descriptor(GenesisAudioPortDescriptor *audio_port_descr) {
    destroy(audio_port_descr, 1);
}

static void destroy_notes_port_descriptor(GenesisNotesPortDescriptor *notes_port_descr) {
    destroy(notes_port_descr, 1);
}

static void destroy_param_port_descriptor(GenesisParamPortDescriptor *param_port_descr) {
    destroy(param_port_descr, 1);
}

void genesis_port_descriptor_destroy(struct GenesisPortDescriptor *port_descriptor) {
    switch (port_descriptor->port_type) {
    case GenesisPortTypeAudioIn:
    case GenesisPortTypeAudioOut:
        destroy_audio_port_descriptor((GenesisAudioPortDescriptor *)port_descriptor);
        break;
    case GenesisPortTypeNotesIn:
    case GenesisPortTypeNotesOut:
        destroy_notes_port_descriptor((GenesisNotesPortDescriptor *)port_descriptor);
        break;
    case GenesisPortTypeParamIn:
    case GenesisPortTypeParamOut:
        destroy_param_port_descriptor((GenesisParamPortDescriptor *)port_descriptor);
        break;
    }
    free(port_descriptor->name);
}

void genesis_node_descriptor_destroy(struct GenesisNodeDescriptor *node_descriptor) {
    if (node_descriptor) {
        if (node_descriptor->destroy_descriptor)
            node_descriptor->destroy_descriptor(node_descriptor);
        for (int i = 0; i < node_descriptor->port_descriptors.length(); i += 1) {
            GenesisPortDescriptor *port_descriptor = node_descriptor->port_descriptors.at(i);
            genesis_port_descriptor_destroy(port_descriptor);
        }

        free(node_descriptor->name);
        free(node_descriptor->description);

        destroy(node_descriptor, 1);
    }
}

static void resolve_channel_layout(GenesisAudioPort *audio_port) {
    GenesisAudioPortDescriptor *port_descr = (GenesisAudioPortDescriptor*)audio_port->port.descriptor;
    if (port_descr->channel_layout_fixed) {
        if (port_descr->same_channel_layout_index >= 0) {
            GenesisAudioPort *other_port = (GenesisAudioPort *)
                &audio_port->port.node->ports[port_descr->same_channel_layout_index];
            audio_port->channel_layout = other_port->channel_layout;
        } else {
            audio_port->channel_layout = port_descr->channel_layout;
        }
    }
}

static void resolve_sample_rate(GenesisAudioPort *audio_port) {
    GenesisAudioPortDescriptor *port_descr = (GenesisAudioPortDescriptor *)audio_port->port.descriptor;
    if (port_descr->sample_rate_fixed) {
        if (port_descr->same_sample_rate_index >= 0) {
            GenesisAudioPort *other_port = (GenesisAudioPort *)
                &audio_port->port.node->ports[port_descr->same_sample_rate_index];
            audio_port->sample_rate = other_port->sample_rate;
        } else {
            audio_port->sample_rate = port_descr->sample_rate;
        }
    }
}

static int connect_audio_ports(GenesisAudioPort *source, GenesisAudioPort *dest) {
    GenesisAudioPortDescriptor *source_audio_descr = (GenesisAudioPortDescriptor *) source->port.descriptor;
    GenesisAudioPortDescriptor *dest_audio_descr = (GenesisAudioPortDescriptor *) dest->port.descriptor;

    resolve_channel_layout(source);
    resolve_channel_layout(dest);
    if (source_audio_descr->channel_layout_fixed &&
        dest_audio_descr->channel_layout_fixed)
    {
        // both fixed. they better match up
        if (!genesis_channel_layout_equal(&source_audio_descr->channel_layout,
                    &dest_audio_descr->channel_layout))
        {
            return GenesisErrorIncompatibleChannelLayouts;
        }
    } else if (!source_audio_descr->channel_layout_fixed &&
               !dest_audio_descr->channel_layout_fixed)
    {
        // anything goes. default to mono
        source->channel_layout = *genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
        dest->channel_layout = source->channel_layout;
    } else if (source_audio_descr->channel_layout_fixed) {
        // source is fixed, use that one
        dest->channel_layout = source->channel_layout;
    } else {
        // dest is fixed, use that one
        source->channel_layout = dest->channel_layout;
    }

    resolve_sample_rate(source);
    resolve_sample_rate(dest);
    if (source_audio_descr->sample_rate_fixed && dest_audio_descr->sample_rate_fixed) {
        // both fixed. they better match up
        if (source_audio_descr->sample_rate != dest_audio_descr->sample_rate)
            return GenesisErrorIncompatibleSampleRates;
    } else if (!source_audio_descr->sample_rate_fixed &&
               !dest_audio_descr->sample_rate_fixed)
    {
        // anything goes. default to 48,000 Hz
        source->sample_rate = 48000;
        dest->sample_rate = source->sample_rate;
    } else if (source_audio_descr->sample_rate_fixed) {
        // source is fixed, use that one
        dest->sample_rate = source->sample_rate;
    } else {
        // dest is fixed, use that one
        source->sample_rate = dest->sample_rate;
    }

    source->port.output_to = &dest->port;
    dest->port.input_from = &source->port;
    return 0;
}

static int connect_notes_ports(GenesisNotesPort *source, GenesisNotesPort *dest) {
    source->port.output_to = &dest->port;
    dest->port.input_from = &source->port;
    return 0;
}

int genesis_connect_ports(struct GenesisPort *source, struct GenesisPort *dest) {
    // perform validation
    switch (source->descriptor->port_type) {
        case GenesisPortTypeAudioOut:
            if (dest->descriptor->port_type != GenesisPortTypeAudioIn)
                return GenesisErrorIncompatiblePorts;

            return connect_audio_ports((GenesisAudioPort *)source, (GenesisAudioPort *)dest);
        case GenesisPortTypeNotesOut:
            if (dest->descriptor->port_type != GenesisPortTypeNotesIn)
                return GenesisErrorIncompatiblePorts;

            return connect_notes_ports((GenesisNotesPort *)source, (GenesisNotesPort *)dest);
        case GenesisPortTypeParamOut:
            if (dest->descriptor->port_type != GenesisPortTypeParamIn)
                return GenesisErrorIncompatiblePorts;
            panic("unimplemented: connect param ports");
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeNotesIn:
        case GenesisPortTypeParamIn:
            return GenesisErrorInvalidPortDirection;
    }
    panic("unknown port type");
}

int genesis_node_descriptor_find_port_index(
        const struct GenesisNodeDescriptor *node_descriptor, const char *name)
{
    for (int i = 0; i < node_descriptor->port_descriptors.length(); i += 1) {
        GenesisPortDescriptor *port_descriptor = node_descriptor->port_descriptors.at(i);
        if (strcmp(port_descriptor->name, name) == 0)
            return i;
    }
    return -1;
}

struct GenesisPort *genesis_node_port(struct GenesisNode *node, int port_index) {
    if (port_index < 0 || port_index >= node->port_count)
        return nullptr;

    return node->ports[port_index];
}

struct GenesisNode *genesis_port_node(struct GenesisPort *port) {
    return port->node;
}

struct GenesisNodeDescriptor *genesis_create_node_descriptor(
        struct GenesisContext *context, int port_count)
{
    GenesisNodeDescriptor *node_descr = create_zero<GenesisNodeDescriptor>();
    if (!node_descr)
        return nullptr;

    if (node_descr->port_descriptors.resize(port_count)) {
        genesis_node_descriptor_destroy(node_descr);
        return nullptr;
    }

    if (context->node_descriptors.append(node_descr)) {
        genesis_node_descriptor_destroy(node_descr);
        return nullptr;
    }

    node_descr->context = context;

    return node_descr;
}

static GenesisAudioPortDescriptor *create_audio_port(GenesisNodeDescriptor *node_descr,
        int port_index, GenesisPortType port_type)
{
    GenesisAudioPortDescriptor *audio_port_descr = create_zero<GenesisAudioPortDescriptor>();
    if (!audio_port_descr)
        return nullptr;
    node_descr->port_descriptors.at(port_index) = &audio_port_descr->port_descriptor;
    audio_port_descr->port_descriptor.port_type = port_type;
    return audio_port_descr;
}

static GenesisNotesPortDescriptor *create_notes_port(GenesisNodeDescriptor *node_descr,
        int port_index, GenesisPortType port_type)
{
    GenesisNotesPortDescriptor *notes_port_descr = create_zero<GenesisNotesPortDescriptor>();
    if (!notes_port_descr)
        return nullptr;
    node_descr->port_descriptors.at(port_index) = &notes_port_descr->port_descriptor;
    notes_port_descr->port_descriptor.port_type = port_type;
    return notes_port_descr;
}

static GenesisParamPortDescriptor *create_param_port(GenesisNodeDescriptor *node_descr,
        int port_index, GenesisPortType port_type)
{
    GenesisParamPortDescriptor *param_port_descr = create_zero<GenesisParamPortDescriptor>();
    if (!param_port_descr)
        return nullptr;
    node_descr->port_descriptors.at(port_index) = &param_port_descr->port_descriptor;
    param_port_descr->port_descriptor.port_type = port_type;
    return param_port_descr;
}

struct GenesisPortDescriptor *genesis_node_descriptor_create_port(
        struct GenesisNodeDescriptor *node_descr, int port_index,
        enum GenesisPortType port_type)
{
    if (port_index < 0 || port_index >= node_descr->port_descriptors.length())
        return nullptr;
    switch (port_type) {
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeAudioOut:
            return &create_audio_port(node_descr, port_index, port_type)->port_descriptor;
        case GenesisPortTypeNotesIn:
        case GenesisPortTypeNotesOut:
            return &create_notes_port(node_descr, port_index, port_type)->port_descriptor;
        case GenesisPortTypeParamIn:
        case GenesisPortTypeParamOut:
            return &create_param_port(node_descr, port_index, port_type)->port_descriptor;
    }
    panic("invalid port type");
}

static void debug_print_audio_port_config(GenesisAudioPort *port) {
    resolve_channel_layout(port);
    resolve_sample_rate(port);

    GenesisAudioPortDescriptor *audio_descr = (GenesisAudioPortDescriptor *)port->port.descriptor;
    const char *chan_layout_fixed = audio_descr->channel_layout_fixed ? "(fixed)" : "(any)";
    const char *sample_rate_fixed = audio_descr->sample_rate_fixed ? "(fixed)" : "(any)";

    fprintf(stderr, "audio port: %s\n", port->port.descriptor->name);
    fprintf(stderr, "sample rate: %s %d\n", sample_rate_fixed, port->sample_rate);
    fprintf(stderr, "channel_layout: %s ", chan_layout_fixed);
    genesis_debug_print_channel_layout(&port->channel_layout);
}

static void debug_print_notes_port_config(GenesisNotesPort *port) {
    fprintf(stderr, "notes port: %s\n", port->port.descriptor->name);
}

static void debug_print_param_port_config(GenesisParamPort *port) {
    fprintf(stderr, "param port: %s\n", port->port.descriptor->name);
}

void genesis_debug_print_port_config(struct GenesisPort *port) {
    switch (port->descriptor->port_type) {
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeAudioOut:
            debug_print_audio_port_config((GenesisAudioPort *)port);
            return;
        case GenesisPortTypeNotesIn:
        case GenesisPortTypeNotesOut:
            debug_print_notes_port_config((GenesisNotesPort *)port);
            return;
        case GenesisPortTypeParamIn:
        case GenesisPortTypeParamOut:
            debug_print_param_port_config((GenesisParamPort *)port);
            return;
    }
    panic("invalid port type");
}

static void pipeline_thread_run(void *userdata) {
    GenesisContext *context = reinterpret_cast<GenesisContext*>(userdata);
    for (;;) {
        GenesisNode *node = context->task_queue.dequeue();
        if (!context->pipeline_running)
            break;

        const GenesisNodeDescriptor *node_descriptor = node->descriptor;
        node_descriptor->run(node);
        node->data_ready = true;
        node->being_processed = false;

        // check each node that depended on this one to see if we can now
        // process that one.
        for (int i = 0; i < node->port_count; i += 1) {
            GenesisPort *port = node->ports[i];
            GenesisPort *parent_port = port->output_to;
            if (!parent_port || parent_port == port)
                continue;
            queue_node_if_ready(context, parent_port->node, false);
        }
    }
}

int genesis_start_pipeline(struct GenesisContext *context) {
    int err = context->task_queue.resize(context->nodes.length() + context->thread_pool.length());
    if (err) {
        genesis_stop_pipeline(context);
        return err;
    }

    // initialize nodes
    for (int node_index = 0; node_index < context->nodes.length(); node_index += 1) {
        GenesisNode *node = context->nodes.at(node_index);
        node->being_processed = false;
        node->data_ready = false;
        for (int port_i = 0; port_i < node->port_count; port_i += 1) {
            GenesisPort *port = node->ports[port_i];
            if (port->descriptor->port_type == GenesisPortTypeAudioOut) {
                GenesisAudioPort *audio_port = reinterpret_cast<GenesisAudioPort*>(port);
                int sample_buffer_frame_count = ceil(context->latency * audio_port->sample_rate);
                audio_port->bytes_per_frame = BYTES_PER_SAMPLE * audio_port->channel_layout.channel_count;
                audio_port->sample_buffer_size = sample_buffer_frame_count * audio_port->bytes_per_frame;

                destroy(audio_port->sample_buffer, 1);
                audio_port->sample_buffer = create_zero<RingBuffer>(audio_port->sample_buffer_size);
                if (!audio_port->sample_buffer) {
                    genesis_stop_pipeline(context);
                    return GenesisErrorNoMem;
                }
            } else if (port->descriptor->port_type == GenesisPortTypeNotesOut) {
                GenesisNotesPort *notes_port = reinterpret_cast<GenesisNotesPort*>(port);
                int min_event_buffer_size = EVENTS_PER_SECOND_CAPACITY * context->latency;

                destroy(notes_port->event_buffer, 1);
                notes_port->event_buffer = create_zero<RingBuffer>(min_event_buffer_size);

                if (!notes_port->event_buffer) {
                    genesis_stop_pipeline(context);
                    return GenesisErrorNoMem;
                }
            }
        }
        node->timestamp = 0.0;
        node->descriptor->seek(node);
    }
    context->pipeline_running = true;

    for (int i = 0; i < context->thread_pool.length(); i += 1) {
        Thread *thread = &context->thread_pool.at(i);
        int err = thread->start(pipeline_thread_run, context);
        if (err) {
            genesis_stop_pipeline(context);
            return err;
        }
    }

    return 0;
}

void genesis_stop_pipeline(struct GenesisContext *context) {
    context->pipeline_running = false;
    context->task_queue.wakeup_all(context->thread_pool.length());
    for (int i = 0; i < context->thread_pool.length(); i += 1) {
        Thread *thread = &context->thread_pool.at(i);
        if (thread->running())
            thread->join();
    }
}

int genesis_set_latency(struct GenesisContext *context, double latency) {
    if (latency <= 0.0)
        return GenesisErrorInvalidParam;
    if (latency > 60.0)
        return GenesisErrorInvalidParam;
    if (context->pipeline_running)
        return GenesisErrorInvalidState;

    context->latency = latency;
    return 0;
}
