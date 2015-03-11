#ifndef MOUSE_EVENT_HPP
#define MOUSE_EVENT_HPP

enum MouseButton {
    MouseButtonNone,
    MouseButtonLeft,
    MouseButtonMiddle,
    MouseButtonRight,
};

enum MouseAction {
    MouseActionMove,
    MouseActionDown,
    MouseActionUp,
    MouseActionDbl,
};

struct MouseButtons {
    unsigned left   : 1;
    unsigned middle : 1;
    unsigned right  : 1;
};

struct MouseEvent {
    int x;
    int y;
    MouseButton button;
    MouseAction action;
    MouseButtons buttons;
    int modifiers;
};

struct MouseWheelEvent {
    int x;
    int y;
    int wheel_x;
    int wheel_y;
    int modifiers;
};

#endif
