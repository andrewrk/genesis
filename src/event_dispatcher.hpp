#ifndef EVENT_DISPATCHER_HPP
#define EVENT_DISPATCHER_HPP

#include "util.hpp"

enum Event {
    EventProjectUndoChanged,
    EventProjectUsersChanged,
    EventProjectTracksChanged,
    EventProjectCommandsChanged,
    EventWindowClose,
    EventWindowPosChange,
    EventWindowSizeChange,
    EventAudioDeviceChange,
    EventMidiDeviceChange,
    EventFlushEvents,
    EventPerspectiveChange,
    EventScrollValueChange,
    EventProjectAudioAssetsChanged,
    EventProjectAudioClipsChanged,
    EventProjectAudioClipSegmentsChanged,
    EventProjectPlayHeadChanged,
    EventProjectPlayingChanged,
    EventProjectMixerLinesChanged,
    EventProjectEffectsChanged,
    EventBufferUnderrun,
    EventSoundBackendDisconnected,
    EventDeviceDesignationChange,
    EventProjectSampleRateChanged,
    EventProjectChannelLayoutChanged,
    EventSelectedIndexChanged,
    EventActivate,
    EventSettingsDefaultRenderFormatChanged,
    EventSettingsDefaultRenderSampleFormatChanged,
    EventSettingsDefaultRenderBitRateChanged,
};

struct EventHandler {
    Event event;
    void (*fn)(Event, void *);
    void *userdata;
};

class EventDispatcher {
public:
    void attach_handler(Event event, void (*fn)(Event, void *), void *userdata) {
        ok_or_panic(event_handlers.add_one());
        EventHandler *event_handler = &event_handlers.last();
        event_handler->event = event;
        event_handler->fn = fn;
        event_handler->userdata = userdata;
    }

    void detach_handler(Event event, void (*fn)(Event, void *)) {
        for (int i = 0; i < event_handlers.length(); i += 1) {
            EventHandler *event_handler = &event_handlers.at(i);
            if (event_handler->event == event && event_handler->fn == fn) {
                event_handlers.swap_remove(i);
                return;
            }
        }
        panic("event handler not attached");
    }

    void trigger(Event event) {
        // we use the stack here because we want to call this function in the main loop
        // need the memory on the stack because calling a handler might destroy this class
        static const int MAX_HANDLERS = 256;
        EventHandler * handlers[MAX_HANDLERS];
        int handler_index = 0;

        for (int i = 0; i < event_handlers.length(); i += 1) {
            EventHandler *handler = &event_handlers.at(i);
            if (handler->event == event) {
                handlers[handler_index++] = handler;
                assert(handler_index <= MAX_HANDLERS);
            }
        }
        // we use a deferred list like this in case any event handlers
        // destroy this EventDispatcher
        for (int i = 0; i < handler_index; i += 1) {
            EventHandler *handler = handlers[i];
            handler->fn(handler->event, handler->userdata);
        }
    }

    List<EventHandler> event_handlers;

};

#endif
