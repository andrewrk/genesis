#ifndef GENESIS_GENESIS_H
#define GENESIS_GENESIS_H

#include "config.h"
#include "channel_layout.h"
#include "sample_format.h"
#include "error.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

////////// Main Context
struct GenesisContext;
int genesis_create_context(struct GenesisContext **context);
void genesis_destroy_context(struct GenesisContext *context);

// when you call genesis_flush_events, device information becomes invalid
// and you need to query it again if you want it.
void genesis_flush_events(struct GenesisContext *context);

// flushes events as they occur, blocks until you call genesis_wakeup
void genesis_wait_events(struct GenesisContext *context);

// makes genesis_wait_events stop blocking
void genesis_wakeup(struct GenesisContext *context);


double genesis_frames_to_whole_notes(struct GenesisContext *context, int frames, int frame_rate);
int genesis_whole_notes_to_frames(struct GenesisContext *context, double whole_notes, int frame_rate);
double genesis_whole_notes_to_seconds(struct GenesisContext *context, double whole_notes, int frame_rate);


///////////////// Audio Devices

struct GenesisAudioDevice;
// when you call this, the information you got previously from these
// functions becomes invalid. This applies to the count of devices and the
// pointers to each GenesisAudioDevice.
// note you can also call genesis_wait_events or genesis_flush_events combined
// with genesis_set_audio_device_callback. the reason to use
// genesis_refresh_audio_devices is that it blocks until a device list is
// obtained, useful for programs that want to block.
void genesis_refresh_audio_devices(struct GenesisContext *context);

// returns -1 on error
int genesis_get_audio_device_count(struct GenesisContext *context);

// returns NULL on error
// the returned pointer becomes invalid when you call genesis_flush_events,
// genesis_wait_events, or genesis_refresh_audio_devices
struct GenesisAudioDevice *genesis_get_audio_device(struct GenesisContext *context, int index);

// returns the index of the default playback device
// returns -1 on error
int genesis_get_default_playback_device_index(struct GenesisContext *context);

// returns the index of the default recording device
// returns -1 on error
int genesis_get_default_recording_device_index(struct GenesisContext *context);

// the name is the identifier for the device. UTF-8 encoded
const char *genesis_audio_device_name(const struct GenesisAudioDevice *device);

// UTF-8 encoded
const char *genesis_audio_device_description(const struct GenesisAudioDevice *device);

enum GenesisAudioDevicePurpose {
    GenesisAudioDevicePurposePlayback,
    GenesisAudioDevicePurposeRecording,
};

enum GenesisAudioDevicePurpose genesis_audio_device_purpose(const struct GenesisAudioDevice *device);

// set callback to be called when audio devices change
// callback is always called from genesis_flush_events or genesis_wait_events
void genesis_set_audio_device_callback(struct GenesisContext *context,
        void (*callback)(void *userdata),
        void *userdata);

///////////// MIDI Devices

struct GenesisMidiDevice;

void genesis_refresh_midi_devices(struct GenesisContext *context);

int genesis_get_midi_device_count(struct GenesisContext *context);

// returns NULL on error.
// the returned pointer becomes invalid when you call genesis_flush_events,
// genesis_wait_events, or genesis_refresh_midi_devices
struct GenesisMidiDevice *genesis_get_midi_device(struct GenesisContext *context, int index);

int genesis_get_default_midi_device_index(struct GenesisContext *context);

const char *genesis_midi_device_name(const struct GenesisMidiDevice *device);
const char *genesis_midi_device_description(const struct GenesisMidiDevice *device);

// set callback for when midi devices change
// callback is always called from genesis_flush_events or genesis_wait_events
void genesis_set_midi_device_callback(struct GenesisContext *context,
        void (*callback)(void *userdata),
        void *userdata);




///////////// Pipeline
enum GenesisPortType {
    GenesisPortTypeAudioIn,
    GenesisPortTypeAudioOut,
    GenesisPortTypeEventsIn,
    GenesisPortTypeEventsOut,
};

struct GenesisPortDescriptor;
struct GenesisNodeDescriptor;
struct GenesisNodeDescriptor *genesis_node_descriptor_find(
        struct GenesisContext *context, const char *name);
const char *genesis_node_descriptor_name(const struct GenesisNodeDescriptor *node_descriptor);
const char *genesis_node_descriptor_description(const struct GenesisNodeDescriptor *node_descriptor);

int genesis_audio_device_create_node_descriptor(
        struct GenesisAudioDevice *audio_device,
        struct GenesisNodeDescriptor **out_node_descriptor);
int genesis_midi_device_create_node_descriptor(
        struct GenesisMidiDevice *midi_device,
        struct GenesisNodeDescriptor **out_node_descriptor);

struct GenesisNodeDescriptor *genesis_create_node_descriptor(
        struct GenesisContext *context, int port_count);

void genesis_node_descriptor_destroy(struct GenesisNodeDescriptor *node_descriptor);

// returns -1 if not found
int genesis_node_descriptor_find_port_index(
        const struct GenesisNodeDescriptor *node_descriptor, const char *name);


struct GenesisPort;
struct GenesisNode;
struct GenesisNode *genesis_node_descriptor_create_node(struct GenesisNodeDescriptor *node_descriptor);
void genesis_node_destroy(struct GenesisNode *node);

struct GenesisPort *genesis_node_port(struct GenesisNode *node, int port_index);

int genesis_connect_ports(struct GenesisPort *source, struct GenesisPort *dest);

struct GenesisNode *genesis_port_node(struct GenesisPort *port);

struct GenesisPortDescriptor *genesis_node_descriptor_create_port(
        struct GenesisNodeDescriptor *node_descriptor, int port_index,
        enum GenesisPortType port_type);

void genesis_debug_print_port_config(struct GenesisPort *port);

int genesis_start_pipeline(struct GenesisContext *context);
void genesis_stop_pipeline(struct GenesisContext *context);

// can only set this when the pipeline is stopped.
// also if you change this, you must destroy and re-create nodes and node
// descriptors based on audio devices
int genesis_set_latency(struct GenesisContext *context, double latency);




////////////// Formats and Codecs
struct GenesisAudioFileFormat;
struct GenesisAudioFileCodec;

struct GenesisExportFormat {
    struct GenesisAudioFileCodec *codec;
    enum GenesisSampleFormat sample_format;
    int bit_rate;
    int sample_rate;
};

int genesis_in_format_count(struct GenesisContext *context);
int genesis_out_format_count(struct GenesisContext *context);

struct GenesisAudioFileFormat *genesis_in_format_index(
        struct GenesisContext *context, int format_index);
struct GenesisAudioFileFormat *genesis_out_format_index(
        struct GenesisContext *context, int format_index);

const char *genesis_audio_file_format_name(const struct GenesisAudioFileFormat *format);
const char *genesis_audio_file_format_description(const struct GenesisAudioFileFormat *format);

int genesis_audio_file_format_codec_count(const struct GenesisAudioFileFormat *format);

struct GenesisAudioFileCodec * genesis_audio_file_format_codec_index(
        struct GenesisAudioFileFormat *format, int codec_index);

struct GenesisAudioFileCodec *genesis_guess_audio_file_codec(
        struct GenesisContext *context, const char *filename_hint,
        const char *format_name, const char *codec_name);

const char *genesis_audio_file_codec_name(
        const struct GenesisAudioFileCodec *codec);

const char *genesis_audio_file_codec_description(
        const struct GenesisAudioFileCodec *codec);

int genesis_audio_file_codec_sample_format_count(
        const struct GenesisAudioFileCodec *codec);

enum GenesisSampleFormat genesis_audio_file_codec_sample_format_index(
        const struct GenesisAudioFileCodec *codec, int index);

int genesis_audio_file_codec_sample_rate_count(
        const struct GenesisAudioFileCodec *codec);

int genesis_audio_file_codec_sample_rate_index(
        const struct GenesisAudioFileCodec *codec, int index);

bool genesis_audio_file_codec_supports_sample_format(
        const struct GenesisAudioFileCodec *codec,
        enum GenesisSampleFormat sample_format);

bool genesis_audio_file_codec_supports_sample_rate(
        const struct GenesisAudioFileCodec *codec, int sample_rate);



////////////////// Audio File
struct GenesisAudioFile;

struct GenesisAudioFileIterator {
    struct GenesisAudioFile *audio_file;
    long start;
    long end;
    float *ptr;
};
int genesis_audio_file_load(struct GenesisContext *context,
        const char *input_filename, struct GenesisAudioFile **audio_file);

void genesis_audio_file_destroy(struct GenesisAudioFile *audio_file);

int genesis_audio_file_export(struct GenesisAudioFile *audio_file,
        const char *output_filename, struct GenesisExportFormat *export_format);

struct GenesisChannelLayout genesis_audio_file_channel_layout(
        const struct GenesisAudioFile *audio_file);
long genesis_audio_file_frame_count(const struct GenesisAudioFile *audio_file);
int genesis_audio_file_sample_rate(const struct GenesisAudioFile *audio_file);

struct GenesisAudioFileIterator genesis_audio_file_iterator(
        struct GenesisAudioFile *audio_file, int channel_index, long start_frame_index);
void genesis_audio_file_iterator_next(struct GenesisAudioFileIterator *it);



#ifdef __cplusplus
}
#endif
#endif
