#ifndef MIDI_CONTROLLER_HPP
#define MIDI_CONTROLLER_HPP

#include "list.hpp"
#include "os.hpp"
#include "genesis.h"
#include "atomics.hpp"

#include <alsa/asoundlib.h>

struct MidiHardware;

enum GenesisMidiEventType {
    GenesisMidiEventTypeNoteOn,
    GenesisMidiEventTypeNoteOff,
    GenesisMidiEventTypePitch,
    GenesisMidiEventTypeSegment,
};

struct MidiEventNoteData {
    float velocity;
    int note;
};

struct MidiEventPitchData {
    float pitch;
};

struct MidiEventSegmentData {
    long start;
    long end;
};

struct GenesisMidiEvent {
    int event_type;
    double start; // in whole notes
    union {
        MidiEventNoteData note_data;
        MidiEventPitchData pitch_data;
        MidiEventSegmentData segment_data;
    } data;
};

struct GenesisMidiDevice {
    MidiHardware *midi_hardware;
    int client_id;
    int port_id;
    char *client_name;
    char *port_name;

    int ref_count;
    bool open;
    int set_index;
    void (*on_event)(struct GenesisMidiDevice *, const struct GenesisMidiEvent *);
    void *userdata;
};

struct MidiDevicesInfo {
    List<GenesisMidiDevice*> devices;
    int default_device_index;
};

struct MidiHardware {
    GenesisContext *context;
    snd_seq_t *seq;
    int client_id;

    // the one that the API user reads directly
    MidiDevicesInfo *current_devices_info;
    // created when changes are detected, queued up
    MidiDevicesInfo *ready_devices_info;
    GenesisMidiDevice *system_announce_device;
    GenesisMidiDevice *system_timer_device;

    OsThread *thread;
    atomic_bool quit_flag;
    OsMutex *mutex;

    void *userdata;
    void (*on_buffer_overrun)(struct MidiHardware *);
    void (*on_devices_change)(struct MidiHardware *);
    void (*events_signal)(struct MidiHardware *);

    List<GenesisMidiDevice*> open_devices;

    // utility pointers, used only within a single function
    snd_seq_client_info_t *client_info;
    snd_seq_port_info_t *port_info;
};

int create_midi_hardware(GenesisContext *context,
        const char *client_name,
        void (*events_signal)(struct MidiHardware *),
        void (*on_devices_change)(struct MidiHardware *),
        void *userdata,
        struct MidiHardware **out_midi_hardware);

void destroy_midi_hardware(struct MidiHardware *midi_hardware);

void midi_hardware_flush_events(MidiHardware *midi_hardware);

int open_midi_device(struct GenesisMidiDevice *device);
int close_midi_device(struct GenesisMidiDevice *device);

#endif
