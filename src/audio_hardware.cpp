#include "audio_hardware.hpp"
#include "util.hpp"
#include "genesis.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

void destroy_audio_hardware(struct AudioHardware *audio_hardware) {
    if (!audio_hardware)
        return;

    if (audio_hardware->destroy)
        audio_hardware->destroy(audio_hardware);

    if (audio_hardware->safe_devices_info) {
        for (int i = 0; i < audio_hardware->safe_devices_info->devices.length(); i += 1) {
            genesis_audio_device_unref(audio_hardware->safe_devices_info->devices.at(i));
        }
        destroy(audio_hardware->safe_devices_info, 1);
    }


    destroy(audio_hardware, 1);
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

    int err;
    // first try pulseaudio
    err = audio_hardware_init_pulseaudio(audio_hardware);
    if (err != GenesisErrorOpeningAudioHardware) {
        destroy_audio_hardware(audio_hardware);
        return err;
    }

    // fall back to dummy
    err = audio_hardware_init_dummy(audio_hardware);
    if (err) {
        destroy_audio_hardware(audio_hardware);
        return err;
    }

    *out_audio_hardware = audio_hardware;
    return 0;
}

void audio_hardware_flush_events(struct AudioHardware *audio_hardware) {
    if (audio_hardware->flush_events)
        audio_hardware->flush_events(audio_hardware);
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
    open_playback_device->userdata = userdata;
    open_playback_device->write_callback = write_callback;
    open_playback_device->underrun_callback = underrun_callback;
    open_playback_device->sample_format = sample_format;
    open_playback_device->latency = latency;
    open_playback_device->bytes_per_frame = genesis_get_bytes_per_frame(sample_format,
            audio_device->channel_layout.channel_count);

    AudioHardware *audio_hardware = audio_device->audio_hardware;
    int err = audio_hardware->open_playback_device_init(audio_hardware, open_playback_device);
    if (err) {
        open_playback_device_destroy(open_playback_device);
        return err;
    }

    *out_open_playback_device = open_playback_device;
    return 0;
}

void open_playback_device_destroy(OpenPlaybackDevice *open_playback_device) {
    if (!open_playback_device)
        return;

    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;

    if (audio_hardware->open_playback_device_destroy)
        audio_hardware->open_playback_device_destroy(audio_hardware, open_playback_device);

    genesis_audio_device_unref(open_playback_device->audio_device);
    destroy(open_playback_device, 1);
}

int open_playback_device_start(OpenPlaybackDevice *open_playback_device) {
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;

    return audio_hardware->open_playback_device_start(audio_hardware, open_playback_device);
}

int open_playback_device_free_count(OpenPlaybackDevice *open_playback_device) {
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;
    return audio_hardware->open_playback_device_free_count(audio_hardware, open_playback_device);
}

void open_playback_device_begin_write(OpenPlaybackDevice *open_playback_device, char **data, int *frame_count) {
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;
    audio_hardware->open_playback_device_begin_write(audio_hardware, open_playback_device, data, frame_count);
}

void open_playback_device_write(OpenPlaybackDevice *open_playback_device, char *data, int frame_count) {
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;
    audio_hardware->open_playback_device_write(audio_hardware, open_playback_device, data, frame_count);
}

void open_playback_device_clear_buffer(OpenPlaybackDevice *open_playback_device) {
    AudioHardware *audio_hardware = open_playback_device->audio_device->audio_hardware;
    audio_hardware->open_playback_device_clear_buffer(audio_hardware, open_playback_device);
}

void open_playback_device_fill_with_silence(OpenPlaybackDevice *open_playback_device) {
    char *buffer;
    int requested_frame_count = open_playback_device_free_count(open_playback_device);
    while (requested_frame_count > 0) {
        int frame_count = requested_frame_count;
        open_playback_device_begin_write(open_playback_device, &buffer, &frame_count);
        memset(buffer, 0, frame_count * open_playback_device->bytes_per_frame);
        open_playback_device_write(open_playback_device, buffer, frame_count);
        requested_frame_count -= frame_count;
    }
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
    open_recording_device->userdata = userdata;
    open_recording_device->read_callback = read_callback;
    open_recording_device->sample_format = sample_format;
    open_recording_device->latency = latency;
    open_recording_device->bytes_per_frame = genesis_get_bytes_per_frame(sample_format,
            audio_device->channel_layout.channel_count);

    AudioHardware *audio_hardware = audio_device->audio_hardware;
    int err = audio_hardware->open_recording_device_init(audio_hardware, open_recording_device);
    if (err) {
        open_recording_device_destroy(open_recording_device);
        return err;
    }

    *out_open_recording_device = open_recording_device;
    return 0;
}

void open_recording_device_destroy(OpenRecordingDevice *open_recording_device) {
    if (!open_recording_device)
        return;

    AudioHardware *audio_hardware = open_recording_device->audio_device->audio_hardware;

    if (audio_hardware->open_recording_device_destroy)
        audio_hardware->open_recording_device_destroy(audio_hardware, open_recording_device);

    genesis_audio_device_unref(open_recording_device->audio_device);
    destroy(open_recording_device, 1);
}

int open_recording_device_start(OpenRecordingDevice *open_recording_device) {
    AudioHardware *audio_hardware = open_recording_device->audio_device->audio_hardware;
    return audio_hardware->open_recording_device_start(audio_hardware, open_recording_device);
}

void open_recording_device_peek(OpenRecordingDevice *open_recording_device,
        const char **data, int *frame_count)
{
    AudioHardware *audio_hardware = open_recording_device->audio_device->audio_hardware;
    audio_hardware->open_recording_device_peek(audio_hardware, open_recording_device,
            data, frame_count);

}

void open_recording_device_drop(OpenRecordingDevice *open_recording_device) {
    AudioHardware *audio_hardware = open_recording_device->audio_device->audio_hardware;
    audio_hardware->open_recording_device_drop(audio_hardware, open_recording_device);
}

void open_recording_device_clear_buffer(OpenRecordingDevice *open_recording_device) {
    AudioHardware *audio_hardware = open_recording_device->audio_device->audio_hardware;
    audio_hardware->open_recording_device_clear_buffer(audio_hardware, open_recording_device);
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
    if (context->audio_hardware->refresh_audio_devices)
        context->audio_hardware->refresh_audio_devices(context->audio_hardware);
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
