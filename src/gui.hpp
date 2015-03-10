#ifndef GUI_HPP
#define GUI_HPP

#include "shader_program_manager.hpp"
#include "freetype.hpp"
#include "list.hpp"
#include "glm.hpp"
#include "hash_map.hpp"
#include "string.hpp"
#include "font_size.hpp"
#include "resource_bundle.hpp"
#include "spritesheet.hpp"
#include "audio_hardware.hpp"
#include "gui_window.hpp"
#include "static_geometry.hpp"
#include "vertex_array.hpp"

#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <SDL2/SDL.h>

uint32_t hash_int(const int &x);

class GlobalSdlContext {
public:
    GlobalSdlContext();
    ~GlobalSdlContext();
private:
    GlobalSdlContext(const GlobalSdlContext &copy) = delete;
    GlobalSdlContext &operator=(const GlobalSdlContext &copy) = delete;
};

class AudioHardware;
class Gui {
public:
    Gui(ResourceBundle *resource_bundle);
    ~Gui();

    void exec();

    GuiWindow *create_window(bool with_borders);
    void destroy_window(GuiWindow *window);

    FontSize *get_font_size(int font_size);

    void fill_rect(GuiWindow *window, const glm::vec4 &color, const glm::mat4 &mvp);
    void draw_image(GuiWindow *window, const SpritesheetImage *img, const glm::mat4 &mvp);

    void start_text_editing(int x, int y, int w, int h);
    void stop_text_editing();


    void set_clipboard_string(const String &str);
    String get_clipboard_string() const;
    bool clipboard_has_string() const;

    bool _running;
    List<GuiWindow*> _window_list;
    List<VertexArray*> _vertex_array_list;
    GuiWindow *_focus_window;

    GlobalSdlContext _global_sdl_context;
    // utility window has 2 purposes. 1. to give us an OpenGL context before a
    // real window is created, and 2. for use as a tool tip, menu, or dropdown
    GuiWindow *_utility_window;
    ShaderProgramManager _shader_program_manager;
    StaticGeometry _static_geometry;

    AudioHardware _audio_hardware;

    SDL_Cursor* _cursor_ibeam;
    SDL_Cursor* _cursor_default;

    FT_Library _ft_library;
    FT_Face _default_font_face;

    // key is font size
    HashMap<int, FontSize *, hash_int> _font_size_cache;

    ResourceBundle *_resource_bundle;
    ByteBuffer _default_font_buffer;


    Spritesheet _spritesheet;

    const SpritesheetImage *_img_entry_dir;
    const SpritesheetImage *_img_entry_file;
    const SpritesheetImage *_img_null;

private:
    GuiWindow * get_window_from_sdl_id(Uint32 id);
    VertexArray _primitive_vertex_array;

    static KeyModifiers get_key_modifiers();
    void init_primitive_vertex_array();

    static void init_primitive_vertex_array(void *userdata) {
        return static_cast<Gui*>(userdata)->init_primitive_vertex_array();
    }

    Gui(const Gui &copy) = delete;
    Gui &operator=(const Gui &copy) = delete;

};

#endif
