#ifndef FIND_FILE_WIDGET_HPP
#define FIND_FILE_WIDGET_HPP

#include "widget.hpp"

class FindFileWidget {
public:
    Widget _widget;

    enum Mode {
        ModeOpen,
        ModeSave,
    };

    void set_mode(Mode mode) {
        _mode = mode;
    }

    void set_pos(int new_left, int new_top) {
        _left = new_left;
        _top = new_top;
        update_model();
    }


private:
    Mode _mode;

    int _left;
    int _top;

    void update_model();
};

#endif
