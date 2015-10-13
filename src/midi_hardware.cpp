#include "midi_hardware.hpp"
#include "util.hpp"
#include "error.h"
#include "genesis.hpp"
#include "limits.h"

static void default_on_buffer_overrun(struct MidiHardware *midi_hardware) {
    fprintf(stderr, "MIDI buffer overrun\n");
}

void genesis_midi_device_unref(struct GenesisMidiDevice *device) {
    if (device) {
        device->ref_count -= 1;
        if (device->ref_count < 0)
            panic("negative ref count");

        if (device->ref_count == 0) {
            close_midi_device(device);
            free(device->client_name);
            free(device->port_name);
            destroy(device, 1);
        }
    }
}

void genesis_midi_device_ref(struct GenesisMidiDevice *device) {
    device->ref_count += 1;
}

static void destroy_devices_info(MidiDevicesInfo *devices_info) {
    if (devices_info) {
        for (int i = 0; i < devices_info->devices.length(); i += 1)
            genesis_midi_device_unref(devices_info->devices.at(i));
        destroy(devices_info, 1);
    }
}

void destroy_midi_hardware(struct MidiHardware *midi_hardware) {
    if (midi_hardware) {
        if (midi_hardware->thread) {
            // send dummy event to make thread wake up from blocking
            midi_hardware->quit_flag = true;
            snd_seq_event_t ev;
            snd_seq_ev_clear(&ev);
            ev.source.client = midi_hardware->client_id;
            ev.source.port = 0;
            snd_seq_ev_set_subs(&ev);
            snd_seq_ev_set_direct(&ev);
            ev.type = SND_SEQ_EVENT_USR0;
            int err = snd_seq_event_output(midi_hardware->seq, &ev);
            if (err < 0)
                panic("unable to send event: %s\n", snd_strerror(err));
            err = snd_seq_drain_output(midi_hardware->seq);
            if (err < 0)
                panic("unable to drain output: %s\n", snd_strerror(err));
            os_thread_destroy(midi_hardware->thread);
        }

        destroy_devices_info(midi_hardware->current_devices_info);
        destroy_devices_info(midi_hardware->ready_devices_info);
        genesis_midi_device_unref(midi_hardware->system_announce_device);
        genesis_midi_device_unref(midi_hardware->system_timer_device);

        if (midi_hardware->seq) {
            if (midi_hardware->client_info)
                snd_seq_client_info_free(midi_hardware->client_info);
            if (midi_hardware->port_info)
                snd_seq_port_info_free(midi_hardware->port_info);
            snd_seq_close(midi_hardware->seq);
        }

        os_mutex_destroy(midi_hardware->mutex);

        destroy(midi_hardware, 1);
    }
}

static int midi_hardware_refresh(MidiHardware *midi_hardware) {
    MidiDevicesInfo *info = create_zero<MidiDevicesInfo>();
    if (!info) {
        destroy_devices_info(info);
        return GenesisErrorNoMem;
    }

    info->devices.clear();
    info->default_device_index = -1;

    // don't default to MIDI through except as a last resort
    bool default_is_midi_through = false;

    snd_seq_client_info_set_client(midi_hardware->client_info, -1);
    while (snd_seq_query_next_client(midi_hardware->seq, midi_hardware->client_info) >= 0) {
        int client = snd_seq_client_info_get_client(midi_hardware->client_info);

        snd_seq_port_info_set_client(midi_hardware->port_info, client);
        snd_seq_port_info_set_port(midi_hardware->port_info, -1);
        while (snd_seq_query_next_port(midi_hardware->seq, midi_hardware->port_info) >= 0) {
            if ((snd_seq_port_info_get_capability(midi_hardware->port_info)
                & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
                != (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
            {
                continue;
            }

            GenesisMidiDevice *device = create_zero<GenesisMidiDevice>();
            if (!device) {
                genesis_midi_device_unref(device);
                return GenesisErrorNoMem;
            }
            device->midi_hardware = midi_hardware;
            device->ref_count = 1;
            device->set_index = -1;
            device->client_id = snd_seq_port_info_get_client(midi_hardware->port_info);
            device->port_id = snd_seq_port_info_get_port(midi_hardware->port_info);
            device->client_name = strdup(snd_seq_client_info_get_name(midi_hardware->client_info));
            device->port_name = strdup(snd_seq_port_info_get_name(midi_hardware->port_info));
            if (!device->client_name || !device->port_name) {
                genesis_midi_device_unref(device);
                return GenesisErrorNoMem;
            }
            if (strcmp(device->client_name, "System") == 0 &&
                strcmp(device->port_name, "Timer") == 0)
            {
                genesis_midi_device_unref(midi_hardware->system_timer_device);
                midi_hardware->system_timer_device = device;
            } else if (strcmp(device->client_name, "System") == 0 &&
                       strcmp(device->port_name, "Announce") == 0)
            {
                genesis_midi_device_unref(midi_hardware->system_announce_device);
                midi_hardware->system_announce_device = device;
            } else {
                if (info->default_device_index == -1 || default_is_midi_through) {
                    info->default_device_index = info->devices.length();
                    default_is_midi_through = strcmp(device->client_name, "Midi Through") == 0;
                }
                int err = info->devices.append(device);
                if (err) {
                    genesis_midi_device_unref(device);
                    return GenesisErrorNoMem;
                }
            }
        }
    }

    os_mutex_lock(midi_hardware->mutex);

    MidiDevicesInfo *old_devices_info = midi_hardware->ready_devices_info;
    midi_hardware->ready_devices_info = info;

    os_mutex_unlock(midi_hardware->mutex);

    destroy_devices_info(old_devices_info);

    midi_hardware->events_signal(midi_hardware);

    return 0;
}

static void dispatch_event(GenesisMidiDevice *device, snd_seq_event_t *event) {
    GenesisMidiEvent midi_event;
    switch (event->type) {
        case SND_SEQ_EVENT_NOTEON:
            midi_event.data.note_data.note = event->data.note.note;
            if (event->data.note.velocity == 0) {
                midi_event.event_type = GenesisMidiEventTypeNoteOff;
                midi_event.data.note_data.velocity = 0.0f;
            } else {
                midi_event.event_type = GenesisMidiEventTypeNoteOn;
                midi_event.data.note_data.velocity = event->data.note.velocity / (float)CHAR_MAX;
            }
            break;
        case SND_SEQ_EVENT_NOTEOFF:
            midi_event.event_type = GenesisMidiEventTypeNoteOff;
            midi_event.data.note_data.note = event->data.note.note;
            midi_event.data.note_data.velocity = event->data.note.off_velocity;
            break;
        case SND_SEQ_EVENT_PITCHBEND:
            midi_event.event_type = GenesisMidiEventTypePitch;
            midi_event.data.pitch_data.pitch = event->data.control.value / 8192.0f;
            break;
        default:
            return;
    }
    device->on_event(device, &midi_event);
}

static void midi_thread(void *userdata) {
    MidiHardware *midi_hardware = reinterpret_cast<MidiHardware*>(userdata);
    for (;;) {
        snd_seq_event_t *event;
        int err = snd_seq_event_input(midi_hardware->seq, &event);
        if (midi_hardware->quit_flag)
            return;
        if (err < 0) {
            if (err == ENOSPC) {
                midi_hardware->on_buffer_overrun(midi_hardware);
            } else {
                // docs say ENOSPC is the only error that can occur in
                // blocking mode.
                panic("unexpected ALSA MIDI error: %s", snd_strerror(err));
            }
            continue;
        }
        if (event->source.client == midi_hardware->system_announce_device->client_id &&
            event->source.port == midi_hardware->system_announce_device->port_id)
        {
            if (event->type == SND_SEQ_EVENT_PORT_START ||
                event->type == SND_SEQ_EVENT_PORT_EXIT)
            {
                midi_hardware_refresh(midi_hardware);
            }
        } else {
            for (int i = 0; i < midi_hardware->open_devices.length(); i += 1) {
                GenesisMidiDevice *device = midi_hardware->open_devices.at(i);
                if (device->client_id == event->source.client &&
                    device->port_id == event->source.port)
                {
                    dispatch_event(device, event);
                    break;
                }
            }
        }
    }
}

void midi_hardware_flush_events(MidiHardware *midi_hardware) {
    MidiDevicesInfo *old_devices_info = nullptr;
    bool change = false;

    os_mutex_lock(midi_hardware->mutex);

    if (midi_hardware->ready_devices_info) {
        old_devices_info = midi_hardware->current_devices_info;
        midi_hardware->current_devices_info = midi_hardware->ready_devices_info;
        midi_hardware->ready_devices_info = nullptr;
        change = true;
    }

    os_mutex_unlock(midi_hardware->mutex);

    destroy_devices_info(old_devices_info);

    if (change)
        midi_hardware->on_devices_change(midi_hardware);
}

int create_midi_hardware(GenesisContext *context,
        const char *client_name,
        void (*events_signal)(struct MidiHardware *),
        void (*on_devices_change)(struct MidiHardware *),
        void *userdata,
        struct MidiHardware **out_midi_hardware)
{
    *out_midi_hardware = nullptr;

    struct MidiHardware *midi_hardware = create_zero<MidiHardware>();
    if (!midi_hardware) {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorNoMem;
    }
    midi_hardware->context = context;
    midi_hardware->on_buffer_overrun = default_on_buffer_overrun;
    midi_hardware->events_signal = events_signal;
    midi_hardware->on_devices_change = on_devices_change;
    midi_hardware->userdata = userdata;

    if (!(midi_hardware->mutex = os_mutex_create())) {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorNoMem;
    }

    if (snd_seq_open(&midi_hardware->seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorOpeningMidiHardware;
    }

    if (snd_seq_set_client_name(midi_hardware->seq, client_name) < 0) {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorOpeningMidiHardware;
    }

    snd_seq_client_info_malloc(&midi_hardware->client_info);
    snd_seq_port_info_malloc(&midi_hardware->port_info);

    if (!midi_hardware->client_info || !midi_hardware->port_info) {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorNoMem;
    }

    if (snd_seq_create_simple_port(midi_hardware->seq, "genesis",
                SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION) < 0)
    {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorOpeningMidiHardware;
    }

    int err = midi_hardware_refresh(midi_hardware);
    if (err) {
        destroy_midi_hardware(midi_hardware);
        return err;
    }

    if (!midi_hardware->system_announce_device) {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorOpeningMidiHardware;
    }

    if (snd_seq_connect_from(midi_hardware->seq, 0,
                midi_hardware->system_announce_device->client_id,
                midi_hardware->system_announce_device->port_id) < 0)
    {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorOpeningMidiHardware;
    }

    if (snd_seq_get_client_info(midi_hardware->seq, midi_hardware->client_info) < 0) {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorOpeningMidiHardware;
    }

    midi_hardware->client_id = snd_seq_client_info_get_client(midi_hardware->client_info);
    if (snd_seq_connect_from(midi_hardware->seq, 0, midi_hardware->client_id, 0) < 0) {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorOpeningMidiHardware;
    }

    if ((err = os_thread_create(midi_thread, midi_hardware, false, &midi_hardware->thread))) {
        destroy_midi_hardware(midi_hardware);
        return err;
    }

    *out_midi_hardware = midi_hardware;
    return 0;
}

void genesis_refresh_midi_devices(struct GenesisContext *context) {
    midi_hardware_flush_events(context->midi_hardware);
}

int genesis_get_midi_device_count(struct GenesisContext *context) {
    return context->midi_hardware->current_devices_info->devices.length();
}

struct GenesisMidiDevice *genesis_get_midi_device(struct GenesisContext *context, int index) {
    struct GenesisMidiDevice *device = context->midi_hardware->current_devices_info->devices.at(index);
    genesis_midi_device_ref(device);
    return device;
}

int genesis_get_default_midi_device_index(struct GenesisContext *context) {
    return context->midi_hardware->current_devices_info->default_device_index;
}

const char *genesis_midi_device_name(const struct GenesisMidiDevice *device) {
    return device->port_name;
}

const char *genesis_midi_device_description(const struct GenesisMidiDevice *device) {
    return device->port_name;
}

void genesis_set_midi_device_callback(struct GenesisContext *context,
        void (*callback)(void *userdata),
        void *userdata)
{
    context->midi_change_callback_userdata = userdata;
    context->midi_change_callback = callback;
}

int open_midi_device(struct GenesisMidiDevice *device) {
    if (device->open)
        return GenesisErrorInvalidState;

    if (snd_seq_connect_from(device->midi_hardware->seq, 0, device->client_id, device->port_id) < 0)
        return GenesisErrorOpeningMidiHardware;
    device->open = true;

    int set_index = device->midi_hardware->open_devices.length();
    if (device->midi_hardware->open_devices.append(device)) {
        close_midi_device(device);
        return GenesisErrorNoMem;
    }
    genesis_midi_device_ref(device);
    device->set_index = set_index;

    return 0;
}

int close_midi_device(struct GenesisMidiDevice *device) {
    if (device->open) {
        if (snd_seq_disconnect_from(device->midi_hardware->seq, 0, device->client_id, device->port_id) < 0)
            return GenesisErrorOpeningMidiHardware;
        device->open = false;
    }
    if (device->set_index >= 0) {
        device->midi_hardware->open_devices.swap_remove(device->set_index);
        if (device->set_index < device->midi_hardware->open_devices.length())
            device->midi_hardware->open_devices.at(device->set_index)->set_index = device->set_index;
        device->set_index = -1;
        genesis_midi_device_unref(device);
    }
    return 0;
}

bool genesis_midi_device_equal(
        const struct GenesisMidiDevice *a,
        const struct GenesisMidiDevice *b)
{
    return a->client_id == b->client_id &&
        a->port_id == b->port_id;
}
