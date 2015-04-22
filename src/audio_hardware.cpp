#include "audio_hardware.hpp"
#include "util.hpp"
#include "genesis.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void subscribe_callback(pa_context *context,
        pa_subscription_event_type_t event_bits, uint32_t index, void *userdata)
{
    AudioHardware *audio_hardware = (AudioHardware *)userdata;
    audio_hardware->device_scan_queued = true;
    pa_threaded_mainloop_signal(audio_hardware->main_loop, 0);
}

static void subscribe_to_events(AudioHardware *audio_hardware) {
    pa_subscription_mask_t events = (pa_subscription_mask_t)(
            PA_SUBSCRIPTION_MASK_SINK|PA_SUBSCRIPTION_MASK_SOURCE|PA_SUBSCRIPTION_MASK_SERVER);
    pa_operation *subscribe_op = pa_context_subscribe(audio_hardware->pulse_context,
            events, nullptr, audio_hardware);
    if (!subscribe_op)
        panic("pa_context_subscribe failed: %s", pa_strerror(pa_context_errno(audio_hardware->pulse_context)));
    pa_operation_unref(subscribe_op);
}

static void context_state_callback(pa_context *context, void *userdata) {
    AudioHardware *audio_hardware = (AudioHardware *)userdata;

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
        audio_hardware->device_scan_queued = true;
        subscribe_to_events(audio_hardware);
        audio_hardware->ready_flag = true;
        pa_threaded_mainloop_signal(audio_hardware->main_loop, 0);
        return;
    case PA_CONTEXT_TERMINATED: // The connection was terminated cleanly.
        pa_threaded_mainloop_signal(audio_hardware->main_loop, 0);
        return;
    case PA_CONTEXT_FAILED: // The connection failed or was disconnected.
        panic("pulsaudio connect failure: %s", pa_strerror(pa_context_errno(context)));
    }
}

static void destroy_current_audio_devices_info(AudioHardware *audio_hardware) {
    if (audio_hardware->current_audio_devices_info) {
        for (int i = 0; i < audio_hardware->current_audio_devices_info->devices.length(); i += 1)
            genesis_audio_device_unref(audio_hardware->current_audio_devices_info->devices.at(i));

        destroy(audio_hardware->current_audio_devices_info, 1);
        audio_hardware->current_audio_devices_info = nullptr;
    }
}

static void destroy_ready_audio_devices_info(AudioHardware *audio_hardware) {
    if (audio_hardware->ready_audio_devices_info) {
        for (int i = 0; i < audio_hardware->ready_audio_devices_info->devices.length(); i += 1)
            genesis_audio_device_unref(audio_hardware->ready_audio_devices_info->devices.at(i));
        destroy(audio_hardware->ready_audio_devices_info, 1);
        audio_hardware->ready_audio_devices_info = nullptr;
    }
}

void destroy_audio_hardware(struct AudioHardware *audio_hardware) {
    if (audio_hardware) {
        if (audio_hardware->main_loop)
            pa_threaded_mainloop_stop(audio_hardware->main_loop);

        destroy_current_audio_devices_info(audio_hardware);
        destroy_ready_audio_devices_info(audio_hardware);

        for (int i = 0; i < audio_hardware->safe_devices_info->devices.length(); i += 1)
            genesis_audio_device_unref(audio_hardware->safe_devices_info->devices.at(i));
        destroy(audio_hardware->safe_devices_info, 1);

        pa_context_disconnect(audio_hardware->pulse_context);
        pa_context_unref(audio_hardware->pulse_context);

        if (audio_hardware->main_loop)
            pa_threaded_mainloop_free(audio_hardware->main_loop);

        if (audio_hardware->props)
            pa_proplist_free(audio_hardware->props);

        free(audio_hardware->default_sink_name);
        free(audio_hardware->default_source_name);

        destroy(audio_hardware, 1);
    }
}

int create_audio_hardware(GenesisContext *context, void *userdata,
        void (*on_devices_change)(AudioHardware *), void (*on_events_signal)(AudioHardware *),
        struct AudioHardware **out_audio_hardware)
{
    *out_audio_hardware = nullptr;

    AudioHardware *audio_hardware = create_zero<AudioHardware>();
    if (!audio_hardware) {
        destroy_audio_hardware(audio_hardware);
        return GenesisErrorNoMem;
    }

    audio_hardware->genesis_context = context;
    audio_hardware->userdata = userdata;
    audio_hardware->on_devices_change = on_devices_change;
    audio_hardware->on_events_signal = on_events_signal;
    audio_hardware->device_scan_queued = false;
    audio_hardware->ready_flag = false;
    audio_hardware->have_devices_flag = false;

    audio_hardware->main_loop = pa_threaded_mainloop_new();
    if (!audio_hardware->main_loop) {
        destroy_audio_hardware(audio_hardware);
        return GenesisErrorNoMem;
    }

    pa_mainloop_api *main_loop_api = pa_threaded_mainloop_get_api(audio_hardware->main_loop);

    audio_hardware->props = pa_proplist_new();
    if (!audio_hardware->props) {
        destroy_audio_hardware(audio_hardware);
        return GenesisErrorNoMem;
    }

    pa_proplist_sets(audio_hardware->props, PA_PROP_APPLICATION_NAME, "Genesis Audio Software");
    pa_proplist_sets(audio_hardware->props, PA_PROP_APPLICATION_VERSION, GENESIS_VERSION_STRING);
    pa_proplist_sets(audio_hardware->props, PA_PROP_APPLICATION_ID, "me.andrewkelley.genesis");

    audio_hardware->pulse_context = pa_context_new_with_proplist(main_loop_api, "Genesis",
            audio_hardware->props);
    if (!audio_hardware->pulse_context) {
        destroy_audio_hardware(audio_hardware);
        return GenesisErrorNoMem;
    }

    pa_context_set_subscribe_callback(audio_hardware->pulse_context, subscribe_callback, audio_hardware);
    pa_context_set_state_callback(audio_hardware->pulse_context, context_state_callback, audio_hardware);

    int err = pa_context_connect(audio_hardware->pulse_context, NULL, (pa_context_flags_t)0, NULL);
    if (err) {
        destroy_audio_hardware(audio_hardware);
        return GenesisErrorOpeningAudioHardware;
    }

    if (pa_threaded_mainloop_start(audio_hardware->main_loop)) {
        destroy_audio_hardware(audio_hardware);
        return GenesisErrorNoMem;
    }

    *out_audio_hardware = audio_hardware;
    return 0;
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
    for (;;) {
        switch (pa_operation_get_state(op)) {
        case PA_OPERATION_RUNNING:
            pa_threaded_mainloop_wait(audio_hardware->main_loop);
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
    if (!audio_hardware->have_sink_list ||
        !audio_hardware->have_source_list ||
        !audio_hardware->have_default_sink)
    {
        return;
    }

    // based on the default sink name, figure out the default output index
    audio_hardware->current_audio_devices_info->default_output_index = -1;
    audio_hardware->current_audio_devices_info->default_input_index = -1;
    for (int i = 0; i < audio_hardware->current_audio_devices_info->devices.length(); i += 1) {
        GenesisAudioDevice *audio_device = audio_hardware->current_audio_devices_info->devices.at(i);
        if (audio_device->purpose == GenesisAudioDevicePurposePlayback &&
            strcmp(audio_device->name, audio_hardware->default_sink_name) == 0)
        {
            audio_hardware->current_audio_devices_info->default_output_index = i;
        } else if (audio_device->purpose == GenesisAudioDevicePurposeRecording &&
            strcmp(audio_device->name, audio_hardware->default_source_name) == 0)
        {
            audio_hardware->current_audio_devices_info->default_input_index = i;
        }
    }

    destroy_ready_audio_devices_info(audio_hardware);
    audio_hardware->ready_audio_devices_info = audio_hardware->current_audio_devices_info;
    audio_hardware->current_audio_devices_info = NULL;
    audio_hardware->have_devices_flag = true;
    pa_threaded_mainloop_signal(audio_hardware->main_loop, 0);
    audio_hardware->on_events_signal(audio_hardware);
}

static void sink_info_callback(pa_context *pulse_context, const pa_sink_info *info, int eol, void *userdata) {
    AudioHardware *audio_hardware = (AudioHardware *)userdata;
    if (eol) {
        audio_hardware->have_sink_list = true;
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

        if (audio_hardware->current_audio_devices_info->devices.append(audio_device))
            panic("out of memory");
    }
    pa_threaded_mainloop_signal(audio_hardware->main_loop, 0);
}

static void source_info_callback(pa_context *pulse_context, const pa_source_info *info, int eol, void *userdata) {
    AudioHardware *audio_hardware = (AudioHardware *)userdata;
    if (eol) {
        audio_hardware->have_source_list = true;
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

        if (audio_hardware->current_audio_devices_info->devices.append(audio_device))
            panic("out of memory");
    }
    pa_threaded_mainloop_signal(audio_hardware->main_loop, 0);
}

static void server_info_callback(pa_context *pulse_context, const pa_server_info *info, void *userdata) {
    AudioHardware *audio_hardware = (AudioHardware *)userdata;

    free(audio_hardware->default_sink_name);
    free(audio_hardware->default_source_name);

    audio_hardware->default_sink_name = strdup(info->default_sink_name);
    audio_hardware->default_source_name = strdup(info->default_source_name);

    if (!audio_hardware->default_sink_name || !audio_hardware->default_source_name)
        panic("out of memory");

    audio_hardware->have_default_sink = true;
    finish_device_query(audio_hardware);
    pa_threaded_mainloop_signal(audio_hardware->main_loop, 0);
}

static void scan_devices(AudioHardware *audio_hardware) {
    audio_hardware->have_sink_list = false;
    audio_hardware->have_default_sink = false;
    audio_hardware->have_source_list = false;

    destroy_current_audio_devices_info(audio_hardware);
    audio_hardware->current_audio_devices_info = create_zero<AudioDevicesInfo>();
    if (!audio_hardware->current_audio_devices_info)
        panic("out of memory");

    pa_threaded_mainloop_lock(audio_hardware->main_loop);

    pa_operation *list_sink_op = pa_context_get_sink_info_list(audio_hardware->pulse_context,
            sink_info_callback, audio_hardware);
    pa_operation *list_source_op = pa_context_get_source_info_list(audio_hardware->pulse_context,
            source_info_callback, audio_hardware);
    pa_operation *server_info_op = pa_context_get_server_info(audio_hardware->pulse_context,
            server_info_callback, audio_hardware);

    if (perform_operation(audio_hardware, list_sink_op))
        panic("list sinks failed");
    if (perform_operation(audio_hardware, list_source_op))
        panic("list sources failed");
    if (perform_operation(audio_hardware, server_info_op))
        panic("get server info failed");

    pa_threaded_mainloop_signal(audio_hardware->main_loop, 0);

    pa_threaded_mainloop_unlock(audio_hardware->main_loop);
}

void audio_hardware_flush_events(struct AudioHardware *audio_hardware) {
    if (audio_hardware->device_scan_queued) {
        audio_hardware->device_scan_queued = false;
        scan_devices(audio_hardware);
    }

    AudioDevicesInfo *old_devices_info = nullptr;
    bool change = false;

    pa_threaded_mainloop_lock(audio_hardware->main_loop);

    if (audio_hardware->ready_audio_devices_info) {
        old_devices_info = audio_hardware->safe_devices_info;
        audio_hardware->safe_devices_info = audio_hardware->ready_audio_devices_info;
        audio_hardware->ready_audio_devices_info = nullptr;
        change = true;
    }

    pa_threaded_mainloop_unlock(audio_hardware->main_loop);

    if (change)
        audio_hardware->on_devices_change(audio_hardware);

    if (old_devices_info) {
        for (int i = 0; i < old_devices_info->devices.length(); i += 1)
            genesis_audio_device_unref(old_devices_info->devices.at(i));
        destroy(old_devices_info, 1);
    }
}

static void block_until_ready(AudioHardware *audio_hardware) {
    if (audio_hardware->ready_flag)
        return;
    pa_threaded_mainloop_lock(audio_hardware->main_loop);
    while (!audio_hardware->ready_flag) {
        pa_threaded_mainloop_wait(audio_hardware->main_loop);
    }
    pa_threaded_mainloop_unlock(audio_hardware->main_loop);
}

static void block_until_have_devices(AudioHardware *audio_hardware) {
    if (audio_hardware->have_devices_flag)
        return;
    pa_threaded_mainloop_lock(audio_hardware->main_loop);
    while (!audio_hardware->have_devices_flag) {
        pa_threaded_mainloop_wait(audio_hardware->main_loop);
    }
    pa_threaded_mainloop_unlock(audio_hardware->main_loop);
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
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_READY:
            open_playback_device->stream_ready = true;
            pa_threaded_mainloop_signal(audio_hardware->main_loop, 0);
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

int open_playback_device_create(GenesisAudioDevice *audio_device, GenesisSampleFormat sample_format,
        double latency, void *userdata, void (*write_callback)(OpenPlaybackDevice *, int),
        void (*underrun_callback)(OpenPlaybackDevice *),
        OpenPlaybackDevice **out_open_playback_device)
{
    *out_open_playback_device = nullptr;

    OpenPlaybackDevice *open_playback_device = create_zero<OpenPlaybackDevice>();
    if (!open_playback_device) {
        open_playback_device_destroy(open_playback_device);
        return GenesisErrorNoMem;
    }

    genesis_audio_device_ref(audio_device);
    open_playback_device->audio_device = audio_device;
    open_playback_device->stream_ready = false;
    open_playback_device->userdata = userdata;
    open_playback_device->write_callback = write_callback;
    open_playback_device->underrun_callback = underrun_callback;

    AudioHardware *audio_hardware = audio_device->audio_hardware;

    pa_threaded_mainloop_lock(audio_hardware->main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_sample_format(sample_format);
    sample_spec.rate = audio_device->default_sample_rate;
    sample_spec.channels = audio_device->channel_layout.channel_count;
    pa_channel_map channel_map = to_pulseaudio_channel_map(&audio_device->channel_layout);

    open_playback_device->stream = pa_stream_new(audio_hardware->pulse_context,
            "Genesis", &sample_spec, &channel_map);
    if (!open_playback_device->stream) {
        pa_threaded_mainloop_unlock(audio_hardware->main_loop);
        open_playback_device_destroy(open_playback_device);
        return GenesisErrorNoMem;
    }

    pa_stream_set_state_callback(open_playback_device->stream, playback_stream_state_callback,
            open_playback_device);
    pa_stream_set_write_callback(open_playback_device->stream, playback_stream_write_callback,
            open_playback_device);
    pa_stream_set_underflow_callback(open_playback_device->stream, playback_stream_underflow_callback,
            open_playback_device);

    open_playback_device->bytes_per_frame = genesis_get_bytes_per_frame(sample_format,
            audio_device->channel_layout.channel_count);
    int bytes_per_second = open_playback_device->bytes_per_frame * audio_device->default_sample_rate;
    int buffer_length = open_playback_device->bytes_per_frame *
        ceil(latency * bytes_per_second / (double)open_playback_device->bytes_per_frame);

    open_playback_device->buffer_attr.maxlength = buffer_length;
    open_playback_device->buffer_attr.tlength = buffer_length;
    open_playback_device->buffer_attr.prebuf = 0;
    open_playback_device->buffer_attr.minreq = UINT32_MAX;
    open_playback_device->buffer_attr.fragsize = UINT32_MAX;

    pa_threaded_mainloop_unlock(audio_hardware->main_loop);

    *out_open_playback_device = open_playback_device;
    return 0;
}

void open_playback_device_destroy(OpenPlaybackDevice *open_playback_device) {
    if (open_playback_device) {
        AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;

        pa_stream *stream = open_playback_device->stream;
        if (stream) {
            pa_threaded_mainloop_lock(audio_hardware->main_loop);

            pa_stream_set_write_callback(stream, nullptr, nullptr);
            pa_stream_set_state_callback(stream, nullptr, nullptr);
            pa_stream_set_underflow_callback(stream, nullptr, nullptr);
            pa_stream_disconnect(stream);
            pa_stream_unref(stream);

            pa_threaded_mainloop_unlock(audio_hardware->main_loop);
        }

        genesis_audio_device_unref(open_playback_device->audio_device);
        destroy(open_playback_device, 1);
    }
}

int open_playback_device_start(OpenPlaybackDevice *open_playback_device) {
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;

    pa_threaded_mainloop_lock(audio_hardware->main_loop);

    int err = pa_stream_connect_playback(open_playback_device->stream,
            open_playback_device->audio_device->name, &open_playback_device->buffer_attr,
            PA_STREAM_ADJUST_LATENCY, nullptr, nullptr);
    if (err) {
        pa_threaded_mainloop_unlock(audio_hardware->main_loop);
        return GenesisErrorOpeningAudioHardware;
    }

    while (!open_playback_device->stream_ready)
        pa_threaded_mainloop_wait(audio_hardware->main_loop);

    pa_threaded_mainloop_unlock(audio_hardware->main_loop);

    return 0;
}

void open_playback_device_lock(OpenPlaybackDevice *open_playback_device) {
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;
    pa_threaded_mainloop_lock(audio_hardware->main_loop);
}

void open_playback_device_unlock(OpenPlaybackDevice *open_playback_device) {
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;
    pa_threaded_mainloop_unlock(audio_hardware->main_loop);
}

int open_playback_device_free_count(OpenPlaybackDevice *open_playback_device) {
    return pa_stream_writable_size(open_playback_device->stream) / open_playback_device->bytes_per_frame;
}

void open_playback_device_begin_write(OpenPlaybackDevice *open_playback_device, char **data, int *frame_count) {
    pa_stream *stream = open_playback_device->stream;
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;
    size_t byte_count = *frame_count * open_playback_device->bytes_per_frame;
    if (pa_stream_begin_write(stream, (void**)data, &byte_count))
        panic("pa_stream_begin_write error: %s", pa_strerror(pa_context_errno(audio_hardware->pulse_context)));

    *frame_count = byte_count / open_playback_device->bytes_per_frame;
}

void open_playback_device_write(OpenPlaybackDevice *open_playback_device, char *data, int frame_count) {
    pa_stream *stream = open_playback_device->stream;
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;
    size_t byte_count = frame_count * open_playback_device->bytes_per_frame;
    if (pa_stream_write(stream, data, byte_count, NULL, 0, PA_SEEK_RELATIVE))
        panic("pa_stream_write error: %s", pa_strerror(pa_context_errno(audio_hardware->pulse_context)));
}

void open_playback_device_clear_buffer(OpenPlaybackDevice *open_playback_device) {
    pa_stream *stream = open_playback_device->stream;
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;
    pa_threaded_mainloop_lock(audio_hardware->main_loop);
    pa_operation *op = pa_stream_flush(stream, NULL, NULL);
    if (!op)
        panic("pa_stream_flush failed: %s", pa_strerror(pa_context_errno(audio_hardware->pulse_context)));
    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(audio_hardware->main_loop);
}

static void recording_stream_state_callback(pa_stream *stream, void *userdata) {
    OpenRecordingDevice *open_recording_device = (OpenRecordingDevice*)userdata;
    switch (pa_stream_get_state(stream)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
            break;
        case PA_STREAM_READY:
            open_recording_device->stream_ready = true;
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

int open_recording_device_create(GenesisAudioDevice *audio_device,
        GenesisSampleFormat sample_format, double latency, void *userdata,
        void (*read_callback)(OpenRecordingDevice *),
        OpenRecordingDevice **out_open_recording_device)
{
    *out_open_recording_device = nullptr;

    OpenRecordingDevice *open_recording_device = create_zero<OpenRecordingDevice>();
    if (!open_recording_device) {
        open_recording_device_destroy(open_recording_device);
        return GenesisErrorNoMem;
    }

    genesis_audio_device_ref(audio_device);
    open_recording_device->audio_device = audio_device;
    open_recording_device->stream_ready = false;
    open_recording_device->userdata = userdata;
    open_recording_device->read_callback = read_callback;

    AudioHardware *audio_hardware = audio_device->audio_hardware;

    pa_threaded_mainloop_lock(audio_hardware->main_loop);

    pa_sample_spec sample_spec;
    sample_spec.format = to_pulseaudio_sample_format(sample_format);
    sample_spec.rate = audio_device->default_sample_rate;
    sample_spec.channels = audio_device->channel_layout.channel_count;

    pa_channel_map channel_map = to_pulseaudio_channel_map(&audio_device->channel_layout);

    open_recording_device->stream = pa_stream_new(audio_hardware->pulse_context,
            "Genesis", &sample_spec, &channel_map);
    if (!open_recording_device) {
        pa_threaded_mainloop_unlock(audio_hardware->main_loop);
        open_recording_device_destroy(open_recording_device);
        return GenesisErrorNoMem;
    }

    pa_stream *stream = open_recording_device->stream;

    pa_stream_set_state_callback(stream, recording_stream_state_callback, open_recording_device);
    pa_stream_set_read_callback(stream, recording_stream_read_callback, open_recording_device);

    open_recording_device->bytes_per_frame = genesis_get_bytes_per_frame(sample_format,
            audio_device->channel_layout.channel_count);
    int bytes_per_second = open_recording_device->bytes_per_frame * audio_device->default_sample_rate;
    int buffer_length = open_recording_device->bytes_per_frame *
        ceil(latency * bytes_per_second / (double)open_recording_device->bytes_per_frame);

    open_recording_device->buffer_attr.maxlength = UINT32_MAX;
    open_recording_device->buffer_attr.tlength = UINT32_MAX;
    open_recording_device->buffer_attr.prebuf = 0;
    open_recording_device->buffer_attr.minreq = UINT32_MAX;
    open_recording_device->buffer_attr.fragsize = buffer_length;

    pa_threaded_mainloop_unlock(audio_hardware->main_loop);

    *out_open_recording_device = open_recording_device;
    return 0;
}

void open_recording_device_destroy(OpenRecordingDevice *open_recording_device) {
    if (open_recording_device) {
        pa_stream *stream = open_recording_device->stream;
        if (stream) {
            AudioHardware *audio_hardware = open_recording_device->audio_device->audio_hardware;
            pa_threaded_mainloop_lock(audio_hardware->main_loop);

            pa_stream_set_state_callback(stream, nullptr, nullptr);
            pa_stream_set_read_callback(stream, nullptr, nullptr);
            pa_stream_disconnect(stream);
            pa_stream_unref(stream);

            pa_threaded_mainloop_unlock(audio_hardware->main_loop);
        }

        genesis_audio_device_unref(open_recording_device->audio_device);
        destroy(open_recording_device, 1);
    }
}

int open_recording_device_start(OpenRecordingDevice *open_recording_device) {
    AudioHardware *audio_hardware = open_recording_device->audio_device->audio_hardware;

    pa_threaded_mainloop_lock(audio_hardware->main_loop);

    int err = pa_stream_connect_record(open_recording_device->stream,
            open_recording_device->audio_device->name,
            &open_recording_device->buffer_attr, PA_STREAM_ADJUST_LATENCY);
    if (err) {
        pa_threaded_mainloop_unlock(audio_hardware->main_loop);
        return GenesisErrorOpeningAudioHardware;
    }

    pa_threaded_mainloop_unlock(audio_hardware->main_loop);
    return 0;
}

void open_recording_device_peek(OpenRecordingDevice *open_recording_device,
        const char **data, int *frame_count)
{
    pa_stream *stream = open_recording_device->stream;
    if (open_recording_device->stream_ready) {
        size_t nbytes;
        if (pa_stream_peek(stream, (const void **)data, &nbytes))
            panic("pa_stream_peek error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
        *frame_count = ((int)nbytes) / open_recording_device->bytes_per_frame;
    } else {
        *data = nullptr;
        *frame_count = 0;
    }
}

void open_recording_device_drop(OpenRecordingDevice *open_recording_device) {
    pa_stream *stream = open_recording_device->stream;
    if (pa_stream_drop(stream))
        panic("pa_stream_drop error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(stream))));
}

void open_recording_device_clear_buffer(OpenRecordingDevice *open_recording_device) {
    if (!open_recording_device->stream_ready)
        return;

    pa_stream *stream = open_recording_device->stream;
    AudioHardware *audio_hardware = open_recording_device->audio_device->audio_hardware;

    pa_threaded_mainloop_lock(audio_hardware->main_loop);

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

    pa_threaded_mainloop_unlock(audio_hardware->main_loop);
}

void genesis_audio_device_unref(struct GenesisAudioDevice *device) {
    if (device) {
        device->ref_count -= 1;
        if (device->ref_count < 0)
            panic("negative ref count");

        if (device->ref_count == 0) {
            free(device->name);
            free(device->description);
            destroy(device, 1);
        }
    }
}

void genesis_audio_device_ref(struct GenesisAudioDevice *device) {
    device->ref_count += 1;
}

void genesis_refresh_audio_devices(struct GenesisContext *context) {
    block_until_ready(context->audio_hardware);
    audio_hardware_flush_events(context->audio_hardware);
    block_until_have_devices(context->audio_hardware);
}

int genesis_get_audio_device_count(struct GenesisContext *context) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware->safe_devices_info;
    if (!audio_device_info)
        return -1;
    return audio_device_info->devices.length();
}

struct GenesisAudioDevice *genesis_get_audio_device(struct GenesisContext *context, int index) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware->safe_devices_info;
    if (!audio_device_info)
        return nullptr;
    if (index < 0 || index >= audio_device_info->devices.length())
        return nullptr;
    GenesisAudioDevice *audio_device = audio_device_info->devices.at(index);
    genesis_audio_device_ref(audio_device);
    return audio_device;
}

int genesis_get_default_playback_device_index(struct GenesisContext *context) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware->safe_devices_info;
    if (!audio_device_info)
        return -1;
    return audio_device_info->default_output_index;
}

int genesis_get_default_recording_device_index(struct GenesisContext *context) {
    const AudioDevicesInfo *audio_device_info = context->audio_hardware->safe_devices_info;
    if (!audio_device_info)
        return -1;
    return audio_device_info->default_input_index;
}

const char *genesis_audio_device_name(const struct GenesisAudioDevice *audio_device) {
    return audio_device->name;
}

const char *genesis_audio_device_description(const struct GenesisAudioDevice *audio_device) {
    return audio_device->description;
}

enum GenesisAudioDevicePurpose genesis_audio_device_purpose(const struct GenesisAudioDevice *audio_device) {
    return audio_device->purpose;
}

const struct GenesisChannelLayout *genesis_audio_device_channel_layout(
        const struct GenesisAudioDevice *audio_device)
{
    return &audio_device->channel_layout;
}

int genesis_audio_device_sample_rate(const struct GenesisAudioDevice *audio_device) {
    return audio_device->default_sample_rate;
}

void genesis_set_audio_device_callback(struct GenesisContext *context,
        void (*callback)(void *userdata),
        void *userdata)
{
    context->devices_change_callback_userdata = userdata;
    context->devices_change_callback = callback;
}


bool genesis_audio_device_equal(
        const struct GenesisAudioDevice *a,
        const struct GenesisAudioDevice *b)
{
    return strcmp(a->name, b->name) == 0;
}
