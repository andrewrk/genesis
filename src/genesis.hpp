#ifndef GENESIS_HPP
#define GENESIS_HPP

#include "genesis.h"
#include "list.hpp"
#include "audio_hardware.hpp"
#include "midi_hardware.hpp"
#include "threads.hpp"
#include "thread_safe_queue.hpp"
#include "ring_buffer.hpp"
#include "atomic_double.hpp"

#include <atomic>
using std::atomic_bool;
using std::atomic_flag;

struct GenesisContext {
    AudioHardware *audio_hardware;
    void (*devices_change_callback)(void *userdata);
    void *devices_change_callback_userdata;

    MidiHardware *midi_hardware;
    void (*midi_change_callback)(void *userdata);
    void *midi_change_callback_userdata;

    MutexCond events_cond;
    Mutex events_mutex;
    void (*event_callback)(void *userdata);
    void *event_callback_userdata;

    List<GenesisAudioFileFormat*> out_formats;
    List<GenesisAudioFileFormat*> in_formats;

    List<GenesisNodeDescriptor*> node_descriptors;
    List<GenesisNode*> nodes;

    List<Thread> thread_pool;
    atomic_bool pipeline_running;
    ThreadSafeQueue<GenesisNode *> task_queue;

    double latency;
    atomic_flag underrun_flag;
};

struct GenesisPortDescriptor {
    enum GenesisPortType port_type;
    char *name;

    int (*connect)(struct GenesisPort *port, struct GenesisPort *other_port);
    void (*disconnect)(struct GenesisPort *port, struct GenesisPort *other_port);
};

struct GenesisEventsPortDescriptor {
    struct GenesisPortDescriptor port_descriptor;
};

struct GenesisAudioPortDescriptor {
    struct GenesisPortDescriptor port_descriptor;

    bool channel_layout_fixed;
    // if channel_layout_fixed is true then this is the index
    // of the other port that it is the same as, or -1 if it is fixed
    // to the value of channel_layout
    int same_channel_layout_index;
    struct GenesisChannelLayout channel_layout;

    bool sample_rate_fixed;
    // if sample_rate_fixed is true then this is the index
    // of the other port that it is the same as, or -1 if it is fixed
    // to the value of sample_rate
    int same_sample_rate_index;
    int sample_rate;
};

struct GenesisNodeDescriptor {
    struct GenesisContext *context;
    char *name;
    char *description;
    List<GenesisPortDescriptor*> port_descriptors;
    int (*create)(struct GenesisNode *node);
    void (*destroy)(struct GenesisNode *node);
    void (*run)(struct GenesisNode *node);
    void (*seek)(struct GenesisNode *node);
    void (*activate)(struct GenesisNode *node);
    void (*deactivate)(struct GenesisNode *node);
    int set_index;

    void *userdata;
    void (*destroy_descriptor)(struct GenesisNodeDescriptor *);
};

struct GenesisPort {
    struct GenesisPortDescriptor *descriptor;
    struct GenesisNode *node;
    struct GenesisPort *input_from;
    struct GenesisPort *output_to;
};

struct GenesisAudioPort {
    struct GenesisPort port;
    struct GenesisChannelLayout channel_layout;
    int sample_rate;
    RingBuffer *sample_buffer;
    int sample_buffer_size; // in bytes
    int bytes_per_frame;
};

struct GenesisEventsPort {
    struct GenesisPort port;
    RingBuffer *event_buffer;
    AtomicDouble time_available; // in whole notes
    AtomicDouble time_requested; // in whole notes
};

struct GenesisNode {
    const struct GenesisNodeDescriptor *descriptor;
    int port_count;
    struct GenesisPort **ports;
    int set_index; // index into context->nodes
    atomic_bool being_processed;
    double timestamp; // in whole notes
    void *userdata;
    bool constructed;
};

#endif
