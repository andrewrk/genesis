#ifndef GENESIS_EDITOR
#define GENESIS_EDITOR

#include "resource_bundle.hpp"
#include "key_event.hpp"
#include "find_file_widget.hpp"

#include <SDL2/SDL.h>

class Gui;
class GenesisEditor {
public:
    GenesisEditor();
    ~GenesisEditor();
    GenesisEditor(const GenesisEditor &copy) = delete;
    GenesisEditor &operator=(const GenesisEditor &copy) = delete;

    void exec();

private:
    ResourceBundle _resource_bundle;
    Gui *_gui;
    SDL_Window *_window;
    SDL_GLContext _context;

    FindFileWidget *_find_file_widget;

    bool on_gui_key(Gui *gui, const KeyEvent *event);

    void destroy_find_file_widget();

    void on_choose_file(const ByteBuffer &file_path);

    static bool static_on_gui_key(Gui *gui, const KeyEvent *event) {
        return reinterpret_cast<GenesisEditor*>(gui->_userdata)->on_gui_key(gui, event);
    }

    static void on_choose_file(FindFileWidget *find_file_widget, const ByteBuffer &file_path) {
        return reinterpret_cast<GenesisEditor*>(find_file_widget->_userdata)->on_choose_file(file_path);
    }

};

#endif
