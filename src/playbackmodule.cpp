#include "playbackmodule.h"

#include <QCoreApplication>
#include <QStringList>

#include <iostream>


static QStringList deviceNames;
static int defaultOutDev = -1;
static int defaultInDev = -1;
static QSet<PlaybackModule *> playbackModules;
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

int PlaybackModule::initialize()
{
    int err = Pa_Initialize();
    if (err != paNoError)
        return err;
    atexit(terminate);

    refreshDevices();

    return 0;
}


PlaybackModule::PlaybackModule(int sampleRate, uint64_t channelLayout, int deviceIndex) :
    currentChLayout(channelLayout),
    currentSampleRate(sampleRate),
    currentDeviceIndex(deviceIndex)
{
    playbackModules.insert(this);
}

PlaybackModule::~PlaybackModule()
{
    playbackModules.remove(this);
}


QString PlaybackModule::errorString(int err)
{
	return Pa_GetErrorText(err);
}

void PlaybackModule::refreshDevices()
{
    int count = Pa_GetDeviceCount();
    defaultOutDev = Pa_GetDefaultOutputDevice();
    defaultInDev = Pa_GetDefaultInputDevice();

    deviceNames.clear();
    for (int i = 0; i < count; i += 1) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        const PaHostApiInfo *apiInfo = Pa_GetHostApiInfo(info->hostApi);
        QString caption = QObject::tr("%1 (%2, %3 in, %4 out)").arg(
                    info->name, apiInfo->name,
                    QString::number(info->maxInputChannels),
                    QString::number(info->maxOutputChannels));
        if (i == defaultInDev) {
            caption.append(QObject::tr(" (default input)"));
        }
        if (i == defaultOutDev) {
            caption.append(QObject::tr(" (default output)"));
        }
        deviceNames.append(caption);
    }
}

QStringList PlaybackModule::devices()
{
    return deviceNames;
}

int PlaybackModule::defaultInputDeviceIndex()
{
    return defaultInDev;
}

int PlaybackModule::defaultOutputDeviceIndex()
{
    return defaultOutDev;
}

void PlaybackModule::cleanupStream() {
    if (!stream) return;

    Pa_AbortStream(stream);
    stream = NULL;
}

int PlaybackModule::reopenStream()
{
    if (!stream) return 0;
    return openStream();
}

int PlaybackModule::openStream()
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

double PlaybackModule::latency()
{
    return currentLatency;
}

int PlaybackModule::setLatency(double latency)
{
    currentLatency = latency;
    int err = 0;
    foreach(PlaybackModule *module, playbackModules) {
        err = err || module->reopenStream();
    }
    return err;
}


int PlaybackModule::playProject(Project *project, int sampleIndex)
{
    // TODO loop over all the modules in project and compile a list of output devices used.
    // next loop over playbackStreams. If any devices are the same, and the stream settings
    // are different, close the stream and re-open it with the new settings. If the devices
    // are the same and the stream settings are the same, steal the stream from that PlaybackModule
    // and give it to ourselves. Then flush the output buffer.
    // open new streams for any new devices.
    return 0;
}

void PlaybackModule::pausePlayback()
{

}

void PlaybackModule::rawRender(int frameCount)
{

}
