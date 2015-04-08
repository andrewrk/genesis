#ifndef AUDIO_HARDWARE_HPP
#define AUDIO_HARDWARE_HPP

#include "list.hpp"
#include "sample_format.h"
#include "channel_layout.h"
#include "genesis.h"

#include <pulse/pulseaudio.h>
#include <atomic>
using std::atomic_bool;

struct AudioHardware;

struct GenesisAudioDevice {
    AudioHardware *audio_hardware;
    char *name;
    char *description;
    GenesisChannelLayout channel_layout;
    GenesisSampleFormat default_sample_format;
    double default_latency;
    int default_sample_rate;
    GenesisAudioDevicePurpose purpose;
    int ref_count;
};

struct AudioDevicesInfo {
    List<GenesisAudioDevice *> devices;
    // can be -1 when default device is unknown
    int default_output_index;
    int default_input_index;
};

typedef void PaStream;

struct OpenPlaybackDevice {
    GenesisAudioDevice *audio_device;
    pa_stream *stream;

    void *userdata;
    void (*underrun_callback)(OpenPlaybackDevice *);
    void (*write_callback)(OpenPlaybackDevice *, int frame_count);

    atomic_bool stream_ready;
    pa_buffer_attr buffer_attr;

    int bytes_per_frame;
};

int open_playback_device_create(GenesisAudioDevice *audio_device, GenesisSampleFormat sample_format,
        double latency, void *userdata, void (*write_callback)(OpenPlaybackDevice *, int),
        void (*underrun_callback)(OpenPlaybackDevice *),
        OpenPlaybackDevice **out_open_playback_device);
void open_playback_device_destroy(OpenPlaybackDevice *open_playback_device);

int open_playback_device_start(OpenPlaybackDevice *open_playback_device);

void open_playback_device_lock(OpenPlaybackDevice *open_playback_device);
void open_playback_device_unlock(OpenPlaybackDevice *open_playback_device);

// number of frames available to write
int open_playback_device_free_count(OpenPlaybackDevice *open_playback_device);
void open_playback_device_begin_write(OpenPlaybackDevice *open_playback_device, char **data, int *frame_count);
void open_playback_device_write(OpenPlaybackDevice *open_playback_device, char *data, int frame_count);

void open_playback_device_clear_buffer(OpenPlaybackDevice *open_playback_device);

struct OpenRecordingDevice {
    GenesisAudioDevice *audio_device;
    pa_stream *stream;
    atomic_bool stream_ready;

    void *userdata;
    void (*read_callback)(OpenRecordingDevice *);

    pa_buffer_attr buffer_attr;

    int bytes_per_frame;
};

int open_recording_device_create(GenesisAudioDevice *audio_device,
        GenesisSampleFormat sample_format, double latency, void *userdata,
        void (*read_callback)(OpenRecordingDevice *),
        OpenRecordingDevice **out_open_recording_device);
void open_recording_device_destroy(OpenRecordingDevice *open_recording_device);

int open_recording_device_start(OpenRecordingDevice *open_recording_device);
void open_recording_device_peek(OpenRecordingDevice *open_recording_device,
        const char **data, int *frame_count);
void open_recording_device_drop(OpenRecordingDevice *open_recording_device);

void open_recording_device_clear_buffer(OpenRecordingDevice *open_recording_device);

struct AudioHardware {
    pa_context *pulse_context;
    GenesisContext *genesis_context;
    atomic_bool device_scan_queued;

    void *userdata;
    void (*on_devices_change)(AudioHardware *);
    void (*on_events_signal)(AudioHardware *);

    // the one that we're working on building
    AudioDevicesInfo *current_audio_devices_info;
    char * default_sink_name;
    char * default_source_name;

    // this one is ready to be read with flush_events. protected by mutex
    AudioDevicesInfo *ready_audio_devices_info;

    // this one is safe to read from the gui thread
    AudioDevicesInfo *safe_devices_info;

    bool have_sink_list;
    bool have_source_list;
    bool have_default_sink;

    atomic_bool ready_flag;
    atomic_bool have_devices_flag;

    pa_threaded_mainloop *main_loop;
    pa_proplist *props;
};

int create_audio_hardware(GenesisContext *context, void *userdata,
        void (*on_devices_change)(AudioHardware *), void (*on_events_signal)(AudioHardware *),
        struct AudioHardware **out_audio_hardware);

void destroy_audio_hardware(struct AudioHardware *audio_hardware);

void audio_hardware_flush_events(struct AudioHardware *audio_hardware);

#endif
