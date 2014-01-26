#ifndef SOUNDENGINE_H
#define SOUNDENGINE_H

#include <QString>

#include "channel_layout.h"

namespace SoundEngine
{
    int initialize();

    int openStream();

	QString errorString(int err);

    void refreshBackends();

    QStringList backends();
    int defaultBackendIndex();

    int selectedBackendIndex();
    int sampleRate();
    uint64_t channelLayout();
    double latency();

    int setSoundParams(int backendIndex, int sampleRate, uint64_t channelLayout, double latency);
}

#endif // SOUNDENGINE_H
