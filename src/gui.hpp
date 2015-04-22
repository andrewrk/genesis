#ifndef GUI_HPP
#define GUI_HPP

#include "shader_program_manager.hpp"
#include "freetype.hpp"
#include "list.hpp"
#include "glm.hpp"
#include "hash_map.hpp"
#include "font_size.hpp"
#include "resource_bundle.hpp"
#include "spritesheet.hpp"
#include "gui_window.hpp"
#include "static_geometry.hpp"
#include "glfw.hpp"

uint32_t hash_int(const int &x);

class GlobalGlfwContext {
public:
    GlobalGlfwContext();
    ~GlobalGlfwContext();
private:
    GlobalGlfwContext(const GlobalGlfwContext &copy) = delete;
    GlobalGlfwContext &operator=(const GlobalGlfwContext &copy) = delete;
};

class AudioHardware;
class Gui {
public:
    Gui(GenesisContext *context, ResourceBundle *resource_bundle);
    ~Gui();

    void exec();

    GuiWindow *create_window(bool with_borders);
    void destroy_window(GuiWindow *window);

    FontSize *get_font_size(int font_size);

    void draw_image(GuiWindow *window, const SpritesheetImage *img, const glm::mat4 &mvp);
    void draw_image_color(GuiWindow *window, const SpritesheetImage *img,
            const glm::mat4 &mvp, const glm::vec4 &color);

    void attach_audio_device_callback(void (*fn)(void *), void *userdata);
    void detach_audio_device_callback(void (*fn)(void *));

    void attach_midi_device_callback(void (*fn)(void *), void *userdata);
    void detach_midi_device_callback(void (*fn)(void *));

    Mutex gui_mutex;


    bool _running;
    List<GuiWindow*> _window_list;
    GuiWindow *_focus_window;

    GlobalGlfwContext _global_glfw_context;
    // utility window has 2 purposes. 1. to give us an OpenGL context before a
    // real window is created, and 2. block on draw() gives our main loop
    // something to block on
    GuiWindow *_utility_window;
    ShaderProgramManager _shader_program_manager;
    StaticGeometry _static_geometry;

    GLFWcursor* _cursor_ibeam;
    GLFWcursor* _cursor_default;

    FT_Library _ft_library;
    FT_Face _default_font_face;

    // key is font size
    HashMap<int, FontSize *, hash_int> _font_size_cache;

    ResourceBundle *_resource_bundle;
    ByteBuffer _default_font_buffer;


    Spritesheet _spritesheet;

    const SpritesheetImage *img_entry_dir;
    const SpritesheetImage *img_entry_file;
    const SpritesheetImage *img_plus;
    const SpritesheetImage *img_minus;
    const SpritesheetImage *img_null;

    GenesisContext *_genesis_context;

    struct Handler {
        void (*fn)(void *);
        void *userdata;
    };
    List<Handler> audio_device_handlers;
    List<Handler> midi_device_handlers;

    void dispatch_handlers(const List<Handler> &list);

    Gui(const Gui &copy) = delete;
    Gui &operator=(const Gui &copy) = delete;

};

#endif
