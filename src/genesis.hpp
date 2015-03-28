#ifndef GENESIS_HPP
#define GENESIS_HPP

#include "genesis.h"
#include "list.hpp"
#include "audio_hardware.hpp"
#include "midi_hardware.hpp"
#include "threads.hpp"

#include <atomic>
using std::atomic_bool;
using std::atomic_flag;

struct GenesisContext {
    AudioHardware audio_hardware;
    void (*devices_change_callback)(void *userdata);
    void *devices_change_callback_userdata;

    MidiHardware *midi_hardware;
    void (*midi_change_callback)(void *userdata);
    void *midi_change_callback_userdata;

    MutexCond events_cond;
    Mutex events_mutex;

    List<GenesisAudioFileFormat*> out_formats;
    List<GenesisAudioFileFormat*> in_formats;

    List<GenesisNodeDescriptor*> node_descriptors;
    List<GenesisNode*> nodes;

    List<Thread> thread_pool;
    Thread manager_thread;
    atomic_bool pipeline_shutdown;
    RingBuffer *task_queue;
    MutexCond task_cond;
    Mutex task_mutex;

    GenesisContext() : audio_hardware(this) {}
};


#endif
