#ifndef PROJECT_H
#define PROJECT_H

#include "channel_layout.h"

#include "module.h"
#include "event.h"

#include <QMap>
#include <QSet>

class Project
{
public:
    Project();

    uint64_t getChannelLayout() const { return channelLayout; }
    void setChannelLayout(uint64_t layout);

    int getSampleRate() const { return sampleRate; }
    void setSampleRate(int rate);

private:
    int sampleRate;
    int channelLayout;

    QSet<Module *> rootModules;

    QMap<int, Event*> events;
};

#endif // PROJECT_H
