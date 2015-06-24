#ifndef AUDIO_BACKEND_DUMMY_HPP
#define AUDIO_BACKEND_DUMMY_HPP

#include "ring_buffer.hpp"
#include "threads.hpp"
#include <atomic>
using std::atomic_flag;

struct AudioHardware;

struct OpenPlaybackDeviceDummy {
    Thread *thread;
    Mutex *mutex;
    MutexCond *cond;
    atomic_flag abort_flag;
    RingBuffer *ring_buffer;
    int buffer_size;
    double period;
};

struct OpenRecordingDeviceDummy {

};

struct AudioHardwareDummy {

};

int audio_hardware_init_dummy(AudioHardware *ah);

#endif

