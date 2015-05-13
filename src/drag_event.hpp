#ifndef DRAG_EVENT_HPP
#define DRAG_EVENT_HPP

#include "mouse_event.hpp"

enum DragType {
    DragTypeViewTab,
    DragTypeSampleFile,
};

enum DragAction {
    DragActionMove,
    DragActionDrop,
    DragActionOut,
};

struct DragData {
    DragType drag_type;
    void *ptr;
    void (*destruct)(DragData *drag_data);
};

struct DragEvent {
    DragAction action;
    MouseEvent orig_event;
    MouseEvent mouse_event;
    const DragData *drag_data;
};

#endif
