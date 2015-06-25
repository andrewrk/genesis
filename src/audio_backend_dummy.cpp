#include "audio_backend_dummy.hpp"
#include "audio_hardware.hpp"
#include "os.hpp"

#include <stdio.h>
#include <string.h>

static void playback_thread_run(void *arg) {
    OpenPlaybackDevice *open_playback_device = (OpenPlaybackDevice *)arg;
    GenesisAudioDevice *audio_device = open_playback_device->audio_device;
    OpenPlaybackDeviceDummy *opd = &open_playback_device->backend.dummy;

    double start_time = os_get_time();
    long frames_consumed = 0;

    double time_per_frame = 1.0 / (double)audio_device->default_sample_rate;
    while (opd->abort_flag.test_and_set()) {
        opd->mutex->lock();
        opd->cond->timed_wait(opd->mutex, opd->period);
        opd->mutex->unlock();

        double now = os_get_time();
        double total_time = now - start_time;
        long total_frames = total_time / time_per_frame;
        int frames_to_kill = total_frames - frames_consumed;
        int read_count = min(frames_to_kill, opd->ring_buffer->fill_count());
        int frames_left = frames_to_kill - read_count;
        int byte_count = read_count * open_playback_device->bytes_per_frame;
        opd->ring_buffer->advance_read_ptr(byte_count);
        frames_consumed += read_count;

        if (frames_left > 0) {
            open_playback_device->underrun_callback(open_playback_device);
        } else if (read_count > 0) {
            open_playback_device->write_callback(open_playback_device, read_count);
        }
    }
}

/*
static void recording_thread_run(void *arg) {
    OpenRecordingDevice *open_recording_device = (OpenRecordingDevice *)arg;
    GenesisAudioDevice *audio_device = open_recording_device->audio_device;
    AudioHardware *audio_hardware = audio_device->audio_hardware;
    // TODO
}
*/

static void destroy_audio_hardware_dummy(AudioHardware *audio_hardware) { }

static void flush_events(AudioHardware *audio_hardware) { }

static void refresh_audio_devices(AudioHardware *audio_hardware) { }

static void open_playback_device_destroy_dummy(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device)
{
    OpenPlaybackDeviceDummy *opd = &open_playback_device->backend.dummy;
    if (opd->thread) {
        if (opd->thread->running()) {
            opd->abort_flag.clear();
            opd->mutex->lock();
            opd->cond->signal();
            opd->mutex->unlock();
            opd->thread->join();
        }
        destroy(opd->thread, 1);
        opd->thread = nullptr;
    }
    destroy(opd->mutex, 1);
    opd->mutex = nullptr;

    destroy(opd->cond, 1);
    opd->cond = nullptr;

    destroy(opd->ring_buffer, 1);
    opd->ring_buffer = nullptr;
}

static int open_playback_device_init_dummy(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device)
{
    int err;
    OpenPlaybackDeviceDummy *opd = &open_playback_device->backend.dummy;
    GenesisAudioDevice *audio_device = open_playback_device->audio_device;
    int buffer_frame_count = open_playback_device->latency * audio_device->default_sample_rate;
    opd->buffer_size = open_playback_device->bytes_per_frame * buffer_frame_count;
    opd->period = open_playback_device->latency * 0.5;

    opd->ring_buffer = create_zero<RingBuffer>(opd->buffer_size);
    if (!opd->ring_buffer) {
        open_playback_device_destroy_dummy(audio_hardware, open_playback_device);
        return GenesisErrorNoMem;
    }

    opd->thread = create_zero<Thread>();
    if (!opd->thread) {
        open_playback_device_destroy_dummy(audio_hardware, open_playback_device);
        return GenesisErrorNoMem;
    }
    opd->thread->set_high_priority();

    opd->mutex = create_zero<Mutex>();
    if (!opd->mutex) {
        open_playback_device_destroy_dummy(audio_hardware, open_playback_device);
        return GenesisErrorNoMem;
    }
    if ((err = opd->mutex->error())) {
        open_playback_device_destroy_dummy(audio_hardware, open_playback_device);
        return err;
    }

    opd->cond = create_zero<MutexCond>();
    if (!opd->cond) {
        open_playback_device_destroy_dummy(audio_hardware, open_playback_device);
        return GenesisErrorNoMem;
    }
    if ((err = opd->cond->error())) {
        open_playback_device_destroy_dummy(audio_hardware, open_playback_device);
        return err;
    }

    return 0;
}

static int open_playback_device_start_dummy(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device)
{
    OpenPlaybackDeviceDummy *opd = &open_playback_device->backend.dummy;

    open_playback_device_fill_with_silence(open_playback_device);
    assert(opd->ring_buffer->fill_count() == opd->buffer_size);

    opd->abort_flag.test_and_set();
    int err;
    if ((err = opd->thread->start(playback_thread_run, open_playback_device)))
        return err;


    return 0;
}

static int open_playback_device_free_count_dummy(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device)
{
    OpenPlaybackDeviceDummy *opd = &open_playback_device->backend.dummy;
    int fill_count = opd->ring_buffer->fill_count();
    int bytes_free_count = opd->buffer_size - fill_count;
    return bytes_free_count / open_playback_device->bytes_per_frame;
}

static void open_playback_device_begin_write_dummy(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device, char **data, int *frame_count)
{
    OpenPlaybackDeviceDummy *opd = &open_playback_device->backend.dummy;

    int byte_count = *frame_count * open_playback_device->bytes_per_frame;
    assert(byte_count <= opd->buffer_size);
    *data = opd->ring_buffer->write_ptr();
}

static void open_playback_device_write_dummy(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device, char *data, int frame_count)
{
    OpenPlaybackDeviceDummy *opd = &open_playback_device->backend.dummy;
    assert(data == opd->ring_buffer->write_ptr());
    int byte_count = frame_count * open_playback_device->bytes_per_frame;
    opd->ring_buffer->advance_write_ptr(byte_count);
}

static void open_playback_device_clear_buffer_dummy(AudioHardware *audio_hardware,
        OpenPlaybackDevice *open_playback_device)
{
    OpenPlaybackDeviceDummy *opd = &open_playback_device->backend.dummy;
    opd->ring_buffer->clear();
}

static int open_recording_device_init_dummy(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device)
{
    // TODO
    return 0;
}

static void open_recording_device_destroy_dummy(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device)
{
    // TODO
}

static int open_recording_device_start_dummy(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device)
{
    // TODO
    return 0;
}

static void open_recording_device_peek_dummy(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device, const char **data, int *frame_count)
{
    // TODO
}

static void open_recording_device_drop_dummy(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device)
{
    // TODO
}

static void open_recording_device_clear_buffer_dummy(AudioHardware *audio_hardware,
        OpenRecordingDevice *open_recording_device)
{
    // TODO
}

int audio_hardware_init_dummy(AudioHardware *audio_hardware) {
    audio_hardware->safe_devices_info = create_zero<AudioDevicesInfo>();
    audio_hardware->safe_devices_info->default_input_index = 0;
    audio_hardware->safe_devices_info->default_output_index = 0;

    // create output device
    {
        GenesisAudioDevice *audio_device = create_zero<GenesisAudioDevice>();
        if (!audio_device)
            return GenesisErrorNoMem;

        audio_device->ref_count = 1;
        audio_device->audio_hardware = audio_hardware;
        audio_device->name = strdup("dummy-out");
        audio_device->description = strdup("Dummy output device");
        if (!audio_device->name || !audio_device->description) {
            free(audio_device->name);
            free(audio_device->description);
            return GenesisErrorNoMem;
        }
        audio_device->channel_layout = *genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
        audio_device->default_sample_format = GenesisSampleFormatFloat;
        audio_device->default_latency = 0.01;
        audio_device->default_sample_rate = 48000;
        audio_device->purpose = GenesisAudioDevicePurposePlayback;

        if (audio_hardware->safe_devices_info->devices.append(audio_device)) {
            genesis_audio_device_unref(audio_device);
            return GenesisErrorNoMem;
        }
    }

    // create input device
    {
        GenesisAudioDevice *audio_device = create_zero<GenesisAudioDevice>();
        if (!audio_device)
            return GenesisErrorNoMem;

        audio_device->ref_count = 1;
        audio_device->audio_hardware = audio_hardware;
        audio_device->name = strdup("dummy-in");
        audio_device->description = strdup("Dummy input device");
        if (!audio_device->name || !audio_device->description) {
            free(audio_device->name);
            free(audio_device->description);
            return GenesisErrorNoMem;
        }
        audio_device->channel_layout = *genesis_channel_layout_get_builtin(GenesisChannelLayoutIdMono);
        audio_device->default_sample_format = GenesisSampleFormatFloat;
        audio_device->default_latency = 0.01;
        audio_device->default_sample_rate = 48000;
        audio_device->purpose = GenesisAudioDevicePurposeRecording;

        if (audio_hardware->safe_devices_info->devices.append(audio_device)) {
            genesis_audio_device_unref(audio_device);
            return GenesisErrorNoMem;
        }
    }


    audio_hardware->destroy = destroy_audio_hardware_dummy;
    audio_hardware->flush_events = flush_events;
    audio_hardware->refresh_audio_devices = refresh_audio_devices;

    audio_hardware->open_playback_device_init = open_playback_device_init_dummy;
    audio_hardware->open_playback_device_destroy = open_playback_device_destroy_dummy;
    audio_hardware->open_playback_device_start = open_playback_device_start_dummy;
    audio_hardware->open_playback_device_free_count = open_playback_device_free_count_dummy;
    audio_hardware->open_playback_device_begin_write = open_playback_device_begin_write_dummy;
    audio_hardware->open_playback_device_write = open_playback_device_write_dummy;
    audio_hardware->open_playback_device_clear_buffer = open_playback_device_clear_buffer_dummy;

    audio_hardware->open_recording_device_init = open_recording_device_init_dummy;
    audio_hardware->open_recording_device_destroy = open_recording_device_destroy_dummy;
    audio_hardware->open_recording_device_start = open_recording_device_start_dummy;
    audio_hardware->open_recording_device_peek = open_recording_device_peek_dummy;
    audio_hardware->open_recording_device_drop = open_recording_device_drop_dummy;
    audio_hardware->open_recording_device_clear_buffer = open_recording_device_clear_buffer_dummy;

    return 0;
}
