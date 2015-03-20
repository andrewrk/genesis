#include "genesis.h"
#include "audio_file.hpp"
#include "list.hpp"
#include "audio_hardware.hpp"
#include "threads.hpp"

struct GenesisContext {
    List<GenesisNodeDescriptor*> node_descriptors;
    AudioHardware audio_hardware;

    void (*devices_change_callback)(void *userdata);
    void *devices_change_callback_userdata;

    List<GenesisAudioFileFormat*> out_formats;
    List<GenesisAudioFileFormat*> in_formats;

    List<GenesisNode *> leaf_nodes;

    List<Thread *> thread_pool;

    GenesisContext() : audio_hardware(this) {}
};

struct GenesisPortDescriptor {
    enum GenesisPortType port_type;
    const char *name;
    void (*run)(struct GenesisPort *port);
};

struct GenesisNotesPortDescriptor {
    struct GenesisPortDescriptor port_descriptor;
};

struct GenesisAudioPortDescriptor {
    struct GenesisPortDescriptor port_descriptor;

    bool channel_layout_fixed;
    // if channel_layout_fixed is true then this is the index
    // of the other port that it is the same as, or -1 if it is fixed
    // to the value of channel_layout
    int same_channel_layout_index;
    struct GenesisChannelLayout channel_layout;

    bool sample_rate_fixed;
    // if sample_rate_fixed is true then this is the index
    // of the other port that it is the same as, or -1 if it is fixed
    // to the value of sample_rate
    int same_sample_rate_index;
    int sample_rate;
};

struct GenesisNodeDescriptor {
    struct GenesisContext *context;
    ByteBuffer name;
    ByteBuffer description;
    List<GenesisPortDescriptor*> port_descriptors;
};

struct GenesisPort {
    struct GenesisPortDescriptor *descriptor;
    struct GenesisNode *node;
    struct GenesisPort *input_from;
    struct GenesisPort *output_to;
};

struct GenesisAudioPort {
    struct GenesisPort port;
    struct GenesisChannelLayout channel_layout;
    int sample_rate;
};

struct GenesisNotesPort {
    struct GenesisPort port;
};

struct GenesisNode {
    const struct GenesisNodeDescriptor *descriptor;
    int port_count;
    struct GenesisPort **ports;
};

static void on_devices_change(AudioHardware *audio_hardware) {
    GenesisContext *context = reinterpret_cast<GenesisContext *>(audio_hardware->_userdata);
    if (context->devices_change_callback)
        context->devices_change_callback(context->devices_change_callback_userdata);
}

struct GenesisContext *genesis_create_context(void) {
    audio_file_init();

    GenesisContext *context = create_zero<GenesisContext>();
    if (!context)
        return nullptr;

    audio_file_get_out_formats(context->out_formats);
    audio_file_get_in_formats(context->in_formats);

    context->audio_hardware.set_on_devices_change(on_devices_change);
    context->audio_hardware._userdata = context;

    GenesisNodeDescriptor *node_descr = create<GenesisNodeDescriptor>();
    context->node_descriptors.append(node_descr);

    node_descr->name = "synth";
    node_descr->description = "A single oscillator.";

    GenesisNotesPortDescriptor *notes_port = create<GenesisNotesPortDescriptor>();
    GenesisAudioPortDescriptor *audio_port = create<GenesisAudioPortDescriptor>();

    node_descr->port_descriptors.append(&notes_port->port_descriptor);
    node_descr->port_descriptors.append(&audio_port->port_descriptor);

    notes_port->port_descriptor.name = "notes_in";
    notes_port->port_descriptor.port_type = GenesisPortTypeNotesIn;

    audio_port->port_descriptor.name = "audio_out";
    audio_port->port_descriptor.port_type = GenesisPortTypeAudioOut;
    audio_port->channel_layout =
        *genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
    audio_port->channel_layout_fixed = false;
    audio_port->same_channel_layout_index = -1;
    audio_port->sample_rate = 48000;
    audio_port->sample_rate_fixed = false;
    audio_port->same_sample_rate_index = -1;

    return context;
}

void genesis_destroy_context(struct GenesisContext *context) {
    if (context) {
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
}

void genesis_wait_events(struct GenesisContext *context) {
    context->audio_hardware.wait_events();
}

void genesis_wakeup(struct GenesisContext *context) {
    context->audio_hardware.wakeup();
}

struct GenesisNodeDescriptor *genesis_node_descriptor_find(
        GenesisContext *context, const char *name)
{
    for (int i = 0; i < context->node_descriptors.length(); i += 1) {
        GenesisNodeDescriptor *descr = context->node_descriptors.at(i);
        if (strcmp(descr->name.raw(), name) == 0)
            return descr;
    }
    return nullptr;
}

const char *genesis_node_descriptor_name(const struct GenesisNodeDescriptor *node_descriptor) {
    return node_descriptor->name.raw();
}

const char *genesis_node_descriptor_description(const struct GenesisNodeDescriptor *node_descriptor) {
    return node_descriptor->description.raw();
}

static GenesisAudioPort *create_audio_port_from_descriptor(GenesisPortDescriptor *port_descriptor) {
    GenesisAudioPort *audio_port = create_zero<GenesisAudioPort>();
    if (!audio_port)
        return nullptr;
    audio_port->port.descriptor = port_descriptor;
    return audio_port;
}

static GenesisNotesPort *create_notes_port_from_descriptor(GenesisPortDescriptor *port_descriptor) {
    GenesisNotesPort *notes_port = create_zero<GenesisNotesPort>();
    if (!notes_port)
        return nullptr;
    notes_port->port.descriptor = port_descriptor;
    return notes_port;
}

static GenesisPort *create_port_from_descriptor(GenesisPortDescriptor *port_descriptor) {
    switch (port_descriptor->port_type) {
        case GenesisPortTypeAudioIn:
        case GenesisPortTypeAudioOut:
            return &create_audio_port_from_descriptor(port_descriptor)->port;

        case GenesisPortTypeNotesIn:
        case GenesisPortTypeNotesOut:
            return &create_notes_port_from_descriptor(port_descriptor)->port;

        case GenesisPortTypeParamIn:
        case GenesisPortTypeParamOut:
            panic("unimplemented");
    }
    panic("invalid port type");
}

struct GenesisNode *genesis_node_descriptor_create_node(struct GenesisNodeDescriptor *node_descriptor) {
    GenesisNode *node = allocate_zero<GenesisNode>(1);
    if (!node) {
        genesis_node_destroy(node);
        return nullptr;
    }
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
        node->ports[i] = port;
    }
    return node;
}

void genesis_node_destroy(struct GenesisNode *node) {
    if (node) {
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

struct GenesisNodeDescriptor *genesis_audio_device_create_node_descriptor(
        struct GenesisAudioDevice *audio_device)
{
    if (audio_device->purpose != GenesisAudioDevicePurposePlayback)
        return nullptr;

    GenesisContext *context = audio_device->context;

    GenesisNodeDescriptor *node_descr = create<GenesisNodeDescriptor>();
    context->node_descriptors.append(node_descr);

    node_descr->name = ByteBuffer::format("playback-device-%s", audio_device->name.raw());
    node_descr->description = ByteBuffer::format("Playback Device: %s", audio_device->description.raw());

    node_descr->port_descriptors.resize(1);
    GenesisAudioPortDescriptor *audio_in_port = create<GenesisAudioPortDescriptor>();
    node_descr->port_descriptors.at(0) = &audio_in_port->port_descriptor;

    audio_in_port->port_descriptor.name = "audio_in";
    audio_in_port->port_descriptor.port_type = GenesisPortTypeAudioIn;
    audio_in_port->channel_layout = audio_device->channel_layout;
    audio_in_port->channel_layout_fixed = true;
    audio_in_port->same_channel_layout_index = -1;
    audio_in_port->sample_rate = audio_device->default_sample_rate;
    audio_in_port->sample_rate_fixed = true;
    audio_in_port->same_sample_rate_index = -1;

    return node_descr;
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

static GenesisError connect_audio_ports(GenesisAudioPort *source, GenesisAudioPort *dest) {
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
    return GenesisErrorNone;
}

enum GenesisError genesis_connect_ports(struct GenesisPort *source, struct GenesisPort *dest) {
    // perform validation
    switch (source->descriptor->port_type) {
        case GenesisPortTypeAudioOut:
            if (dest->descriptor->port_type != GenesisPortTypeAudioIn)
                return GenesisErrorIncompatiblePorts;

            return connect_audio_ports((GenesisAudioPort *)source, (GenesisAudioPort *)dest);
        case GenesisPortTypeNotesOut:
            if (dest->descriptor->port_type != GenesisPortTypeNotesIn)
                return GenesisErrorIncompatiblePorts;
            panic("TODO: connect notes ports");
        case GenesisPortTypeParamOut:
            if (dest->descriptor->port_type != GenesisPortTypeParamIn)
                return GenesisErrorIncompatiblePorts;
            panic("TODO: connect param ports");
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
    GenesisNodeDescriptor *node_descr = create<GenesisNodeDescriptor>();
    context->node_descriptors.append(node_descr);

    node_descr->port_descriptors.resize(port_count);

    return node_descr;
}

static GenesisAudioPortDescriptor *create_audio_port(GenesisNodeDescriptor *node_descr,
        int port_index, GenesisPortType port_type)
{
    GenesisAudioPortDescriptor *audio_port_descr = create_zero<GenesisAudioPortDescriptor>();
    node_descr->port_descriptors.at(port_index) = &audio_port_descr->port_descriptor;
    audio_port_descr->port_descriptor.port_type = port_type;
    return audio_port_descr;
}

static GenesisNotesPortDescriptor *create_notes_port(GenesisNodeDescriptor *node_descr,
        int port_index, GenesisPortType port_type)
{
    GenesisNotesPortDescriptor *notes_port_descr = create_zero<GenesisNotesPortDescriptor>();
    node_descr->port_descriptors.at(port_index) = &notes_port_descr->port_descriptor;
    notes_port_descr->port_descriptor.port_type = port_type;
    return notes_port_descr;
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
            panic("unimplemented");
    }
    panic("invalid port type");
}

void genesis_port_descriptor_set_run_callback(
        struct GenesisPortDescriptor *port_descriptor,
        void (*run)(struct GenesisPort *port))
{
    port_descriptor->run = run;
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
            panic("unimplemented");
    }
    panic("invalid port type");
}

enum GenesisError genesis_start_pipeline(struct GenesisContext *context) {
    int cpu_count = Thread::concurrency();
    fprintf(stderr, "cpu count: %d\n", cpu_count);

    return GenesisErrorNone;
}
