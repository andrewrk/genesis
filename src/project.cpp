#include "project.h"

Project::Project() :
    tempo("Tempo")
{
    sampleRate = 44100;
    channelLayout = GENESIS_CH_LAYOUT_STEREO;
}

void Project::setChannelLayout(uint64_t layout)
{
    channelLayout = layout;
}

void Project::setSampleRate(int rate)
{
    sampleRate = rate;
}
