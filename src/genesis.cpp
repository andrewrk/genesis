#include "genesis.hpp"
#include "audio_file.hpp"
#include "midi_note_pitch.hpp"
#include "synth.hpp"
#include "delay.hpp"
#include "resample.hpp"

static atomic_flag lib_init_flag = ATOMIC_FLAG_INIT;

static const int BYTES_PER_SAMPLE = 4; // assuming float samples
static const int EVENTS_PER_SECOND_CAPACITY = 16000;
static const double whole_notes_per_second = 140.0 / 60.0;

static int (*plugin_create_list[])(GenesisContext *context) = {
    create_synth_descriptor,
    create_delay_descriptor,
    create_resample_descriptor,
};

static_assert(GENESIS_NOTES_COUNT == array_length(midi_note_to_pitch), "");

struct PlaybackNodeContext {
    OpenPlaybackDevice *playback_device;
    GenesisContext *context;
};

struct ControllerNodeContext {
    GenesisMidiDevice *midi_device;
};

struct RecordingNodeContext {
    OpenRecordingDevice *recording_device;
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

    for (int i = 0; i < array_length(plugin_create_list); i += 1) {
        int (*create_fn)(GenesisContext *) = plugin_create_list[i];
        int err = create_fn(context);
        if (err) {
            genesis_destroy_context(context);
            return err;
        }
    }

    *out_context = context;
    return 0;
}

void genesis_destroy_context(struct GenesisContext *context) {
    if (context) {
        if (context->pipeline_running)
            genesis_stop_pipeline(context);

        if (context->midi_hardware)
            destroy_midi_hardware(context->midi_hardware);

        while (context->nodes.length()) {
            genesis_node_destroy(context->nodes.at(context->nodes.length() - 1));
        }

        while (context->node_descriptors.length()) {
            int last_index = context->node_descriptors.length() - 1;
            genesis_node_descriptor_destroy(context->node_descriptors.at(last_index));
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

static GenesisPort *create_port_from_descriptor(GenesisPortDescriptor *port_descriptor) {
    GenesisPort *port = nullptr;
    switch (port_descriptor->port_type) {
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeAudioOut:
            port = (GenesisPort*)create_zero<GenesisAudioPort>();
            break;

        case GenesisPortTypeEventsIn:
        case GenesisPortTypeEventsOut:
            port = (GenesisPort*)create_zero<GenesisEventsPort>();
            break;

    }
    if (!port)
        return nullptr;
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
    if (node_descriptor->create) {
        if (node_descriptor->create(node)) {
            genesis_node_destroy(node);
            return nullptr;
        }
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

static void destroy_audio_port(GenesisAudioPort *audio_port) {
    destroy(audio_port->sample_buffer, 1);
    destroy(audio_port, 1);
}

static void destroy_events_port(GenesisEventsPort *events_port) {
    destroy(events_port->event_buffer, 1);
    destroy(events_port, 1);
}

static void destroy_port(struct GenesisPort *port) {
    switch (port->descriptor->port_type) {
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeAudioOut:
            destroy_audio_port((GenesisAudioPort *)port);
            return;
        case GenesisPortTypeEventsIn:
        case GenesisPortTypeEventsOut:
            destroy_events_port((GenesisEventsPort *)port);
            return;
    }
    panic("invalid port type");
}

void genesis_node_destroy(struct GenesisNode *node) {
    if (node) {
        // first all disconnect methods on all ports
        if (node->ports) {
            for (int i = 0; i < node->port_count; i += 1) {
                if (node->ports[i]) {
                    GenesisPort *port = node->ports[i];
                    if (port->output_to)
                        genesis_disconnect_ports(port, port->output_to);
                    if (port->input_from)
                        genesis_disconnect_ports(port->input_from, port);
                }
            }
        }

        // call destructor on node
        if (node->constructed && node->descriptor->destroy)
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
                    GenesisPort *port = node->ports[i];
                    destroy_port(port);
                }
            }
            destroy(node->ports, node->port_count);
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

const struct GenesisChannelLayout *genesis_audio_device_channel_layout(
        const struct GenesisAudioDevice *audio_device)
{
    return &audio_device->channel_layout;
}

int genesis_audio_device_sample_rate(const struct GenesisAudioDevice *audio_device) {
    return audio_device->default_sample_rate;
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

static void playback_node_run(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    OpenPlaybackDevice *playback_device = playback_node_context->playback_device;
    int writable_size = playback_device->writable_size();
    if (writable_size <= 0)
        return;
    playback_device->lock();
    fill_playback_buffer(node, playback_node_context, playback_device, writable_size);
    playback_device->unlock();
}

static void playback_node_destroy(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    if (playback_node_context) {
        if (playback_node_context->playback_device) {
            destroy(playback_node_context->playback_device, 1);
        }
        destroy(playback_node_context, 1);
    }
}

static void playback_node_callback(int requested_byte_count, void *userdata) {
    GenesisNode *node = (GenesisNode *)userdata;
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    OpenPlaybackDevice *playback_device = playback_node_context->playback_device;
    GenesisContext *context = playback_node_context->context;

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

    fill_playback_buffer(node, playback_node_context, playback_device, requested_byte_count);

    GenesisAudioPort *audio_in_port = (GenesisAudioPort *)node->ports[0];
    GenesisAudioPort *audio_out_port = (GenesisAudioPort *)audio_in_port->port.input_from;
    GenesisNode *child_node = audio_out_port->port.node;
    child_node->data_ready = false;
    queue_node_if_ready(context, child_node, true);
}

static int playback_node_create(struct GenesisNode *node) {
    GenesisContext *context = node->descriptor->context;

    PlaybackNodeContext *playback_node_context = create_zero<PlaybackNodeContext>();
    if (!playback_node_context) {
        playback_node_destroy(node);
        return GenesisErrorNoMem;
    }
    node->userdata = playback_node_context;
    playback_node_context->context = context;

    GenesisAudioDevice *device = (GenesisAudioDevice*)node->descriptor->userdata;

    playback_node_context->playback_device = create_zero<OpenPlaybackDevice>(
            &context->audio_hardware, &device->channel_layout,
            GenesisSampleFormatFloat, context->latency, device->default_sample_rate,
            playback_node_callback, node);
    if (!playback_node_context->playback_device) {
        playback_node_destroy(node);
        return GenesisErrorNoMem;
    }

    int err = playback_node_context->playback_device->start(device->name.raw());
    if (err) {
        playback_node_destroy(node);
        return err;
    }

    return 0;
}

static void playback_node_seek(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    playback_node_context->playback_device->clear_buffer();
}

static void destroy_audio_device_node_descriptor(struct GenesisNodeDescriptor *node_descriptor) {
    GenesisAudioDevice *audio_device = (GenesisAudioDevice*)node_descriptor->userdata;
    destroy_audio_device(audio_device);
}

static void recording_node_callback(void *userdata) {
    GenesisNode *node = (GenesisNode *)userdata;
    GenesisContext *context = node->descriptor->context;
    RecordingNodeContext *recording_node_context = (RecordingNodeContext*)node->userdata;
    OpenRecordingDevice *device = recording_node_context->recording_device;

    if (!context->pipeline_running) {
        for (;;) {
            const char *in_buf;
            int byte_count;

            device->peek(&in_buf, &byte_count);

            if (byte_count <= 0)
                break;

            device->drop();
        }

        return;
    }

    GenesisPort *audio_out_port = node->ports[0];

    for (;;) {
        const char *in_buf;
        int byte_count;

        device->peek(&in_buf, &byte_count);

        if (byte_count <= 0)
            break;

        int free_count = genesis_audio_out_port_free_count(audio_out_port);
        int write_count = min(byte_count, free_count);
        if (write_count > 0) {
            char *out_buf = genesis_audio_out_port_write_ptr(audio_out_port);
            if (in_buf) {
                memcpy(out_buf, in_buf, write_count);
            } else {
                // there's a hole. fill it with silence
                memset(out_buf, 0, write_count);
            }

            genesis_audio_out_port_advance_write_ptr(audio_out_port, write_count);
        }

        device->drop();
    }
}

static void recording_node_run(struct GenesisNode *node) {
    // do nothing
}

static void recording_node_seek(struct GenesisNode *node) {
    RecordingNodeContext *recording_node_context = (RecordingNodeContext*)node->userdata;
    recording_node_context->recording_device->clear_buffer();
}

static void recording_node_destroy(struct GenesisNode *node) {
    RecordingNodeContext *recording_node_context = (RecordingNodeContext*)node->userdata;
    destroy(recording_node_context, 1);
}

static int recording_node_create(struct GenesisNode *node) {
    GenesisContext *context = node->descriptor->context;
    RecordingNodeContext *recording_node_context = create_zero<RecordingNodeContext>();
    if (!recording_node_context) {
        recording_node_destroy(node);
        return GenesisErrorNoMem;
    }
    node->userdata = recording_node_context;

    GenesisAudioDevice *device = (GenesisAudioDevice*)node->descriptor->userdata;

    recording_node_context->recording_device = create_zero<OpenRecordingDevice>(
            &context->audio_hardware, &device->channel_layout,
            GenesisSampleFormatFloat, context->latency, device->default_sample_rate,
            recording_node_callback, node);
    if (!recording_node_context->recording_device) {
        recording_node_destroy(node);
        return GenesisErrorNoMem;
    }

    int err = recording_node_context->recording_device->start(device->name.raw());
    if (err) {
        recording_node_destroy(node);
        return err;
    }

    return 0;
}

int genesis_audio_device_create_node_descriptor(struct GenesisAudioDevice *audio_device,
        struct GenesisNodeDescriptor **out)
{
    GenesisContext *context = audio_device->context;

    *out = nullptr;

    const char *name_fmt_str = (audio_device->purpose == GenesisAudioDevicePurposePlayback) ?
        "playback-device-%s" : "recording-device-%s";
    const char *description_fmt_str = (audio_device->purpose == GenesisAudioDevicePurposePlayback) ?
        "Playback Device: %s" : "Recording Device: %s";
    char *name = create_formatted_str(name_fmt_str, audio_device->name.raw());
    char *description = create_formatted_str(description_fmt_str, audio_device->description.raw());
    if (!name || !description) {
        free(name);
        free(description);
        return GenesisErrorNoMem;
    }

    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 1, name, description);

    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }
    free(name);
    free(description);
    name = nullptr;
    description = nullptr;

    if (audio_device->purpose == GenesisAudioDevicePurposePlayback) {
        node_descr->run = playback_node_run;
        node_descr->create = playback_node_create;
        node_descr->destroy = playback_node_destroy;
        node_descr->seek = playback_node_seek;
    } else {
        node_descr->run = recording_node_run;
        node_descr->create = recording_node_create;
        node_descr->destroy = recording_node_destroy;
        node_descr->seek = recording_node_seek;
    }

    node_descr->userdata = duplicate_audio_device(audio_device);
    if (!node_descr->userdata) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }
    node_descr->destroy_descriptor = destroy_audio_device_node_descriptor;

    GenesisAudioPortDescriptor *audio_port = create_zero<GenesisAudioPortDescriptor>();
    if (!audio_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->port_descriptors.at(0) = &audio_port->port_descriptor;

    const char *port_name = (audio_device->purpose == GenesisAudioDevicePurposePlayback) ?
        "audio_in" : "audio_out";

    audio_port->port_descriptor.name = strdup(port_name);
    if (!audio_port->port_descriptor.name) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    audio_port->port_descriptor.port_type = (audio_device->purpose == GenesisAudioDevicePurposePlayback)
        ? GenesisPortTypeAudioIn : GenesisPortTypeAudioOut;
    audio_port->channel_layout = audio_device->channel_layout;
    audio_port->channel_layout_fixed = true;
    audio_port->same_channel_layout_index = -1;
    audio_port->sample_rate = audio_device->default_sample_rate;
    audio_port->sample_rate_fixed = true;
    audio_port->same_sample_rate_index = -1;

    *out = node_descr;
    return 0;
}

static void midi_node_on_event(struct GenesisMidiDevice *device, const struct GenesisMidiEvent *event) {
    GenesisNode *node = (GenesisNode *)device->userdata;
    GenesisEventsPort *events_out_port = (GenesisEventsPort *)node->ports[0];
    int free_count = events_out_port->event_buffer->free_count();
    int midi_event_size = sizeof(GenesisMidiEvent);
    if (free_count < midi_event_size) {
        fprintf(stderr, "event buffer overflow\n");
        return;
    }
    char *ptr = events_out_port->event_buffer->write_ptr();
    memcpy(ptr, event, midi_event_size);
    events_out_port->event_buffer->advance_write_ptr(midi_event_size);
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

    char *name = create_formatted_str("midi-device-%s", genesis_midi_device_name(midi_device));
    char *description = create_formatted_str("Midi Device: %s", genesis_midi_device_description(midi_device));

    if (!name || !description) {
        free(name);
        free(description);
        return GenesisErrorNoMem;
    }

    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 1, name, description);

    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    free(name);
    free(description);
    name = nullptr;
    description = nullptr;

    node_descr->run = midi_node_run;
    node_descr->create = midi_node_create;
    node_descr->destroy = midi_node_destroy;
    node_descr->seek = midi_node_seek;

    node_descr->userdata = midi_device;
    midi_device_ref(midi_device);

    node_descr->destroy_descriptor = destroy_midi_device_node_descriptor;

    GenesisEventsPortDescriptor *events_out_port = create_zero<GenesisEventsPortDescriptor>();
    if (!events_out_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    node_descr->port_descriptors.at(0) = &events_out_port->port_descriptor;

    events_out_port->port_descriptor.name = strdup("events_out");
    if (!events_out_port->port_descriptor.name) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }
    events_out_port->port_descriptor.port_type = GenesisPortTypeEventsOut;

    *out = node_descr;
    return 0;
}

static void destroy_audio_port_descriptor(GenesisAudioPortDescriptor *audio_port_descr) {
    destroy(audio_port_descr, 1);
}

static void destroy_events_port_descriptor(GenesisEventsPortDescriptor *events_port_descr) {
    destroy(events_port_descr, 1);
}

void genesis_port_descriptor_destroy(struct GenesisPortDescriptor *port_descriptor) {
    free(port_descriptor->name);
    switch (port_descriptor->port_type) {
    case GenesisPortTypeAudioIn:
    case GenesisPortTypeAudioOut:
        destroy_audio_port_descriptor((GenesisAudioPortDescriptor *)port_descriptor);
        break;
    case GenesisPortTypeEventsIn:
    case GenesisPortTypeEventsOut:
        destroy_events_port_descriptor((GenesisEventsPortDescriptor *)port_descriptor);
        break;
    }
}

void genesis_node_descriptor_destroy(struct GenesisNodeDescriptor *node_descriptor) {
    if (node_descriptor) {
        GenesisContext *context = node_descriptor->context;
        if (node_descriptor->set_index >= 0) {
            context->node_descriptors.swap_remove(node_descriptor->set_index);
            if (node_descriptor->set_index < context->node_descriptors.length())
                context->node_descriptors.at(node_descriptor->set_index)->set_index = node_descriptor->set_index;
            node_descriptor->set_index = -1;
        }

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
                audio_port->port.node->ports[port_descr->same_channel_layout_index];
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
                audio_port->port.node->ports[port_descr->same_sample_rate_index];
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
        if (!genesis_channel_layout_equal(&source->channel_layout, &dest->channel_layout)) {
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
        if (source->sample_rate != dest->sample_rate)
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

    return 0;
}

static int connect_events_ports(GenesisEventsPort *source, GenesisEventsPort *dest) {
    return 0;
}

void genesis_disconnect_ports(struct GenesisPort *source, struct GenesisPort *dest) {
    source->output_to = nullptr;
    dest->input_from = nullptr;
    if (source->descriptor->disconnect)
        source->descriptor->disconnect(source, dest);
    if (dest->descriptor->disconnect)
        dest->descriptor->disconnect(dest, source);
}

int genesis_connect_ports(struct GenesisPort *source, struct GenesisPort *dest) {
    int err = GenesisErrorInvalidPortType;
    switch (source->descriptor->port_type) {
        case GenesisPortTypeAudioOut:
            if (dest->descriptor->port_type != GenesisPortTypeAudioIn)
                return GenesisErrorInvalidPortDirection;

            err = connect_audio_ports((GenesisAudioPort *)source, (GenesisAudioPort *)dest);
            break;
        case GenesisPortTypeEventsOut:
            if (dest->descriptor->port_type != GenesisPortTypeEventsIn)
                return GenesisErrorInvalidPortDirection;

            err = connect_events_ports((GenesisEventsPort *)source, (GenesisEventsPort *)dest);
            break;
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeEventsIn:
            return GenesisErrorInvalidPortDirection;
    }
    if (err)
        return err;

    source->output_to = dest;
    dest->input_from = source;

    if (source->descriptor->connect) {
        err = source->descriptor->connect(source, dest);
        if (err) {
            source->output_to = nullptr;
            dest->input_from = nullptr;
            return err;
        }
    }
    if (dest->descriptor->connect) {
        err = dest->descriptor->connect(dest, source);
        if (err) {
            if (source->descriptor->disconnect)
                source->descriptor->disconnect(source, dest);
            source->output_to = nullptr;
            dest->input_from = nullptr;
            return err;
        }
    }

    return 0;
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
        struct GenesisContext *context, int port_count,
        const char *name, const char *description)
{
    GenesisNodeDescriptor *node_descr = create_zero<GenesisNodeDescriptor>();
    if (!node_descr)
        return nullptr;
    node_descr->set_index = -1;
    node_descr->context = context;

    node_descr->name = strdup(name);
    node_descr->description = strdup(description);
    if (!node_descr->name || !node_descr->description) {
        genesis_node_descriptor_destroy(node_descr);
        return nullptr;
    }

    if (node_descr->port_descriptors.resize(port_count)) {
        genesis_node_descriptor_destroy(node_descr);
        return nullptr;
    }

    int set_index = context->node_descriptors.length();
    if (context->node_descriptors.append(node_descr)) {
        genesis_node_descriptor_destroy(node_descr);
        return nullptr;
    }
    node_descr->set_index = set_index;

    return node_descr;
}

struct GenesisPortDescriptor *genesis_node_descriptor_create_port(
        struct GenesisNodeDescriptor *node_descr, int port_index,
        enum GenesisPortType port_type, const char *name)
{
    if (port_index < 0 || port_index >= node_descr->port_descriptors.length())
        return nullptr;

    GenesisPortDescriptor *port_descr = nullptr;
    switch (port_type) {
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeAudioOut:
            port_descr = (GenesisPortDescriptor*)create_zero<GenesisAudioPortDescriptor>();
            break;
        case GenesisPortTypeEventsIn:
        case GenesisPortTypeEventsOut:
            port_descr = (GenesisPortDescriptor*)create_zero<GenesisEventsPortDescriptor>();
            break;
    }
    if (!port_descr)
        return nullptr;

    port_descr->port_type = port_type;

    port_descr->name = strdup(name);
    if (!port_descr->name) {
        genesis_port_descriptor_destroy(port_descr);
        return nullptr;
    }

    node_descr->port_descriptors.at(port_index) = port_descr;

    return port_descr;
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

static void debug_print_events_port_config(GenesisEventsPort *port) {
    fprintf(stderr, "events port: %s\n", port->port.descriptor->name);
}

void genesis_debug_print_port_config(struct GenesisPort *port) {
    switch (port->descriptor->port_type) {
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeAudioOut:
            debug_print_audio_port_config((GenesisAudioPort *)port);
            return;
        case GenesisPortTypeEventsIn:
        case GenesisPortTypeEventsOut:
            debug_print_events_port_config((GenesisEventsPort *)port);
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
    // initialize nodes
    for (int node_index = 0; node_index < context->nodes.length(); node_index += 1) {
        GenesisNode *node = context->nodes.at(node_index);
        for (int port_i = 0; port_i < node->port_count; port_i += 1) {
            GenesisPort *port = node->ports[port_i];
            if (port->descriptor->port_type == GenesisPortTypeAudioOut) {
                GenesisAudioPort *audio_port = reinterpret_cast<GenesisAudioPort*>(port);
                if (audio_port->sample_buffer)
                    audio_port->sample_buffer->clear();
            } else if (port->descriptor->port_type == GenesisPortTypeEventsOut) {
                GenesisEventsPort *events_port = reinterpret_cast<GenesisEventsPort*>(port);
                if (events_port->event_buffer)
                    events_port->event_buffer->clear();
            }
        }
        node->timestamp = 0.0;
        if (node->descriptor->seek)
            node->descriptor->seek(node);
    }

    return genesis_resume_pipeline(context);
}

void genesis_stop_pipeline(struct GenesisContext *context) {
    context->pipeline_running = false;
    context->task_queue.wakeup_all(context->thread_pool.length());
    for (int i = 0; i < context->thread_pool.length(); i += 1) {
        Thread *thread = &context->thread_pool.at(i);
        thread->join();
    }
}

int genesis_resume_pipeline(struct GenesisContext *context) {
    int err = context->task_queue.resize(context->nodes.length() + context->thread_pool.length());
    if (err) {
        genesis_stop_pipeline(context);
        return err;
    }

    for (int node_index = 0; node_index < context->nodes.length(); node_index += 1) {
        GenesisNode *node = context->nodes.at(node_index);
        node->being_processed = false;
        for (int port_i = 0; port_i < node->port_count; port_i += 1) {
            GenesisPort *port = node->ports[port_i];
            if (port->descriptor->port_type == GenesisPortTypeAudioIn) {
                GenesisAudioPort *audio_port = reinterpret_cast<GenesisAudioPort*>(port);
                audio_port->bytes_per_frame = BYTES_PER_SAMPLE * audio_port->channel_layout.channel_count;
            } else if (port->descriptor->port_type == GenesisPortTypeAudioOut) {
                GenesisAudioPort *audio_port = reinterpret_cast<GenesisAudioPort*>(port);
                int sample_buffer_frame_count = ceil(context->latency * audio_port->sample_rate);
                audio_port->bytes_per_frame = BYTES_PER_SAMPLE * audio_port->channel_layout.channel_count;
                int new_sample_buffer_size = sample_buffer_frame_count * audio_port->bytes_per_frame;
                bool different = new_sample_buffer_size != audio_port->sample_buffer_size;
                audio_port->sample_buffer_size = new_sample_buffer_size;

                if (!audio_port->sample_buffer || different) {
                    node->data_ready = false;

                    destroy(audio_port->sample_buffer, 1);
                    audio_port->sample_buffer = create_zero<RingBuffer>(audio_port->sample_buffer_size);
                    if (!audio_port->sample_buffer) {
                        genesis_stop_pipeline(context);
                        return GenesisErrorNoMem;
                    }
                }
            } else if (port->descriptor->port_type == GenesisPortTypeEventsOut) {
                GenesisEventsPort *events_port = reinterpret_cast<GenesisEventsPort*>(port);
                int min_event_buffer_size = EVENTS_PER_SECOND_CAPACITY * context->latency;
                if (!events_port->event_buffer || events_port->event_buffer->capacity() != min_event_buffer_size) {
                    node->data_ready = false;

                    destroy(events_port->event_buffer, 1);
                    events_port->event_buffer = create_zero<RingBuffer>(min_event_buffer_size);

                    if (!events_port->event_buffer) {
                        genesis_stop_pipeline(context);
                        return GenesisErrorNoMem;
                    }
                }
            }
        }
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

const struct GenesisNodeDescriptor *genesis_node_descriptor(const struct GenesisNode *node) {
    return node->descriptor;
}

float genesis_midi_note_to_pitch(int note) {
    return midi_note_to_pitch[note];
}

int genesis_audio_in_port_fill_count(GenesisPort *port) {
    struct GenesisAudioPort *audio_in_port = (struct GenesisAudioPort *) port;
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) audio_in_port->port.input_from;
    return audio_out_port->sample_buffer->fill_count();
}

char *genesis_audio_in_port_read_ptr(GenesisPort *port) {
    struct GenesisAudioPort *audio_in_port = (struct GenesisAudioPort *) port;
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) audio_in_port->port.input_from;
    return audio_out_port->sample_buffer->read_ptr();
}

void genesis_audio_in_port_advance_read_ptr(GenesisPort *port, int byte_count) {
    struct GenesisAudioPort *audio_in_port = (struct GenesisAudioPort *) port;
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) audio_in_port->port.input_from;
    audio_out_port->sample_buffer->advance_read_ptr(byte_count);
    audio_out_port->port.node->data_ready = false;
}

int genesis_audio_out_port_free_count(GenesisPort *port) {
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) port;
    return audio_out_port->sample_buffer_size - audio_out_port->sample_buffer->fill_count();
}

char *genesis_audio_out_port_write_ptr(GenesisPort *port) {
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) port;
    return audio_out_port->sample_buffer->write_ptr();
}

void genesis_audio_out_port_advance_write_ptr(GenesisPort *port, int byte_count) {
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) port;
    audio_out_port->sample_buffer->advance_write_ptr(byte_count);
    port->node->data_ready = true;
    GenesisAudioPort *audio_in_port = (GenesisAudioPort *)audio_out_port->port.output_to;
    GenesisNode *other_node = audio_in_port->port.node;
    GenesisContext *context = port->node->descriptor->context;
    queue_node_if_ready(context, other_node, false);
}

int genesis_audio_port_bytes_per_frame(struct GenesisPort *port) {
    struct GenesisAudioPort *audio_port = (struct GenesisAudioPort *)port;
    return audio_port->bytes_per_frame;
}

int genesis_audio_port_sample_rate(struct GenesisPort *port) {
    struct GenesisAudioPort *audio_port = (struct GenesisAudioPort *)port;
    return audio_port->sample_rate;
}

const GenesisChannelLayout *genesis_audio_port_channel_layout(struct GenesisPort *port) {
    struct GenesisAudioPort *audio_port = (struct GenesisAudioPort *)port;
    return &audio_port->channel_layout;
}

void genesis_node_descriptor_set_run_callback(struct GenesisNodeDescriptor *node_descriptor,
        void (*run)(struct GenesisNode *node))
{
    node_descriptor->run = run;
}

void genesis_node_descriptor_set_seek_callback(struct GenesisNodeDescriptor *node_descriptor,
        void (*seek)(struct GenesisNode *node))
{
    node_descriptor->seek = seek;
}

void genesis_node_descriptor_set_create_callback(struct GenesisNodeDescriptor *node_descriptor,
        int (*create)(struct GenesisNode *node))
{
    node_descriptor->create = create;
}

void genesis_node_descriptor_set_destroy_callback(struct GenesisNodeDescriptor *node_descriptor,
        void (*destroy)(struct GenesisNode *node))
{
    node_descriptor->destroy = destroy;
}

void genesis_node_descriptor_set_userdata(struct GenesisNodeDescriptor *node_descriptor, void *userdata) {
    node_descriptor->userdata = userdata;
}

int genesis_audio_port_descriptor_set_channel_layout(
        struct GenesisPortDescriptor *port_descr,
        const GenesisChannelLayout *channel_layout, bool fixed, int other_port_index)
{
    if (port_descr->port_type != GenesisPortTypeAudioIn &&
        port_descr->port_type != GenesisPortTypeAudioOut)
    {
        return GenesisErrorInvalidPortType;
    }

    GenesisAudioPortDescriptor *audio_port_descr = (GenesisAudioPortDescriptor *)port_descr;

    audio_port_descr->channel_layout = *channel_layout;
    audio_port_descr->channel_layout_fixed = fixed;
    audio_port_descr->same_channel_layout_index = other_port_index;

    return 0;
}

int genesis_audio_port_descriptor_set_sample_rate(
        struct GenesisPortDescriptor *port_descr,
        int sample_rate, bool fixed, int other_port_index)
{
    if (port_descr->port_type != GenesisPortTypeAudioIn &&
        port_descr->port_type != GenesisPortTypeAudioOut)
    {
        return GenesisErrorInvalidPortType;
    }

    GenesisAudioPortDescriptor *audio_port_descr = (GenesisAudioPortDescriptor *)port_descr;

    audio_port_descr->sample_rate = sample_rate;
    audio_port_descr->sample_rate_fixed = fixed;
    audio_port_descr->same_sample_rate_index = other_port_index;

    return 0;
}

int genesis_connect_audio_nodes(struct GenesisNode *source, struct GenesisNode *dest) {
    int audio_out_port_index = genesis_node_descriptor_find_port_index(source->descriptor, "audio_out");
    if (audio_out_port_index < 0)
        return GenesisErrorPortNotFound;

    int audio_in_port_index = genesis_node_descriptor_find_port_index(dest->descriptor, "audio_in");
    if (audio_in_port_index < 0)
        return GenesisErrorPortNotFound;

    struct GenesisPort *audio_out_port = genesis_node_port(source, audio_out_port_index);
    struct GenesisPort *audio_in_port = genesis_node_port(dest, audio_in_port_index);

    return genesis_connect_ports(audio_out_port, audio_in_port);
}

void genesis_port_descriptor_set_connect_callback(
        struct GenesisPortDescriptor *port_descr,
        int (*connect)(struct GenesisPort *port, struct GenesisPort *other_port))
{
    port_descr->connect = connect;
}

void genesis_port_descriptor_set_disconnect_callback(
        struct GenesisPortDescriptor *port_descr,
        void (*disconnect)(struct GenesisPort *port, struct GenesisPort *other_port))
{
    port_descr->disconnect = disconnect;
}

void *genesis_node_descriptor_userdata(const struct GenesisNodeDescriptor *node_descriptor) {
    return node_descriptor->userdata;
}
