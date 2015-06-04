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
// be ready for spurious wakeups
void genesis_wait_events(struct GenesisContext *context);

// makes genesis_wait_events stop blocking
void genesis_wakeup(struct GenesisContext *context);

// optionally set a callback to be called when an event becomes ready
// it might be called spuriously, and it will be called from an
// internal genesis thread. You would typically use this to wake up another
// blocking call in order to call genesis_flush_events and then go back to
// blocking.
void genesis_set_event_callback(struct GenesisContext *context,
        void (*callback)(void *userdata), void *userdata);

// set callback to be called when a buffer underrun occurs.
// callback is always called from genesis_flush_events or genesis_wait_events
void genesis_set_underrun_callback(struct GenesisContext *context,
        void (*callback)(void *userdata), void *userdata);


double genesis_frames_to_whole_notes(struct GenesisContext *context, int frames, int frame_rate);
int genesis_whole_notes_to_frames(struct GenesisContext *context, double whole_notes, int frame_rate);
double genesis_whole_notes_to_seconds(struct GenesisContext *context, double whole_notes, int frame_rate);


///////////////// Audio Devices

struct GenesisAudioDevice;
// note you can also call genesis_wait_events or genesis_flush_events combined
// with genesis_set_audio_device_callback. the reason to use
// genesis_refresh_audio_devices is that it blocks until a device list is
// obtained, useful for programs that want to block.
void genesis_refresh_audio_devices(struct GenesisContext *context);

// returns -1 on error
int genesis_get_audio_device_count(struct GenesisContext *context);

// returns NULL on error
// call genesis_audio_device_unref when you no longer have a reference to the pointer.
struct GenesisAudioDevice *genesis_get_audio_device(struct GenesisContext *context, int index);

// returns the index of the default playback device
// returns -1 on error
int genesis_get_default_playback_device_index(struct GenesisContext *context);

// returns the index of the default recording device
// returns -1 on error
int genesis_get_default_recording_device_index(struct GenesisContext *context);

void genesis_audio_device_ref(struct GenesisAudioDevice *device);
void genesis_audio_device_unref(struct GenesisAudioDevice *device);

// the name is the identifier for the device. UTF-8 encoded
const char *genesis_audio_device_name(const struct GenesisAudioDevice *device);

// UTF-8 encoded
const char *genesis_audio_device_description(const struct GenesisAudioDevice *device);

const struct GenesisChannelLayout *genesis_audio_device_channel_layout(const struct GenesisAudioDevice *device);
int genesis_audio_device_sample_rate(const struct GenesisAudioDevice *device);

bool genesis_audio_device_equal(
        const struct GenesisAudioDevice *a,
        const struct GenesisAudioDevice *b);

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
// you must call genesis_midi_device_unref when done with the device pointer.
struct GenesisMidiDevice *genesis_get_midi_device(struct GenesisContext *context, int index);

int genesis_get_default_midi_device_index(struct GenesisContext *context);

void genesis_midi_device_ref(struct GenesisMidiDevice *device);
void genesis_midi_device_unref(struct GenesisMidiDevice *device);
const char *genesis_midi_device_name(const struct GenesisMidiDevice *device);
const char *genesis_midi_device_description(const struct GenesisMidiDevice *device);

bool genesis_midi_device_equal(
        const struct GenesisMidiDevice *a,
        const struct GenesisMidiDevice *b);

// set callback for when midi devices change
// callback is always called from genesis_flush_events or genesis_wait_events
void genesis_set_midi_device_callback(struct GenesisContext *context,
        void (*callback)(void *userdata),
        void *userdata);


#define GENESIS_NOTES_COUNT 128
float genesis_midi_note_to_pitch(int note);




///////////// Pipeline
enum GenesisPortType {
    GenesisPortTypeAudioIn,
    GenesisPortTypeAudioOut,
    GenesisPortTypeEventsIn,
    GenesisPortTypeEventsOut,
};

struct GenesisPortDescriptor;
struct GenesisNodeDescriptor;
struct GenesisPort;
struct GenesisNode;

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

// name and description are copied internally
struct GenesisNodeDescriptor *genesis_create_node_descriptor(
        struct GenesisContext *context, int port_count, const char *name,
        const char *description);

void genesis_node_descriptor_destroy(struct GenesisNodeDescriptor *node_descriptor);

void genesis_node_descriptor_set_userdata(struct GenesisNodeDescriptor *node_descriptor,
        void *userdata);
void genesis_node_descriptor_set_run_callback(struct GenesisNodeDescriptor *node_descriptor,
        void (*run)(struct GenesisNode *node));
void genesis_node_descriptor_set_seek_callback(struct GenesisNodeDescriptor *node_descriptor,
        void (*seek)(struct GenesisNode *node));
void genesis_node_descriptor_set_create_callback(struct GenesisNodeDescriptor *node_descriptor,
        int (*create)(struct GenesisNode *node));
void genesis_node_descriptor_set_destroy_callback(struct GenesisNodeDescriptor *node_descriptor,
        void (*destroy)(struct GenesisNode *node));
void *genesis_node_descriptor_userdata(const struct GenesisNodeDescriptor *node_descriptor);

// returns -1 if not found
int genesis_node_descriptor_find_port_index(
        const struct GenesisNodeDescriptor *node_descriptor, const char *name);


struct GenesisNode *genesis_node_descriptor_create_node(struct GenesisNodeDescriptor *node_descriptor);
void genesis_node_destroy(struct GenesisNode *node);

struct GenesisPort *genesis_node_port(struct GenesisNode *node, int port_index);
const struct GenesisNodeDescriptor *genesis_node_descriptor(const struct GenesisNode *node);
struct GenesisContext *genesis_node_context(struct GenesisNode *node);
void genesis_node_disconnect_all_ports(struct GenesisNode *node);

int genesis_connect_ports(struct GenesisPort *source, struct GenesisPort *dest);
void genesis_disconnect_ports(struct GenesisPort *source, struct GenesisPort *dest);
// shortcut for connecting audio nodes. calls genesis_connect_ports internally
int genesis_connect_audio_nodes(struct GenesisNode *source, struct GenesisNode *dest);


struct GenesisNode *genesis_port_node(struct GenesisPort *port);

// name is duplicated internally
struct GenesisPortDescriptor *genesis_node_descriptor_create_port(
        struct GenesisNodeDescriptor *node_descriptor, int port_index,
        enum GenesisPortType port_type, const char *name);

void genesis_port_descriptor_set_connect_callback(
        struct GenesisPortDescriptor *port_descr,
        int (*connect)(struct GenesisPort *port, struct GenesisPort *other_port));

void genesis_port_descriptor_set_disconnect_callback(
        struct GenesisPortDescriptor *port_descr,
        void (*disconnect)(struct GenesisPort *port, struct GenesisPort *other_port));


// if fixed is true then other_port_index is the index
// of the other port that it is the same as, or -1 if it is fixed
// to the value of channel_layout
int genesis_audio_port_descriptor_set_channel_layout(
        struct GenesisPortDescriptor *audio_port_descr,
        const struct GenesisChannelLayout *channel_layout, bool fixed, int other_port_index);

// if fixed is true then other_port_index is the index
// of the other port that it is the same as, or -1 if it is fixed
// to the value of sample_rate
int genesis_audio_port_descriptor_set_sample_rate(
        struct GenesisPortDescriptor *audio_port_descr,
        int sample_rate, bool fixed, int other_port_index);

void genesis_port_descriptor_destroy(struct GenesisPortDescriptor *port_descriptor);

void genesis_debug_print_port_config(struct GenesisPort *port);
void genesis_debug_print_pipeline(struct GenesisContext *context);

int genesis_start_pipeline(struct GenesisContext *context, double time);
void genesis_stop_pipeline(struct GenesisContext *context);
int genesis_resume_pipeline(struct GenesisContext *context);

// can only set this when the pipeline is stopped.
// also if you change this, you must destroy and re-create nodes and node
// descriptors based on audio devices
int genesis_set_latency(struct GenesisContext *context, double latency);

// returns the number of frames available to read
int genesis_audio_in_port_fill_count(struct GenesisPort *port);
float *genesis_audio_in_port_read_ptr(struct GenesisPort *port);
void genesis_audio_in_port_advance_read_ptr(struct GenesisPort *port, int frame_count);

// returns the number of frames that can be written
int genesis_audio_out_port_free_count(struct GenesisPort *port);
float *genesis_audio_out_port_write_ptr(struct GenesisPort *port);
void genesis_audio_out_port_advance_write_ptr(struct GenesisPort *port, int frame_count);

int genesis_audio_port_bytes_per_frame(struct GenesisPort *port);
int genesis_audio_port_sample_rate(struct GenesisPort *port);
const struct GenesisChannelLayout *genesis_audio_port_channel_layout(struct GenesisPort *port);

// time_requested is how much time in whole notes you want to be available.
// event_count is the number of events available to read.
// time_available is how much time in whole notes is accounted for in the buffer.
void genesis_events_in_port_fill_count(struct GenesisPort *port,
        double time_requested, int *event_count, double *time_available);
// event_count is how many events you consumed. buf_size is the amount of whole notes you consumed.
void genesis_events_in_port_advance_read_ptr(struct GenesisPort *port, int event_count, double buf_size);
struct GenesisMidiEvent *genesis_events_in_port_read_ptr(struct GenesisPort *port);

// event_count is the number of events that can be written.
// time_requested is how much time in whole notes you should account for if you can.
void genesis_events_out_port_free_count(struct GenesisPort *port,
        int *event_count, double *time_requested);
// event_count is how many events you wrote to the buffer. buf_size is the amount of whole notes
// you accounted for.
void genesis_events_out_port_advance_write_ptr(struct GenesisPort *port, int event_count, double buf_size);
struct GenesisMidiEvent *genesis_events_out_port_write_ptr(struct GenesisPort *port);


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
    long start; // absolute frame index
    long end; // absolute frame index
    float *ptr;
};
int genesis_audio_file_load(struct GenesisContext *context,
        const char *input_filename, struct GenesisAudioFile **audio_file);

struct GenesisAudioFile *genesis_audio_file_create(struct GenesisContext *context);
void genesis_audio_file_set_sample_rate(struct GenesisAudioFile *audio_file,
        int sample_rate);
int genesis_audio_file_set_channel_layout(struct GenesisAudioFile *audio_file,
        const struct GenesisChannelLayout *channel_layout);

void genesis_audio_file_destroy(struct GenesisAudioFile *audio_file);

int genesis_audio_file_export(struct GenesisAudioFile *audio_file,
        const char *output_filename, struct GenesisExportFormat *export_format);

const struct GenesisChannelLayout *genesis_audio_file_channel_layout(
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
