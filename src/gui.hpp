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
#include "audio_hardware.hpp"
#include "gui_window.hpp"
#include "static_geometry.hpp"
#include "vertex_array.hpp"
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
    Gui(ResourceBundle *resource_bundle);
    ~Gui();

    void exec();

    GuiWindow *create_window(bool with_borders);
    void destroy_window(GuiWindow *window);

    FontSize *get_font_size(int font_size);

    void fill_rect(GuiWindow *window, const glm::vec4 &color, const glm::mat4 &mvp);
    void draw_image(GuiWindow *window, const SpritesheetImage *img, const glm::mat4 &mvp);


    GuiWindow *get_bound_window();


    bool _running;
    List<GuiWindow*> _window_list;
    List<VertexArray*> _vertex_array_list;
    GuiWindow *_focus_window;

    GlobalGlfwContext _global_glfw_context;
    // utility window has 2 purposes. 1. to give us an OpenGL context before a
    // real window is created, and 2. for use as a tool tip, menu, or dropdown
    GuiWindow *_utility_window;
    ShaderProgramManager _shader_program_manager;
    StaticGeometry _static_geometry;

    AudioHardware _audio_hardware;

    GLFWcursor* _cursor_ibeam;
    GLFWcursor* _cursor_default;

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
    VertexArray _primitive_vertex_array;

    void init_primitive_vertex_array();

    static void init_primitive_vertex_array(void *userdata) {
        return static_cast<Gui*>(userdata)->init_primitive_vertex_array();
    }

    Gui(const Gui &copy) = delete;
    Gui &operator=(const Gui &copy) = delete;

};

#endif
