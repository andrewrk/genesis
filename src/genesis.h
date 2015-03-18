#ifndef GENESIS_GENESIS_H
#define GENESIS_GENESIS_H

#include "config.h"
#include "channel_layout.h"
#include "sample_format.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

struct GenesisContext;
struct GenesisContext *genesis_create_context(void);
void genesis_destroy_context(struct GenesisContext *context);
void genesis_flush_events(struct GenesisContext *context);

// flushes events as they occur, blocks until you call genesis_wakeup
void genesis_wait_events(struct GenesisContext *context);

// makes genesis_wait_events stop blocking
void genesis_wakeup(struct GenesisContext *context);

enum GenesisChannelConfig {
    // there is a fixed channel layout that must be respected
    GenesisChannelConfigFixed,
    // any channel layout is supported
    GenesisChannelConfigAny,
    // any layout; the output layout is the same as the input layout
    GenesisChannelConfigSame,
};

enum GenesisPortType {
    GenesisPortTypeAudioIn,
    GenesisPortTypeAudioOut,
    GenesisPortTypeNotesIn,
    GenesisPortTypeNotesOut,
    GenesisPortTypeParamIn,
    GenesisPortTypeParamOut,
};

struct GenesisPortDescriptor;
struct GenesisNodeDescriptor;
struct GenesisNodeDescriptor *genesis_node_descriptor_find(
        struct GenesisContext *context, const char *name);
const char *genesis_node_descriptor_name(const struct GenesisNodeDescriptor *node_descriptor);


struct GenesisPort;
struct GenesisNode;
struct GenesisNode *genesis_node_create(struct GenesisNodeDescriptor *node_descriptor);
void genesis_node_destroy(struct GenesisNode *node);


enum GenesisAudioDevicePurpose {
    GenesisAudioDevicePurposePlayback,
    GenesisAudioDevicePurposeRecording,
};

struct GenesisAudioDevice;
// call flush_events to refresh this information. when you call flush_events,
// the information you got previously from these methods becomes invalid. This
// applies to the count of devices and the pointers to each GenesisAudioDevice

// returns -1 on error
int genesis_audio_device_count(struct GenesisContext *context);

// returns NULL on error
struct GenesisAudioDevice *genesis_audio_device_get(struct GenesisContext *context, int index);

// returns -1 on error
int genesis_audio_device_default_playback(struct GenesisContext *context);

// returns -1 on error
int genesis_audio_device_default_recording(struct GenesisContext *context);

// the name is the identifier for the device. UTF-8 encoded
const char *genesis_audio_device_name(const struct GenesisAudioDevice *device);

// UTF-8 encoded
const char *genesis_audio_device_description(const struct GenesisAudioDevice *device);

enum GenesisAudioDevicePurpose genesis_audio_device_purpose(const struct GenesisAudioDevice *device);

// set callback to be called when audio devices change
void genesis_audio_device_set_callback(struct GenesisContext *context,
        void (*callback)(void *userdata),
        void *userdata);



struct GenesisAudioFile;

struct GenesisAudioFileFormat;
struct GenesisAudioFileCodec;

struct GenesisExportFormat {
    enum GenesisSampleFormat sample_format;
    bool planar;
    int bit_rate;
    struct GenesisAudioFileFormat *format;
    struct GenesisAudioFileCodec *codec;
};
enum GenesisError genesis_audio_file_load(struct GenesisContext *context,
        const char *input_filename, struct GenesisAudioFile **audio_file);

enum GenesisError genesis_audio_file_export(struct GenesisAudioFile *audio_file,
        const char *output_filename, struct GenesisExportFormat *export_format);

void genesis_audio_file_destroy(struct GenesisAudioFile *audio_file);

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
const char *genesis_audio_file_codec_name(const struct GenesisAudioFileCodec *codec);
const char *genesis_audio_file_codec_description(const struct GenesisAudioFileCodec *codec);


#ifdef __cplusplus
}
#endif
#endif
