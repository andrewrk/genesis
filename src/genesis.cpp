#include "genesis.h"
#include "audio_file.hpp"
#include "list.hpp"
#include "audio_hardware.hpp"

struct GenesisContext {
    List<GenesisNodeDescriptor> node_descriptors;
    AudioHardware audio_hardware;

    void (*devices_change_callback)(void *userdata);
    void *devices_change_callback_userdata;

    List<GenesisAudioFileFormat*> out_formats;
    List<GenesisAudioFileFormat*> in_formats;
};

struct GenesisPortDescriptorAudio {
    struct GenesisChannelLayout channel_layout;
    enum GenesisChannelConfig channel_config;
    int sample_rate;
    enum GenesisChannelConfig sample_rate_config;
};

struct GenesisPortDescriptor {
    enum GenesisPortType port_type;
    const char *name;
    union {
        struct GenesisPortDescriptorAudio audio;
    } data;
};

struct GenesisNodeDescriptor {
    struct GenesisContext *context;
    const char *name;
    List<GenesisPortDescriptor> port_descriptors;
};

struct GenesisPort {
    struct GenesisPortDescriptor *descriptor;
};

struct GenesisNode {
    const struct GenesisNodeDescriptor *descriptor;
    int port_count;
    struct GenesisPort *ports;
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

    context->node_descriptors.resize(1);
    GenesisNodeDescriptor *node_descr = &context->node_descriptors.at(0);
    node_descr->name = "synth";
    node_descr->port_descriptors.resize(2);

    GenesisPortDescriptor *notes_port = &node_descr->port_descriptors.at(0);
    GenesisPortDescriptor *audio_port = &node_descr->port_descriptors.at(1);

    notes_port->name = "notes_in";
    notes_port->port_type = GenesisPortTypeNotesIn;

    audio_port->name = "audio_out";
    audio_port->port_type = GenesisPortTypeAudioOut;
    audio_port->data.audio.channel_layout =
        *genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
    audio_port->data.audio.channel_config = GenesisChannelConfigFixed;
    audio_port->data.audio.sample_rate = 48000;
    audio_port->data.audio.sample_rate_config = GenesisChannelConfigFixed;

    return context;
}

void genesis_destroy_context(struct GenesisContext *context) {
    if (context) {
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
        GenesisNodeDescriptor *descr = &context->node_descriptors.at(i);
        if (strcmp(descr->name, name) == 0)
            return descr;
    }
    return nullptr;
}

const char *genesis_node_descriptor_name(const struct GenesisNodeDescriptor *node_descriptor) {
    return node_descriptor->name;
}

struct GenesisNode *genesis_node_create(struct GenesisNodeDescriptor *node_descriptor) {
    GenesisNode *node = allocate_zero<GenesisNode>(1);
    if (!node) {
        genesis_node_destroy(node);
        return nullptr;
    }
    node->descriptor = node_descriptor;
    node->port_count = node_descriptor->port_descriptors.length();
    node->ports = allocate_zero<GenesisPort>(node->port_count);
    if (!node->ports) {
        genesis_node_destroy(node);
        return nullptr;
    }
    for (int i = 0; i < node->port_count; i += 1) {
        GenesisPort *port = &node->ports[i];
        port->descriptor = &node_descriptor->port_descriptors.at(i);
    }
    return node;
}

void genesis_node_destroy(struct GenesisNode *node) {
    if (node) {
        if (node->ports)
            destroy(node->ports, 1);
        destroy(node, 1);
    }
}

int genesis_audio_device_count(struct GenesisContext *context) {
    context->audio_hardware.block_until_ready();
    context->audio_hardware.flush_events();
    context->audio_hardware.block_until_have_devices();

    const AudioDevicesInfo *audio_device_info = context->audio_hardware.devices_info();
    if (!audio_device_info)
        return -1;
    return audio_device_info->devices.length();
}

struct GenesisAudioDevice *genesis_audio_device_get(struct GenesisContext *context, int index) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware.devices_info();
    if (!audio_device_info)
        return nullptr;
    if (index < 0 || index >= audio_device_info->devices.length())
        return nullptr;
    return (GenesisAudioDevice *) &audio_device_info->devices.at(index);
}

int genesis_audio_device_default_playback(struct GenesisContext *context) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware.devices_info();
    if (!audio_device_info)
        return -1;
    return audio_device_info->default_output_index;
}

int genesis_audio_device_default_recording(struct GenesisContext *context) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware.devices_info();
    if (!audio_device_info)
        return -1;
    return audio_device_info->default_input_index;
}

const char *genesis_audio_device_name(const struct GenesisAudioDevice *audio_device) {
    const AudioDevice *device = reinterpret_cast<const AudioDevice *>(audio_device);
    return device->name.raw();
}

const char *genesis_audio_device_description(const struct GenesisAudioDevice *audio_device) {
    const AudioDevice *device = reinterpret_cast<const AudioDevice *>(audio_device);
    return device->description.raw();
}

enum GenesisAudioDevicePurpose genesis_audio_device_purpose(const struct GenesisAudioDevice *audio_device) {
    const AudioDevice *device = reinterpret_cast<const AudioDevice *>(audio_device);
    return device->purpose;
}

void genesis_audio_device_set_callback(struct GenesisContext *context,
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
