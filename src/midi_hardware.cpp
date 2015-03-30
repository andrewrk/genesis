#include "midi_hardware.hpp"
#include "util.hpp"
#include "error.h"
#include "genesis.hpp"

static void default_on_buffer_overrun(struct MidiHardware *midi_hardware) {
    fprintf(stderr, "MIDI buffer overrun\n");
}

void destroy_midi_device(struct GenesisMidiDevice *device) {
    if (device) {
        free(device->client_name);
        free(device->port_name);
        destroy(device, 1);
    }
}

static void destroy_devices_info(MidiDevicesInfo *devices_info) {
    if (devices_info) {
        for (int i = 0; i < devices_info->devices.length(); i += 1)
            destroy_midi_device(devices_info->devices.at(i));
    }
}

void destroy_midi_hardware(struct MidiHardware *midi_hardware) {
    if (midi_hardware) {
        if (midi_hardware->thread.running()) {
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
            midi_hardware->thread.join();
        }

        destroy_devices_info(midi_hardware->current_devices_info);
        destroy_devices_info(midi_hardware->ready_devices_info);
        destroy_midi_device(midi_hardware->system_announce_device);
        destroy_midi_device(midi_hardware->system_timer_device);

        if (midi_hardware->seq) {
            if (midi_hardware->client_info)
                snd_seq_client_info_free(midi_hardware->client_info);
            if (midi_hardware->port_info)
                snd_seq_port_info_free(midi_hardware->port_info);
            snd_seq_close(midi_hardware->seq);
        }

        destroy(midi_hardware, 1);
    }
}

static int midi_hardware_refresh(MidiHardware *midi_hardware) {
    MidiDevicesInfo *info = allocate_zero<MidiDevicesInfo>(1);
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

            GenesisMidiDevice *device = allocate_zero<GenesisMidiDevice>(1);
            if (!device) {
                destroy_midi_device(device);
                return GenesisErrorNoMem;
            }
            device->midi_hardware = midi_hardware;
            device->client_id = snd_seq_port_info_get_client(midi_hardware->port_info);
            device->port_id = snd_seq_port_info_get_port(midi_hardware->port_info);
            device->client_name = strdup(snd_seq_client_info_get_name(midi_hardware->client_info));
            device->port_name = strdup(snd_seq_port_info_get_name(midi_hardware->port_info));
            if (!device->client_name || !device->port_name) {
                destroy_midi_device(device);
                return GenesisErrorNoMem;
            }
            if (strcmp(device->client_name, "System") == 0 &&
                strcmp(device->port_name, "Timer") == 0)
            {
                destroy_midi_device(midi_hardware->system_timer_device);
                midi_hardware->system_timer_device = device;
            } else if (strcmp(device->client_name, "System") == 0 &&
                       strcmp(device->port_name, "Announce") == 0)
            {
                destroy_midi_device(midi_hardware->system_announce_device);
                midi_hardware->system_announce_device = device;
            } else {
                if (info->default_device_index == -1 || default_is_midi_through) {
                    info->default_device_index = info->devices.length();
                    default_is_midi_through = strcmp(device->client_name, "Midi Through") == 0;
                }
                int err = info->devices.append(device);
                if (err) {
                    destroy_midi_device(device);
                    return GenesisErrorNoMem;
                }
            }
        }
    }

    midi_hardware->mutex.lock();

    MidiDevicesInfo *old_devices_info = midi_hardware->ready_devices_info;
    midi_hardware->ready_devices_info = info;

    midi_hardware->mutex.unlock();

    destroy_devices_info(old_devices_info);

    midi_hardware->events_signal(midi_hardware);

    return 0;
}

static void midi_thread(void *userdata) {
    MidiHardware *midi_hardware = reinterpret_cast<MidiHardware*>(userdata);
    for (;;) {
        snd_seq_event_t *event;
        int err = snd_seq_event_input(midi_hardware->seq, &event);
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
        if (midi_hardware->quit_flag)
            return;
        if (event->source.client == midi_hardware->system_announce_device->client_id &&
            event->source.port == midi_hardware->system_announce_device->port_id)
        {
            if (event->type == SND_SEQ_EVENT_PORT_START ||
                event->type == SND_SEQ_EVENT_PORT_EXIT)
            {
                midi_hardware_refresh(midi_hardware);
            }
        } else {
            fprintf(stderr, "TODO handle midi event\n");
        }
    }
}

void midi_hardware_flush_events(MidiHardware *midi_hardware) {
    MidiDevicesInfo *old_devices_info = nullptr;
    bool change = false;

    midi_hardware->mutex.lock();

    if (midi_hardware->ready_devices_info) {
        old_devices_info = midi_hardware->current_devices_info;
        midi_hardware->current_devices_info = midi_hardware->ready_devices_info;
        midi_hardware->ready_devices_info = nullptr;
        change = true;
    }

    midi_hardware->mutex.unlock();

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

    struct MidiHardware *midi_hardware = allocate_zero<MidiHardware>(1);
    if (!midi_hardware) {
        destroy_midi_hardware(midi_hardware);
        return GenesisErrorNoMem;
    }
    midi_hardware->context = context;
    midi_hardware->on_buffer_overrun = default_on_buffer_overrun;
    midi_hardware->events_signal = events_signal;
    midi_hardware->on_devices_change = on_devices_change;
    midi_hardware->userdata = userdata;

    if (midi_hardware->mutex.error()) {
        destroy_midi_hardware(midi_hardware);
        return midi_hardware->mutex.error();
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

    err = midi_hardware->thread.start(midi_thread, midi_hardware);
    if (err) {
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
    return context->midi_hardware->current_devices_info->devices.at(index);
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

struct GenesisMidiDevice *duplicate_midi_device(struct GenesisMidiDevice *midi_device) {
    GenesisMidiDevice *new_device = create_zero<GenesisMidiDevice>();
    if (!new_device)
        return nullptr;

    new_device->midi_hardware = midi_device->midi_hardware;
    new_device->client_id = midi_device->client_id;
    new_device->port_id = midi_device->port_id;
    new_device->client_name = strdup(midi_device->client_name);
    new_device->port_name = strdup(midi_device->port_name);
    if (!new_device->client_name || !new_device->port_name) {
        destroy(new_device, 1);
        return nullptr;
    }
    return new_device;
}
