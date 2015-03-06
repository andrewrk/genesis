#ifndef GENESIS_EDITOR
#define GENESIS_EDITOR

#include "resource_bundle.hpp"
#include "key_event.hpp"
#include "find_file_widget.hpp"
#include "shader_program_manager.hpp"
#include "audio_hardware.hpp"

#include <SDL2/SDL.h>


class Gui;
class AudioEditWidget;
class GenesisEditor {
public:
    GenesisEditor();
    ~GenesisEditor();
    GenesisEditor(const GenesisEditor &copy) = delete;
    GenesisEditor &operator=(const GenesisEditor &copy) = delete;

    void edit_file(const char *filename);

    void exec();

private:
    ResourceBundle _resource_bundle;
    Gui *_gui;
    SDL_Window *_window;
    SDL_GLContext _context;

    FindFileWidget *_find_file_widget;
    AudioEditWidget *_audio_edit_widget;
    ShaderProgramManager *_shader_program_manager;
    AudioHardware _audio_hardware;

    bool on_gui_key(Gui *gui, const KeyEvent *event);

    void destroy_find_file_widget();
    void destroy_audio_edit_widget();

    void on_choose_file(const ByteBuffer &file_path);
    void on_choose_save_file(const ByteBuffer &file_path);
    void show_open_file();
    void show_save_file();
    void ensure_find_file_widget();

    static bool static_on_gui_key(Gui *gui, const KeyEvent *event) {
        return reinterpret_cast<GenesisEditor*>(gui->_userdata)->on_gui_key(gui, event);
    }

    static void on_choose_file(FindFileWidget *find_file_widget, const ByteBuffer &file_path) {
        return reinterpret_cast<GenesisEditor*>(find_file_widget->_userdata)->on_choose_file(file_path);
    }

    static void on_choose_save_file(FindFileWidget *find_file_widget, const ByteBuffer &file_path) {
        return reinterpret_cast<GenesisEditor*>(find_file_widget->_userdata)->on_choose_save_file(file_path);
    }

};

#endif
