#ifndef DRAG_EVENT_HPP
#define DRAG_EVENT_HPP

#include "mouse_event.hpp"

enum DragType {
    DragTypeViewTab,
};

enum DragAction {
    DragActionMove,
    DragActionDrop,
};

struct DragData {
    DragType drag_type;
    void *ptr;
    void (*destruct)(DragData *drag_data);
};

struct DragEvent {
    DragAction action;
    const MouseEvent *orig_event;
    MouseEvent mouse_event;
    const DragData *drag_data;
};

#endif
