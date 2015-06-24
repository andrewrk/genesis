#ifndef AUDIO_HARDWARE_HPP
#define AUDIO_HARDWARE_HPP

#include "list.hpp"
#include "sample_format.h"
#include "channel_layout.h"
#include "genesis.h"
#include "audio_backend_pulseaudio.hpp"
#include "audio_backend_dummy.hpp"

struct AudioHardware;

enum AudioBackend {
    AudioBackendPulseAudio,
    AudioBackendDummy,
};

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

struct OpenPlaybackDevice {
    GenesisAudioDevice *audio_device;
    GenesisSampleFormat sample_format;
    double latency;
    int bytes_per_frame;

    void *userdata;
    void (*underrun_callback)(OpenPlaybackDevice *);
    void (*write_callback)(OpenPlaybackDevice *, int frame_count);

    union {
        OpenPlaybackDevicePulseAudio pulse_audio;
        OpenPlaybackDeviceDummy dummy;
    } backend;
};

struct OpenRecordingDevice {
    GenesisAudioDevice *audio_device;
    GenesisSampleFormat sample_format;
    double latency;
    int bytes_per_frame;

    void *userdata;
    void (*read_callback)(OpenRecordingDevice *);

    union {
        OpenRecordingDevicePulseAudio pulse_audio;
        OpenRecordingDeviceDummy dummy;
    } backend;
};

struct AudioHardware {
    AudioBackend current_backend;
    GenesisContext *genesis_context;

    // this one is safe to read from the gui thread
    AudioDevicesInfo *safe_devices_info;

    void *userdata;
    void (*on_devices_change)(AudioHardware *);
    void (*on_events_signal)(AudioHardware *);


    union {
        AudioHardwarePulseAudio pulse_audio;
        AudioHardwareDummy dummy;
    } backend;
    void (*destroy)(AudioHardware *);
    void (*flush_events)(AudioHardware *);
    void (*refresh_audio_devices)(AudioHardware *);

    int (*open_playback_device_init)(AudioHardware *, OpenPlaybackDevice *);
    void (*open_playback_device_destroy)(AudioHardware *, OpenPlaybackDevice *);
    int (*open_playback_device_start)(AudioHardware *, OpenPlaybackDevice *);
    int (*open_playback_device_free_count)(AudioHardware *, OpenPlaybackDevice *);
    void (*open_playback_device_begin_write)(AudioHardware *, OpenPlaybackDevice *,
            char **data, int *frame_count);
    void (*open_playback_device_write)(AudioHardware *, OpenPlaybackDevice *,
            char *data, int frame_count);
    void (*open_playback_device_clear_buffer)(AudioHardware *, OpenPlaybackDevice *);

    int (*open_recording_device_init)(AudioHardware *, OpenRecordingDevice *);
    void (*open_recording_device_destroy)(AudioHardware *, OpenRecordingDevice *);
    int (*open_recording_device_start)(AudioHardware *, OpenRecordingDevice *);
    void (*open_recording_device_peek)(AudioHardware *, OpenRecordingDevice *,
            const char **data, int *frame_count);
    void (*open_recording_device_drop)(AudioHardware *, OpenRecordingDevice *);
    void (*open_recording_device_clear_buffer)(AudioHardware *, OpenRecordingDevice *);
};

int create_audio_hardware(GenesisContext *context, void *userdata,
        void (*on_devices_change)(AudioHardware *), void (*on_events_signal)(AudioHardware *),
        struct AudioHardware **out_audio_hardware);

void destroy_audio_hardware(struct AudioHardware *audio_hardware);

void audio_hardware_flush_events(struct AudioHardware *audio_hardware);



int open_playback_device_create(GenesisAudioDevice *audio_device, GenesisSampleFormat sample_format,
        double latency, void *userdata, void (*write_callback)(OpenPlaybackDevice *, int),
        void (*underrun_callback)(OpenPlaybackDevice *),
        OpenPlaybackDevice **out_open_playback_device);
void open_playback_device_destroy(OpenPlaybackDevice *open_playback_device);

int open_playback_device_start(OpenPlaybackDevice *open_playback_device);

void open_playback_device_fill_with_silence(OpenPlaybackDevice *open_playback_device);


// number of frames available to write
int open_playback_device_free_count(OpenPlaybackDevice *open_playback_device);
void open_playback_device_begin_write(OpenPlaybackDevice *open_playback_device, char **data, int *frame_count);
void open_playback_device_write(OpenPlaybackDevice *open_playback_device, char *data, int frame_count);

void open_playback_device_clear_buffer(OpenPlaybackDevice *open_playback_device);



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

#endif
