#include "audio_hardware.hpp"
#include "util.hpp"

#include <portaudio.h>
#include <stdlib.h>
#include <stdio.h>

AudioHardware::AudioHardware() :
    _initialized(false)
{
    rescan_devices();
}

AudioHardware::~AudioHardware() {
    deinitialize();
}

void AudioHardware::deinitialize() {
    if (!_initialized)
        return;

    PaError err = Pa_Terminate();
    if (err != paNoError)
        panic("unable to terminate PortAudio: %s", Pa_GetErrorText(err));

    _initialized = false;
}

void AudioHardware::initialize() {
    if (_initialized)
        return;

    PaError err = Pa_Initialize();

    if (err != paNoError)
        panic("unable to init PortAudio: %s", Pa_GetErrorText(err));

    _initialized = true;
}

void AudioHardware::rescan_devices() {
    deinitialize();
    initialize();

    _devices.resize(Pa_GetDeviceCount());
    for (int i = 0; i < _devices.length(); i += 1) {
        const PaDeviceInfo *device_info = Pa_GetDeviceInfo(i);
        AudioDevice *audio_device = &_devices.at(i);
        audio_device->name = device_info->name;
        audio_device->max_input_channels = device_info->maxInputChannels;
        audio_device->max_output_channels = device_info->maxOutputChannels;
        audio_device->default_low_input_latency = device_info->defaultLowInputLatency;
        audio_device->default_low_output_latency = device_info->defaultLowOutputLatency;
        audio_device->default_high_input_latency = device_info->defaultHighInputLatency;
        audio_device->default_high_output_latency = device_info->defaultHighOutputLatency;
        audio_device->default_sample_rate = device_info->defaultSampleRate;
    }

    _default_input_index = Pa_GetDefaultInputDevice();
    _default_output_index = Pa_GetDefaultOutputDevice();
}

static PaSampleFormat to_portaudio_sample_format(SampleFormat sample_format) {
    switch (sample_format) {
    case SampleFormatUInt8:
        return paUInt8;
    case SampleFormatInt16:
        return paInt16;
    case SampleFormatInt32:
        return paInt32;
    case SampleFormatFloat:
        return paFloat32;
    case SampleFormatDouble:
        panic("cannot use double sample format with portaudio");
    }
    panic("invalid sample format");
}

static int playback_callback(const void *input_buffer, void *output_buffer,
        unsigned long frame_count, const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags status_flags, void *user_data)
{
    OpenPlaybackDevice *playback_device = reinterpret_cast<OpenPlaybackDevice*>(user_data);

    unsigned long read_count = frame_count * playback_device->_bytes_per_frame;
    memcpy(output_buffer, playback_device->_ring_buffer->read_ptr(), read_count);
    playback_device->_ring_buffer->advance_read_ptr(read_count);

    return paContinue;
}

OpenPlaybackDevice::OpenPlaybackDevice(AudioHardware *audio_hardware, int device_index,
        int channel_count, SampleFormat sample_format, double latency,
        int sample_rate, bool *ok) :
    _ring_buffer(NULL),
    _channel_count(channel_count),
    _stream(NULL)
{
    PaStreamParameters out_params;
    out_params.channelCount = channel_count;
    out_params.device = device_index;
    out_params.hostApiSpecificStreamInfo = NULL;
    out_params.sampleFormat = to_portaudio_sample_format(sample_format);
    out_params.suggestedLatency = latency;
    out_params.hostApiSpecificStreamInfo = NULL;

    PaError err = Pa_OpenStream(&_stream, NULL, &out_params, sample_rate, paFramesPerBufferUnspecified,
                paClipOff|paDitherOff, playback_callback, this);

    if (err) {
        panic("unable to init PortAudio: %s", Pa_GetErrorText(err));
        *ok = false;
        return;
    }

    _ring_buffer = create<RingBuffer>(latency * get_bytes_per_second(sample_format, channel_count, sample_rate));
    _bytes_per_frame = get_bytes_per_frame(sample_format, channel_count);

    *ok = true;
}

OpenPlaybackDevice::~OpenPlaybackDevice() {
    if (_stream)
        Pa_AbortStream(_stream);
    if (_ring_buffer)
        destroy(_ring_buffer, 1);
}
