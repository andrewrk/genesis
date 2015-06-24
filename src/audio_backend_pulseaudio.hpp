#ifndef AUDIO_BACKEND_PULSEAUDIO_HPP
#define AUDIO_BACKEND_PULSEAUDIO_HPP

#include <pulse/pulseaudio.h>
#include <atomic>
using std::atomic_bool;

struct AudioHardware;
struct AudioDevicesInfo;

struct OpenPlaybackDevicePulseAudio {
    pa_stream *stream;
    atomic_bool stream_ready;
    pa_buffer_attr buffer_attr;
};

struct OpenRecordingDevicePulseAudio {
    pa_stream *stream;
    atomic_bool stream_ready;
    pa_buffer_attr buffer_attr;
};

struct AudioHardwarePulseAudio {
    bool connection_refused;

    pa_context *pulse_context;
    atomic_bool device_scan_queued;

    // the one that we're working on building
    AudioDevicesInfo *current_audio_devices_info;
    char * default_sink_name;
    char * default_source_name;

    // this one is ready to be read with flush_events. protected by mutex
    AudioDevicesInfo *ready_audio_devices_info;

    bool have_sink_list;
    bool have_source_list;
    bool have_default_sink;

    atomic_bool ready_flag;
    atomic_bool have_devices_flag;

    pa_threaded_mainloop *main_loop;
    pa_proplist *props;
};

int audio_hardware_init_pulseaudio(AudioHardware *ah);

#endif
