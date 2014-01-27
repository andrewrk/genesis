#include "project.h"

Project::Project() :
    tempo("Tempo", 20, 1000, 130)
{
    tempo.setInt(true);

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

void Project::addRootModule(Module *module)
{
    rootModules.insert(module);
}
