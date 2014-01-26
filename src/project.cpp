#include "project.h"

Project::Project()
{
    sampleRate = 44100;
    channelLayout = Stereo;
}

void Project::setChannelLayout(int layout)
{
    channelLayout = layout;
}

void Project::setSampleRate(int rate)
{
    sampleRate = rate;
}
