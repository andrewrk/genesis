#include "audio_hardware.hpp"
#include "util.hpp"
#include "genesis.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

void default_on_devices_change(AudioHardware *audio_hardware) {
    // do nothing
}

AudioHardware::AudioHardware() :
    _userdata(NULL),
    _device_scan_queued(false),
    _on_devices_change(default_on_devices_change),
    _current_audio_devices_info(NULL),
    _ready_audio_devices_info(NULL),
    _safe_devices_info(NULL),
    _ready_flag(false),
    _have_devices_flag(false)
{
    _main_loop = pa_threaded_mainloop_new();
    if (!_main_loop)
        panic("unable to create pulseaudio mainloop");

    pa_mainloop_api *main_loop_api = pa_threaded_mainloop_get_api(_main_loop);

    pa_proplist *props = pa_proplist_new();
    if (!props)
        panic("unable to allocate property list");
    pa_proplist_sets(props, PA_PROP_APPLICATION_NAME, "Genesis Audio Software");
    pa_proplist_sets(props, PA_PROP_APPLICATION_VERSION, GENESIS_VERSION_STRING);
    pa_proplist_sets(props, PA_PROP_APPLICATION_ID, "me.andrewkelley.genesis");

    _context = pa_context_new_with_proplist(main_loop_api, "Genesis", props);
    if (!_context)
        panic("unable to create pulseaudio context");

    pa_proplist_free(props);

    pa_context_set_subscribe_callback(_context, static_subscribe_callback, this);
    pa_context_set_state_callback(_context, context_state_callback, this);

    int err = pa_context_connect(_context, NULL, (pa_context_flags_t)0, NULL);
    if (err)
        panic("pulseaudio context connect failed");

    if (pa_threaded_mainloop_start(_main_loop))
        panic("unable to start pulseaudio main loop");
}

AudioHardware::~AudioHardware() {
    pa_threaded_mainloop_stop(_main_loop);

    destroy_current_audio_devices_info();
    destroy_ready_audio_devices_info();

    pa_context_disconnect(_context);
    pa_context_unref(_context);

    pa_threaded_mainloop_free(_main_loop);
}

void AudioHardware::clear_on_devices_change() {
    _on_devices_change = default_on_devices_change;
}

void AudioHardware::subscribe_callback(pa_context *context,
        pa_subscription_event_type_t event_bits, uint32_t index)
{
    int facility = (event_bits & PA_SUBSCRIPTION_EVENT_FACILITY_MASK);
    if (facility == PA_SUBSCRIPTION_EVENT_SOURCE || facility == PA_SUBSCRIPTION_EVENT_SINK)
        _device_scan_queued = true;
}

void AudioHardware::destroy_ready_audio_devices_info() {
    if (_ready_audio_devices_info) {
        destroy(_ready_audio_devices_info, 1);
        _ready_audio_devices_info = NULL;
    }
}

void AudioHardware::destroy_current_audio_devices_info() {
    if (_current_audio_devices_info) {
        destroy(_current_audio_devices_info, 1);
        _current_audio_devices_info = NULL;
    }
}

void AudioHardware::initialize_current_device_list() {
    destroy_current_audio_devices_info();
    _current_audio_devices_info = create<AudioDevicesInfo>();
}

void AudioHardware::context_state_callback(pa_context *context) {
    switch (pa_context_get_state(context)) {
    case PA_CONTEXT_UNCONNECTED: // The context hasn't been connected yet.
        return;
    case PA_CONTEXT_CONNECTING: // A connection is being established.
        return;
    case PA_CONTEXT_AUTHORIZING: // The client is authorizing itself to the daemon.
        return;
    case PA_CONTEXT_SETTING_NAME: // The client is passing its application name to the daemon.
        return;
    case PA_CONTEXT_READY: // The connection is established, the context is ready to execute operations.
        _device_scan_queued = true;
        subscribe_to_events();
        _ready_flag = true;
        pa_threaded_mainloop_signal(_main_loop, 0);
        return;
    case PA_CONTEXT_TERMINATED: // The connection was terminated cleanly.
        pa_threaded_mainloop_signal(_main_loop, 0);
        return;
    case PA_CONTEXT_FAILED: // The connection failed or was disconnected.
        panic("pulsaudio connect failure: %s", pa_strerror(pa_context_errno(context)));
    }
}

static double usec_to_sec(pa_usec_t usec) {
    return (double)usec / (double)PA_USEC_PER_SEC;
}

static SampleFormat sample_format_from_pulseaudio(pa_sample_spec sample_spec) {
    switch (sample_spec.format) {
    case PA_SAMPLE_U8:        return SampleFormatUInt8;
    case PA_SAMPLE_S16NE:     return SampleFormatInt16;
    case PA_SAMPLE_S32NE:     return SampleFormatInt32;
    case PA_SAMPLE_FLOAT32NE: return SampleFormatFloat;
    default:                  return SampleFormatInvalid;
    }
}

static int sample_rate_from_pulseaudio(pa_sample_spec sample_spec) {
    return sample_spec.rate;
}

void AudioHardware::finish_device_query() {
    if (!_have_sink_list || !_have_source_list || !_have_default_sink)
        return;

    // based on the default sink name, figure out the default output index
    _current_audio_devices_info->default_output_index = -1;
    _current_audio_devices_info->default_input_index = -1;
    for (int i = 0; i < _current_audio_devices_info->devices.length(); i += 1) {
        AudioDevice *audio_device = &_current_audio_devices_info->devices.at(i);
        if (audio_device->purpose == GenesisAudioDevicePurposePlayback &&
            ByteBuffer::equal(audio_device->name, _default_sink_name))
        {
            _current_audio_devices_info->default_output_index = i;
        } else if (audio_device->purpose == GenesisAudioDevicePurposeRecording &&
            ByteBuffer::equal(audio_device->name, _default_source_name))
        {
            _current_audio_devices_info->default_input_index = i;
        }
    }

    destroy_ready_audio_devices_info();
    _ready_audio_devices_info = _current_audio_devices_info;
    _current_audio_devices_info = NULL;
    _have_devices_flag = true;
    pa_threaded_mainloop_signal(_main_loop, 0);
}

static GenesisChannelId from_pulseaudio_channel_pos(pa_channel_position_t pos) {
    switch (pos) {
    case PA_CHANNEL_POSITION_MONO: return GenesisChannelIdFrontCenter;
    case PA_CHANNEL_POSITION_FRONT_LEFT: return GenesisChannelIdFrontLeft;
    case PA_CHANNEL_POSITION_FRONT_RIGHT: return GenesisChannelIdFrontRight;
    case PA_CHANNEL_POSITION_FRONT_CENTER: return GenesisChannelIdFrontCenter;
    case PA_CHANNEL_POSITION_REAR_CENTER: return GenesisChannelIdBackCenter;
    case PA_CHANNEL_POSITION_REAR_LEFT: return GenesisChannelIdBackLeft;
    case PA_CHANNEL_POSITION_REAR_RIGHT: return GenesisChannelIdBackRight;
    case PA_CHANNEL_POSITION_LFE: return GenesisChannelIdLowFrequency;
    case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER: return GenesisChannelIdFrontLeftOfCenter;
    case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER: return GenesisChannelIdFrontRightOfCenter;
    case PA_CHANNEL_POSITION_SIDE_LEFT: return GenesisChannelIdSideLeft;
    case PA_CHANNEL_POSITION_SIDE_RIGHT: return GenesisChannelIdSideRight;
    case PA_CHANNEL_POSITION_TOP_CENTER: return GenesisChannelIdTopCenter;
    case PA_CHANNEL_POSITION_TOP_FRONT_LEFT: return GenesisChannelIdTopFrontLeft;
    case PA_CHANNEL_POSITION_TOP_FRONT_RIGHT: return GenesisChannelIdTopFrontRight;
    case PA_CHANNEL_POSITION_TOP_FRONT_CENTER: return GenesisChannelIdTopFrontCenter;
    case PA_CHANNEL_POSITION_TOP_REAR_LEFT: return GenesisChannelIdTopBackLeft;
    case PA_CHANNEL_POSITION_TOP_REAR_RIGHT: return GenesisChannelIdTopBackRight;
    case PA_CHANNEL_POSITION_TOP_REAR_CENTER: return GenesisChannelIdTopBackCenter;

    default:
        panic("cannot map pulseaudio channel to genesis");
    }
}

static void set_from_pulseaudio_channel_map(pa_channel_map channel_map, GenesisChannelLayout *channel_layout) {
    channel_layout->channel_count = channel_map.channels;
    for (int i = 0; i < channel_map.channels; i += 1) {
        channel_layout->channels[i] = from_pulseaudio_channel_pos(channel_map.map[i]);
    }
}

void AudioHardware::sink_info_callback(pa_context *context, const pa_sink_info *info, int eol) {
    if (eol) {
        _have_sink_list = true;
        finish_device_query();
    } else {
        List<AudioDevice> *devices = &_current_audio_devices_info->devices;
        _current_audio_devices_info->devices.resize(devices->length() + 1);
        AudioDevice *audio_device = &devices->at(devices->length() - 1);
        audio_device->name = info->name;
        audio_device->description = info->description;
        set_from_pulseaudio_channel_map(info->channel_map, &audio_device->channel_layout);
        audio_device->default_sample_format = sample_format_from_pulseaudio(info->sample_spec);
        audio_device->default_latency = usec_to_sec(info->configured_latency);
        audio_device->default_sample_rate = sample_rate_from_pulseaudio(info->sample_spec);
        audio_device->purpose = GenesisAudioDevicePurposePlayback;
    }
    pa_threaded_mainloop_signal(_main_loop, 0);
}

void AudioHardware::source_info_callback(pa_context *context, const pa_source_info *info, int eol) {
    if (eol) {
        _have_source_list = true;
        finish_device_query();
    } else {
        List<AudioDevice> *devices = &_current_audio_devices_info->devices;
        _current_audio_devices_info->devices.resize(devices->length() + 1);
        AudioDevice *audio_device = &devices->at(devices->length() - 1);
        audio_device->name = info->name;
        audio_device->description = info->description;
        set_from_pulseaudio_channel_map(info->channel_map, &audio_device->channel_layout);
        audio_device->default_sample_format = sample_format_from_pulseaudio(info->sample_spec);
        audio_device->default_latency = usec_to_sec(info->configured_latency);
        audio_device->default_sample_rate = sample_rate_from_pulseaudio(info->sample_spec);
        audio_device->purpose = GenesisAudioDevicePurposeRecording;
    }
    pa_threaded_mainloop_signal(_main_loop, 0);
}

void AudioHardware::server_info_callback(pa_context *context, const pa_server_info *info) {
    _default_sink_name = info->default_sink_name;
    _default_source_name = info->default_source_name;

    _have_default_sink = true;
    finish_device_query();
    pa_threaded_mainloop_signal(_main_loop, 0);
}

int AudioHardware::perform_operation(pa_operation *op) {
    for (;;) {
        switch (pa_operation_get_state(op)) {
        case PA_OPERATION_RUNNING:
            pa_threaded_mainloop_wait(_main_loop);
            continue;
        case PA_OPERATION_DONE:
            pa_operation_unref(op);
            return 0;
        case PA_OPERATION_CANCELLED:
            pa_operation_unref(op);
            return -1;
        }
    }
}

void AudioHardware::subscribe_to_events() {
    pa_subscription_mask_t events = (pa_subscription_mask_t)(
            PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE);
    pa_operation *subscribe_op = pa_context_subscribe(_context, events, nullptr, this);
    if (!subscribe_op)
        panic("pa_context_subscribe failed: %s", pa_strerror(pa_context_errno(_context)));
    pa_operation_unref(subscribe_op);
}

void AudioHardware::scan_devices() {
    _have_sink_list = false;
    _have_default_sink = false;
    _have_source_list = false;

    initialize_current_device_list();

    pa_threaded_mainloop_lock(_main_loop);

    pa_operation *list_sink_op = pa_context_get_sink_info_list(_context, sink_info_callback, this);
    pa_operation *list_source_op = pa_context_get_source_info_list(_context,
            static_source_info_callback, this);
    pa_operation *server_info_op = pa_context_get_server_info(_context, server_info_callback, this);

    if (perform_operation(list_sink_op))
        panic("list sinks failed");
    if (perform_operation(list_source_op))
        panic("list sources failed");
    if (perform_operation(server_info_op))
        panic("get server info failed");

    pa_threaded_mainloop_signal(_main_loop, 0);

    pa_threaded_mainloop_unlock(_main_loop);
}

void AudioHardware::flush_events() {
    if (_device_scan_queued) {
        _device_scan_queued = false;
        scan_devices();
    }

    AudioDevicesInfo *old_devices_info = nullptr;
    bool change = false;

    pa_threaded_mainloop_lock(_main_loop);

    if (_ready_audio_devices_info) {
        old_devices_info = _safe_devices_info;
        _safe_devices_info = _ready_audio_devices_info;
        _ready_audio_devices_info = nullptr;
        change = true;
    }

    pa_threaded_mainloop_unlock(_main_loop);

    if (change)
        _on_devices_change(this);

    if (old_devices_info)
        destroy(old_devices_info, 1);
}

void AudioHardware::block_until_ready() {
    if (_ready_flag)
        return;
    pa_threaded_mainloop_lock(_main_loop);
    while (!_ready_flag) {
        pa_threaded_mainloop_wait(_main_loop);
    }
    pa_threaded_mainloop_unlock(_main_loop);
}

void AudioHardware::block_until_have_devices() {
    if (_have_devices_flag)
        return;
    pa_threaded_mainloop_lock(_main_loop);
    while (!_have_devices_flag) {
        pa_threaded_mainloop_wait(_main_loop);
    }
    pa_threaded_mainloop_unlock(_main_loop);
}

static pa_sample_format_t to_pulseaudio_sample_format(SampleFormat sample_format) {
    switch (sample_format) {
    case SampleFormatUInt8:
        return PA_SAMPLE_U8;
    case SampleFormatInt16:
        return PA_SAMPLE_S16NE;
    case SampleFormatInt32:
        return PA_SAMPLE_S32NE;
    case SampleFormatFloat:
        return PA_SAMPLE_FLOAT32NE;
    case SampleFormatDouble:
        panic("cannot use double sample format with pulseaudio");
    case SampleFormatInvalid:
        panic("invalid sample format");
    }
    panic("invalid sample format");
}

static pa_channel_position_t to_pulseaudio_channel_pos(GenesisChannelId channel_id) {
    switch (channel_id) {
    case GenesisChannelIdInvalid:
        panic("invalid channel id");
    case GenesisChannelIdFrontLeft:
        return PA_CHANNEL_POSITION_FRONT_LEFT;
    case GenesisChannelIdFrontRight:
        return PA_CHANNEL_POSITION_FRONT_RIGHT;
    case GenesisChannelIdFrontCenter:
        return PA_CHANNEL_POSITION_FRONT_CENTER;
    case GenesisChannelIdLowFrequency:
        return PA_CHANNEL_POSITION_LFE;
    case GenesisChannelIdBackLeft:
        return PA_CHANNEL_POSITION_REAR_LEFT;
    case GenesisChannelIdBackRight:
        return PA_CHANNEL_POSITION_REAR_RIGHT;
    case GenesisChannelIdFrontLeftOfCenter:
        return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
    case GenesisChannelIdFrontRightOfCenter:
        return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
    case GenesisChannelIdBackCenter:
        return PA_CHANNEL_POSITION_REAR_CENTER;
    case GenesisChannelIdSideLeft:
        return PA_CHANNEL_POSITION_SIDE_LEFT;
    case GenesisChannelIdSideRight:
        return PA_CHANNEL_POSITION_SIDE_RIGHT;
    case GenesisChannelIdTopCenter:
        return PA_CHANNEL_POSITION_TOP_CENTER;
    case GenesisChannelIdTopFrontLeft:
        return PA_CHANNEL_POSITION_TOP_FRONT_LEFT;
    case GenesisChannelIdTopFrontCenter:
        return PA_CHANNEL_POSITION_TOP_FRONT_CENTER;
    case GenesisChannelIdTopFrontRight:
        return PA_CHANNEL_POSITION_TOP_FRONT_RIGHT;
    case GenesisChannelIdTopBackLeft:
        return PA_CHANNEL_POSITION_TOP_REAR_LEFT;
    case GenesisChannelIdTopBackCenter:
        return PA_CHANNEL_POSITION_TOP_REAR_CENTER;
    case GenesisChannelIdTopBackRight:
        return PA_CHANNEL_POSITION_TOP_REAR_RIGHT;
    }
    panic("invalid channel id");
}

static pa_channel_map to_pulseaudio_channel_map(const GenesisChannelLayout *channel_layout) {
    pa_channel_map channel_map;
    channel_map.channels = channel_layout->channel_count;

    if ((unsigned)channel_layout->channel_count > PA_CHANNELS_MAX)
        panic("channel layout greater than pulseaudio max channels");

    for (int i = 0; i < channel_layout->channel_count; i += 1)
        channel_map.map[i] = to_pulseaudio_channel_pos(channel_layout->channels[i]);

    return channel_map;
}

void OpenPlaybackDevice::stream_state_callback(pa_stream *stream) {
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_READY:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_FAILED:
            panic("pulseaudio stream error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
            break;
    }
}

OpenPlaybackDevice::OpenPlaybackDevice(AudioHardware *audio_hardware, const char *device_name,
        const GenesisChannelLayout *channel_layout, SampleFormat sample_format, double latency,
        int sample_rate, bool *ok) :
    _audio_hardware(audio_hardware),
    _stream(NULL)
{
    if (!_audio_hardware->_ready_flag)
        panic("audio hardware not ready");

    pa_threaded_mainloop_lock(_audio_hardware->_main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_sample_format(sample_format);
    sample_spec.rate = sample_rate;
    sample_spec.channels = channel_layout->channel_count;
    pa_channel_map channel_map = to_pulseaudio_channel_map(channel_layout);
    _stream = pa_stream_new(_audio_hardware->_context, "Genesis", &sample_spec, &channel_map);
    if (!_stream)
        panic("unable to create pulseaudio stream");

    pa_stream_set_state_callback(_stream, stream_state_callback, this);

    int bytes_per_second = get_bytes_per_second(sample_format, channel_layout->channel_count, sample_rate);
    int buffer_length = latency * bytes_per_second;

    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = buffer_length;
    buffer_attr.tlength = buffer_length;
    buffer_attr.prebuf = buffer_attr.maxlength;
    buffer_attr.minreq = UINT32_MAX;
    buffer_attr.fragsize = UINT32_MAX;

    int err = pa_stream_connect_playback(_stream, device_name, &buffer_attr, PA_STREAM_ADJUST_LATENCY, NULL, NULL);
    if (err)
        panic("unable to connect pulseaudio playback stream");

    *ok = true;

    pa_threaded_mainloop_unlock(_audio_hardware->_main_loop);
}

OpenPlaybackDevice::~OpenPlaybackDevice() {
    pa_threaded_mainloop_lock(_audio_hardware->_main_loop);

    pa_stream_set_state_callback(_stream, NULL, this);
    pa_stream_disconnect(_stream);
    pa_stream_unref(_stream);

    pa_threaded_mainloop_unlock(_audio_hardware->_main_loop);
}

int OpenPlaybackDevice::writable_size() {
    return pa_stream_writable_size(_stream);
}

void OpenPlaybackDevice::begin_write(char **data, int *byte_count) {
    size_t size_t_byte_count = *byte_count;
    pa_threaded_mainloop_lock(_audio_hardware->_main_loop);
    if (pa_stream_begin_write(_stream, (void**)data, &size_t_byte_count))
        panic("pa_stream_begin_write error: %s", pa_strerror(pa_context_errno(_audio_hardware->_context)));
    pa_threaded_mainloop_unlock(_audio_hardware->_main_loop);
    *byte_count = size_t_byte_count;
}

void OpenPlaybackDevice::write(char *data, int byte_count) {
    pa_threaded_mainloop_lock(_audio_hardware->_main_loop);
    if (pa_stream_write(_stream, data, byte_count, NULL, 0, PA_SEEK_RELATIVE))
        panic("pa_stream_write error: %s", pa_strerror(pa_context_errno(_audio_hardware->_context)));
    pa_threaded_mainloop_unlock(_audio_hardware->_main_loop);
}

void OpenPlaybackDevice::clear_buffer() {
    pa_threaded_mainloop_lock(_audio_hardware->_main_loop);
    pa_operation *op = pa_stream_flush(_stream, NULL, NULL);
    if (!op)
        panic("pa_stream_flush failed: %s", pa_strerror(pa_context_errno(_audio_hardware->_context)));
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(_audio_hardware->_main_loop);
}

OpenRecordingDevice::OpenRecordingDevice(AudioHardware *audio_hardware, const char *device_name,
        const GenesisChannelLayout *channel_layout, SampleFormat sample_format, double latency,
        int sample_rate, bool *ok) :
    _audio_hardware(audio_hardware),
    _stream_ready(false)
{
    if (!_audio_hardware->_ready_flag)
        panic("audio hardware not ready");

    pa_threaded_mainloop_lock(_audio_hardware->_main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_sample_format(sample_format);
    sample_spec.rate = sample_rate;
    sample_spec.channels = channel_layout->channel_count;

    pa_channel_map channel_map = to_pulseaudio_channel_map(channel_layout);

    _stream = pa_stream_new(_audio_hardware->_context, "Genesis", &sample_spec, &channel_map);
    if (!_stream)
        panic("unable to create pulseaudio stream");

    pa_stream_set_state_callback(_stream, static_stream_state_callback, this);

    int bytes_per_second = get_bytes_per_second(sample_format, channel_layout->channel_count, sample_rate);
    int buffer_length = latency * bytes_per_second;

    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = UINT32_MAX;
    buffer_attr.tlength = UINT32_MAX;
    buffer_attr.prebuf = UINT32_MAX;
    buffer_attr.minreq = UINT32_MAX;
    buffer_attr.fragsize = buffer_length;

    int err = pa_stream_connect_record(_stream, device_name, &buffer_attr, PA_STREAM_ADJUST_LATENCY);
    if (err)
        panic("unable to connect pulseaudio record stream");

    *ok = true;

    pa_threaded_mainloop_unlock(_audio_hardware->_main_loop);
}

OpenRecordingDevice::~OpenRecordingDevice() {
    pa_threaded_mainloop_lock(_audio_hardware->_main_loop);

    pa_stream_set_state_callback(_stream, NULL, this);
    pa_stream_disconnect(_stream);
    pa_stream_unref(_stream);

    pa_threaded_mainloop_unlock(_audio_hardware->_main_loop);
}

void OpenRecordingDevice::stream_state_callback(pa_stream *stream) {
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_READY:
            _stream_ready = true;
            break;
        case PA_STREAM_FAILED:
            panic("pulseaudio stream error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
            break;
    }
}

void OpenRecordingDevice::peek(const char **data, int *byte_count) {
    if (_stream_ready) {
        pa_threaded_mainloop_lock(_audio_hardware->_main_loop);
        size_t nbytes;
        if (pa_stream_peek(_stream, (const void **)data, &nbytes))
            panic("pa_stream_peek error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(_stream))));
        pa_threaded_mainloop_unlock(_audio_hardware->_main_loop);
        *byte_count = nbytes;
    } else {
        *data = nullptr;
        *byte_count = 0;
    }
}

void OpenRecordingDevice::drop() {
    pa_threaded_mainloop_lock(_audio_hardware->_main_loop);
    if (pa_stream_drop(_stream))
        panic("pa_stream_drop error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(_stream))));
    pa_threaded_mainloop_unlock(_audio_hardware->_main_loop);
}
