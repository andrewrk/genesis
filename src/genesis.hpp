#ifndef GENESIS_HPP
#define GENESIS_HPP

#include "genesis.h"
#include "list.hpp"
#include "audio_hardware.hpp"
#include "midi_hardware.hpp"
#include "threads.hpp"

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

    List<Thread> thread_pool;

    GenesisContext() : audio_hardware(this) {}
};


#endif
