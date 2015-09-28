#ifndef GENESIS_HPP
#define GENESIS_HPP

#include "genesis.h"
#include "list.hpp"
#include "midi_hardware.hpp"
#include "os.hpp"
#include "thread_safe_queue.hpp"
#include "ring_buffer.hpp"
#include "atomic_double.hpp"
#include "atomics.hpp"

#include <soundio/soundio.h>

struct GenesisContext {
    SoundIo *soundio;
    int soundio_connect_err;
    void (*devices_change_callback)(void *userdata);
    void *devices_change_callback_userdata;

    MidiHardware *midi_hardware;
    void (*midi_change_callback)(void *userdata);
    void *midi_change_callback_userdata;

    OsCond *events_cond;
    OsMutex *events_mutex;
    void (*event_callback)(void *userdata);
    void *event_callback_userdata;

    void (*underrun_callback)(void *userdata);
    void *underrun_callback_userdata;
    atomic_flag underrun_flag;

    List<GenesisAudioFileFormat*> out_formats;
    List<GenesisAudioFileFormat*> in_formats;

    List<GenesisNodeDescriptor*> node_descriptors;
    List<GenesisNode*> nodes;

    OsThread **thread_pool;
    int thread_pool_size;
    atomic_bool pipeline_running;
    ThreadSafeQueue<GenesisNode *> task_queue;

    double latency;

    // The sample rate that we use if a range of sample rates are available. For example
    // if a device supports 44100 - 96000, and target_sample_rate is 48000, then 48000
    // is chosen. Otherwise the closest supported rate is chosen.
    int target_sample_rate;
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
    struct SoundIoChannelLayout channel_layout;

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
    int (*activate)(struct GenesisNode *node);
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
    struct SoundIoChannelLayout channel_layout;
    int sample_rate;
    RingBuffer sample_buffer;
    int sample_buffer_err;
    int sample_buffer_size; // in bytes
    int bytes_per_frame;
};

struct GenesisEventsPort {
    struct GenesisPort port;
    RingBuffer event_buffer;
    int event_buffer_err;
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
