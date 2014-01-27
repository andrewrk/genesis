#ifndef PLAYBACKMODULE_H
#define PLAYBACKMODULE_H

#include <portaudio.h>

#include <QString>
#include <QSet>

#include "project.h"
#include "channel_layout.h"
#include "module.h"

class PlaybackModule : public Module
{
public:
    PlaybackModule(int sampleRate, uint64_t channelLayout, int deviceIndex);
    ~PlaybackModule();

    int openStream();
    void cleanupStream();
    int reopenStream();

    static QString errorString(int err);
    static int initialize();

    static void refreshDevices();
    static QStringList devices();
    static int defaultInputDeviceIndex();
    static int defaultOutputDeviceIndex();

    static double latency();
    static int setLatency(double latency);

    static int playProject(Project *project, int sampleIndex);
    static void pausePlayback();

protected:

    void rawRender(int frameCount) override;

private:
    uint64_t currentChLayout;
    int currentSampleRate;
    int currentDeviceIndex;
    PaStream *stream = NULL;
};

#endif // PLAYBACKMODULE_H
