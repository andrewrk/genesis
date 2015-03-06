#ifndef AUDIO_HARDWARE_HPP
#define AUDIO_HARDWARE_HPP

#include "list.hpp"
#include "sample_format.hpp"
#include "ring_buffer.hpp"

struct AudioDevice {
    const char * name;
    int max_input_channels;
    int max_output_channels;
    double default_low_input_latency;
    double default_low_output_latency;
    double default_high_input_latency;
    double default_high_output_latency;
    double default_sample_rate;
};

class AudioHardware;
typedef void PaStream;
class OpenPlaybackDevice {
public:
    OpenPlaybackDevice(AudioHardware *audio_hardware, int device_index,
            int channel_count, SampleFormat sample_format, double latency,
            int sample_rate, bool *ok);
    ~OpenPlaybackDevice();

    RingBuffer *_ring_buffer;
    AudioHardware *_audio_hardware;
    int _channel_count;
    unsigned long _bytes_per_frame;
    PaStream *_stream;
};

class AudioHardware {
public:
    AudioHardware();
    ~AudioHardware();

    // calling this invalidates all pointers to AudioDevice instances
    void rescan_devices();

    const List<AudioDevice> *devices() const {
        return &_devices;
    }

private:
    bool _initialized;
    List<AudioDevice> _devices;
    int _default_input_index;
    int _default_output_index;

    void deinitialize();
    void initialize();

    AudioHardware(const AudioHardware &copy) = delete;
    AudioHardware &operator=(const AudioHardware &copy) = delete;
};

#endif
