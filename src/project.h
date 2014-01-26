#ifndef PROJECT_H
#define PROJECT_H

class Project
{
public:
    Project();

    enum ChannelLayout {
        Mono,
        Stereo,
        Surround51,
    };

    int getChannelLayout() const { return channelLayout; }
    void setChannelLayout(int layout);

    int getSampleRate() const { return sampleRate; }
    void setSampleRate(int rate);

private:
    int sampleRate;
    int channelLayout;
};

#endif // PROJECT_H
