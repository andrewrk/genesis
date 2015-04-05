#ifndef AUDIO_HARDWARE_HPP
#define AUDIO_HARDWARE_HPP

#include "list.hpp"
#include "sample_format.h"
#include "channel_layout.h"
#include "byte_buffer.hpp"
#include "genesis.h"

#include <pulse/pulseaudio.h>
#include <atomic>
using std::atomic_bool;

struct GenesisAudioDevice {
    GenesisContext *context;
    ByteBuffer name;
    ByteBuffer description;
    GenesisChannelLayout channel_layout;
    GenesisSampleFormat default_sample_format;
    double default_latency;
    int default_sample_rate;
    GenesisAudioDevicePurpose purpose;
};

GenesisAudioDevice *duplicate_audio_device(GenesisAudioDevice *device);
void destroy_audio_device(GenesisAudioDevice *device);

struct AudioDevicesInfo {
    List<GenesisAudioDevice> devices;
    // can be -1 when default device is unknown
    int default_output_index;
    int default_input_index;
};

class AudioHardware;
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

class AudioHardware {
public:
    AudioHardware(GenesisContext *context);
    ~AudioHardware();

    // the device list is valid only for the duration of the callback
    void set_on_devices_change(void (*on_devices_change)(AudioHardware *)) {
        _on_devices_change = on_devices_change;
    }
    void clear_on_devices_change();

    void set_on_events_signal(void (*on_events_signal)(AudioHardware *)) {
        _on_events_signal = on_events_signal;
    }

    const AudioDevicesInfo *devices_info() const {
        return _safe_devices_info;
    }

    // call this and on_devices_change will be called 0 or 1 times, and then
    // this function will return
    void flush_events();

    void block_until_ready();
    void block_until_have_devices();

    void *_userdata;



    pa_context *_context;
    GenesisContext *_genesis_context;
private:
    atomic_bool _device_scan_queued;
    void (*_on_devices_change)(AudioHardware *);
    void (*_on_events_signal)(AudioHardware *);

    // the one that we're working on building
    AudioDevicesInfo *_current_audio_devices_info;
    ByteBuffer _default_sink_name;
    ByteBuffer _default_source_name;

    // this one is ready to be read with flush_events. protected by mutex
    AudioDevicesInfo *_ready_audio_devices_info;

    // this one is safe to read from the gui thread
    AudioDevicesInfo *_safe_devices_info;

    bool _have_sink_list;
    bool _have_source_list;
    bool _have_default_sink;

    atomic_bool _ready_flag;
    atomic_bool _have_devices_flag;

    pa_threaded_mainloop *_main_loop;

    void subscribe_to_events();
    void scan_devices();

    void context_state_callback(pa_context *context);
    void sink_info_callback(pa_context *context, const pa_sink_info *info, int eol);
    void source_info_callback(pa_context *context, const pa_source_info *info, int eol);
    void server_info_callback(pa_context *context, const pa_server_info *info);

    void destroy_current_audio_devices_info();
    void destroy_ready_audio_devices_info();
    void initialize_current_device_list();
    void set_ready_flag();
    void finish_device_query();
    int perform_operation(pa_operation *op);
    void subscribe_callback(pa_context *context, pa_subscription_event_type_t event_type, uint32_t index);

    static void context_state_callback(pa_context *context, void *userdata) {
        return static_cast<AudioHardware*>(userdata)->context_state_callback(context);
    }
    static void sink_info_callback(pa_context *context, const pa_sink_info *info, int eol, void *userdata) {
        return static_cast<AudioHardware*>(userdata)->sink_info_callback(context, info, eol);
    }
    static void static_source_info_callback(pa_context *context,
            const pa_source_info *info, int eol, void *userdata)
    {
        return static_cast<AudioHardware*>(userdata)->source_info_callback(context, info, eol);
    }
    static void server_info_callback(pa_context *context, const pa_server_info *info, void *userdata) {
        return static_cast<AudioHardware*>(userdata)->server_info_callback(context, info);
    }

    static void static_subscribe_callback(pa_context *context,
            pa_subscription_event_type_t event_type, uint32_t index, void *userdata)
    {
        return static_cast<AudioHardware*>(userdata)->subscribe_callback(context, event_type, index);
    }

    AudioHardware(const AudioHardware &copy) = delete;
    AudioHardware &operator=(const AudioHardware &copy) = delete;

    friend class OpenPlaybackDevice;
    friend class OpenRecordingDevice;
};

#endif
