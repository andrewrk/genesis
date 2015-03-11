#include "audio_hardware.hpp"
#include "util.hpp"
#include "genesis.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

void default_on_devices_change(AudioHardware *, const AudioDevicesInfo *devices_info) {
    fprintf(stderr, "\n");
    for (int i = 0; i < devices_info->devices.length(); i += 1) {
        const AudioDevice *audio_device = &devices_info->devices.at(i);
        const char *purpose_str = (audio_device->purpose == AudioDevicePurposePlayback) ?
            "playback" : "recording";
        const char *default_str = (i == devices_info->default_output_index) ? " (default)" : "";
        fprintf(stderr, "%s device: %s%s\n", purpose_str, audio_device->description.encode().raw(), default_str);
    }
}

AudioHardware::AudioHardware() :
    _userdata(NULL),
    _device_scan_requests(0),
    _on_devices_change(default_on_devices_change),
    _current_audio_devices_info(NULL),
    _ready_audio_devices_info(NULL),
    _ready_flag(false),
    _thread_exit_flag(false)
{
    if (pthread_mutex_init(&_thread_mutex, NULL))
        panic("pthread_mutex_init failure");
    if (pthread_cond_init(&_ready_cond, NULL))
        panic("pthread_cond_init failure");

    _main_loop = pa_mainloop_new();
    if (!_main_loop)
        panic("unable to create pulseaudio mainloop");

    pa_mainloop_api *main_loop_api = pa_mainloop_get_api(_main_loop);

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

    if (pthread_create(&_pulseaudio_thread_id, NULL, pulseaudio_thread, this))
        panic("unable to create playback thread");

    scan_devices();
}

AudioHardware::~AudioHardware() {
    _thread_exit_flag = true;
    pa_mainloop_wakeup(_main_loop);
    pthread_join(_pulseaudio_thread_id, NULL);

    destroy_current_audio_devices_info();
    destroy_ready_audio_devices_info();

    pa_context_disconnect(_context);
    pa_context_unref(_context);
    pa_mainloop_free(_main_loop);

    if (pthread_cond_destroy(&_ready_cond))
        panic("pthread_cond_destroy failure");
    if (pthread_mutex_destroy(&_thread_mutex))
        panic("pthread_mutex_destroy failure");
}

void AudioHardware::subscribe_callback(pa_context *context,
        pa_subscription_event_type_t event_bits, uint32_t index)
{
    int facility = (event_bits & PA_SUBSCRIPTION_EVENT_FACILITY_MASK);
    if (facility == PA_SUBSCRIPTION_EVENT_SOURCE || facility == PA_SUBSCRIPTION_EVENT_SINK)
        scan_devices();
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

void AudioHardware::set_ready_flag() {
    _ready_flag = true;
    pthread_mutex_lock(&_thread_mutex);
    pthread_cond_signal(&_ready_cond);
    pthread_mutex_unlock(&_thread_mutex);
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
    case PA_CONTEXT_TERMINATED: // The connection was terminated cleanly.
        set_ready_flag();
        pa_mainloop_wakeup(_main_loop);
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
    for (int i = 0; i < _current_audio_devices_info->devices.length(); i += 1) {
        AudioDevice *audio_device = &_current_audio_devices_info->devices.at(i);
        if (String::equal(audio_device->name, _default_audio_device_name)) {
            _current_audio_devices_info->default_output_index = i;
            break;
        }
    }

    pthread_mutex_lock(&_thread_mutex);
    destroy_ready_audio_devices_info();
    _ready_audio_devices_info = _current_audio_devices_info;
    _current_audio_devices_info = NULL;
    pthread_mutex_unlock(&_thread_mutex);
}

static ChannelId from_pulseaudio_channel_pos(pa_channel_position_t pos) {
    switch (pos) {
    case PA_CHANNEL_POSITION_MONO: return ChannelIdFrontCenter;
    case PA_CHANNEL_POSITION_FRONT_LEFT: return ChannelIdFrontLeft;
    case PA_CHANNEL_POSITION_FRONT_RIGHT: return ChannelIdFrontRight;
    case PA_CHANNEL_POSITION_FRONT_CENTER: return ChannelIdFrontCenter;
    case PA_CHANNEL_POSITION_REAR_CENTER: return ChannelIdBackCenter;
    case PA_CHANNEL_POSITION_REAR_LEFT: return ChannelIdBackLeft;
    case PA_CHANNEL_POSITION_REAR_RIGHT: return ChannelIdBackRight;
    case PA_CHANNEL_POSITION_LFE: return ChannelIdLowFrequency;
    case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER: return ChannelIdFrontLeftOfCenter;
    case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER: return ChannelIdFrontRightOfCenter;
    case PA_CHANNEL_POSITION_SIDE_LEFT: return ChannelIdSideLeft;
    case PA_CHANNEL_POSITION_SIDE_RIGHT: return ChannelIdSideRight;
    case PA_CHANNEL_POSITION_TOP_CENTER: return ChannelIdTopCenter;
    case PA_CHANNEL_POSITION_TOP_FRONT_LEFT: return ChannelIdTopFrontLeft;
    case PA_CHANNEL_POSITION_TOP_FRONT_RIGHT: return ChannelIdTopFrontRight;
    case PA_CHANNEL_POSITION_TOP_FRONT_CENTER: return ChannelIdTopFrontCenter;
    case PA_CHANNEL_POSITION_TOP_REAR_LEFT: return ChannelIdTopBackLeft;
    case PA_CHANNEL_POSITION_TOP_REAR_RIGHT: return ChannelIdTopBackRight;
    case PA_CHANNEL_POSITION_TOP_REAR_CENTER: return ChannelIdTopBackCenter;

    default:
        panic("cannot map pulseaudio channel to genesis");
    }
}

static void set_from_pulseaudio_channel_map(pa_channel_map channel_map, ChannelLayout *channel_layout) {
    channel_layout->channels.clear();
    for (int i = 0; i < channel_map.channels; i += 1) {
        channel_layout->channels.append(from_pulseaudio_channel_pos(channel_map.map[i]));
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
        audio_device->name = String(info->name);
        audio_device->description = String(info->description);
        set_from_pulseaudio_channel_map(info->channel_map, &audio_device->channel_layout);
        audio_device->default_sample_format = sample_format_from_pulseaudio(info->sample_spec);
        audio_device->default_latency = usec_to_sec(info->configured_latency);
        audio_device->default_sample_rate = sample_rate_from_pulseaudio(info->sample_spec);
        audio_device->purpose = AudioDevicePurposePlayback;
    }
}

void AudioHardware::source_info_callback(pa_context *context, const pa_source_info *info, int eol) {
    if (eol) {
        _have_source_list = true;
        finish_device_query();
    } else {
        List<AudioDevice> *devices = &_current_audio_devices_info->devices;
        _current_audio_devices_info->devices.resize(devices->length() + 1);
        AudioDevice *audio_device = &devices->at(devices->length() - 1);
        audio_device->name = String(info->name);
        audio_device->description = String(info->description);
        set_from_pulseaudio_channel_map(info->channel_map, &audio_device->channel_layout);
        audio_device->default_sample_format = sample_format_from_pulseaudio(info->sample_spec);
        audio_device->default_latency = usec_to_sec(info->configured_latency);
        audio_device->default_sample_rate = sample_rate_from_pulseaudio(info->sample_spec);
        audio_device->purpose = AudioDevicePurposeRecording;
    }
}

void AudioHardware::server_info_callback(pa_context *context, const pa_server_info *info) {
    _default_audio_device_name = info->default_sink_name;

    _have_default_sink = true;
    finish_device_query();
}

int AudioHardware::perform_operation(pa_operation *op, int *return_value) {
    int total_dispatch_count = 0;
    for (;;) {
        switch (pa_operation_get_state(op)) {
        case PA_OPERATION_RUNNING:
            {
                int dispatch_count = pa_mainloop_iterate(_main_loop, 1, return_value);
                if (dispatch_count >= 0) {
                    total_dispatch_count += dispatch_count;
                    continue;
                } else {
                    return dispatch_count;
                }
            }
        case PA_OPERATION_DONE:
            pa_operation_unref(op);
            return total_dispatch_count;
        case PA_OPERATION_CANCELLED:
            pa_operation_unref(op);
            return -1;
        }
    }
}

void *AudioHardware::pulseaudio_thread() {
    int return_value;

    // wait until we get to the ready state
    while (!_ready_flag) {
        if (pa_mainloop_iterate(_main_loop, 1, &return_value) < 0)
            return nullptr;
    }

    pa_subscription_mask_t events = (pa_subscription_mask_t)(
            PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE);
    pa_operation *subscribe_op = pa_context_subscribe(_context, events, nullptr, this);
    if (!subscribe_op)
        panic("pa_context_subscribe failed: %s", pa_strerror(pa_context_errno(_context)));
    if (perform_operation(subscribe_op, &return_value) < 0)
        return nullptr;

    for (;;) {
        if (_thread_exit_flag)
            break;
        if (_device_scan_requests >= 1) {
            _device_scan_requests = 0;

            _have_sink_list = false;
            _have_default_sink = false;
            _have_source_list = false;

            initialize_current_device_list();
            pa_operation *list_sink_op = pa_context_get_sink_info_list(_context, sink_info_callback, this);
            pa_operation *list_source_op = pa_context_get_source_info_list(_context,
                    static_source_info_callback, this);
            pa_operation *server_info_op = pa_context_get_server_info(_context, server_info_callback, this);

            if (perform_operation(list_sink_op, &return_value) < 0)
                break;
            if (perform_operation(list_source_op, &return_value) < 0)
                break;
            if (perform_operation(server_info_op, &return_value) < 0)
                break;
        }
        if (pa_mainloop_iterate(_main_loop, 1, &return_value) < 0)
            break;
    }
    return nullptr;
}

void AudioHardware::scan_devices() {
    _device_scan_requests += 1;
    pa_mainloop_wakeup(_main_loop);
}

void AudioHardware::flush_events() {
    pthread_mutex_lock(&_thread_mutex);

    AudioDevicesInfo *devices_info = _ready_audio_devices_info;
    _ready_audio_devices_info = NULL;

    pthread_mutex_unlock(&_thread_mutex);

    if (devices_info) {
        _on_devices_change(this, devices_info);
        destroy(devices_info, 1);
    }
}

void AudioHardware::block_until_ready() {
    if (_ready_flag)
        return;
    pthread_mutex_lock(&_thread_mutex);
    while (!_ready_flag) {
        pthread_cond_wait(&_ready_cond, &_thread_mutex);
    }
    pthread_mutex_unlock(&_thread_mutex);
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

static pa_channel_position_t to_pulseaudio_channel_pos(ChannelId channel_id) {
    switch (channel_id) {
    case ChannelIdFrontLeft:
        return PA_CHANNEL_POSITION_FRONT_LEFT;
    case ChannelIdFrontRight:
        return PA_CHANNEL_POSITION_FRONT_RIGHT;
    case ChannelIdFrontCenter:
        return PA_CHANNEL_POSITION_FRONT_CENTER;
    case ChannelIdLowFrequency:
        return PA_CHANNEL_POSITION_LFE;
    case ChannelIdBackLeft:
        return PA_CHANNEL_POSITION_REAR_LEFT;
    case ChannelIdBackRight:
        return PA_CHANNEL_POSITION_REAR_RIGHT;
    case ChannelIdFrontLeftOfCenter:
        return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
    case ChannelIdFrontRightOfCenter:
        return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
    case ChannelIdBackCenter:
        return PA_CHANNEL_POSITION_REAR_CENTER;
    case ChannelIdSideLeft:
        return PA_CHANNEL_POSITION_SIDE_LEFT;
    case ChannelIdSideRight:
        return PA_CHANNEL_POSITION_SIDE_RIGHT;
    case ChannelIdTopCenter:
        return PA_CHANNEL_POSITION_TOP_CENTER;
    case ChannelIdTopFrontLeft:
        return PA_CHANNEL_POSITION_TOP_FRONT_LEFT;
    case ChannelIdTopFrontCenter:
        return PA_CHANNEL_POSITION_TOP_FRONT_CENTER;
    case ChannelIdTopFrontRight:
        return PA_CHANNEL_POSITION_TOP_FRONT_RIGHT;
    case ChannelIdTopBackLeft:
        return PA_CHANNEL_POSITION_TOP_REAR_LEFT;
    case ChannelIdTopBackCenter:
        return PA_CHANNEL_POSITION_TOP_REAR_CENTER;
    case ChannelIdTopBackRight:
        return PA_CHANNEL_POSITION_TOP_REAR_RIGHT;
    case ChannelIdStereoLeft:
    case ChannelIdStereoRight:
    case ChannelIdWideLeft:
    case ChannelIdWideRight:
    case ChannelIdSurroundDirectLeft:
    case ChannelIdSurroundDirectRight:
    case ChannelIdLowFrequency2:
        panic("unable to map channel id to pulseaudio");
    }
    panic("invalid channel id");
}

static pa_channel_map to_pulseaudio_channel_map(const ChannelLayout *channel_layout) {
    pa_channel_map channel_map;
    channel_map.channels = channel_layout->channels.length();

    if (channel_layout->channels.length() > PA_CHANNELS_MAX)
        panic("channel layout greater than pulseaudio max channels");

    for (int i = 0; i < channel_layout->channels.length(); i += 1)
        channel_map.map[i] = to_pulseaudio_channel_pos(channel_layout->channels.at(i));

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
        const ChannelLayout *channel_layout, SampleFormat sample_format, double latency,
        int sample_rate, bool *ok) :
    _stream(NULL)
{
    audio_hardware->block_until_ready();

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_sample_format(sample_format);
    sample_spec.rate = sample_rate;
    sample_spec.channels = channel_layout->channels.length();
    pa_channel_map channel_map = to_pulseaudio_channel_map(channel_layout);
    _stream = pa_stream_new(audio_hardware->_context, "Genesis", &sample_spec, &channel_map);
    if (!_stream)
        panic("unable to create pulseaudio stream");

    pa_stream_set_state_callback(_stream, stream_state_callback, this);

    int bytes_per_second = get_bytes_per_second(sample_format, channel_layout->channels.length(), sample_rate);
    int buffer_length = latency * bytes_per_second;

    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = buffer_length;
    buffer_attr.tlength = buffer_length;
    buffer_attr.prebuf = buffer_attr.maxlength;
    buffer_attr.minreq = UINT32_MAX;
    buffer_attr.fragsize = UINT32_MAX;

    int err = pa_stream_connect_playback(_stream, device_name, &buffer_attr, PA_STREAM_NOFLAGS, NULL, NULL);
    if (err)
        panic("unable to connect pulseaudio playback stream");

    *ok = true;
}

OpenPlaybackDevice::~OpenPlaybackDevice() {
    pa_stream_set_state_callback(_stream, NULL, this);
    pa_stream_disconnect(_stream);
    pa_stream_unref(_stream);
}

int OpenPlaybackDevice::writable_size() {
    return pa_stream_writable_size(_stream);
}

void OpenPlaybackDevice::begin_write(char **data, int *byte_count) {
    size_t size_t_byte_count = *byte_count;
    if (pa_stream_begin_write(_stream, (void**)data, &size_t_byte_count))
        panic("pa_stream_begin_write error: %s", pa_strerror(pa_context_errno(_audio_hardware->_context)));
    *byte_count = size_t_byte_count;
}

void OpenPlaybackDevice::write(char *data, int byte_count) {
    if (pa_stream_write(_stream, data, byte_count, NULL, 0, PA_SEEK_RELATIVE))
        panic("pa_stream_write error: %s", pa_strerror(pa_context_errno(_audio_hardware->_context)));
}

void OpenPlaybackDevice::clear_buffer() {
    pa_operation_unref(pa_stream_flush(_stream, NULL, NULL));
}

OpenRecordingDevice::OpenRecordingDevice(AudioHardware *audio_hardware, const char *device_name,
        const ChannelLayout *channel_layout, SampleFormat sample_format, double latency,
        int sample_rate, bool *ok)
{
    audio_hardware->block_until_ready();

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_sample_format(sample_format);
    sample_spec.rate = sample_rate;
    sample_spec.channels = channel_layout->channels.length();

    pa_channel_map channel_map = to_pulseaudio_channel_map(channel_layout);

    _stream = pa_stream_new(audio_hardware->_context, "Genesis", &sample_spec, &channel_map);
    if (!_stream)
        panic("unable to create pulseaudio stream");

    pa_stream_set_state_callback(_stream, static_stream_state_callback, this);

    int bytes_per_second = get_bytes_per_second(sample_format, channel_layout->channels.length(), sample_rate);
    int buffer_length = latency * bytes_per_second;

    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = UINT32_MAX;
    buffer_attr.tlength = UINT32_MAX;
    buffer_attr.prebuf = UINT32_MAX;
    buffer_attr.minreq = UINT32_MAX;
    buffer_attr.fragsize = buffer_length;

    int err = pa_stream_connect_record(_stream, device_name, &buffer_attr, PA_STREAM_NOFLAGS);
    if (err)
        panic("unable to connect pulseaudio record stream");

    *ok = true;
}

OpenRecordingDevice::~OpenRecordingDevice() {
    pa_stream_set_state_callback(_stream, NULL, this);
    pa_stream_disconnect(_stream);
    pa_stream_unref(_stream);
}

void OpenRecordingDevice::stream_state_callback(pa_stream *stream) {
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

void OpenRecordingDevice::peek(const char **data, int *byte_count) {
    size_t nbytes;
    if (pa_stream_peek(_stream, (const void **)&data, &nbytes))
        panic("pa_stream_peek error");
    *byte_count = nbytes;
}

void OpenRecordingDevice::drop() {
    if (pa_stream_drop(_stream))
        panic("pa_stream_drop error");
}
