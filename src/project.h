#ifndef PROJECT_H
#define PROJECT_H

#include "channel_layout.h"

#include "module.h"
#include "event.h"
#include "parameter.h"

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

    void addRootModule(Module *module);

private:
    int sampleRate;
    int channelLayout;

    Parameter tempo;

    QSet<Module *> rootModules;

    // indexed by sample frame index
    QMap<int, Event*> events;
};

#endif // PROJECT_H
