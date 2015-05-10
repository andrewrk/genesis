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
    EventFpsChange,
    EventPerspectiveChange,
    EventScrollValueChange,
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
        List<EventHandler *> handlers;
        for (int i = 0; i < event_handlers.length(); i += 1) {
            EventHandler *handler = &event_handlers.at(i);
            if (handler->event == event) {
                ok_or_panic(handlers.append(handler));
            }
        }
        // we use a deferred list like this in case any event handlers
        // destroy this EventDispatcher
        for (int i = 0; i < handlers.length(); i += 1) {
            EventHandler *handler = handlers.at(i);
            handler->fn(handler->event, handler->userdata);
        }
    }

    List<EventHandler> event_handlers;

};

#endif
