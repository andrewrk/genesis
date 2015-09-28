#include "genesis.hpp"
#include "audio_file.hpp"
#include "midi_note_pitch.hpp"
#include "synth.hpp"
#include "delay.hpp"
#include "resample.hpp"
#include "config.h"

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
    SoundIoOutStream *outstream;
    void (*write_sample)(char *ptr, float sample);
    atomic_bool ongoing_recovery;
    bool stream_started;
};

struct RecordingNodeContext {
    SoundIoInStream *instream;
    void (*read_sample)(char *ptr, float *sample);
};

struct SampleFormatInfo {
    SoundIoFormat format;
    void (*write_sample)(char *ptr, float sample);
    void (*read_sample)(char *ptr, float *sample);
};

template<int byte_count>
static void swap_endian(char *ptr) {
    for (int i = 0; i < byte_count / 2; i += 1) {
        char value = ptr[i];
        ptr[i] = ptr[byte_count - i - 1];
        ptr[byte_count - i - 1] = value;
    }
}

static void write_sample_s16ne(char *ptr, float sample) {
    int16_t *buf = (int16_t *)ptr;
    float range = (float)INT16_MAX - (float)INT16_MIN;
    float val = sample * range / 2.0f;
    *buf = val;
}

static void read_sample_s16ne(char *ptr, float *sample) {
    int16_t *buf = (int16_t *)ptr;
    float range = (float)INT16_MAX - (float)INT16_MIN;
    *sample = (float)(*buf) / range * 2.0f;
}

static void write_sample_s16fe(char *ptr, float sample) {
    write_sample_s16ne(ptr, sample);
    swap_endian<2>(ptr);
}

static void read_sample_s16fe(char *ptr, float *sample) {
    read_sample_s16ne(ptr, sample);
    swap_endian<2>(ptr);
}

static void write_sample_u16ne(char *ptr, float sample) {
    uint32_t *buf = (uint32_t *)ptr;
    float val = sample * (1.0f + (float)UINT16_MAX);
    *buf = val;
}

static void read_sample_u16ne(char *ptr, float *sample) {
    uint16_t *buf = (uint16_t *)ptr;
    *sample = (float)(*buf) / (1.0f + (float)UINT16_MAX);
}

static void write_sample_u16fe(char *ptr, float sample) {
    write_sample_u16ne(ptr, sample);
    swap_endian<2>(ptr);
}

static void read_sample_u16fe(char *ptr, float *sample) {
    read_sample_u16ne(ptr, sample);
    swap_endian<2>(ptr);
}

static void write_sample_s32ne(char *ptr, float sample) {
    int32_t *buf = (int32_t *)ptr;
    float range = (float)INT32_MAX - (float)INT32_MIN;
    float val = sample * range / 2.0;
    *buf = val;
}

static void read_sample_s32ne(char *ptr, float *sample) {
    int32_t *buf = (int32_t *)ptr;
    float range = (float)INT32_MAX - (float)INT32_MIN;
    *sample = (float)(*buf) / range * 2.0f;
}

static void write_sample_s32fe(char *ptr, float sample) {
    write_sample_s32ne(ptr, sample);
    swap_endian<4>(ptr);
}

static void read_sample_s32fe(char *ptr, float *sample) {
    read_sample_s32ne(ptr, sample);
    swap_endian<4>(ptr);
}

static void write_sample_float32ne(char *ptr, float sample) {
    float *buf = (float *)ptr;
    *buf = sample;
}

static void read_sample_float32ne(char *ptr, float *sample) {
    float *buf = (float *)ptr;
    *sample = *buf;
}

static void write_sample_float32fe(char *ptr, float sample) {
    write_sample_float32ne(ptr, sample);
    swap_endian<4>(ptr);
}

static void read_sample_float32fe(char *ptr, float *sample) {
    read_sample_float32ne(ptr, sample);
    swap_endian<4>((char*)sample);
}

static void write_sample_float64ne(char *ptr, float sample) {
    float *buf = (float *)ptr;
    *buf = sample;
}

static void read_sample_float64ne(char *ptr, float *sample) {
    double *buf = (double *)ptr;
    *sample = *buf;
}

static void write_sample_float64fe(char *ptr, float sample) {
    write_sample_float64ne(ptr, sample);
    swap_endian<8>(ptr);
}

static void read_sample_float64fe(char *ptr, float *sample) {
    read_sample_float64ne(ptr, sample);
    swap_endian<8>((char*)sample);
}

static void write_sample_u32ne(char *ptr, float sample) {
    uint32_t *buf = (uint32_t *)ptr;
    float val = sample * (1.0f + (float)UINT32_MAX);
    *buf = val;
}

static void read_sample_u32ne(char *ptr, float *sample) {
    uint32_t *buf = (uint32_t *)ptr;
    *sample = (float)(*buf) / (1.0f + (float)UINT32_MAX);
}

static void write_sample_u32fe(char *ptr, float sample) {
    write_sample_u32ne(ptr, sample);
    swap_endian<4>(ptr);
}

static void read_sample_u32fe(char *ptr, float *sample) {
    read_sample_u32ne(ptr, sample);
    swap_endian<4>(ptr);
}

static void write_sample_s24ne(char *ptr, float sample) {
    int32_t *buf = (int32_t *)ptr;
    float range = 16777216.0f;
    float val = sample * range / 2.0;
    *buf = val;
}

static void read_sample_s24ne(char *ptr, float *sample) {
    int32_t *buf = (int32_t *)ptr;
    float range = 16777216.0f;
    *sample = (float)(*buf) / range * 2.0f;
}

static void write_sample_s24fe(char *ptr, float sample) {
    write_sample_s24ne(ptr, sample);
    swap_endian<4>(ptr);
}

static void read_sample_s24fe(char *ptr, float *sample) {
    read_sample_s24ne(ptr, sample);
    swap_endian<4>(ptr);
}

static void write_sample_u24ne(char *ptr, float sample) {
    uint32_t *buf = (uint32_t *)ptr;
    float val = sample * 16777217.0f;
    *buf = val;
}

static void read_sample_u24ne(char *ptr, float *sample) {
    uint32_t *buf = (uint32_t *)ptr;
    *sample = (float)(*buf) / 16777217.0f;
}

static void write_sample_u24fe(char *ptr, float sample) {
    write_sample_u24ne(ptr, sample);
    swap_endian<4>(ptr);
}

static void read_sample_u24fe(char *ptr, float *sample) {
    read_sample_u24ne(ptr, sample);
    swap_endian<4>(ptr);
}

static void write_sample_s8(char *ptr, float sample) {
    int8_t *buf = (int8_t *)ptr;
    float range = (float)INT8_MAX - (float)INT8_MIN;
    float val = sample * range / 2.0;
    *buf = val;
}

static void read_sample_s8(char *ptr, float *sample) {
    int8_t *buf = (int8_t *)ptr;
    float range = (float)INT8_MAX - (float)INT8_MIN;
    *sample = (float)(*buf) / range * 2.0f;
}

static void write_sample_u8(char *ptr, float sample) {
    uint8_t *buf = (uint8_t *)ptr;
    float val = sample * (1.0f + (float)UINT8_MAX);
    *buf = val;
}

static void read_sample_u8(char *ptr, float *sample) {
    uint8_t *buf = (uint8_t *)ptr;
    *sample = (float)(*buf) / (1.0f + (float)UINT8_MAX);
}

static SampleFormatInfo prioritized_sample_format_infos[] = {
    {
        SoundIoFormatFloat32NE,
        write_sample_float32ne,
        read_sample_float32ne
    },
    {
        SoundIoFormatFloat32FE,
        write_sample_float32fe,
        read_sample_float32fe
    },
    {
        SoundIoFormatFloat64NE,
        write_sample_float64ne,
        read_sample_float64ne,
    },
    {
        SoundIoFormatFloat64FE,
        write_sample_float64fe,
        read_sample_float64fe,
    },
    {
        SoundIoFormatS32NE,
        write_sample_s32ne,
        read_sample_s32ne,
    },
    {
        SoundIoFormatS32FE,
        write_sample_s32fe,
        read_sample_s32fe,
    },
    {
        SoundIoFormatU32NE,
        write_sample_u32ne,
        read_sample_u32ne,
    },
    {
        SoundIoFormatU32FE,
        write_sample_u32fe,
        read_sample_u32fe,
    },
    {
        SoundIoFormatS24NE,
        write_sample_s24ne,
        read_sample_s24ne,
    },
    {
        SoundIoFormatS24FE,
        write_sample_s24fe,
        read_sample_s24fe,
    },
    {
        SoundIoFormatU24NE,
        write_sample_u24ne,
        read_sample_u24ne,
    },
    {
        SoundIoFormatU24FE,
        write_sample_u24fe,
        read_sample_u24fe,
    },
    {
        SoundIoFormatS16NE,
        write_sample_s16ne,
        read_sample_s16ne,
    },
    {
        SoundIoFormatS16FE,
        write_sample_s16fe,
        read_sample_s16fe,
    },
    {
        SoundIoFormatU16NE,
        write_sample_u16ne,
        read_sample_u16ne,
    },
    {
        SoundIoFormatU16FE,
        write_sample_u16fe,
        read_sample_u16fe,
    },
    {
        SoundIoFormatS8,
        write_sample_s8,
        read_sample_s8,
    },
    {
        SoundIoFormatU8,
        write_sample_u8,
        read_sample_u8,
    },
};

static enum SoundIoChannelLayoutId prioritized_layouts[] = {
    SoundIoChannelLayoutIdOctagonal,
    SoundIoChannelLayoutId7Point1WideBack,
    SoundIoChannelLayoutId7Point1Wide,
    SoundIoChannelLayoutId7Point1,
    SoundIoChannelLayoutId7Point0Front,
    SoundIoChannelLayoutId7Point0,
    SoundIoChannelLayoutId6Point1Front,
    SoundIoChannelLayoutId6Point1Back,
    SoundIoChannelLayoutId6Point1,
    SoundIoChannelLayoutIdHexagonal,
    SoundIoChannelLayoutId6Point0Front,
    SoundIoChannelLayoutId6Point0Side,
    SoundIoChannelLayoutId5Point1Back,
    SoundIoChannelLayoutId5Point0Back,
    SoundIoChannelLayoutId5Point1,
    SoundIoChannelLayoutId5Point0Side,
    SoundIoChannelLayoutId4Point1,
    SoundIoChannelLayoutIdQuad,
    SoundIoChannelLayoutId4Point0,
    SoundIoChannelLayoutId3Point1,
    SoundIoChannelLayoutId2Point1,
    SoundIoChannelLayoutIdStereo,
    SoundIoChannelLayoutIdMono,
};

static void emit_event_ready(struct GenesisContext *context) {
    if (context->event_callback)
        context->event_callback(context->event_callback_userdata);
    genesis_wakeup(context);
}

static void on_midi_devices_change(MidiHardware *midi_hardware) {
    GenesisContext *context = reinterpret_cast<GenesisContext *>(midi_hardware->userdata);
    if (context->midi_change_callback)
        context->midi_change_callback(context->midi_change_callback_userdata);
}

static void midi_events_signal(MidiHardware *midi_hardware) {
    GenesisContext *context = reinterpret_cast<GenesisContext *>(midi_hardware->userdata);
    emit_event_ready(context);
}

static void on_devices_change(SoundIo *soundio) {
    GenesisContext *context = reinterpret_cast<GenesisContext *>(soundio->userdata);
    if (context->devices_change_callback)
        context->devices_change_callback(context->devices_change_callback_userdata);
}

static void on_audio_hardware_events_signal(SoundIo *soundio) {
    GenesisContext *context = reinterpret_cast<GenesisContext *>(soundio->userdata);
    emit_event_ready(context);
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

static void on_backend_disconnect(struct SoundIo *soundio, int err) {
    GenesisContext *context = (GenesisContext *)soundio->userdata;
    context->soundio_connect_err = err;
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
    context->latency = 0.020; // 20ms
    context->target_sample_rate = 48000;

    if (!(context->events_cond = os_cond_create())) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }

    if (!(context->events_mutex = os_mutex_create())) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }

    int concurrency = os_concurrency();
    // subtract one to make room for GUI thread, OS, and other miscellaneous
    // interruptions.
    context->thread_pool_size = max(1, concurrency - 1);
    context->thread_pool = allocate_zero<OsThread *>(context->thread_pool_size);
    if (!context->thread_pool) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }

    int err = create_midi_hardware(context, "genesis", midi_events_signal, on_midi_devices_change,
            context, &context->midi_hardware);
    if (err) {
        genesis_destroy_context(context);
        return err;
    }

    context->soundio = soundio_create();
    if (!context->soundio) {
        genesis_destroy_context(context);
        return GenesisErrorNoMem;
    }
    context->soundio->userdata = context;
    context->soundio->on_devices_change = on_devices_change;
    context->soundio->on_events_signal = on_audio_hardware_events_signal;
    context->soundio->on_backend_disconnect = on_backend_disconnect;
    context->soundio->app_name = "Genesis";

    if ((err = soundio_connect(context->soundio))) {
        genesis_destroy_context(context);
        return GenesisErrorOpeningAudioHardware;
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

    for (int i = 0; i < array_length(plugin_create_list); i += 1) {
        int (*create_fn)(GenesisContext *) = plugin_create_list[i];
        int err = create_fn(context);
        if (err) {
            genesis_destroy_context(context);
            return err;
        }
    }

    context->underrun_flag.test_and_set();

    *out_context = context;
    return 0;
}

void genesis_destroy_context(struct GenesisContext *context) {
    if (!context)
        return;

    genesis_stop_pipeline(context);

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

    if (context->midi_hardware)
        destroy_midi_hardware(context->midi_hardware);

    if (context->soundio)
        soundio_destroy(context->soundio);

    destroy(context, 1);
}

void genesis_flush_events(struct GenesisContext *context) {
    soundio_flush_events(context->soundio);
    if (context->soundio_connect_err) {
        context->soundio_connect_err = soundio_connect(context->soundio);
    }
    midi_hardware_flush_events(context->midi_hardware);
    if (!context->underrun_flag.test_and_set()) {
        if (context->underrun_callback)
            context->underrun_callback(context->underrun_callback_userdata);
    }
}

void genesis_wait_events(struct GenesisContext *context) {
    genesis_flush_events(context);
    os_mutex_lock(context->events_mutex);
    os_cond_wait(context->events_cond, context->events_mutex);
    os_mutex_unlock(context->events_mutex);
}

void genesis_wakeup(struct GenesisContext *context) {
    os_mutex_lock(context->events_mutex);
    os_cond_signal(context->events_cond, context->events_mutex);
    os_mutex_unlock(context->events_mutex);
}

void genesis_set_event_callback(struct GenesisContext *context,
        void (*callback)(void *userdata), void *userdata)
{
    context->event_callback_userdata = userdata;
    context->event_callback = callback;
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
            {
                GenesisAudioPort *audio_port = create_zero<GenesisAudioPort>();
                if (!audio_port)
                    return nullptr;
                audio_port->sample_buffer_err = GenesisErrorInvalidState;
                port = (GenesisPort*)audio_port;
                break;
            }
        case GenesisPortTypeEventsIn:
        case GenesisPortTypeEventsOut:
            {
                GenesisEventsPort *events_port = create_zero<GenesisEventsPort>();
                if (!events_port)
                    return nullptr;
                events_port->event_buffer_err = GenesisErrorInvalidState;
                port = (GenesisPort*)events_port;
                break;
            }

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
    if (!audio_port->sample_buffer_err)
        ring_buffer_deinit(&audio_port->sample_buffer);
    destroy(audio_port, 1);
}

static void destroy_events_port(GenesisEventsPort *events_port) {
    if (!events_port->event_buffer_err)
        ring_buffer_deinit(&events_port->event_buffer);
    destroy(events_port, 1);
}

static void destroy_port(struct GenesisPort *port) {
    if (!port)
        return;
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

void genesis_node_disconnect_all_ports(struct GenesisNode *node) {
    assert(node);
    for (int i = 0; i < node->port_count; i += 1) {
        GenesisPort *port = node->ports[i];
        if (port) {
            if (port->output_to)
                genesis_disconnect_ports(port, port->output_to);
            if (port->input_from)
                genesis_disconnect_ports(port->input_from, port);
        }
    }
}

void genesis_node_destroy(struct GenesisNode *node) {
    if (!node)
        return;

    GenesisContext *context = node->descriptor->context;

    // first all disconnect methods on all ports
    if (node->ports) {
        genesis_node_disconnect_all_ports(node);
    }

    // call destructor on node
    if (node->constructed && node->descriptor->destroy)
        node->descriptor->destroy(node);

    if (node->set_index >= 0) {
        context->nodes.swap_remove(node->set_index);
        if (node->set_index < context->nodes.length())
            context->nodes.at(node->set_index)->set_index = node->set_index;
        node->set_index = -1;
    }

    if (node->ports) {
        for (int i = 0; i < node->port_count; i += 1) {
            GenesisPort *port = node->ports[i];
            destroy_port(port);
        }
        destroy(node->ports, node->port_count);
    }

    destroy(node, 1);
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

enum SoundIoFormat genesis_audio_file_codec_sample_format_index(
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

static void get_audio_port_status(GenesisAudioPort *audio_out_port, bool *empty, bool *full) {
    int fill_count = ring_buffer_fill_count(&audio_out_port->sample_buffer);
    *empty = (fill_count == 0);
    *full = (fill_count == audio_out_port->sample_buffer_size);
}

static void get_events_port_status(GenesisEventsPort *events_out_port, bool *empty, bool *full) {
    *full = (events_out_port->time_requested.load() == 0.0);
    *empty = (*full) ? false : (events_out_port->time_available.load() == 0.0);
}

static void get_port_status(GenesisPort *port, bool *empty, bool *full) {
    switch (port->descriptor->port_type) {
        case GenesisPortTypeAudioOut:
            get_audio_port_status((GenesisAudioPort *)port, empty, full);
            return;
        case GenesisPortTypeEventsOut:
            get_events_port_status((GenesisEventsPort *)port, empty, full);
            return;
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeEventsIn:
            panic("expected out port");
    }
    panic("invalid port type");
}

static void run_node(GenesisNode *node) {
    const GenesisNodeDescriptor *node_descriptor = node->descriptor;
    //fprintf(stderr, "run %s\n", node_descriptor->name);
    node_descriptor->run(node);
    node->being_processed = false;
}

static void queue_node_if_ready(GenesisContext *context, GenesisNode *node, bool recursive) {
    if (node->being_processed) {
        // this node is already being processed; no point in queueing it again
        return;
    }
    if (!node->descriptor->run) {
        // this node has no run function; no point in queuing it
        return;
    }
    // make sure all the children of this node are ready
    bool waiting_for_any_children = false;
    bool any_output_has_room = false;
    for (int parent_port_i = 0; parent_port_i < node->port_count; parent_port_i += 1) {
        GenesisPort *port = node->ports[parent_port_i];

        if (port->output_to) {
            bool empty, full;
            get_port_status(port, &empty, &full);
            if (!full)
                any_output_has_room = true;
        } else {
            GenesisPort *child_port = port->input_from;
            if (child_port && child_port != port) {
                bool empty, full;
                get_port_status(child_port, &empty, &full);
                waiting_for_any_children = waiting_for_any_children || empty;
                if (!full) {
                    GenesisNode *child_node = child_port->node;
                    if (recursive)
                        queue_node_if_ready(context, child_node, true);
                }
            }
        }
    }
    if (!waiting_for_any_children && any_output_has_room) {
        // we know that we want it enqueued. now make sure it only happens once.
        if (!node->being_processed.exchange(true))
            context->task_queue.enqueue(node);
    }
}

static void playback_node_destroy(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    if (playback_node_context) {
        soundio_outstream_destroy(playback_node_context->outstream);
        destroy(playback_node_context, 1);
    }
}

static void playback_node_error_callback(SoundIoOutStream *outstream, int err) {
    GenesisNode *node = (GenesisNode *)outstream->userdata;
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;

    playback_node_context->ongoing_recovery.store(true);
}

static void playback_node_fill_silence(SoundIoOutStream *outstream, int frame_count_min) {
    struct SoundIoChannelArea *areas;
    int channel_count = outstream->layout.channel_count;
    int err;
    int frames_left = frame_count_min;
    while (frames_left > 0) {
        int frame_count = frames_left;
        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
            playback_node_error_callback(outstream, err);
            return;
        }

        if (!frame_count)
            break;

        for (int frame = 0; frame < frame_count; frame += 1) {
            for (int channel = 0; channel < channel_count; channel += 1) {
                memset(areas[channel].ptr, 0, outstream->bytes_per_sample);
                areas[channel].ptr += areas[channel].step;
            }
        }

        if ((err = soundio_outstream_end_write(outstream))) {
            playback_node_error_callback(outstream, err);
            return;
        }

        frames_left -= frame_count;
    }
}

static void playback_node_callback(SoundIoOutStream *outstream, int frame_count_min, int frame_count_max) {
    GenesisNode *node = (GenesisNode *)outstream->userdata;
    GenesisContext *context = node->descriptor->context;
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    struct SoundIoChannelArea *areas;
    int err;

    assert(context->pipeline_running);

    if (playback_node_context->ongoing_recovery.load()) {
        playback_node_fill_silence(outstream, frame_count_min);
        return;
    }


    GenesisPort *audio_in_port = genesis_node_port(node, 0);
    int input_frame_count = genesis_audio_in_port_fill_count(audio_in_port);
    float *in_buf = genesis_audio_in_port_read_ptr(audio_in_port);
    const struct SoundIoChannelLayout *layout = &outstream->layout;

    if (frame_count_max > input_frame_count) {
        soundio_outstream_pause(playback_node_context->outstream, 1);
        playback_node_context->ongoing_recovery.store(true);
        playback_node_fill_silence(outstream, frame_count_min);
        return;
    }

    int frames_left = frame_count_max;
    while (frames_left > 0) {
        int frame_count = frames_left;
        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
            playback_node_error_callback(outstream, err);
            return;
        }

        if (!frame_count)
            break;

        for (int frame = 0; frame < frame_count; frame += 1) {
            for (int channel = 0; channel < layout->channel_count; channel += 1) {
                playback_node_context->write_sample(areas[channel].ptr, *in_buf);
                memcpy(areas[channel].ptr, in_buf, outstream->bytes_per_sample);
                areas[channel].ptr += areas[channel].step;
                in_buf += 1;
            }
        }

        if ((err = soundio_outstream_end_write(outstream))) {
            playback_node_error_callback(outstream, err);
            return;
        }

        frames_left -= frame_count;
    }

    genesis_audio_in_port_advance_read_ptr(audio_in_port, frame_count_max);
}

static void playback_node_underrun_callback(SoundIoOutStream *outstream) {
    playback_node_error_callback(outstream, SoundIoErrorUnderflow);
}

static void playback_node_deactivate(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    soundio_outstream_destroy(playback_node_context->outstream);
    playback_node_context->outstream = nullptr;
}

static int playback_choose_best_format(PlaybackNodeContext *playback_node_context, SoundIoDevice *device) {
    for (int i = 0; i < array_length(prioritized_sample_format_infos); i += 1) {
        struct SampleFormatInfo *sample_format_info = &prioritized_sample_format_infos[i];
        if (soundio_device_supports_format(device, sample_format_info->format)) {
            playback_node_context->write_sample = sample_format_info->write_sample;
            playback_node_context->outstream->format = sample_format_info->format;
            return 0;
        }
    }
    return GenesisErrorIncompatibleDevice;
}

static int playback_node_activate(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    GenesisContext *context = node->descriptor->context;
    SoundIoDevice *device = (SoundIoDevice*)node->descriptor->userdata;

    playback_node_context->ongoing_recovery.store(false);

    assert(!playback_node_context->outstream);
    if (!(playback_node_context->outstream = soundio_outstream_create(device))) {
        playback_node_destroy(node);
        return GenesisErrorNoMem;
    }

    int err;
    if ((err = playback_choose_best_format(playback_node_context, device))) {
        playback_node_destroy(node);
        return err;
    }

    GenesisAudioPort *audio_port = (GenesisAudioPort*)node->ports[0];


    playback_node_context->outstream->name = "Genesis";
    playback_node_context->outstream->userdata = playback_node_context;
    playback_node_context->outstream->underflow_callback = playback_node_underrun_callback;
    playback_node_context->outstream->error_callback = playback_node_error_callback;
    playback_node_context->outstream->write_callback = playback_node_callback;
    playback_node_context->outstream->sample_rate = audio_port->sample_rate;
    playback_node_context->outstream->layout = audio_port->channel_layout;
    // Spend 1/4 of the latency on the device buffer and 3/4 of the latency in ring buffers for
    // nodes in the audio pipeline.
    playback_node_context->outstream->software_latency = context->latency * 0.25;

    if ((err = soundio_outstream_open(playback_node_context->outstream))) {
        playback_node_destroy(node);
        return GenesisErrorOpeningAudioHardware;
    }

    return 0;
}

static void playback_node_run(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;

    if (playback_node_context->ongoing_recovery.load()) {
        struct GenesisPort *audio_in_port = genesis_node_port(node, 0);
        int input_frame_count = genesis_audio_in_port_fill_count(audio_in_port);
        int input_capacity = genesis_audio_in_port_capacity(audio_in_port);

        if (input_frame_count == input_capacity) {
            if (!playback_node_context->stream_started) {
                soundio_outstream_start(playback_node_context->outstream);
                playback_node_context->stream_started = true;
            } else {
                soundio_outstream_pause(playback_node_context->outstream, 0);
            }
            playback_node_context->ongoing_recovery.store(false);
        }
    }
}

static int playback_node_create(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = create_zero<PlaybackNodeContext>();
    if (!playback_node_context) {
        playback_node_destroy(node);
        return GenesisErrorNoMem;
    }
    node->userdata = playback_node_context;

    return 0;
}

static void playback_node_seek(struct GenesisNode *node) {
    PlaybackNodeContext *playback_node_context = (PlaybackNodeContext*)node->userdata;
    playback_node_context->ongoing_recovery.store(true);
    if (playback_node_context->outstream) {
        soundio_outstream_pause(playback_node_context->outstream, 1);
        soundio_outstream_clear_buffer(playback_node_context->outstream);
    }
}

static void destroy_audio_device_node_descriptor(struct GenesisNodeDescriptor *node_descriptor) {
    SoundIoDevice *audio_device = (SoundIoDevice*)node_descriptor->userdata;
    soundio_device_unref(audio_device);
}

static void recording_node_error_callback(SoundIoInStream *instream, int err) {
    panic("TODO recording_node_error_callback");
}

static void recording_node_overflow_callback(SoundIoInStream *instream) {
    recording_node_error_callback(instream, SoundIoErrorUnderflow);
}

static void recording_node_callback(SoundIoInStream *instream, int frame_count_min, int frame_count_max) {
    GenesisNode *node = (GenesisNode *)instream->userdata;
    GenesisContext *context = node->descriptor->context;
    RecordingNodeContext *recording_node_context = (RecordingNodeContext *)node->userdata;
    struct SoundIoChannelArea *areas;
    int err;

    assert(context->pipeline_running);

    GenesisPort *audio_out_port = genesis_node_port(node, 0);
    int output_frame_count = genesis_audio_out_port_free_count(audio_out_port);
    float *out_buf = genesis_audio_out_port_write_ptr(audio_out_port);

    int write_frames = min(output_frame_count, frame_count_max);
    int read_frames = clamp(frame_count_min, output_frame_count, frame_count_max);
    int read_frames_left = read_frames;
    int write_frames_left = write_frames;

    if (read_frames > write_frames) {
        panic("TODO handle data dropped; read_frames > write_frames");
    }

    while (read_frames_left > 0) {
        int read_frame_count = read_frames_left;
        if ((err = soundio_instream_begin_read(instream, &areas, &read_frame_count))) {
            recording_node_error_callback(instream, err);
            return;
        }

        if (!read_frame_count)
            break;

        int write_frame_count = min(read_frame_count, write_frames_left);
        if (!areas) {
            panic("TODO handle data dropped; hole");
        } else {
            for (int frame = 0; frame < write_frame_count; frame += 1) {
                for (int ch = 0; ch < instream->layout.channel_count; ch += 1) {
                    recording_node_context->read_sample(areas[ch].ptr, out_buf);
                    areas[ch].ptr += areas[ch].step;
                    out_buf += 1;
                }
            }
        }

        if ((err = soundio_instream_end_read(instream))) {
            recording_node_error_callback(instream, err);
            return;
        }

        read_frames_left -= read_frame_count;
        write_frames_left -= write_frame_count;
    }

    genesis_audio_out_port_advance_write_ptr(audio_out_port, write_frames);
}

static void recording_node_seek(struct GenesisNode *node) {
    //RecordingNodeContext *recording_node_context = (RecordingNodeContext*)node->userdata;
    panic("TODO recording_node_seek");
}

static void recording_node_destroy(struct GenesisNode *node) {
    RecordingNodeContext *recording_node_context = (RecordingNodeContext*)node->userdata;
    if (recording_node_context) {
        soundio_instream_destroy(recording_node_context->instream);
        destroy(recording_node_context, 1);
    }
}

static void recording_node_deactivate(struct GenesisNode *node) {
    RecordingNodeContext *recording_node_context = (RecordingNodeContext*)node->userdata;
    soundio_instream_destroy(recording_node_context->instream);
    recording_node_context->instream = nullptr;
}

static int recording_node_activate(struct GenesisNode *node) {
    //RecordingNodeContext *recording_node_context = (RecordingNodeContext*)node->userdata;
    panic("TODO recording_node_activate");
}

static int recording_choose_best_format(RecordingNodeContext *recording_node_context, SoundIoDevice *device) {
    for (int i = 0; i < array_length(prioritized_sample_format_infos); i += 1) {
        struct SampleFormatInfo *sample_format_info = &prioritized_sample_format_infos[i];
        if (soundio_device_supports_format(device, sample_format_info->format)) {
            recording_node_context->read_sample = sample_format_info->read_sample;
            recording_node_context->instream->format = sample_format_info->format;
            return 0;
        }
    }
    return GenesisErrorIncompatibleDevice;
}

static int recording_node_create(struct GenesisNode *node) {
    struct GenesisContext *context = node->descriptor->context;
    RecordingNodeContext *recording_node_context = create_zero<RecordingNodeContext>();
    if (!recording_node_context) {
        recording_node_destroy(node);
        return GenesisErrorNoMem;
    }
    node->userdata = recording_node_context;

    SoundIoDevice *device = (SoundIoDevice*)node->descriptor->userdata;

    if (!(recording_node_context->instream = soundio_instream_create(device))) {
        recording_node_destroy(node);
        return GenesisErrorNoMem;
    }

    int err;
    if ((err = recording_choose_best_format(recording_node_context, device))) {
        recording_node_destroy(node);
        return err;
    }

    GenesisAudioPort *audio_port = (GenesisAudioPort *)node->ports[0];

    recording_node_context->instream->name = "Genesis";
    recording_node_context->instream->userdata = recording_node_context;
    recording_node_context->instream->read_callback = recording_node_callback;
    recording_node_context->instream->error_callback = recording_node_error_callback;
    recording_node_context->instream->overflow_callback = recording_node_overflow_callback;
    recording_node_context->instream->sample_rate = audio_port->sample_rate;
    recording_node_context->instream->layout = audio_port->channel_layout;
    // Spend 1/4 of the latency on the device buffer and 3/4 of the latency in ring buffers for
    // nodes in the audio pipeline.
    recording_node_context->instream->software_latency = context->latency * 0.25;

    if ((err = soundio_instream_open(recording_node_context->instream))) {
        recording_node_destroy(node);
        return err;
    }

    return 0;
}

static void get_best_supported_layout(SoundIoDevice *device, SoundIoChannelLayout *out_layout) {
    for (int i = 0; i < array_length(prioritized_layouts); i += 1) {
        enum SoundIoChannelLayoutId layout_id = prioritized_layouts[i];
        const struct SoundIoChannelLayout *layout = soundio_channel_layout_get_builtin(layout_id);
        if (soundio_device_supports_layout(device, layout)) {
            *out_layout = *layout;
            return;
        }
    }

    *out_layout = device->layouts[0];
}

int genesis_audio_device_create_node_descriptor(struct GenesisContext *context,
        struct SoundIoDevice *audio_device,
        struct GenesisNodeDescriptor **out)
{

    *out = nullptr;

    const char *name_fmt_str = (audio_device->aim == SoundIoDeviceAimOutput) ?
        "playback-device-%s" : "recording-device-%s";
    const char *description_fmt_str = (audio_device->aim == SoundIoDeviceAimOutput) ?
        "Playback Device: %s" : "Recording Device: %s";
    char *name = create_formatted_str(name_fmt_str, audio_device->id);
    char *description = create_formatted_str(description_fmt_str, audio_device->name);
    if (!name || !description) {
        free(name);
        free(description);
        return GenesisErrorNoMem;
    }

    GenesisNodeDescriptor *node_descr = genesis_create_node_descriptor(context, 1, name, description);
    free(name); name = nullptr;
    free(description); description = nullptr;

    if (!node_descr) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    soundio_device_ref(audio_device);
    node_descr->userdata = audio_device;
    node_descr->destroy_descriptor = destroy_audio_device_node_descriptor;

    struct GenesisPortDescriptor *audio_port;
    if (audio_device->aim == SoundIoDeviceAimOutput) {
        audio_port = genesis_node_descriptor_create_port(node_descr, 0, GenesisPortTypeAudioIn, "audio_in");
    } else {
        audio_port = genesis_node_descriptor_create_port(node_descr, 0, GenesisPortTypeAudioOut, "audio_out");
    }
    if (!audio_port) {
        genesis_node_descriptor_destroy(node_descr);
        return GenesisErrorNoMem;
    }

    if (audio_device->aim == SoundIoDeviceAimOutput) {
        node_descr->activate = playback_node_activate;
        node_descr->deactivate = playback_node_deactivate;
        node_descr->run = playback_node_run;
        node_descr->create = playback_node_create;
        node_descr->destroy = playback_node_destroy;
        node_descr->seek = playback_node_seek;
    } else {
        node_descr->activate = recording_node_activate;
        node_descr->deactivate = recording_node_deactivate;
        node_descr->run = nullptr;
        node_descr->create = recording_node_create;
        node_descr->destroy = recording_node_destroy;
        node_descr->seek = recording_node_seek;
    }

    int chosen_sample_rate = soundio_device_nearest_sample_rate(audio_device, context->target_sample_rate);
    genesis_audio_port_descriptor_set_sample_rate(audio_port, chosen_sample_rate, true, -1);

    SoundIoChannelLayout layout;
    get_best_supported_layout(audio_device, &layout);
    genesis_audio_port_descriptor_set_channel_layout(audio_port, &layout, true, -1);

    *out = node_descr;
    return 0;
}

static void midi_node_on_event(struct GenesisMidiDevice *device, const struct GenesisMidiEvent *event) {
    GenesisNode *node = (GenesisNode *)device->userdata;
    GenesisPort *events_out_port = genesis_node_port(node, 0);

    int event_count;
    double time_requested;
    genesis_events_out_port_free_count(events_out_port, &event_count, &time_requested);

    assert(event_count >= 1); // TODO handle this error condition

    GenesisMidiEvent *out_ptr = genesis_events_out_port_write_ptr(events_out_port);
    memcpy(out_ptr, event, sizeof(GenesisMidiEvent));

    genesis_events_out_port_advance_write_ptr(events_out_port, 1, time_requested);
}

static void midi_node_run(struct GenesisNode *node) {
    GenesisPort *events_out_port = genesis_node_port(node, 0);
    int event_count;
    double time_requested;
    genesis_events_out_port_free_count(events_out_port, &event_count, &time_requested);
    genesis_events_out_port_advance_write_ptr(events_out_port, 0, time_requested);
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
    genesis_midi_device_unref(midi_device);
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
    genesis_midi_device_ref(midi_device);
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
        if (!soundio_channel_layout_equal(&source->channel_layout, &dest->channel_layout)) {
            return GenesisErrorIncompatibleChannelLayouts;
        }
    } else if (!source_audio_descr->channel_layout_fixed &&
               !dest_audio_descr->channel_layout_fixed)
    {
        // anything goes. default to mono
        source->channel_layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdMono);
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
    assert(port_index >= 0);
    assert(port_index < node->port_count);
    return node->ports[port_index];
}

struct GenesisContext *genesis_node_context(struct GenesisNode *node) {
    return node->descriptor->context;
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

static void genesis_debug_print_channel_layout(const struct SoundIoChannelLayout *layout) {
    if (layout->name) {
        fprintf(stderr, "%s\n", layout->name);
    } else {
        fprintf(stderr, "%s", soundio_get_channel_name(layout->channels[0]));
        for (int i = 1; i < layout->channel_count; i += 1) {
            fprintf(stderr, ", %s", soundio_get_channel_name(layout->channels[i]));
        }
        fprintf(stderr, "\n");
    }
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

        run_node(node);
    }
}

int genesis_start_pipeline(struct GenesisContext *context, double time) {
    for (int node_index = 0; node_index < context->nodes.length(); node_index += 1) {
        GenesisNode *node = context->nodes.at(node_index);
        for (int port_i = 0; port_i < node->port_count; port_i += 1) {
            GenesisPort *port = node->ports[port_i];
            if (port->descriptor->port_type == GenesisPortTypeAudioOut) {
                GenesisAudioPort *audio_port = reinterpret_cast<GenesisAudioPort*>(port);
                if (!audio_port->sample_buffer_err)
                    ring_buffer_clear(&audio_port->sample_buffer);
            } else if (port->descriptor->port_type == GenesisPortTypeEventsOut) {
                GenesisEventsPort *events_port = reinterpret_cast<GenesisEventsPort*>(port);
                if (!events_port->event_buffer_err)
                    ring_buffer_clear(&events_port->event_buffer);
            }
        }
        node->timestamp = time;
        if (node->descriptor->seek)
            node->descriptor->seek(node);
    }

    return genesis_resume_pipeline(context);
}

void genesis_stop_pipeline(struct GenesisContext *context) {
    context->pipeline_running = false;
    context->task_queue.wakeup_all();
    for (int i = 0; i < context->thread_pool_size; i += 1) {
        OsThread *thread = context->thread_pool[i];
        os_thread_destroy(thread);
        context->thread_pool[i] = nullptr;
    }
    for (int i = 0; i < context->nodes.length(); i += 1) {
        GenesisNode *node = context->nodes.at(i);
        if (node->descriptor->deactivate)
            node->descriptor->deactivate(node);
    }
}

int genesis_resume_pipeline(struct GenesisContext *context) {
    int err = context->task_queue.resize(context->nodes.length() + context->thread_pool_size);
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
                // the 0.75 is because the outstream software_latency is context->latency * 0.25
                int sample_buffer_frame_count = ceil(context->latency * audio_port->sample_rate * 0.75);
                audio_port->bytes_per_frame = BYTES_PER_SAMPLE * audio_port->channel_layout.channel_count;
                int new_sample_buffer_size = sample_buffer_frame_count * audio_port->bytes_per_frame;
                bool different = new_sample_buffer_size != audio_port->sample_buffer_size;
                audio_port->sample_buffer_size = new_sample_buffer_size;

                if (audio_port->sample_buffer_err || different) {
                    ring_buffer_deinit(&audio_port->sample_buffer);
                    if ((audio_port->sample_buffer_err =
                            ring_buffer_init(&audio_port->sample_buffer, audio_port->sample_buffer_size)))
                    {
                        genesis_stop_pipeline(context);
                        return GenesisErrorNoMem;
                    }
                }
            } else if (port->descriptor->port_type == GenesisPortTypeEventsOut) {
                GenesisEventsPort *events_port = reinterpret_cast<GenesisEventsPort*>(port);
                // 0.75 to match audio ports
                int min_event_buffer_size = EVENTS_PER_SECOND_CAPACITY * context->latency * 0.75;
                if (events_port->event_buffer_err ||
                    events_port->event_buffer.capacity != min_event_buffer_size)
                {
                    ring_buffer_deinit(&events_port->event_buffer);
                    if ((events_port->event_buffer_err = ring_buffer_init(&events_port->event_buffer,
                                    min_event_buffer_size)))
                    {
                        genesis_stop_pipeline(context);
                        return GenesisErrorNoMem;
                    }
                }
            }
        }
    }

    context->pipeline_running = true;

    for (int i = 0; i < context->nodes.length(); i += 1) {
        GenesisNode *node = context->nodes.at(i);
        if (node->descriptor->activate) {
            if ((err = node->descriptor->activate(node))) {
                genesis_stop_pipeline(context);
                return err;
            }
        }
    }

    for (int i = 0; i < context->thread_pool_size; i += 1) {
        if ((err = os_thread_create(pipeline_thread_run, context, true, &context->thread_pool[i]))) {
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

int genesis_audio_in_port_capacity(struct GenesisPort *port) {
    struct GenesisAudioPort *audio_in_port = (struct GenesisAudioPort *) port;
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) audio_in_port->port.input_from;
    return audio_out_port->sample_buffer_size / audio_out_port->bytes_per_frame;
}

int genesis_audio_in_port_fill_count(GenesisPort *port) {
    struct GenesisAudioPort *audio_in_port = (struct GenesisAudioPort *) port;
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) audio_in_port->port.input_from;
    return ring_buffer_fill_count(&audio_out_port->sample_buffer) / audio_out_port->bytes_per_frame;
}

float *genesis_audio_in_port_read_ptr(GenesisPort *port) {
    struct GenesisAudioPort *audio_in_port = (struct GenesisAudioPort *) port;
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) audio_in_port->port.input_from;
    return (float*)ring_buffer_read_ptr(&audio_out_port->sample_buffer);
}

void genesis_audio_in_port_advance_read_ptr(GenesisPort *port, int frame_count) {
    struct GenesisAudioPort *audio_in_port = (struct GenesisAudioPort *) port;
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) audio_in_port->port.input_from;
    int byte_count = frame_count * audio_out_port->bytes_per_frame;
    assert(byte_count >= 0);
    assert(byte_count <= audio_out_port->sample_buffer_size);
    ring_buffer_advance_read_ptr(&audio_out_port->sample_buffer, byte_count);
    struct GenesisNode *child_node = audio_out_port->port.node;
    queue_node_if_ready(child_node->descriptor->context, child_node, true);
}

int genesis_audio_out_port_free_count(GenesisPort *port) {
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) port;
    int fill_count = ring_buffer_fill_count(&audio_out_port->sample_buffer);
    int bytes_free_count = audio_out_port->sample_buffer_size - fill_count;
    int result = bytes_free_count / audio_out_port->bytes_per_frame;
    assert(result >= 0);
    return result;
}

float *genesis_audio_out_port_write_ptr(GenesisPort *port) {
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) port;
    return (float*)ring_buffer_write_ptr(&audio_out_port->sample_buffer);
}

void genesis_audio_out_port_advance_write_ptr(GenesisPort *port, int frame_count) {
    struct GenesisAudioPort *audio_out_port = (struct GenesisAudioPort *) port;
    int byte_count = frame_count * audio_out_port->bytes_per_frame;
    assert(byte_count >= 0);
    assert(byte_count <= (audio_out_port->sample_buffer_size - ring_buffer_fill_count(&audio_out_port->sample_buffer)));
    ring_buffer_advance_write_ptr(&audio_out_port->sample_buffer, byte_count);
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

const SoundIoChannelLayout *genesis_audio_port_channel_layout(struct GenesisPort *port) {
    struct GenesisAudioPort *audio_port = (struct GenesisAudioPort *)port;
    return &audio_port->channel_layout;
}

void genesis_events_in_port_fill_count(struct GenesisPort *port,
        double time_requested, int *event_count, double *time_available)
{
    struct GenesisEventsPort *events_in_port = (struct GenesisEventsPort *) port;
    struct GenesisEventsPort *events_out_port = (struct GenesisEventsPort *) events_in_port->port.input_from;
    assert(events_out_port); // assume it is connected
    *event_count = ring_buffer_fill_count(&events_out_port->event_buffer) / sizeof(GenesisMidiEvent);
    *time_available = events_out_port->time_available.load();
    events_out_port->time_requested.add(time_requested);
}

void genesis_events_in_port_advance_read_ptr(struct GenesisPort *port, int event_count, double buf_size) {
    struct GenesisEventsPort *events_in_port = (struct GenesisEventsPort *) port;
    struct GenesisEventsPort *events_out_port = (struct GenesisEventsPort *) events_in_port->port.input_from;
    assert(events_out_port); // assume it is connected
    ring_buffer_advance_read_ptr(&events_out_port->event_buffer, event_count * sizeof(GenesisMidiEvent));
    events_out_port->time_available.add(-buf_size);

    struct GenesisNode *child_node = events_out_port->port.node;
    queue_node_if_ready(child_node->descriptor->context, child_node, true);
}

GenesisMidiEvent *genesis_events_in_port_read_ptr(GenesisPort *port) {
    struct GenesisEventsPort *events_in_port = (struct GenesisEventsPort *) port;
    struct GenesisEventsPort *events_out_port = (struct GenesisEventsPort *) events_in_port->port.input_from;
    assert(events_out_port); // assume it is connected
    return (GenesisMidiEvent*)ring_buffer_read_ptr(&events_out_port->event_buffer);
}

void genesis_events_out_port_free_count(struct GenesisPort *port,
        int *event_count, double *time_requested)
{
    struct GenesisEventsPort *events_out_port = (struct GenesisEventsPort *) port;
    int bytes_free_count = events_out_port->event_buffer.capacity -
        ring_buffer_fill_count(&events_out_port->event_buffer);
    *event_count = bytes_free_count / sizeof(GenesisMidiEvent);
    *time_requested = events_out_port->time_requested.load();
}

void genesis_events_out_port_advance_write_ptr(struct GenesisPort *port, int event_count, double buf_size) {
    struct GenesisEventsPort *events_out_port = (struct GenesisEventsPort *) port;
    ring_buffer_advance_write_ptr(&events_out_port->event_buffer, event_count * sizeof(GenesisMidiEvent));
    events_out_port->time_requested.add(-buf_size);
    events_out_port->time_available.add(buf_size);

    GenesisEventsPort *events_in_port = (GenesisEventsPort *)events_out_port->port.output_to;
    assert(events_in_port);
    GenesisNode *other_node = events_in_port->port.node;
    GenesisContext *context = port->node->descriptor->context;
    queue_node_if_ready(context, other_node, false);
}

struct GenesisMidiEvent *genesis_events_out_port_write_ptr(struct GenesisPort *port) {
    struct GenesisEventsPort *events_out_port = (struct GenesisEventsPort *) port;
    return (GenesisMidiEvent*)ring_buffer_write_ptr(&events_out_port->event_buffer);
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
        const SoundIoChannelLayout *channel_layout, bool fixed, int other_port_index)
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

void genesis_set_underrun_callback(struct GenesisContext *context,
        void (*callback)(void *userdata), void *userdata)
{
    context->underrun_callback_userdata = userdata;
    context->underrun_callback = callback;
}

void genesis_debug_print_pipeline(struct GenesisContext *context) {
    fprintf(stderr, "\n");
    for (int node_i = 0; node_i < context->nodes.length(); node_i += 1) {
        GenesisNode *node = context->nodes.at(node_i);
        const GenesisNodeDescriptor *descriptor = node->descriptor;
        fprintf(stderr, "%s:\n", descriptor->name);
        for (int port_i = 0; port_i < node->port_count; port_i += 1) {
            GenesisPortDescriptor *port_descr = descriptor->port_descriptors.at(port_i);
            GenesisPort *port = node->ports[port_i];
            if (!port->output_to)
                continue;
            bool empty, full;
            get_port_status(port, &empty, &full);
            fprintf(stderr, " - %s: empty: %d full: %d\n", port_descr->name, (int)empty, (int)full);
        }
    }
}

int genesis_default_input_device_index(struct GenesisContext *context) {
    return soundio_default_input_device_index(context->soundio);
}

int genesis_default_output_device_index(struct GenesisContext *context) {
    return soundio_default_output_device_index(context->soundio);
}

struct SoundIoDevice *genesis_get_input_device(struct GenesisContext *context, int index) {
    return soundio_get_input_device(context->soundio, index);
}

struct SoundIoDevice *genesis_get_output_device(struct GenesisContext *context, int index) {
    return soundio_get_output_device(context->soundio, index);
}

void genesis_set_audio_device_callback(struct GenesisContext *context,
        void (*callback)(void *userdata), void *userdata)
{
    context->devices_change_callback_userdata = userdata;
    context->devices_change_callback = callback;
}

int genesis_input_device_count(struct GenesisContext *context) {
    return soundio_input_device_count(context->soundio);
}

int genesis_output_device_count(struct GenesisContext *context) {
    return soundio_output_device_count(context->soundio);
}

const char *genesis_version_string(void) {
    return GENESIS_VERSION_STRING;
}
