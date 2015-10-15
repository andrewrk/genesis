#ifndef GENESIS_GENESIS_H
#define GENESIS_GENESIS_H

#include <stdbool.h>
#include <soundio/soundio.h>

/// \cond
#ifdef __cplusplus
#define GENESIS_EXTERN_C extern "C"
#else
#define GENESIS_EXTERN_C
#endif

#if defined(_WIN32)
#if defined(GENESIS_BUILDING_LIBRARY)
#define GENESIS_EXPORT GENESIS_EXTERN_C __declspec(dllexport)
#else
#define GENESIS_EXPORT GENESIS_EXTERN_C __declspec(dllimport)
#endif
#else
#define GENESIS_EXPORT GENESIS_EXTERN_C __attribute__((visibility ("default")))
#endif
/// \endcond

#define GENESIS_NOTES_COUNT 128
#define GENESIS_MAX_CHANNELS SOUNDIO_MAX_CHANNELS

/// How many SoundIoChannelId values there are.
#define GENESIS_CHANNEL_ID_COUNT 70

enum GenesisError {
    GenesisErrorNone,
    GenesisErrorNoMem,
    GenesisErrorMaxChannelsExceeded,
    GenesisErrorIncompatiblePorts,
    GenesisErrorInvalidPortDirection,
    GenesisErrorIncompatibleSampleRates,
    GenesisErrorIncompatibleChannelLayouts,
    GenesisErrorOpeningMidiHardware,
    GenesisErrorOpeningAudioHardware,
    GenesisErrorInvalidState,
    GenesisErrorSystemResources,
    GenesisErrorDecodingAudio,
    GenesisErrorInvalidParam,
    GenesisErrorInvalidPortType,
    GenesisErrorPortNotFound,
    GenesisErrorNoAudioFound,
    GenesisErrorUnimplemented,
    GenesisErrorAborted,
    GenesisErrorFileAccess,
    GenesisErrorInvalidFormat,
    GenesisErrorEmptyFile,
    GenesisErrorKeyNotFound,
    GenesisErrorFileNotFound,
    GenesisErrorPermissionDenied,
    GenesisErrorNotDir,
    GenesisErrorNoDecoderFound,
    GenesisErrorAlreadyExists,
    GenesisErrorConnectionRefused,
    GenesisErrorIncompatibleDevice,
    GenesisErrorDeviceNotFound,
    GenesisErrorDecodingString,
};

enum GenesisPortType {
    GenesisPortTypeAudioIn,
    GenesisPortTypeAudioOut,
    GenesisPortTypeEventsIn,
    GenesisPortTypeEventsOut,
};

struct GenesisContext;

struct GenesisExportFormat {
    struct GenesisAudioFileCodec *codec;
    enum SoundIoFormat sample_format;
    int bit_rate;
    int sample_rate;
};

struct GenesisAudioFileIterator {
    struct GenesisAudioFile *audio_file;
    long start; // absolute frame index
    long end; // absolute frame index
    float *ptr;
};

struct GenesisSoundBackend {
    struct GenesisContext *context;
    enum SoundIoBackend backend;
    struct SoundIo *soundio;
    int connect_err;
};

struct GenesisMidiDevice;

struct GenesisPortDescriptor;
struct GenesisNodeDescriptor;
struct GenesisPort;
struct GenesisNode;

struct GenesisAudioFileFormat;
struct GenesisRenderFormat;
struct GenesisAudioFileCodec;
struct GenesisAudioFile;

////////// Main Context
GENESIS_EXPORT const char *genesis_version_string(void);

GENESIS_EXPORT int genesis_create_context(struct GenesisContext **context);
GENESIS_EXPORT void genesis_destroy_context(struct GenesisContext *context);

GENESIS_EXPORT const char *genesis_strerror(int error);

// when you call genesis_flush_events, device information becomes invalid
// and you need to query it again if you want it.
GENESIS_EXPORT void genesis_flush_events(struct GenesisContext *context);

// flushes events as they occur, blocks until you call genesis_wakeup
// be ready for spurious wakeups
GENESIS_EXPORT void genesis_wait_events(struct GenesisContext *context);

// makes genesis_wait_events stop blocking
GENESIS_EXPORT void genesis_wakeup(struct GenesisContext *context);

// optionally set a callback to be called when an event becomes ready
// it might be called spuriously, and it will be called from an
// internal genesis thread. You would typically use this to wake up another
// blocking call in order to call genesis_flush_events and then go back to
// blocking.
GENESIS_EXPORT void genesis_set_event_callback(struct GenesisContext *context,
        void (*callback)(void *userdata), void *userdata);

// set callback to be called when a buffer underrun occurs.
// callback is always called from genesis_flush_events or genesis_wait_events
GENESIS_EXPORT void genesis_set_underrun_callback(struct GenesisContext *context,
        void (*callback)(void *userdata), void *userdata);


GENESIS_EXPORT double genesis_frames_to_whole_notes(struct GenesisContext *context, int frames, int frame_rate);
GENESIS_EXPORT int genesis_whole_notes_to_frames(struct GenesisContext *context, double whole_notes, int frame_rate);
GENESIS_EXPORT double genesis_whole_notes_to_seconds(struct GenesisContext *context, double whole_notes, int frame_rate);


GENESIS_EXPORT int genesis_default_input_device_index(struct GenesisSoundBackend *sound_backend);
GENESIS_EXPORT int genesis_default_output_device_index(struct GenesisSoundBackend *sound_backend);

GENESIS_EXPORT struct SoundIoDevice *genesis_get_input_device(
        struct GenesisSoundBackend *sound_backend, int index);

GENESIS_EXPORT struct SoundIoDevice *genesis_get_output_device(
        struct GenesisSoundBackend *sound_backend, int index);


GENESIS_EXPORT void genesis_set_audio_device_callback(struct GenesisContext *context,
        void (*callback)(void *userdata), void *userdata);

GENESIS_EXPORT void genesis_set_sound_backend_disconnect_callback(struct GenesisContext *context,
        void (*callback)(void *userdata), void *userdata);

GENESIS_EXPORT int genesis_input_device_count(struct GenesisSoundBackend *sound_backend);
GENESIS_EXPORT int genesis_output_device_count(struct GenesisSoundBackend *sound_backend);

GENESIS_EXPORT struct GenesisSoundBackend *genesis_default_backend(struct GenesisContext *context);

GENESIS_EXPORT struct SoundIoDevice *genesis_get_default_input_device(struct GenesisContext *context);
GENESIS_EXPORT struct SoundIoDevice *genesis_get_default_output_device(struct GenesisContext *context);

GENESIS_EXPORT struct GenesisSoundBackend *genesis_get_sound_backends(
        struct GenesisContext *context, int *count);

GENESIS_EXPORT struct GenesisSoundBackend *genesis_find_sound_backend(
        struct GenesisContext *context, enum SoundIoBackend backend);

GENESIS_EXPORT struct SoundIoDevice *genesis_find_output_device(struct GenesisContext *context,
        enum SoundIoBackend backend, const char *device_id, bool is_raw);

GENESIS_EXPORT struct SoundIoDevice *genesis_find_input_device(struct GenesisContext *context,
        enum SoundIoBackend backend, const char *device_id, bool is_raw);

///////////// MIDI Devices

GENESIS_EXPORT void genesis_refresh_midi_devices(struct GenesisContext *context);

GENESIS_EXPORT int genesis_get_midi_device_count(struct GenesisContext *context);

// returns NULL on error.
// you must call genesis_midi_device_unref when done with the device pointer.
GENESIS_EXPORT struct GenesisMidiDevice *genesis_get_midi_device(struct GenesisContext *context, int index);

GENESIS_EXPORT int genesis_get_default_midi_device_index(struct GenesisContext *context);

GENESIS_EXPORT void genesis_midi_device_ref(struct GenesisMidiDevice *device);
GENESIS_EXPORT void genesis_midi_device_unref(struct GenesisMidiDevice *device);
GENESIS_EXPORT const char *genesis_midi_device_name(const struct GenesisMidiDevice *device);
GENESIS_EXPORT const char *genesis_midi_device_description(const struct GenesisMidiDevice *device);

GENESIS_EXPORT bool genesis_midi_device_equal(
        const struct GenesisMidiDevice *a,
        const struct GenesisMidiDevice *b);

// set callback for when midi devices change
// callback is always called from genesis_flush_events or genesis_wait_events
GENESIS_EXPORT void genesis_set_midi_device_callback(struct GenesisContext *context,
        void (*callback)(void *userdata),
        void *userdata);


GENESIS_EXPORT float genesis_midi_note_to_pitch(int note);




///////////// Pipeline


GENESIS_EXPORT struct GenesisNodeDescriptor *genesis_node_descriptor_find(
        struct GenesisContext *context, const char *name);
GENESIS_EXPORT const char *genesis_node_descriptor_name(const struct GenesisNodeDescriptor *node_descriptor);
GENESIS_EXPORT const char *genesis_node_descriptor_description(const struct GenesisNodeDescriptor *node_descriptor);

GENESIS_EXPORT int genesis_audio_device_create_node_descriptor(
        struct GenesisContext *context,
        struct SoundIoDevice *audio_device,
        struct GenesisNodeDescriptor **out_node_descriptor);
GENESIS_EXPORT int genesis_midi_device_create_node_descriptor(
        struct GenesisMidiDevice *midi_device,
        struct GenesisNodeDescriptor **out_node_descriptor);

// name and description are copied internally
GENESIS_EXPORT struct GenesisNodeDescriptor *genesis_create_node_descriptor(
        struct GenesisContext *context, int port_count, const char *name,
        const char *description);

GENESIS_EXPORT void genesis_node_descriptor_destroy(struct GenesisNodeDescriptor *node_descriptor);

GENESIS_EXPORT void genesis_node_descriptor_set_userdata(struct GenesisNodeDescriptor *node_descriptor,
        void *userdata);
GENESIS_EXPORT void genesis_node_descriptor_set_run_callback(struct GenesisNodeDescriptor *node_descriptor,
        void (*run)(struct GenesisNode *node));
GENESIS_EXPORT void genesis_node_descriptor_set_seek_callback(struct GenesisNodeDescriptor *node_descriptor,
        void (*seek)(struct GenesisNode *node));
GENESIS_EXPORT void genesis_node_descriptor_set_create_callback(struct GenesisNodeDescriptor *node_descriptor,
        int (*create)(struct GenesisNode *node));
GENESIS_EXPORT void genesis_node_descriptor_set_destroy_callback(struct GenesisNodeDescriptor *node_descriptor,
        void (*destroy)(struct GenesisNode *node));
GENESIS_EXPORT void *genesis_node_descriptor_userdata(const struct GenesisNodeDescriptor *node_descriptor);

// returns -1 if not found
GENESIS_EXPORT int genesis_node_descriptor_find_port_index(
        const struct GenesisNodeDescriptor *node_descriptor, const char *name);


GENESIS_EXPORT struct GenesisNode *genesis_node_descriptor_create_node(struct GenesisNodeDescriptor *node_descriptor);
GENESIS_EXPORT void genesis_node_destroy(struct GenesisNode *node);

GENESIS_EXPORT struct GenesisPort *genesis_node_port(struct GenesisNode *node, int port_index);
GENESIS_EXPORT struct GenesisNodeDescriptor *genesis_node_descriptor(struct GenesisNode *node);
GENESIS_EXPORT struct GenesisContext *genesis_node_context(struct GenesisNode *node);
GENESIS_EXPORT void genesis_node_disconnect_all_ports(struct GenesisNode *node);

GENESIS_EXPORT int genesis_connect_ports(struct GenesisPort *source, struct GenesisPort *dest);
GENESIS_EXPORT void genesis_disconnect_ports(struct GenesisPort *source, struct GenesisPort *dest);
// shortcut for connecting audio nodes. calls genesis_connect_ports internally
GENESIS_EXPORT int genesis_connect_audio_nodes(struct GenesisNode *source, struct GenesisNode *dest);


GENESIS_EXPORT struct GenesisNode *genesis_port_node(struct GenesisPort *port);

// name is duplicated internally
GENESIS_EXPORT struct GenesisPortDescriptor *genesis_node_descriptor_create_port(
        struct GenesisNodeDescriptor *node_descriptor, int port_index,
        enum GenesisPortType port_type, const char *name);

GENESIS_EXPORT void genesis_port_descriptor_set_connect_callback(
        struct GenesisPortDescriptor *port_descr,
        int (*connect)(struct GenesisPort *port, struct GenesisPort *other_port));

GENESIS_EXPORT void genesis_port_descriptor_set_disconnect_callback(
        struct GenesisPortDescriptor *port_descr,
        void (*disconnect)(struct GenesisPort *port, struct GenesisPort *other_port));


// if fixed is true then other_port_index is the index
// of the other port that it is the same as, or -1 if it is fixed
// to the value of channel_layout
GENESIS_EXPORT int genesis_audio_port_descriptor_set_channel_layout(
        struct GenesisPortDescriptor *audio_port_descr,
        const struct SoundIoChannelLayout *channel_layout, bool fixed, int other_port_index);

// if fixed is true then other_port_index is the index
// of the other port that it is the same as, or -1 if it is fixed
// to the value of sample_rate
GENESIS_EXPORT int genesis_audio_port_descriptor_set_sample_rate(
        struct GenesisPortDescriptor *audio_port_descr,
        int sample_rate, bool fixed, int other_port_index);

GENESIS_EXPORT void genesis_port_descriptor_destroy(struct GenesisPortDescriptor *port_descriptor);

GENESIS_EXPORT void genesis_debug_print_port_config(struct GenesisPort *port);
GENESIS_EXPORT void genesis_debug_print_pipeline(struct GenesisContext *context);

GENESIS_EXPORT int genesis_start_pipeline(struct GenesisContext *context, double time);
GENESIS_EXPORT void genesis_stop_pipeline(struct GenesisContext *context);
GENESIS_EXPORT int genesis_resume_pipeline(struct GenesisContext *context);

// can only set this when the pipeline is stopped.
// also if you change this, you must destroy and re-create nodes and node
// descriptors based on audio devices
GENESIS_EXPORT int genesis_set_latency(struct GenesisContext *context, double latency);
GENESIS_EXPORT double genesis_get_latency(struct GenesisContext *context);

// can only set this when the pipeline is stopped.
// also if you change this, you must destroy and re-create all nodes and node
// descriptors
GENESIS_EXPORT int genesis_set_sample_rate(struct GenesisContext *context, int sample_rate);
GENESIS_EXPORT int genesis_get_sample_rate(struct GenesisContext *context);

// can only set this when the pipeline is stopped.
// also if you change this, you must destroy and re-create all nodes and node
// descriptors
GENESIS_EXPORT void genesis_set_channel_layout(struct GenesisContext *context,
        const struct SoundIoChannelLayout *layout);
GENESIS_EXPORT struct SoundIoChannelLayout *genesis_get_channel_layout(struct GenesisContext *context);

// returns the number of frames available to read
GENESIS_EXPORT int genesis_audio_in_port_fill_count(struct GenesisPort *port);
GENESIS_EXPORT float *genesis_audio_in_port_read_ptr(struct GenesisPort *port);
GENESIS_EXPORT void genesis_audio_in_port_advance_read_ptr(struct GenesisPort *port, int frame_count);
GENESIS_EXPORT int genesis_audio_in_port_capacity(struct GenesisPort *port);

// returns the number of frames that can be written
GENESIS_EXPORT int genesis_audio_out_port_free_count(struct GenesisPort *port);
GENESIS_EXPORT float *genesis_audio_out_port_write_ptr(struct GenesisPort *port);
GENESIS_EXPORT void genesis_audio_out_port_advance_write_ptr(struct GenesisPort *port, int frame_count);

GENESIS_EXPORT int genesis_audio_port_bytes_per_frame(struct GenesisPort *port);
GENESIS_EXPORT int genesis_audio_port_sample_rate(struct GenesisPort *port);
GENESIS_EXPORT const struct SoundIoChannelLayout *genesis_audio_port_channel_layout(struct GenesisPort *port);

// time_requested is how much time in whole notes you want to be available.
// event_count is the number of events available to read.
// time_available is how much time in whole notes is accounted for in the buffer.
GENESIS_EXPORT void genesis_events_in_port_fill_count(struct GenesisPort *port,
        double time_requested, int *event_count, double *time_available);
// event_count is how many events you consumed. buf_size is the amount of whole notes you consumed.
GENESIS_EXPORT void genesis_events_in_port_advance_read_ptr(struct GenesisPort *port, int event_count, double buf_size);
GENESIS_EXPORT struct GenesisMidiEvent *genesis_events_in_port_read_ptr(struct GenesisPort *port);

// event_count is the number of events that can be written.
// time_requested is how much time in whole notes you should account for if you can.
GENESIS_EXPORT void genesis_events_out_port_free_count(struct GenesisPort *port,
        int *event_count, double *time_requested);
// event_count is how many events you wrote to the buffer. buf_size is the amount of whole notes
// you accounted for.
GENESIS_EXPORT void genesis_events_out_port_advance_write_ptr(struct GenesisPort *port, int event_count, double buf_size);
GENESIS_EXPORT struct GenesisMidiEvent *genesis_events_out_port_write_ptr(struct GenesisPort *port);


////////////// Formats and Codecs

GENESIS_EXPORT int genesis_in_format_count(struct GenesisContext *context);
GENESIS_EXPORT int genesis_out_format_count(struct GenesisContext *context);

GENESIS_EXPORT struct GenesisAudioFileFormat *genesis_in_format_index(
        struct GenesisContext *context, int format_index);
GENESIS_EXPORT struct GenesisRenderFormat *genesis_out_format_index(
        struct GenesisContext *context, int format_index);

GENESIS_EXPORT const char *genesis_audio_file_format_name(const struct GenesisAudioFileFormat *format);
GENESIS_EXPORT const char *genesis_audio_file_format_description(const struct GenesisAudioFileFormat *format);

GENESIS_EXPORT const char *genesis_render_format_name(const struct GenesisRenderFormat *format);
GENESIS_EXPORT const char *genesis_render_format_description(const struct GenesisRenderFormat *format);

GENESIS_EXPORT int genesis_audio_file_format_codec_count(const struct GenesisAudioFileFormat *format);

GENESIS_EXPORT struct GenesisAudioFileCodec * genesis_audio_file_format_codec_index(
        struct GenesisAudioFileFormat *format, int codec_index);

/// GenesisRenderFormat always have exactly one codec.
GENESIS_EXPORT struct GenesisAudioFileCodec * genesis_render_format_codec(struct GenesisRenderFormat *format);

GENESIS_EXPORT struct GenesisAudioFileCodec *genesis_guess_audio_file_codec(
        struct GenesisContext *context, const char *filename_hint,
        const char *format_name, const char *codec_name);

GENESIS_EXPORT const char *genesis_audio_file_codec_name(
        const struct GenesisAudioFileCodec *codec);

GENESIS_EXPORT const char *genesis_audio_file_codec_description(
        const struct GenesisAudioFileCodec *codec);


GENESIS_EXPORT int genesis_audio_file_codec_sample_format_count(
        const struct GenesisAudioFileCodec *codec);

GENESIS_EXPORT enum SoundIoFormat genesis_audio_file_codec_sample_format_index(
        const struct GenesisAudioFileCodec *codec, int index);

/// Returns the index
GENESIS_EXPORT int genesis_audio_file_codec_best_sample_format(
        const struct GenesisAudioFileCodec *codec);

GENESIS_EXPORT bool genesis_audio_file_codec_supports_sample_format(
        const struct GenesisAudioFileCodec *codec,
        enum SoundIoFormat sample_format);


GENESIS_EXPORT int genesis_audio_file_codec_sample_rate_count(
        const struct GenesisAudioFileCodec *codec);

GENESIS_EXPORT int genesis_audio_file_codec_sample_rate_index(
        const struct GenesisAudioFileCodec *codec, int index);

/// Returns the index
GENESIS_EXPORT int genesis_audio_file_codec_best_sample_rate(
        const struct GenesisAudioFileCodec *codec);

GENESIS_EXPORT bool genesis_audio_file_codec_supports_sample_rate(
        const struct GenesisAudioFileCodec *codec, int sample_rate);


GENESIS_EXPORT int genesis_audio_file_codec_bit_rate_count(
        const struct GenesisAudioFileCodec *codec);

GENESIS_EXPORT int genesis_audio_file_codec_bit_rate_index(
        const struct GenesisAudioFileCodec *codec, int index);

/// Returns the index
GENESIS_EXPORT int genesis_audio_file_codec_best_bit_rate(
        const struct GenesisAudioFileCodec *codec);


////////////////// Audio File

GENESIS_EXPORT int genesis_audio_file_load(struct GenesisContext *context,
        const char *input_filename, struct GenesisAudioFile **audio_file);

GENESIS_EXPORT struct GenesisAudioFile *genesis_audio_file_create(struct GenesisContext *context);
GENESIS_EXPORT void genesis_audio_file_set_sample_rate(struct GenesisAudioFile *audio_file,
        int sample_rate);
GENESIS_EXPORT int genesis_audio_file_set_channel_layout(struct GenesisAudioFile *audio_file,
        const struct SoundIoChannelLayout *channel_layout);

GENESIS_EXPORT void genesis_audio_file_destroy(struct GenesisAudioFile *audio_file);

GENESIS_EXPORT int genesis_audio_file_export(struct GenesisAudioFile *audio_file,
        const char *output_filename, struct GenesisExportFormat *export_format);

GENESIS_EXPORT const struct SoundIoChannelLayout *genesis_audio_file_channel_layout(
        const struct GenesisAudioFile *audio_file);
GENESIS_EXPORT long genesis_audio_file_frame_count(const struct GenesisAudioFile *audio_file);
GENESIS_EXPORT int genesis_audio_file_sample_rate(const struct GenesisAudioFile *audio_file);

GENESIS_EXPORT struct GenesisAudioFileIterator genesis_audio_file_iterator(
        struct GenesisAudioFile *audio_file, int channel_index, long start_frame_index);
GENESIS_EXPORT void genesis_audio_file_iterator_next(struct GenesisAudioFileIterator *it);



#endif
