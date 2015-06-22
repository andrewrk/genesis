#include "audio_backend_pulseaudio.hpp"
#include "audio_hardware.hpp"

#include <string.h>

static void subscribe_callback(pa_context *context,
        pa_subscription_event_type_t event_bits, uint32_t index, void *userdata)
{
    AudioHardware *audio_hardware = (AudioHardware *)userdata;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    ah->device_scan_queued = true;
    pa_threaded_mainloop_signal(ah->main_loop, 0);
}

static void subscribe_to_events(AudioHardware *audio_hardware) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    pa_subscription_mask_t events = (pa_subscription_mask_t)(
            PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE|PA_SUBSCRIPTION_MASK_SERVER);
    pa_operation *subscribe_op = pa_context_subscribe(ah->pulse_context,
            events, nullptr, audio_hardware);
    if (!subscribe_op)
        panic("pa_context_subscribe failed: %s", pa_strerror(pa_context_errno(ah->pulse_context)));
    pa_operation_unref(subscribe_op);
}

static void context_state_callback(pa_context *context, void *userdata) {
    AudioHardware *audio_hardware = (AudioHardware *)userdata;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;

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
        ah->device_scan_queued = true;
        subscribe_to_events(audio_hardware);
        ah->ready_flag = true;
        pa_threaded_mainloop_signal(ah->main_loop, 0);
        return;
    case PA_CONTEXT_TERMINATED: // The connection was terminated cleanly.
        pa_threaded_mainloop_signal(ah->main_loop, 0);
        return;
    case PA_CONTEXT_FAILED: // The connection failed or was disconnected.
        panic("pulseaudio connect failure: %s", pa_strerror(pa_context_errno(context)));
    }
}

static void destroy_current_audio_devices_info(AudioHardware *audio_hardware) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    if (ah->current_audio_devices_info) {
        for (int i = 0; i < ah->current_audio_devices_info->devices.length(); i += 1)
            genesis_audio_device_unref(ah->current_audio_devices_info->devices.at(i));

        destroy(ah->current_audio_devices_info, 1);
        ah->current_audio_devices_info = nullptr;
    }
}

static void destroy_ready_audio_devices_info(AudioHardware *audio_hardware) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    if (ah->ready_audio_devices_info) {
        for (int i = 0; i < ah->ready_audio_devices_info->devices.length(); i += 1)
            genesis_audio_device_unref(ah->ready_audio_devices_info->devices.at(i));
        destroy(ah->ready_audio_devices_info, 1);
        ah->ready_audio_devices_info = nullptr;
    }
}


static void destroy_audio_hardware_pa(AudioHardware *audio_hardware) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    if (ah->main_loop)
        pa_threaded_mainloop_stop(ah->main_loop);

    destroy_current_audio_devices_info(audio_hardware);
    destroy_ready_audio_devices_info(audio_hardware);

    if (audio_hardware->safe_devices_info) {
        for (int i = 0; i < audio_hardware->safe_devices_info->devices.length(); i += 1) {
            genesis_audio_device_unref(audio_hardware->safe_devices_info->devices.at(i));
        }
        destroy(audio_hardware->safe_devices_info, 1);
    }

    pa_context_disconnect(ah->pulse_context);
    pa_context_unref(ah->pulse_context);

    if (ah->main_loop)
        pa_threaded_mainloop_free(ah->main_loop);

    if (ah->props)
        pa_proplist_free(ah->props);

    free(ah->default_sink_name);
    free(ah->default_source_name);
}

static double usec_to_sec(pa_usec_t usec) {
    return (double)usec / (double)PA_USEC_PER_SEC;
}

static GenesisSampleFormat sample_format_from_pulseaudio(pa_sample_spec sample_spec) {
    switch (sample_spec.format) {
    case PA_SAMPLE_U8:        return GenesisSampleFormatUInt8;
    case PA_SAMPLE_S16NE:     return GenesisSampleFormatInt16;
    case PA_SAMPLE_S32NE:     return GenesisSampleFormatInt32;
    case PA_SAMPLE_FLOAT32NE: return GenesisSampleFormatFloat;
    default:                  return GenesisSampleFormatInvalid;
    }
}

static int sample_rate_from_pulseaudio(pa_sample_spec sample_spec) {
    return sample_spec.rate;
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
    channel_layout->name = nullptr;
    int builtin_layout_count = genesis_channel_layout_builtin_count();
    for (int i = 0; i < builtin_layout_count; i += 1) {
        const GenesisChannelLayout *builtin_layout = genesis_channel_layout_get_builtin(i);
        if (genesis_channel_layout_equal(builtin_layout, channel_layout)) {
            channel_layout->name = builtin_layout->name;
            break;
        }
    }
}

static int perform_operation(AudioHardware *audio_hardware, pa_operation *op) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    for (;;) {
        switch (pa_operation_get_state(op)) {
        case PA_OPERATION_RUNNING:
            pa_threaded_mainloop_wait(ah->main_loop);
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

static void finish_device_query(AudioHardware *audio_hardware) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;

    if (!ah->have_sink_list ||
        !ah->have_source_list ||
        !ah->have_default_sink)
    {
        return;
    }

    // based on the default sink name, figure out the default output index
    ah->current_audio_devices_info->default_output_index = -1;
    ah->current_audio_devices_info->default_input_index = -1;
    for (int i = 0; i < ah->current_audio_devices_info->devices.length(); i += 1) {
        GenesisAudioDevice *audio_device = ah->current_audio_devices_info->devices.at(i);
        if (audio_device->purpose == GenesisAudioDevicePurposePlayback &&
            strcmp(audio_device->name, ah->default_sink_name) == 0)
        {
            ah->current_audio_devices_info->default_output_index = i;
        } else if (audio_device->purpose == GenesisAudioDevicePurposeRecording &&
            strcmp(audio_device->name, ah->default_source_name) == 0)
        {
            ah->current_audio_devices_info->default_input_index = i;
        }
    }

    destroy_ready_audio_devices_info(audio_hardware);
    ah->ready_audio_devices_info = ah->current_audio_devices_info;
    ah->current_audio_devices_info = NULL;
    ah->have_devices_flag = true;
    pa_threaded_mainloop_signal(ah->main_loop, 0);
    audio_hardware->on_events_signal(audio_hardware);
}

static void sink_info_callback(pa_context *pulse_context, const pa_sink_info *info, int eol, void *userdata) {
    AudioHardware *audio_hardware = (AudioHardware *)userdata;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    if (eol) {
        ah->have_sink_list = true;
        finish_device_query(audio_hardware);
    } else {
        GenesisAudioDevice *audio_device = create_zero<GenesisAudioDevice>();
        if (!audio_device)
            panic("out of memory");

        audio_device->ref_count = 1;
        audio_device->audio_hardware = audio_hardware;
        audio_device->name = strdup(info->name);
        audio_device->description = strdup(info->description);
        if (!audio_device->name || !audio_device->description)
            panic("out of memory");
        set_from_pulseaudio_channel_map(info->channel_map, &audio_device->channel_layout);
        audio_device->default_sample_format = sample_format_from_pulseaudio(info->sample_spec);
        audio_device->default_latency = usec_to_sec(info->configured_latency);
        audio_device->default_sample_rate = sample_rate_from_pulseaudio(info->sample_spec);
        audio_device->purpose = GenesisAudioDevicePurposePlayback;

        if (ah->current_audio_devices_info->devices.append(audio_device))
            panic("out of memory");
    }
    pa_threaded_mainloop_signal(ah->main_loop, 0);
}

static void source_info_callback(pa_context *pulse_context, const pa_source_info *info, int eol, void *userdata) {
    AudioHardware *audio_hardware = (AudioHardware *)userdata;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    if (eol) {
        ah->have_source_list = true;
        finish_device_query(audio_hardware);
    } else {
        GenesisAudioDevice *audio_device = create_zero<GenesisAudioDevice>();
        if (!audio_device)
            panic("out of memory");

        audio_device->ref_count = 1;
        audio_device->audio_hardware = audio_hardware;
        audio_device->name = strdup(info->name);
        audio_device->description = strdup(info->description);
        if (!audio_device->name || !audio_device->description)
            panic("out of memory");
        set_from_pulseaudio_channel_map(info->channel_map, &audio_device->channel_layout);
        audio_device->default_sample_format = sample_format_from_pulseaudio(info->sample_spec);
        audio_device->default_latency = usec_to_sec(info->configured_latency);
        audio_device->default_sample_rate = sample_rate_from_pulseaudio(info->sample_spec);
        audio_device->purpose = GenesisAudioDevicePurposeRecording;

        if (ah->current_audio_devices_info->devices.append(audio_device))
            panic("out of memory");
    }
    pa_threaded_mainloop_signal(ah->main_loop, 0);
}

static void server_info_callback(pa_context *pulse_context, const pa_server_info *info, void *userdata) {
    AudioHardware *audio_hardware = (AudioHardware *)userdata;
    assert(audio_hardware);
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;

    free(ah->default_sink_name);
    free(ah->default_source_name);

    ah->default_sink_name = strdup(info->default_sink_name);
    ah->default_source_name = strdup(info->default_source_name);

    if (!ah->default_sink_name || !ah->default_source_name)
        panic("out of memory");

    ah->have_default_sink = true;
    finish_device_query(audio_hardware);
    pa_threaded_mainloop_signal(ah->main_loop, 0);
}

static void scan_devices(AudioHardware *audio_hardware) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;

    ah->have_sink_list = false;
    ah->have_default_sink = false;
    ah->have_source_list = false;

    destroy_current_audio_devices_info(audio_hardware);
    ah->current_audio_devices_info = create_zero<AudioDevicesInfo>();
    if (!ah->current_audio_devices_info)
        panic("out of memory");

    pa_threaded_mainloop_lock(ah->main_loop);

    pa_operation *list_sink_op = pa_context_get_sink_info_list(ah->pulse_context,
            sink_info_callback, audio_hardware);
    pa_operation *list_source_op = pa_context_get_source_info_list(ah->pulse_context,
            source_info_callback, audio_hardware);
    pa_operation *server_info_op = pa_context_get_server_info(ah->pulse_context,
            server_info_callback, audio_hardware);

    if (perform_operation(audio_hardware, list_sink_op))
        panic("list sinks failed");
    if (perform_operation(audio_hardware, list_source_op))
        panic("list sources failed");
    if (perform_operation(audio_hardware, server_info_op))
        panic("get server info failed");

    pa_threaded_mainloop_signal(ah->main_loop, 0);

    pa_threaded_mainloop_unlock(ah->main_loop);
}

static void flush_events(AudioHardware *audio_hardware) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;

    if (ah->device_scan_queued) {
        ah->device_scan_queued = false;
        scan_devices(audio_hardware);
    }

    AudioDevicesInfo *old_devices_info = nullptr;
    bool change = false;

    pa_threaded_mainloop_lock(ah->main_loop);

    if (ah->ready_audio_devices_info) {
        old_devices_info = audio_hardware->safe_devices_info;
        audio_hardware->safe_devices_info = ah->ready_audio_devices_info;
        ah->ready_audio_devices_info = nullptr;
        change = true;
    }

    pa_threaded_mainloop_unlock(ah->main_loop);

    if (change)
        audio_hardware->on_devices_change(audio_hardware);

    if (old_devices_info) {
        for (int i = 0; i < old_devices_info->devices.length(); i += 1)
            genesis_audio_device_unref(old_devices_info->devices.at(i));
        destroy(old_devices_info, 1);
    }
}

static void block_until_ready(AudioHardware *audio_hardware) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    if (ah->ready_flag)
        return;
    pa_threaded_mainloop_lock(ah->main_loop);
    while (!ah->ready_flag) {
        pa_threaded_mainloop_wait(ah->main_loop);
    }
    pa_threaded_mainloop_unlock(ah->main_loop);
}

static void block_until_have_devices(AudioHardware *audio_hardware) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    if (ah->have_devices_flag)
        return;
    pa_threaded_mainloop_lock(ah->main_loop);
    while (!ah->have_devices_flag) {
        pa_threaded_mainloop_wait(ah->main_loop);
    }
    pa_threaded_mainloop_unlock(ah->main_loop);
}

static pa_sample_format_t to_pulseaudio_sample_format(GenesisSampleFormat sample_format) {
    switch (sample_format) {
    case GenesisSampleFormatUInt8:
        return PA_SAMPLE_U8;
    case GenesisSampleFormatInt16:
        return PA_SAMPLE_S16NE;
    case GenesisSampleFormatInt24:
        return PA_SAMPLE_S24NE;
    case GenesisSampleFormatInt32:
        return PA_SAMPLE_S32NE;
    case GenesisSampleFormatFloat:
        return PA_SAMPLE_FLOAT32NE;
    case GenesisSampleFormatDouble:
        panic("cannot use double sample format with pulseaudio");
    case GenesisSampleFormatInvalid:
        panic("invalid sample format");
    }
    panic("invalid sample format");
}

static pa_channel_position_t to_pulseaudio_channel_pos(GenesisChannelId channel_id) {
    switch (channel_id) {
    case GenesisChannelIdInvalid:
    case GenesisChannelIdCount:
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

static void playback_stream_state_callback(pa_stream *stream, void *userdata) {
    OpenPlaybackDevice *open_playback_device = (OpenPlaybackDevice*) userdata;
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    OpenPlaybackDevicePulseAudio *opd = &open_playback_device->backend.pulse_audio;
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_READY:
            opd->stream_ready = true;
            pa_threaded_mainloop_signal(ah->main_loop, 0);
            break;
        case PA_STREAM_FAILED:
            panic("pulseaudio stream error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
            break;
    }
}

static void playback_stream_underflow_callback(pa_stream *stream, void *userdata) {
    OpenPlaybackDevice *open_playback_device = (OpenPlaybackDevice*)userdata;
    open_playback_device->underrun_callback(open_playback_device);
}


static void playback_stream_write_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    OpenPlaybackDevice *open_playback_device = (OpenPlaybackDevice*)(userdata);
    int frame_count = ((int)nbytes) / open_playback_device->bytes_per_frame;
    open_playback_device->write_callback(open_playback_device, frame_count);
}

static int open_playback_device_init_pa(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device)
{
    OpenPlaybackDevicePulseAudio *opd = &open_playback_device->backend.pulse_audio;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    GenesisAudioDevice *audio_device = open_playback_device->audio_device; 
    opd->stream_ready = false;

    assert(ah->pulse_context);

    pa_threaded_mainloop_lock(ah->main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_sample_format(open_playback_device->sample_format);
    sample_spec.rate = audio_device->default_sample_rate;
    sample_spec.channels = audio_device->channel_layout.channel_count;
    pa_channel_map channel_map = to_pulseaudio_channel_map(&audio_device->channel_layout);

    opd->stream = pa_stream_new(ah->pulse_context, "Genesis", &sample_spec, &channel_map);
    if (!opd->stream) {
        pa_threaded_mainloop_unlock(ah->main_loop);
        return GenesisErrorNoMem;
    }
    pa_stream_set_state_callback(opd->stream, playback_stream_state_callback, open_playback_device);
    pa_stream_set_write_callback(opd->stream, playback_stream_write_callback, open_playback_device);
    pa_stream_set_underflow_callback(opd->stream, playback_stream_underflow_callback, open_playback_device);

    int bytes_per_second = open_playback_device->bytes_per_frame * audio_device->default_sample_rate;
    int buffer_length = open_playback_device->bytes_per_frame *
        ceil(open_playback_device->latency * bytes_per_second / (double)open_playback_device->bytes_per_frame);

    opd->buffer_attr.maxlength = buffer_length;
    opd->buffer_attr.tlength = buffer_length;
    opd->buffer_attr.prebuf = 0;
    opd->buffer_attr.minreq = UINT32_MAX;
    opd->buffer_attr.fragsize = UINT32_MAX;

    pa_threaded_mainloop_unlock(ah->main_loop);

    return 0;
}

static void open_playback_device_destroy_pa(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device)
{
    OpenPlaybackDevicePulseAudio *opd = &open_playback_device->backend.pulse_audio;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    pa_stream *stream = opd->stream;
    if (stream) {
        pa_threaded_mainloop_lock(ah->main_loop);

        pa_stream_set_write_callback(stream, nullptr, nullptr);
        pa_stream_set_state_callback(stream, nullptr, nullptr);
        pa_stream_set_underflow_callback(stream, nullptr, nullptr);
        pa_stream_disconnect(stream);

        pa_stream_unref(stream);

        pa_threaded_mainloop_unlock(ah->main_loop);

        opd->stream = nullptr;
    }
}

static int open_playback_device_start_pa(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device)
{
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    OpenPlaybackDevicePulseAudio *opd = &open_playback_device->backend.pulse_audio;

    pa_threaded_mainloop_lock(ah->main_loop);


    int err = pa_stream_connect_playback(opd->stream,
            open_playback_device->audio_device->name, &opd->buffer_attr,
            PA_STREAM_ADJUST_LATENCY, nullptr, nullptr);
    if (err) {
        pa_threaded_mainloop_unlock(ah->main_loop);
        return GenesisErrorOpeningAudioHardware;
    }

    while (!opd->stream_ready)
        pa_threaded_mainloop_wait(ah->main_loop);

    {
        // fill with silence
        char *buffer;
        int requested_frame_count = open_playback_device_free_count(open_playback_device);
        while (requested_frame_count > 0) {
            int frame_count = requested_frame_count;
            open_playback_device_begin_write(open_playback_device, &buffer, &frame_count);
            memset(buffer, 0, frame_count * open_playback_device->bytes_per_frame);
            open_playback_device_write(open_playback_device, buffer, frame_count);
        }
    }

    pa_threaded_mainloop_unlock(ah->main_loop);

    return 0;
}

static int open_playback_device_free_count_pa(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device)
{
    OpenPlaybackDevicePulseAudio *opd = &open_playback_device->backend.pulse_audio;
    return pa_stream_writable_size(opd->stream) / open_playback_device->bytes_per_frame;
}


static void open_playback_device_begin_write_pa(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device, char **data, int *frame_count)
{
    OpenPlaybackDevicePulseAudio *opd = &open_playback_device->backend.pulse_audio;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    pa_stream *stream = opd->stream;
    size_t byte_count = *frame_count * open_playback_device->bytes_per_frame;
    if (pa_stream_begin_write(stream, (void**)data, &byte_count))
        panic("pa_stream_begin_write error: %s", pa_strerror(pa_context_errno(ah->pulse_context)));

    *frame_count = byte_count / open_playback_device->bytes_per_frame;
}

static void open_playback_device_write_pa(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device, char *data, int frame_count)
{
    OpenPlaybackDevicePulseAudio *opd = &open_playback_device->backend.pulse_audio;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    pa_stream *stream = opd->stream;
    size_t byte_count = frame_count * open_playback_device->bytes_per_frame;
    if (pa_stream_write(stream, data, byte_count, NULL, 0, PA_SEEK_RELATIVE))
        panic("pa_stream_write error: %s", pa_strerror(pa_context_errno(ah->pulse_context)));
}

static void open_playback_device_clear_buffer_pa(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device)
{
    OpenPlaybackDevicePulseAudio *opd = &open_playback_device->backend.pulse_audio;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    pa_stream *stream = opd->stream;
    pa_threaded_mainloop_lock(ah->main_loop);
    pa_operation *op = pa_stream_flush(stream, NULL, NULL);
    if (!op)
        panic("pa_stream_flush failed: %s", pa_strerror(pa_context_errno(ah->pulse_context)));
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(ah->main_loop);
}

static void recording_stream_state_callback(pa_stream *stream, void *userdata) {
    OpenRecordingDevice *open_recording_device = (OpenRecordingDevice*)userdata;
    OpenRecordingDevicePulseAudio *ord = &open_recording_device->backend.pulse_audio;
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_READY:
            ord->stream_ready = true;
            break;
        case PA_STREAM_FAILED:
            panic("pulseaudio stream error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
            break;
    }
}

static void recording_stream_read_callback(pa_stream *stream, size_t nbytes, void *userdata) {
    OpenRecordingDevice *open_recording_device = (OpenRecordingDevice*)userdata;
    open_recording_device->read_callback(open_recording_device);
}

static int open_recording_device_init_pa(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device)
{
    OpenRecordingDevicePulseAudio *ord = &open_recording_device->backend.pulse_audio;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    GenesisAudioDevice *audio_device = open_recording_device->audio_device;
    ord->stream_ready = false;

    pa_threaded_mainloop_lock(ah->main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_sample_format(open_recording_device->sample_format);
    sample_spec.rate = audio_device->default_sample_rate;
    sample_spec.channels = audio_device->channel_layout.channel_count;

    pa_channel_map channel_map = to_pulseaudio_channel_map(&audio_device->channel_layout);

    ord->stream = pa_stream_new(ah->pulse_context, "Genesis", &sample_spec, &channel_map);
    if (!open_recording_device) {
        pa_threaded_mainloop_unlock(ah->main_loop);
        return GenesisErrorNoMem;
    }

    pa_stream *stream = ord->stream;

    pa_stream_set_state_callback(stream, recording_stream_state_callback, open_recording_device);
    pa_stream_set_read_callback(stream, recording_stream_read_callback, open_recording_device);

    int bytes_per_second = open_recording_device->bytes_per_frame * audio_device->default_sample_rate;
    int buffer_length = open_recording_device->bytes_per_frame *
        ceil(open_recording_device->latency * bytes_per_second / (double)open_recording_device->bytes_per_frame);

    ord->buffer_attr.maxlength = UINT32_MAX;
    ord->buffer_attr.tlength = UINT32_MAX;
    ord->buffer_attr.prebuf = 0;
    ord->buffer_attr.minreq = UINT32_MAX;
    ord->buffer_attr.fragsize = buffer_length;

    pa_threaded_mainloop_unlock(ah->main_loop);

    return 0;
}

static void open_recording_device_destroy_pa(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device)
{
    OpenRecordingDevicePulseAudio *ord = &open_recording_device->backend.pulse_audio;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    pa_stream *stream = ord->stream;
    if (stream) {
        pa_threaded_mainloop_lock(ah->main_loop);

        pa_stream_set_state_callback(stream, nullptr, nullptr);
        pa_stream_set_read_callback(stream, nullptr, nullptr);
        pa_stream_disconnect(stream);
        pa_stream_unref(stream);

        pa_threaded_mainloop_unlock(ah->main_loop);

        ord->stream = nullptr;
    }
}

static int open_recording_device_start_pa(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device)
{
    OpenRecordingDevicePulseAudio *ord = &open_recording_device->backend.pulse_audio;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;
    pa_threaded_mainloop_lock(ah->main_loop);

    int err = pa_stream_connect_record(ord->stream,
            open_recording_device->audio_device->name,
            &ord->buffer_attr, PA_STREAM_ADJUST_LATENCY);
    if (err) {
        pa_threaded_mainloop_unlock(ah->main_loop);
        return GenesisErrorOpeningAudioHardware;
    }

    pa_threaded_mainloop_unlock(ah->main_loop);
    return 0;
}

static void open_recording_device_peek_pa(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device, const char **data, int *frame_count)
{
    OpenRecordingDevicePulseAudio *ord = &open_recording_device->backend.pulse_audio;
    pa_stream *stream = ord->stream;
    if (ord->stream_ready) {
        size_t nbytes;
        if (pa_stream_peek(stream, (const void **)data, &nbytes))
            panic("pa_stream_peek error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
        *frame_count = ((int)nbytes) / open_recording_device->bytes_per_frame;
    } else {
        *data = nullptr;
        *frame_count = 0;
    }
}

static void open_recording_device_drop_pa(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device)
{
    OpenRecordingDevicePulseAudio *ord = &open_recording_device->backend.pulse_audio;
    pa_stream *stream = ord->stream;
    if (pa_stream_drop(stream))
        panic("pa_stream_drop error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
}

static void open_recording_device_clear_buffer_pa(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device)
{
    OpenRecordingDevicePulseAudio *ord = &open_recording_device->backend.pulse_audio;
    if (!ord->stream_ready)
        return;

    pa_stream *stream = ord->stream;
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;

    pa_threaded_mainloop_lock(ah->main_loop);

    for (;;) {
        const char *data;
        size_t nbytes;
        if (pa_stream_peek(stream, (const void **)&data, &nbytes))
            panic("pa_stream_peek error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));

        if (nbytes == 0)
            break;

        if (pa_stream_drop(stream))
            panic("pa_stream_drop error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
    }

    pa_threaded_mainloop_unlock(ah->main_loop);
}

static void refresh_audio_devices(AudioHardware *audio_hardware) {
    block_until_ready(audio_hardware);
    audio_hardware_flush_events(audio_hardware);
    block_until_have_devices(audio_hardware);
}

int audio_hardware_init_pulseaudio(AudioHardware *audio_hardware) {
    AudioHardwarePulseAudio *ah = &audio_hardware->backend.pulse_audio;

    ah->device_scan_queued = false;
    ah->ready_flag = false;
    ah->have_devices_flag = false;

    ah->main_loop = pa_threaded_mainloop_new();
    if (!ah->main_loop) {
        destroy_audio_hardware_pa(audio_hardware);
        return GenesisErrorNoMem;
    }

    pa_mainloop_api *main_loop_api = pa_threaded_mainloop_get_api(ah->main_loop);

    ah->props = pa_proplist_new();
    if (!ah->props) {
        destroy_audio_hardware_pa(audio_hardware);
        return GenesisErrorNoMem;
    }

    pa_proplist_sets(ah->props, PA_PROP_APPLICATION_NAME, "Genesis Audio Software");
    pa_proplist_sets(ah->props, PA_PROP_APPLICATION_VERSION, GENESIS_VERSION_STRING);
    pa_proplist_sets(ah->props, PA_PROP_APPLICATION_ID, "me.andrewkelley.genesis");

    ah->pulse_context = pa_context_new_with_proplist(main_loop_api, "Genesis",
            ah->props);
    if (!ah->pulse_context) {
        destroy_audio_hardware_pa(audio_hardware);
        return GenesisErrorNoMem;
    }

    pa_context_set_subscribe_callback(ah->pulse_context, subscribe_callback, audio_hardware);
    pa_context_set_state_callback(ah->pulse_context, context_state_callback, audio_hardware);

    int err = pa_context_connect(ah->pulse_context, NULL, (pa_context_flags_t)0, NULL);
    if (err) {
        destroy_audio_hardware_pa(audio_hardware);
        return GenesisErrorOpeningAudioHardware;
    }

    if (pa_threaded_mainloop_start(ah->main_loop)) {
        destroy_audio_hardware_pa(audio_hardware);
        return GenesisErrorNoMem;
    }

    audio_hardware->destroy = destroy_audio_hardware_pa;
    audio_hardware->flush_events = flush_events;
    audio_hardware->refresh_audio_devices = refresh_audio_devices;

    audio_hardware->open_playback_device_init = open_playback_device_init_pa;
    audio_hardware->open_playback_device_destroy = open_playback_device_destroy_pa;
    audio_hardware->open_playback_device_start = open_playback_device_start_pa;
    audio_hardware->open_playback_device_free_count = open_playback_device_free_count_pa;
    audio_hardware->open_playback_device_begin_write = open_playback_device_begin_write_pa;
    audio_hardware->open_playback_device_write = open_playback_device_write_pa;
    audio_hardware->open_playback_device_clear_buffer = open_playback_device_clear_buffer_pa;

    audio_hardware->open_recording_device_init = open_recording_device_init_pa;
    audio_hardware->open_recording_device_destroy = open_recording_device_destroy_pa;
    audio_hardware->open_recording_device_start = open_recording_device_start_pa;
    audio_hardware->open_recording_device_peek = open_recording_device_peek_pa;
    audio_hardware->open_recording_device_drop = open_recording_device_drop_pa;
    audio_hardware->open_recording_device_clear_buffer = open_recording_device_clear_buffer_pa;

    return 0;
}

