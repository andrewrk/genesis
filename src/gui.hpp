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
#include "event_dispatcher.hpp"
#include "drag_event.hpp"

uint32_t hash_int(const int &x);

class GlobalGlfwContext {
public:
    GlobalGlfwContext();
    ~GlobalGlfwContext();
private:
    GlobalGlfwContext(const GlobalGlfwContext &copy) = delete;
    GlobalGlfwContext &operator=(const GlobalGlfwContext &copy) = delete;
};

class Gui {
public:
    Gui(GenesisContext *context, ResourceBundle *resource_bundle);
    ~Gui();

    void exec();

    GuiWindow *create_window(int left, int top, int width, int height);
    void destroy_window(GuiWindow *window);

    FontSize *get_font_size(int font_size);

    void draw_image(GuiWindow *window, const SpritesheetImage *img, const glm::mat4 &mvp);
    void draw_image_color(GuiWindow *window, const SpritesheetImage *img,
            const glm::mat4 &mvp, const glm::vec4 &color);

    void start_drag(GuiWindow *gui_window, const MouseEvent *event, DragData *drag_data);
    void end_drag();

    OsMutex *gui_mutex;


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

    GLFWcursor* cursor_ibeam;
    GLFWcursor* cursor_default;
    GLFWcursor* cursor_hresize;
    GLFWcursor* cursor_vresize;

    FT_Library _ft_library;
    FT_Face _default_font_face;

    // key is font size
    HashMap<int, FontSize *, hash_int> _font_size_cache;

    ResourceBundle *_resource_bundle;
    ByteBuffer _default_font_buffer;


    Spritesheet _spritesheet;

    const SpritesheetImage *img_entry_dir;
    const SpritesheetImage *img_entry_dir_open;
    const SpritesheetImage *img_entry_file;
    const SpritesheetImage *img_plus;
    const SpritesheetImage *img_minus;
    const SpritesheetImage *img_microphone;
    const SpritesheetImage *img_volume_up;
    const SpritesheetImage *img_check;
    const SpritesheetImage *img_caret_right;
    const SpritesheetImage *img_arrow_up;
    const SpritesheetImage *img_arrow_down;
    const SpritesheetImage *img_arrow_left;
    const SpritesheetImage *img_arrow_right;
    const SpritesheetImage *img_music;
    const SpritesheetImage *img_plug;
    const SpritesheetImage *img_null;

    const SpritesheetImage *img_play_head;

    GenesisContext *_genesis_context;

    EventDispatcher events;

    double fps;

    bool dragging;
    DragData *drag_data;
    GuiWindow *drag_window;
    MouseEvent drag_orig_event;

    GuiWindow *create_utility_window();
    GuiWindow *create_generic_window(bool is_utility, int left, int top, int width, int height);

    Gui(const Gui &copy) = delete;
    Gui &operator=(const Gui &copy) = delete;

};

#endif
