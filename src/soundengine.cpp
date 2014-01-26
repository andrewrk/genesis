#include "soundengine.h"
#include <portaudio.h>

#include <QCoreApplication>
#include <QStringList>

#include <iostream>


static PaStream *stream = NULL;
static QStringList backendNames;
static int defaultBackend = -1;

static int currentBackend = -1;
static uint64_t currentChLayout = GENESIS_CH_LAYOUT_STEREO;
static int currentSampleRate = 44100;
static double currentLatency = 0.005;

static void terminate() {
	Pa_Terminate();
}

static int callback(const void *input, void *output,
		unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo,
		PaStreamCallbackFlags statusFlags, void *userData)
{
	memset(output, 0, frameCount * 4);
	return paContinue;
}

int SoundEngine::initialize()
{
    int err = Pa_Initialize();
    if (err != paNoError)
        return err;
    atexit(terminate);

    refreshBackends();
    currentBackend = defaultBackend;

    return openStream();
}


QString SoundEngine::errorString(int err)
{
	return Pa_GetErrorText(err);
}


void SoundEngine::refreshBackends()
{
    int count = Pa_GetHostApiCount();
    defaultBackend = Pa_GetDefaultHostApi();

    backendNames.clear();
    for (int i = 0; i < count; i += 1) {
        const PaHostApiInfo *info = Pa_GetHostApiInfo(i);
        QString caption = QObject::tr("%1 (%2 devices)").arg(QString(info->name), QString::number(info->deviceCount));
        if (i == defaultBackend) {
            caption.append(QObject::tr(" (default)"));
        }
        backendNames.append(caption);
    }
}


QStringList SoundEngine::backends()
{
    return backendNames;
}


int SoundEngine::defaultBackendIndex()
{
    return defaultBackend;
}


int SoundEngine::selectedBackendIndex()
{
    return currentBackend;
}

static void cleanupStream() {
    if (!stream) return;

    Pa_AbortStream(stream);
    stream = NULL;
}

int SoundEngine::openStream()
{
    cleanupStream();
    PaStreamParameters outputParams;
    outputParams.device = Pa_GetDefaultOutputDevice();
    outputParams.channelCount = genesis_get_channel_layout_nb_channels(currentChLayout);
    outputParams.sampleFormat = paFloat32;
    outputParams.suggestedLatency = currentLatency;
    outputParams.hostApiSpecificStreamInfo = NULL;
    PaStreamFlags flags = paClipOff|paDitherOff;
    int err = Pa_OpenStream(&stream, NULL, &outputParams, currentSampleRate, paFramesPerBufferUnspecified, flags, callback, NULL);
    if (err != paNoError)
        stream = NULL;
    return err;
}


int SoundEngine::setSoundParams(int backendIndex, int sampleRate, uint64_t channelLayout, double latency)
{
    if (stream &&
        backendIndex == currentBackend &&
        sampleRate == currentSampleRate &&
        channelLayout == currentChLayout &&
        latency == currentLatency)
    {
        return 0;
    }
    currentSampleRate = sampleRate;
    currentChLayout = channelLayout;
    currentLatency = latency;
    return openStream();
}


int SoundEngine::sampleRate()
{
    return currentSampleRate;
}



uint64_t SoundEngine::channelLayout()
{
    return currentChLayout;
}


double SoundEngine::latency()
{
    return currentLatency;
}
