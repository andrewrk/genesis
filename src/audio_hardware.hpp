#ifndef AUDIO_HARDWARE_HPP
#define AUDIO_HARDWARE_HPP

class AudioHardware {
public:
    AudioHardware();
    ~AudioHardware();

    void rescan_devices();

private:
    bool _initialized;
    int _device_count;

    void deinitialize();
    void initialize();

    AudioHardware(const AudioHardware &copy) = delete;
    AudioHardware &operator=(const AudioHardware &copy) = delete;
};

#endif
