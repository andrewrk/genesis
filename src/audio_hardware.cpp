#include "audio_hardware.hpp"
#include "util.hpp"
#include <portaudio.h>
#include <stdlib.h>
#include <stdio.h>

AudioHardware::AudioHardware() :
    _initialized(false),
    _device_count(-1)
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

    _device_count = Pa_GetDeviceCount();
    for (int i = 0; i < _device_count; i += 1) {
        const PaDeviceInfo *device_info = Pa_GetDeviceInfo(i);
        fprintf(stderr, "device: %s\n", device_info->name);
    }
}

/*
static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
                           */
