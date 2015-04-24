#ifndef GENESIS_EDITOR
#define GENESIS_EDITOR

#include "resource_bundle.hpp"
#include "key_event.hpp"
#include "gui.hpp"

class GuiWindow;
struct GenesisContext;
class Project;

class GenesisEditor {
public:
    GenesisEditor();
    ~GenesisEditor();

    void edit_file(const char *filename);

    void exec();
    void create_window();


    GenesisContext *_genesis_context;
    ResourceBundle _resource_bundle;
    Gui *_gui;

    List<GuiWindow *> windows;
    Project *project;

    bool on_key_event(GuiWindow *window, const KeyEvent *event);
    bool on_text_event(GuiWindow *window, const TextInputEvent *event);

    void destroy_find_file_widget();
    void destroy_audio_edit_widget();

    GenesisEditor(const GenesisEditor &copy) = delete;
    GenesisEditor &operator=(const GenesisEditor &copy) = delete;
};

#endif
