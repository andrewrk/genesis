#ifndef PROJECT_H
#define PROJECT_H

#include "channel_layout.h"

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
};

#endif // PROJECT_H
