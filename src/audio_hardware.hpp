#ifndef AUDIO_HARDWARE_HPP
#define AUDIO_HARDWARE_HPP

#include "list.hpp"
#include "sample_format.h"
#include "channel_layout.h"
#include "genesis.h"

#include <pulse/pulseaudio.h>
#include <atomic>
using std::atomic_bool;

struct GenesisAudioDevice {
    GenesisContext *context;
    char *name;
    char *description;
    GenesisChannelLayout channel_layout;
    GenesisSampleFormat default_sample_format;
    double default_latency;
    int default_sample_rate;
    GenesisAudioDevicePurpose purpose;
    int ref_count;
};

void audio_device_unref(struct GenesisAudioDevice *device);
void audio_device_ref(struct GenesisAudioDevice *device);

struct AudioDevicesInfo {
    List<GenesisAudioDevice *> devices;
    // can be -1 when default device is unknown
    int default_output_index;
    int default_input_index;
};

struct AudioHardware;
typedef void PaStream;
class OpenPlaybackDevice {
public:
    OpenPlaybackDevice(AudioHardware *audio_hardware,
            const GenesisChannelLayout *channel_layout, GenesisSampleFormat sample_format, double latency,
            int sample_rate, void (*callback)(int byte_count, void *), void *userdata);
    ~OpenPlaybackDevice();

    int start(const char *device_name);

    int writable_size();
    void begin_write(char **data, int *byte_count);
    void write(char *data, int byte_count);
    void clear_buffer();

    void lock();
    void unlock();

    AudioHardware *_audio_hardware;
    pa_stream *_stream;

    void (*_underrun_callback)(void *);
    void set_underrun_callback(void (*fn)(void *)) {
        _underrun_callback = fn;
    }


    atomic_bool _stream_ready;
    void *_callback_userdata;
    void (*_callback)(int byte_count, void *);
    pa_buffer_attr _buffer_attr;

    void stream_state_callback(pa_stream *stream);

    static void stream_state_callback(pa_stream *stream, void *userdata) {
        return static_cast<OpenPlaybackDevice*>(userdata)->stream_state_callback(stream);
    }

    static void stream_write_callback(pa_stream *stream, size_t nbytes, void *userdata) {
        OpenPlaybackDevice *device = static_cast<OpenPlaybackDevice*>(userdata);
        device->_callback((int)nbytes, device->_callback_userdata);
    }

    OpenPlaybackDevice(const OpenPlaybackDevice &copy) = delete;
    OpenPlaybackDevice &operator=(const OpenPlaybackDevice &copy) = delete;
};

class OpenRecordingDevice {
public:
    OpenRecordingDevice(AudioHardware *audio_hardware,
        const GenesisChannelLayout *channel_layout, GenesisSampleFormat sample_format, double latency,
        int sample_rate, void (*callback)(void *), void *userdata);
    ~OpenRecordingDevice();

    int start(const char *device_name);

    void peek(const char **data, int *byte_count);
    void drop();
    void clear_buffer();

private:
    AudioHardware *_audio_hardware;
    pa_stream *_stream;
    atomic_bool _stream_ready;
    void *_callback_userdata;
    void (*_callback)(void *);
    pa_buffer_attr _buffer_attr;

    void stream_state_callback(pa_stream *stream);

    static void static_stream_state_callback(pa_stream *stream, void *userdata) {
        return static_cast<OpenRecordingDevice*>(userdata)->stream_state_callback(stream);
    }

    static void stream_read_callback(pa_stream *stream, size_t nbytes, void *userdata) {
        OpenRecordingDevice *device = static_cast<OpenRecordingDevice*>(userdata);
        device->_callback(device->_callback_userdata);
    }

    OpenRecordingDevice(const OpenRecordingDevice &copy) = delete;
    OpenRecordingDevice &operator=(const OpenRecordingDevice &copy) = delete;
};

struct AudioHardware {
    pa_context *pulse_context;
    GenesisContext *genesis_context;
    atomic_bool device_scan_queued;
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

    void *userdata;
};

int create_audio_hardware(GenesisContext *context, void *userdata,
        void (*on_devices_change)(AudioHardware *), void (*on_events_signal)(AudioHardware *),
        struct AudioHardware **out_audio_hardware);

void destroy_audio_hardware(struct AudioHardware *audio_hardware);

void audio_hardware_flush_events(struct AudioHardware *audio_hardware);

#endif
