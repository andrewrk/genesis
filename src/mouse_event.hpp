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
};

struct MouseButtons {
    bool left;
    bool middle;
    bool right;
};

struct MouseEvent {
    int x;
    int y;
    MouseButton button;
    MouseAction action;
    MouseButtons buttons;
    int modifiers;
    int dbl_click_count;
};

struct MouseWheelEvent {
    int x;
    int y;
    int wheel_x;
    int wheel_y;
    int modifiers;
};

#endif
